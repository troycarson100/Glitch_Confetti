#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <memory>
#include <vector>

// Forward declaration
struct FilterProcessor;

// Crossfade ramp helper for pop-free mode switching
struct CrossfadeRamp {
    void start(double fs, double ms) {
        n = (int)(fs * ms * 0.001);
        i = 0;
        active = true;
    }
    
    bool isActive() const { return active; }
    
    float next() {
        if (!active) return 1.0f;
        float t = (float)i / juce::jmax(1, n);
        if (++i >= n) {
            active = false;
            return 1.0f;
        }
        // Equal-power crossfade
        return std::sin(0.5f * juce::MathConstants<float>::pi * t);
    }
    
    int n = 0;
    int i = 0;
    bool active = false;
};

// Filter interface
struct IFilter {
    virtual ~IFilter() = default;
    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) = 0;
    virtual void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) = 0;
};

// State Variable Filter processor (LP/HP/BP)
struct SVFProc : IFilter {
    SVFProc(int type) : filterType(type) {
        // type: 0=LP, 1=HP, 2=BP
    }
    
    void prepare(const juce::dsp::ProcessSpec& spec) override {
        specCached = spec;
        svf_L.prepare(spec);
        svf_R.prepare(spec);
        
        if (filterType == 0) {
            svf_L.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            svf_R.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        } else if (filterType == 1) {
            svf_L.setType(juce::dsp::StateVariableTPTFilterType::highpass);
            svf_R.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        } else {
            svf_L.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
            svf_R.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        }
        
        // Cascade for 24dB mode
        svf2_L.prepare(spec);
        svf2_R.prepare(spec);
        svf2_L.setType(svf_L.getType());
        svf2_R.setType(svf_R.getType());
    }
    
    void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) override {
        // Map res to Q: Q = 0.5 + 11*res^2
        float Q = 0.5f + 11.0f * res * res;
        
        // Stereo spread: offset cutoff per channel
        float spreadFactorL = std::pow(2.0f, spreadCents / 1200.0f);
        float spreadFactorR = std::pow(2.0f, -spreadCents / 1200.0f);
        
        float cutoffL = cutoffHz * spreadFactorL;
        float cutoffR = cutoffHz * spreadFactorR;
        
        // Clamp to safe range
        cutoffL = juce::jlimit(20.0f, 20000.0f, cutoffL);
        cutoffR = juce::jlimit(20.0f, 20000.0f, cutoffR);
        
        // Update filters
        svf_L.setCutoffFrequency(cutoffL);
        svf_L.setResonance(Q);
        svf_R.setCutoffFrequency(cutoffR);
        svf_R.setResonance(Q);
        
        // For 24dB mode, cascade with same settings
        if (slopeSel == 1) { // 24dB
            svf2_L.setCutoffFrequency(cutoffL);
            svf2_L.setResonance(Q);
            svf2_R.setCutoffFrequency(cutoffR);
            svf2_R.setResonance(Q);
        }
        
        use24dB = (slopeSel == 1);
        
        // Auto-gain trim at high resonance
        float gainTrim = 1.0f;
        if (res > 0.7f) {
            float excessGain = res * 0.3f; // Reduce output at high Q
            gainTrim = 1.0f / (1.0f + excessGain);
        }
        outputGain = gainTrim;
    }
    
    void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) override {
        auto numChannels = in.getNumChannels();
        auto numSamples = in.getNumSamples();
        
        if (numChannels < 2) return;
        
        // Copy input to output first
        out.copyFrom(in);
        
        // Process left channel
        auto blockL = out.getSingleChannelBlock(0);
        
        if (use24dB) {
            svf_L.process(juce::dsp::ProcessContextReplacing<float>(blockL));
            svf2_L.process(juce::dsp::ProcessContextReplacing<float>(blockL));
        } else {
            svf_L.process(juce::dsp::ProcessContextReplacing<float>(blockL));
        }
        
        // Process right channel
        auto blockR = out.getSingleChannelBlock(1);
        
        if (use24dB) {
            svf_R.process(juce::dsp::ProcessContextReplacing<float>(blockR));
            svf2_R.process(juce::dsp::ProcessContextReplacing<float>(blockR));
        } else {
            svf_R.process(juce::dsp::ProcessContextReplacing<float>(blockR));
        }
        
        // Apply output gain trim
        // For BP mode, apply additional gain boost to compensate for natural attenuation
        float finalGain = outputGain;
        if (filterType == 2) { // BP mode
            // Bandpass filters naturally attenuate signal, so boost by ~6dB (2x) for audibility
            finalGain *= 2.0f;
        }
        out.multiplyBy(finalGain);
    }
    
private:
    int filterType; // 0=LP, 1=HP, 2=BP
    juce::dsp::StateVariableTPTFilter<float> svf_L, svf_R;
    juce::dsp::StateVariableTPTFilter<float> svf2_L, svf2_R; // For 24dB cascade
    bool use24dB = false;
    float outputGain = 1.0f;
    float currentRes = 0.35f;  // Store resonance value for BP mode dry/wet mixing
    juce::dsp::ProcessSpec specCached;
};

// Fractional delay line for Lagrange interpolation
class FractionalDelay {
public:
    FractionalDelay(int maxSize) : bufferSize(maxSize), buffer(maxSize, 0.0f), writePos(0), bufferMask(maxSize - 1) {}
    
    void write(float sample) {
        buffer[writePos] = sample;
        writePos = (writePos + 1) & bufferMask;
    }
    
    float read(float delaySamples) {
        // Clamp delay to valid range
        delaySamples = juce::jlimit(1.0f, static_cast<float>(bufferSize - 1), delaySamples);
        
        float readPos = static_cast<float>(writePos) - delaySamples;
        while (readPos < 0.0f) readPos += static_cast<float>(bufferSize);
        while (readPos >= static_cast<float>(bufferSize)) readPos -= static_cast<float>(bufferSize);
        
        return lagrange3(buffer, readPos);
    }
    
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }
    
    int getBufferSize() const { return bufferSize; }
    
private:
    int bufferSize;
    int bufferMask;
    std::vector<float> buffer;
    int writePos;
    
    inline float lagrange3(const std::vector<float>& buf, float readPos) {
        const int i0 = static_cast<int>(readPos);
        const float frac = readPos - static_cast<float>(i0);
        
        const int i1 = (i0 - 1 + bufferSize) & bufferMask;
        const int i2 = i0 & bufferMask;
        const int i3 = (i0 + 1) & bufferMask;
        const int i4 = (i0 + 2) & bufferMask;
        
        const float y1 = buf[i1];
        const float y2 = buf[i2];
        const float y3 = buf[i3];
        const float y4 = buf[i4];
        
        // Lagrange 3rd order
        const float c0 = y2;
        const float c1 = 0.5f * (y3 - y1);
        const float c2 = y1 - 2.5f * y2 + 2.0f * y3 - 0.5f * y4;
        const float c3 = 0.5f * (y4 - y1) + 1.5f * (y2 - y3);
        
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};

// Simple cubic soft clip - branch-light and fast
// This is the core protection that prevents nasty digital clipping
inline float softClip(float x) {
    // Cubic soft saturation: x - (x^3) * (1/3)
    // This provides smooth saturation without harsh clipping
    // Works well for values up to about ±1.5, then saturates smoothly
    const float x3 = x * x * x;
    return x - (x3 * 0.333333f);
}

// Comb filter processor (Comb- / Comb+)
// Clean, robust design based on proven comb filter topology
// Design choices:
// - inputGain: Creates internal headroom to prevent clipping (0.4 = -7.9dB headroom)
// - wetGain: Controls output level (1.0 for Comb+, 1.4 for Comb- to make it more obvious)
// - maxFeedback: Hard caps to prevent instability (0.94 for Comb+, -0.97 for Comb-)
// - damping: One-pole LPF in feedback path (0.1 = bright, 0.3 = smooth)
// - softClip: Cubic saturation prevents digital clipping while maintaining musical character
struct CombProc : IFilter {
    // Tuning constants - adjust these to change character:
    static constexpr float MAX_FB_PLUS = 0.94f;   // Comb+ max feedback (lower = safer, less wild)
    static constexpr float MAX_FB_MINUS = -0.97f; // Comb- max feedback (more negative = deeper notches)
    static constexpr float INPUT_GAIN_PLUS = 0.4f;  // Comb+ input gain (lower = more headroom, less clipping)
    static constexpr float INPUT_GAIN_MINUS = 0.4f; // Comb- input gain (same for consistency)
    static constexpr float WET_GAIN_PLUS = 1.0f;    // Comb+ wet gain (1.0 = unity, lower = quieter)
    static constexpr float WET_GAIN_MINUS = 1.4f;   // Comb- wet gain (higher = more prominent/obvious)
    static constexpr float DAMPING_PLUS = 0.1f;     // Comb+ damping (lower = brighter, more resonant)
    static constexpr float DAMPING_MINUS = 0.15f;   // Comb- damping (slightly higher for stability, but still sharp)
    
    CombProc(int polarity) : combPolarity(polarity), isNegative(polarity < 0) {
        // polarity: -1 for Comb-, +1 for Comb+
        // Set defaults based on comb type
        if (isNegative) {
            inputGain = INPUT_GAIN_MINUS;
            wetGain = WET_GAIN_MINUS;
            currentDamping = DAMPING_MINUS;
        } else {
            inputGain = INPUT_GAIN_PLUS;
            wetGain = WET_GAIN_PLUS;
            currentDamping = DAMPING_PLUS;
        }
    }
    
    void prepare(const juce::dsp::ProcessSpec& spec) override {
        specCached = spec;
        double fs = spec.sampleRate;
        sampleRateHz = fs;
        
        // Maximum delay: 1 second at highest sample rate
        const size_t maxSamples = static_cast<size_t>(std::ceil(1.0 * fs));
        
        // Allocate delay buffers (simple circular buffers)
        bufferSizeL = maxSamples + 1;
        bufferSizeR = maxSamples + 1;
        bufferL.assign(bufferSizeL, 0.0f);
        bufferR.assign(bufferSizeR, 0.0f);
        
        writeIndexL = 0;
        writeIndexR = 0;
        
        // Initialize damping state
        lastLowpassOutL = 0.0f;
        lastLowpassOutR = 0.0f;
        
        // Reset delay state
        currentDelayL = 1.0f;
        currentDelayR = 1.0f;
    }
    
    void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) override {
        // cutoff becomes Tune (Hz) for comb
        float tuneHz = juce::jlimit(40.0f, 8000.0f, cutoffHz);
        
        // res becomes Feedback - use curved mapping for sweet spot
        // Shape the resonance parameter: res^2 gives more control in lower range
        float shaped = res * res;
        shaped = juce::jlimit(0.0f, 1.0f, shaped);
        
        // Set feedback based on comb type with different caps
        if (isNegative) {
            // Comb-: use stronger negative feedback for deeper notches
            feedback = MAX_FB_MINUS * shaped;
        } else {
            // Comb+: use positive feedback, slightly lower cap for safety
            feedback = MAX_FB_PLUS * shaped;
        }
        
        // slopeOrDepth becomes Depth (feed-forward tap) - not used in this design
        // We use wetGain instead for output level control
        depth = juce::jlimit(0.0f, 1.0f, slopeOrDepth);
        
        // Calculate delay length: L = fs / TuneHz
        float delaySamples = static_cast<float>(fs) / tuneHz;
        delaySamples = juce::jlimit(1.0f, static_cast<float>(bufferSizeL - 2), delaySamples);
        
        // Stereo spread: offset tune per channel
        float spreadFactorL = std::pow(2.0f, spreadCents / 1200.0f);
        float spreadFactorR = std::pow(2.0f, -spreadCents / 1200.0f);
        
        currentDelayL = delaySamples * spreadFactorL;
        currentDelayR = delaySamples * spreadFactorR;
        
        currentDelayL = juce::jlimit(1.0f, static_cast<float>(bufferSizeL - 2), currentDelayL);
        currentDelayR = juce::jlimit(1.0f, static_cast<float>(bufferSizeR - 2), currentDelayR);
        
        // Damping is set based on comb type in constructor, but can be adjusted here if needed
        // For now, keep the defaults set in constructor
    }
    
    void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) override {
        auto numChannels = in.getNumChannels();
        auto numSamples = in.getNumSamples();
        
        if (numChannels < 2) return;
        
        auto* inL = in.getChannelPointer(0);
        auto* inR = in.getChannelPointer(1);
        auto* outL = out.getChannelPointer(0);
        auto* outR = out.getChannelPointer(1);
        
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = processSampleL(inL[i]);
            outR[i] = processSampleR(inR[i]);
        }
    }
    
private:
    // Process single sample for left channel
    inline float processSampleL(float inputSample) {
        // Read delayed sample with linear interpolation
        float readIndex = static_cast<float>(writeIndexL) - currentDelayL;
        if (readIndex < 0.0f) {
            readIndex += static_cast<float>(bufferSizeL);
        }
        
        const int idxA = static_cast<int>(readIndex);
        const int idxB = (idxA + 1) % static_cast<int>(bufferSizeL);
        const float frac = readIndex - static_cast<float>(idxA);
        
        const float sa = bufferL[static_cast<size_t>(idxA)];
        const float sb = bufferL[static_cast<size_t>(idxB)];
        const float delayedSample = sa + frac * (sb - sa);
        
        // Feedback path damping (one-pole LPF)
        const float filtered = (1.0f - currentDamping) * delayedSample + currentDamping * lastLowpassOutL;
        lastLowpassOutL = filtered;
        
        // Calculate feedback sample
        float feedbackSample = filtered * feedback;
        
        // Sum input (with headroom) + feedback
        float internal = inputSample * inputGain + feedbackSample;
        
        // Soft clip to prevent clipping inside loop
        internal = softClip(internal);
        
        // Safety check
        if (!std::isfinite(internal)) {
            internal = 0.0f;
        }
        
        // Write into buffer
        bufferL[writeIndexL] = internal;
        
        // Increment write index (circular)
        if (++writeIndexL >= bufferSizeL) {
            writeIndexL = 0;
        }
        
        // Output: input + delayed signal (standard comb filter topology)
        // The wetGain scales the delayed component for Comb- prominence
        float output = inputSample + delayedSample * wetGain;
        
        // Final soft clip on output for safety
        output = softClip(output);
        
        return output;
    }
    
    // Process single sample for right channel
    inline float processSampleR(float inputSample) {
        // Read delayed sample with linear interpolation
        float readIndex = static_cast<float>(writeIndexR) - currentDelayR;
        if (readIndex < 0.0f) {
            readIndex += static_cast<float>(bufferSizeR);
        }
        
        const int idxA = static_cast<int>(readIndex);
        const int idxB = (idxA + 1) % static_cast<int>(bufferSizeR);
        const float frac = readIndex - static_cast<float>(idxA);
        
        const float sa = bufferR[static_cast<size_t>(idxA)];
        const float sb = bufferR[static_cast<size_t>(idxB)];
        const float delayedSample = sa + frac * (sb - sa);
        
        // Feedback path damping (one-pole LPF)
        const float filtered = (1.0f - currentDamping) * delayedSample + currentDamping * lastLowpassOutR;
        lastLowpassOutR = filtered;
        
        // Calculate feedback sample
        float feedbackSample = filtered * feedback;
        
        // Sum input (with headroom) + feedback
        float internal = inputSample * inputGain + feedbackSample;
        
        // Soft clip to prevent clipping inside loop
        internal = softClip(internal);
        
        // Safety check
        if (!std::isfinite(internal)) {
            internal = 0.0f;
        }
        
        // Write into buffer
        bufferR[writeIndexR] = internal;
        
        // Increment write index (circular)
        if (++writeIndexR >= bufferSizeR) {
            writeIndexR = 0;
        }
        
        // Output is the delayed signal with wet gain
        float output = delayedSample * wetGain;
        
        // Final soft clip on output for safety
        output = softClip(output);
        
        return output;
    }
    
    int combPolarity; // -1 for Comb-, +1 for Comb+
    bool isNegative;  // true for Comb-, false for Comb+
    
    // Delay buffers (simple circular buffers)
    std::vector<float> bufferL, bufferR;
    size_t bufferSizeL = 0;
    size_t bufferSizeR = 0;
    size_t writeIndexL = 0;
    size_t writeIndexR = 0;
    
    // Parameters
    double sampleRateHz = 44100.0;
    float currentDelayL = 1.0f;
    float currentDelayR = 1.0f;
    float feedback = 0.0f;
    float depth = 0.0f; // Not used in this design, kept for API compatibility
    float inputGain = 0.4f; // Headroom gain (0.4 = -7.9dB)
    float wetGain = 1.0f;   // Output gain (1.0 for Comb+, 1.4 for Comb-)
    float currentDamping = 0.1f; // One-pole LPF damping (0-1)
    float lastLowpassOutL = 0.0f; // Damping state for left channel
    float lastLowpassOutR = 0.0f; // Damping state for right channel
    juce::dsp::ProcessSpec specCached;
};

// Main filter processor with pop-free mode switching
class FilterProcessor
{
public:
    FilterProcessor();
    ~FilterProcessor() = default;
    
    void prepare(double sampleRate, int maxBlockSize);
    
    struct Targets {
        int type = 0; // 0=LP, 1=HP, 2=BP, 3=Comb-, 4=Comb+
        float cutoff = 1200.0f; // Hz (or Tune for Comb)
        float res = 0.35f; // Resonance (or Feedback for Comb)
        int slope = 1; // 0=12dB, 1=24dB (or Depth for Comb)
        float drive = 6.0f; // dB
        float spread = 0.0f; // cents
        float keytrack = 0.0f; // 0..1
        float mix = 1.0f; // 0..1
    };
    
    void setTargets(const Targets& t);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);
    
    bool isPrepared = false; // Public for safety checks
    
private:
    double fs = 48000.0;
    int block = 512;
    juce::dsp::ProcessSpec specCached;
    
    int currentType = 0;
    int targetType = 0;
    int slope = 1;
    float drive = 6.0f;
    float spreadCents = 0.0f;
    float kt = 0.0f;
    float mix = 1.0f;
    
    juce::SmoothedValue<float> cutoffSm;
    juce::SmoothedValue<float> resSm;
    
    std::unique_ptr<IFilter> cur;
    std::unique_ptr<IFilter> newF;
    
    juce::AudioBuffer<float> tmpA;
    juce::AudioBuffer<float> tmpB;
    
    CrossfadeRamp ramp;
    
    float lastMidiNote = 60.0f; // Middle C default
    
    void makeFilter(int type);
    void applyKeyTrack(float& cutoffHz);
};

