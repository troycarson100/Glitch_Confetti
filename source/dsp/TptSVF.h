#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// TPT State-Variable Filter (ZDF/TPT) - LP/HP/BP (12/24 dB)
// Reference: Zavalishin, The Art of VA Filter Design
struct TptSVF
{
    enum Mode { LP = 0, HP = 1, BP = 2 };

    void prepare(double sampleRate, int maxBlock, int numCh = 2)
    {
        fs = sampleRate;
        invFs = 1.0 / fs;
        channels = numCh;
        
        ic1eq.assign(channels, 0.0f);
        ic2eq.assign(channels, 0.0f);
        ic1eq2.assign(channels, 0.0f); // for cascade stage
        ic2eq2.assign(channels, 0.0f);

        setCutoff(1200.0f);
        setQ(0.707f);
        setMode(LP);
        setCascade(false);
    }

    void setMode(Mode m) { mode = m; }
    void setCascade(bool on) { cascade = on; }
    
    void setCutoff(float hz)
    {
        cutoff = juce::jlimit(20.0f, 20000.0f, hz);
        const double g = std::tan(juce::MathConstants<double>::pi * cutoff * invFs);
        G = (float)g;
    }
    
    void setQ(float q)
    {
        Q = juce::jlimit(0.5f, 12.0f, q); // Clamp Q to valid range
        K = 1.0f / Q;
    }
    
    void setDrive(float dB)
    {
        // Make drive more apparent - add some saturation curve
        clampedDb = juce::jlimit(0.0f, 36.0f, dB);
        // Apply gain - convert dB to linear gain
        preGain = juce::Decibels::decibelsToGain(clampedDb);
        // Store drive amount for saturation (0-1)
        driveAmount = clampedDb / 36.0f; // 0-1
    }

    // Musical mapping for res 0..1.0 -> Q
    static inline float mapResToQ(float res)
    {
        // Clamp res to 0-1.0 and map to Q (allows up to 100% resonance)
        res = juce::jlimit(0.0f, 1.0f, res);
        // Use linear mapping for predictable, responsive control
        // Q ranges from 0.5 (min) to 12.0 (max) for full resonance control
        // Linear ensures resonance knob works across full range
        return 0.5f + (res * 11.5f); // Linear: 0.5 at res=0, 12.0 at res=1.0
    }

    void processBlock(juce::AudioBuffer<float>& io,
                      float cutoffHz, float qMapped, int slopeSel,
                      float driveDb, float spreadCents)
    {
        setCutoff(cutoffHz);
        setQ(qMapped);
        setCascade(slopeSel > 0);
        setDrive(driveDb);

        // Stereo spread by detuning cutoff per channel
        // Apply spread with aggressive scaling to make it very noticeable
        // Scale spread by 8x to make it extremely apparent (±50 cents becomes ±400 cents = 4 semitones)
        const float spreadScale = 8.0f;
        const float effectiveSpread = spreadCents * spreadScale;
        const float ratioL = centsToRatio(+effectiveSpread);
        const float ratioR = centsToRatio(-effectiveSpread);

        for (int ch = 0; ch < juce::jmin(2, io.getNumChannels()); ++ch)
        {
            auto* x = io.getWritePointer(ch);
            const float ratio = (ch == 0 ? ratioL : ratioR);
            const double g = std::tan(juce::MathConstants<double>::pi * (cutoff * ratio) * invFs);
            const float Gl = (float)g;

            // Stage 1 states
            float ic1 = ic1eq[(size_t)ch];
            float ic2 = ic2eq[(size_t)ch];
            
            // Stage 2 states (for cascade)
            float ic1b = ic1eq2[(size_t)ch];
            float ic2b = ic2eq2[(size_t)ch];

            for (int n = 0; n < io.getNumSamples(); ++n)
            {
                juce::ScopedNoDenormals nd;
                
                // Apply drive gain - only if drive > 0dB
                // At 0dB, preGain should be exactly 1.0, so we skip drive entirely
                float v0 = x[n];
                if (clampedDb > 0.01f) // Only apply if drive > 0.01dB (tiny threshold)
                {
                    v0 *= preGain;
                    // Add soft saturation when drive is applied (makes drive more apparent)
                    if (driveAmount > 0.1f)
                    {
                        float driveSat = driveAmount; // 0-1
                        // Soft saturation: blend between linear and tanh
                        v0 = v0 * (1.0f - 0.4f * driveSat) + 0.4f * driveSat * std::tanh(v0 * 1.5f);
                    }
                }
                
                // Safety check
                if (!std::isfinite(v0))
                    v0 = 0.0f;

                // SVF core (Chamberlin/Zavalishin TPT form)
                const float v1 = (v0 - ic2) / (1.0f + K * Gl + Gl * Gl);
                const float v2 = Gl * v1;
                const float v3 = Gl * v2;

                const float lp = v3 + ic2;
                const float hp = v0 - K * v2 - lp;
                const float bp = v2 + ic1;

                ic1 += 2.0f * v2;
                ic2 += 2.0f * v3;

                float y = 0.0f;
                switch (mode)
                {
                    case LP: y = lp; break;
                    case HP: y = hp; break;
                    case BP: y = bp; break;
                }
                
                // Safety check
                if (!std::isfinite(y))
                    y = 0.0f;

                if (cascade)
                {
                    // Run a second identical stage for 24 dB slope
                    const float v1b = (y - ic2b) / (1.0f + K * Gl + Gl * Gl);
                    const float v2b = Gl * v1b;
                    const float v3b = Gl * v2b;

                    const float lpb = v3b + ic2b;
                    const float hpb = y - K * v2b - lpb;
                    const float bpb = v2b + ic1b;

                    ic1b += 2.0f * v2b;
                    ic2b += 2.0f * v3b;

                    switch (mode)
                    {
                        case LP: y = lpb; break;
                        case HP: y = hpb; break;
                        case BP: y = bpb; break;
                    }
                    
                    // Safety check
                    if (!std::isfinite(y))
                        y = 0.0f;
                }

                // For HP and BP modes, add compensation at low frequencies to prevent cutting out
                // These modes naturally get quieter at low cutoff frequencies
                if (mode == HP || mode == BP)
                {
                    // Calculate normalized cutoff position (0-1) on log scale
                    float logMin = std::log10(20.0f);
                    float logMax = std::log10(20000.0f);
                    float logCutoff = std::log10(cutoff);
                    float normalizedFreq = (logCutoff - logMin) / (logMax - logMin); // 0-1
                    
                    // Boost when frequency is in the problem range (around 25%)
                    // Use a wider bell curve that provides significant boost from 10% to 40%
                    float centerFreq = 0.25f; // 25% of log scale
                    float distance = std::abs(normalizedFreq - centerFreq);
                    // Wider bell curve: max boost at center, tapering off more gradually
                    float boostAmount = 1.0f - (distance * 2.0f); // 1.0 at center, 0.0 at ±0.5
                    boostAmount = juce::jlimit(0.0f, 1.0f, boostAmount); // Clamp to 0-1
                    // Apply boost: 1.0x to 6.0x compensation at problem frequencies (more aggressive)
                    float compensation = 1.0f + (boostAmount * 5.0f);
                    y *= compensation;
                }
                
                // Soft limiter to keep resonance civilized (but less aggressive for BP)
                float limiterAmount = (mode == BP) ? 0.3f : 0.5f;
                float output = y / (1.0f + limiterAmount * std::abs(y));
                
                // Final safety check
                if (!std::isfinite(output))
                    output = 0.0f;
                
                x[n] = output;
            }

            ic1eq[(size_t)ch] = ic1;
            ic2eq[(size_t)ch] = ic2;
            ic1eq2[(size_t)ch] = ic1b;
            ic2eq2[(size_t)ch] = ic2b;
        }
    }

private:
    double fs = 48000.0;
    double invFs = 1.0 / 48000.0;
    int channels = 2;
    float cutoff = 1000.0f;
    float Q = 0.707f;
    float K = 1.4142f;
    float G = 0.0f;
    float preGain = 1.0f;
    float driveAmount = 0.0f;  // 0-1 for drive saturation
    float clampedDb = 0.0f;    // Store clamped drive value for processing
    Mode mode = LP;
    bool cascade = false;

    std::vector<float> ic1eq, ic2eq;   // Stage 1 states per channel
    std::vector<float> ic1eq2, ic2eq2; // Stage 2 states per channel

    static inline float centsToRatio(float c)
    {
        return std::pow(2.0f, c / 1200.0f);
    }
};

