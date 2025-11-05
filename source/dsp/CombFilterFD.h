#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

// Fractional-Delay Comb Filter with ±feedback, damping, micro-mod, spread
struct CombFilterFD
{
    // sign: +1 = Comb+, -1 = Comb-
    explicit CombFilterFD(int signFeedback = +1) : sign(signFeedback) {}

    void prepare(double sampleRate, int maxBlock, int numCh = 2)
    {
        fs = sampleRate;
        maxBlockSize = maxBlock;
        channels = numCh;

        for (int i = 0; i < 2; ++i)
        {
            dl[i] = std::make_unique<Delay>(int(fs * 0.5)); // Up to 0.5 sec
            dl[i]->prepare(fs, maxBlock);
            lp[i].reset();
            // Damping frequency tied to resonance/feedback to prevent hiss
            lp[i].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(fs, 10000.0f);
        }

        setTuneHz(400.0f);
        setFeedback(0.6f);
        setDepth(0.0f);
        setSpreadCents(0.0f);
        setModAmountMs(0.0f);  // Disabled - no modulation for stable comb filter
        setModRateHz(0.3f);
    }

    void setTuneHz(float hz)
    {
        tuneHz = juce::jlimit(40.0f, 8000.0f, hz);
        // For comb filters, the "tune" parameter should map to delay time
        // Map frequency range (40-8000 Hz) to delay range (1-20 ms) for musical comb filtering
        // Higher frequency = shorter delay, lower frequency = longer delay
        // Use inverse mapping: delayMs = (1 / tuneHz) scaled to 1-20ms range
        float minDelayMs = 1.0f;
        float maxDelayMs = 20.0f;
        float minFreq = 40.0f;
        float maxFreq = 8000.0f;
        
        // Map frequency to delay: lower freq -> longer delay, higher freq -> shorter delay
        float normalizedFreq = (hz - minFreq) / (maxFreq - minFreq); // 0-1
        float delayMs = maxDelayMs - (normalizedFreq * (maxDelayMs - minDelayMs)); // Inverted: 20ms at 40Hz, 1ms at 8000Hz
        delayMs = juce::jlimit(minDelayMs, maxDelayMs, delayMs);
        baseDelaySamp = (delayMs / 1000.0f) * fs;
    }

    void setFeedback(float fb)
    {
        // Limit feedback more conservatively for stability
        feedback = juce::jlimit(0.0f, 0.9f, fb);
        // Update damping based on feedback (higher feedback = more damping)
        // Reduced damping frequency to 2-5kHz for more natural sound
        float dampHz = 2000.0f + (feedback * 3000.0f); // 2-5 kHz
        setDampHz(dampHz);
    }

    void setDepth(float d)
    {
        depth = juce::jlimit(0.0f, 1.0f, d); // Feed-forward weight
    }

    void setSpreadCents(float c)
    {
        spreadCents = juce::jlimit(-50.0f, 50.0f, c);
    }

    void setDampHz(float hz)
    {
        for (int i = 0; i < 2; ++i)
        {
            lp[i].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(fs, hz);
        }
    }

    void setModAmountMs(float ms)
    {
        modAmtSamp = (ms / 1000.0f) * fs;
    }

    void setModRateHz(float r)
    {
        modRate = r;
    }

    // Process in place
    void processBlock(juce::AudioBuffer<float>& io,
                      float tuneHz, float feedback, float depth,
                      float spreadCents, float driveDb)
    {
        setTuneHz(tuneHz);
        setFeedback(feedback);
        setDepth(depth);
        setSpreadCents(spreadCents);

        const float preGain = juce::Decibels::decibelsToGain(juce::jlimit(0.0f, 36.0f, driveDb));
        // Make spread more apparent - scale by 4x
        const float spreadScale = 4.0f;
        const float effectiveSpread = spreadCents * spreadScale;
        const float detL = centsToRatio(+effectiveSpread);
        const float detR = centsToRatio(-effectiveSpread);

        for (int ch = 0; ch < juce::jmin(2, io.getNumChannels()); ++ch)
        {
            auto* x = io.getWritePointer(ch);
            auto& D = *dl[ch];
            auto& LP = lp[ch];

            const float det = (ch == 0 ? detL : detR);
            float& ph = (ch == 0 ? phL : phR);

            for (int n = 0; n < io.getNumSamples(); ++n)
            {
                juce::ScopedNoDenormals nd;
                
                // Apply pre-drive
                float input = x[n] * preGain;
                
                // Safety check for NaN/infinity
                if (!std::isfinite(input))
                    input = 0.0f;

                // No modulation - use stable delay time
                // Clamp delay to reasonable range
                float delaySamples = juce::jmax(2.0f, juce::jmin((float)(fs * 0.5f), (float)(baseDelaySamp * det)));

                // Read delayed signal
                float delayed = D.readFrac(delaySamples);
                
                // Safety check
                if (!std::isfinite(delayed))
                    delayed = 0.0f;

                // Apply damping in feedback path (before feedback multiplication)
                float damped = LP.processSample(delayed);
                
                // Safety check
                if (!std::isfinite(damped))
                    damped = 0.0f;

                // Standard comb filter: output = input + (feedback * delayed_output)
                // For Comb+/Comb-, use positive/negative feedback respectively
                // Ensure feedback is applied correctly for stability
                float fbAmount = sign * feedback * damped;
                float output = input + fbAmount;
                
                // Clamp output for stability - use tighter limits to prevent oscillation
                // Also apply soft limiting to prevent harsh clipping
                if (std::abs(output) > 1.5f)
                {
                    output = (output > 0.0f ? 1.5f : -1.5f) + 0.3f * (output - (output > 0.0f ? 1.5f : -1.5f));
                }
                output = juce::jlimit(-2.0f, 2.0f, output);
                
                // Safety check before writing
                if (!std::isfinite(output))
                    output = 0.0f;

                // Write to delay line for next iteration
                D.writeNext(output);

                // Output is the processed signal (no blending)
                x[n] = output;
            }
        }
    }

private:
    struct Delay
    {
        explicit Delay(int maxSamp)
            : buf(1, juce::nextPowerOfTwo(maxSamp)), mask(buf.getNumSamples() - 1)
        {
        }

        void prepare(double sampleRate, int maxBlock)
        {
            juce::ignoreUnused(sampleRate, maxBlock);
            writeIdx = 0;
            // Clear buffer to prevent initial noise
            buf.clear();
        }

        void writeNext(float s)
        {
            // Safety check
            if (!std::isfinite(s))
                s = 0.0f;
            buf.setSample(0, writeIdx & mask, s);
            ++writeIdx;
        }

        float readFrac(float dSamp) const
        {
            // Clamp delay to valid range (ensure we have at least 2 samples and don't exceed buffer)
            const int maxDelay = (mask + 1) - 2; // Leave at least 2 samples buffer
            dSamp = juce::jmax(2.0f, juce::jmin((float)maxDelay, dSamp));
            
            float read = (float)writeIdx - dSamp;
            
            // Handle wrap-around properly
            if (read < 0.0f)
                read += (float)(mask + 1);
            else if (read >= (float)(mask + 1))
                read -= (float)(mask + 1);
            
            const int i0 = ((int)std::floor(read)) & mask;
            const int i1 = (i0 + 1) & mask;
            const float t = read - std::floor(read);
            const float a = buf.getSample(0, i0);
            const float b = buf.getSample(0, i1);
            float result = a + t * (b - a); // Linear interpolation
            
            // Safety check
            if (!std::isfinite(result))
                result = 0.0f;
            
            return result;
        }

        juce::AudioBuffer<float> buf;
        int writeIdx = 0;
        int mask = 0;
    };

    static inline float centsToRatio(float c)
    {
        return std::pow(2.0f, c / 1200.0f);
    }

    double fs = 48000.0;
    int maxBlockSize = 512;
    int channels = 2;
    int sign = +1; // +1 Comb+, -1 Comb-
    float tuneHz = 400.0f;
    double baseDelaySamp = 120.0;
    float feedback = 0.6f;
    float depth = 0.0f;
    float spreadCents = 0.0f;

    juce::dsp::IIR::Filter<float> lp[2];
    std::unique_ptr<Delay> dl[2];

    // De-metalizing modulation
    float modAmtSamp = 0.0f;
    float modRate = 0.3f;
    float phL = 0.0f;
    float phR = 1.3f;
};

