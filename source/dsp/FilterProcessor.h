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
        out.multiplyBy(outputGain);
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

// Soft clipping function for feedback path (tanh-based, efficient)
inline float softClip(float x, float threshold = 1.0f) {
    // Soft clipping: tanh for smooth saturation
    // Threshold controls where saturation begins
    if (std::abs(x) < threshold) {
        return x; // No clipping below threshold
    }
    // Smooth tanh saturation above threshold
    return std::tanh(x * (1.0f / threshold)) * threshold;
}

// Comb filter processor (Comb- / Comb+)
// Redesigned for musical resonance, stability, and clarity
struct CombProc : IFilter {
    // Constants (easily tweakable)
    static constexpr float MAX_FEEDBACK = 0.98f;        // Maximum feedback gain (±0.98)
    static constexpr float COMB_MIN_DAMPING = 0.1f;     // Comb+ minimum damping (brighter)
    static constexpr float COMB_MAX_DAMPING = 0.6f;     // Comb- maximum damping (darker)
    static constexpr float SOFT_CLIP_THRESHOLD = 0.85f; // Soft clip threshold in feedback path
    static constexpr float HPF_CUTOFF = 30.0f;          // DC blocking HPF frequency
    
    CombProc(int polarity) : combPolarity(polarity) {
        // polarity: -1 for Comb-, +1 for Comb+
    }
    
    void prepare(const juce::dsp::ProcessSpec& spec) override {
        specCached = spec;
        double fs = spec.sampleRate;
        
        // Maximum delay: 1 second at highest sample rate
        int maxDelaySamples = static_cast<int>(fs * 1.0);
        // Round up to power of 2
        int bufferSize = 1;
        while (bufferSize < maxDelaySamples) bufferSize *= 2;
        
        delayL = std::make_unique<FractionalDelay>(bufferSize);
        delayR = std::make_unique<FractionalDelay>(bufferSize);
        
        // Reset delay lines to ensure clean state
        if (delayL) delayL->reset();
        if (delayR) delayR->reset();
        
        // Lowpass damping filter in feedback path
        lpL.prepare(spec);
        lpR.prepare(spec);
        lpL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        lpR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        // Initial cutoff will be set dynamically based on damping
        lpL.setCutoffFrequency(8000.0f);
        lpR.setCutoffFrequency(8000.0f);
        lpL.reset();
        lpR.reset();
        
        // HPF for DC blocking in feedback path (prevents DC buildup)
        hpfL.prepare(spec);
        hpfR.prepare(spec);
        hpfL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        hpfR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        hpfL.setCutoffFrequency(HPF_CUTOFF);
        hpfR.setCutoffFrequency(HPF_CUTOFF);
        hpfL.reset();
        hpfR.reset();
        
        // Initialize damping state
        lastLowpassOutL = 0.0f;
        lastLowpassOutR = 0.0f;
    }
    
    void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) override {
        // cutoff becomes Tune (Hz) for comb
        float tuneHz = juce::jlimit(40.0f, 8000.0f, cutoffHz);
        
        // res becomes Feedback - map 0-1 to ±MAX_FEEDBACK with polarity
        // This gives full range for musical resonance
        float rawFeedback = res * MAX_FEEDBACK;
        feedback = juce::jlimit(-MAX_FEEDBACK, MAX_FEEDBACK, rawFeedback * combPolarity);
        
        // slopeOrDepth becomes Depth (feed-forward tap)
        depth = juce::jlimit(0.0f, 1.0f, slopeOrDepth);
        
        // Calculate delay length: L = fs / TuneHz
        float delaySamples = static_cast<float>(fs) / tuneHz;
        delaySamples = juce::jlimit(1.0f, static_cast<float>(delayL->getBufferSize() - 1), delaySamples);
        baseDelaySamples = delaySamples;
        
        // Stereo spread: offset tune per channel
        float spreadFactorL = std::pow(2.0f, spreadCents / 1200.0f);
        float spreadFactorR = std::pow(2.0f, -spreadCents / 1200.0f);
        
        delaySamplesL = delaySamples * spreadFactorL;
        delaySamplesR = delaySamples * spreadFactorR;
        
        delaySamplesL = juce::jlimit(1.0f, static_cast<float>(delayL->getBufferSize() - 1), delaySamplesL);
        delaySamplesR = juce::jlimit(1.0f, static_cast<float>(delayR->getBufferSize() - 1), delaySamplesR);
        
        // Calculate damping amount based on comb type and resonance
        // Comb- gets more damping (darker), Comb+ gets less (brighter)
        float dampingAmount;
        if (combPolarity < 0) {
            // Comb-: more damping, increases with resonance for stability
            dampingAmount = COMB_MIN_DAMPING + (COMB_MAX_DAMPING - COMB_MIN_DAMPING) * std::abs(feedback) / MAX_FEEDBACK;
        } else {
            // Comb+: less damping, only increases slightly at high resonance
            dampingAmount = COMB_MIN_DAMPING + (COMB_MAX_DAMPING * 0.3f - COMB_MIN_DAMPING) * std::abs(feedback) / MAX_FEEDBACK;
        }
        
        // Map damping to LP cutoff frequency (more damping = lower cutoff)
        // Range: 2kHz (high damping) to 12kHz (low damping)
        float lpCutoff = 12000.0f - (dampingAmount * 10000.0f);
        lpCutoff = juce::jlimit(2000.0f, 12000.0f, lpCutoff);
        lpL.setCutoffFrequency(lpCutoff);
        lpR.setCutoffFrequency(lpCutoff);
        
        currentDamping = dampingAmount;
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
            // ========== LEFT CHANNEL ==========
            // Read delayed signal from delay line
            float delayedL = delayL->read(delaySamplesL);
            
            // Apply damping (lowpass smoothing) in feedback path
            // This creates the characteristic comb filter sound
            float filteredL = (1.0f - currentDamping) * delayedL + currentDamping * lastLowpassOutL;
            lastLowpassOutL = filteredL;
            
            // Apply HPF to remove DC buildup (prevents oscillation)
            float fbProcessL = hpfL.processSample(0, filteredL);
            
            // Calculate feedback sample with polarity
            float feedbackSampleL = fbProcessL * feedback;
            
            // Apply soft clipping in feedback path to tame peaks and prevent harshness
            feedbackSampleL = softClip(feedbackSampleL, SOFT_CLIP_THRESHOLD);
            
            // Safety checks: prevent NaN/Inf and runaway feedback
            if (!std::isfinite(feedbackSampleL)) {
                feedbackSampleL = 0.0f;
            }
            feedbackSampleL = juce::jlimit(-0.95f, 0.95f, feedbackSampleL);
            
            // Write input + feedback to delay line (standard feedback comb structure)
            float delayInputL = inL[i] + feedbackSampleL;
            // Limit delay input to prevent runaway
            delayInputL = juce::jlimit(-1.5f, 1.5f, delayInputL);
            delayL->write(delayInputL);
            
            // Output: input + delayed * feedback + feedforward (depth)
            // Feedforward adds the delayed signal directly to output for more presence
            float outputL = inL[i] + delayedL * feedback + delayedL * depth;
            
            // Soft limit output to prevent clipping
            if (std::abs(outputL) > 1.5f) {
                outputL = softClip(outputL, 1.5f);
            }
            outputL = juce::jlimit(-2.0f, 2.0f, outputL);
            outL[i] = outputL;
            
            // ========== RIGHT CHANNEL ==========
            // Same processing as left channel
            float delayedR = delayR->read(delaySamplesR);
            
            float filteredR = (1.0f - currentDamping) * delayedR + currentDamping * lastLowpassOutR;
            lastLowpassOutR = filteredR;
            
            float fbProcessR = hpfR.processSample(0, filteredR);
            float feedbackSampleR = fbProcessR * feedback;
            feedbackSampleR = softClip(feedbackSampleR, SOFT_CLIP_THRESHOLD);
            
            if (!std::isfinite(feedbackSampleR)) {
                feedbackSampleR = 0.0f;
            }
            feedbackSampleR = juce::jlimit(-0.95f, 0.95f, feedbackSampleR);
            
            float delayInputR = inR[i] + feedbackSampleR;
            delayInputR = juce::jlimit(-1.5f, 1.5f, delayInputR);
            delayR->write(delayInputR);
            
            float outputR = inR[i] + delayedR * feedback + delayedR * depth;
            if (std::abs(outputR) > 1.5f) {
                outputR = softClip(outputR, 1.5f);
            }
            outputR = juce::jlimit(-2.0f, 2.0f, outputR);
            outR[i] = outputR;
        }
    }
    
private:
    int combPolarity; // -1 for Comb-, +1 for Comb+
    std::unique_ptr<FractionalDelay> delayL, delayR;
    juce::dsp::StateVariableTPTFilter<float> lpL, lpR; // Lowpass for damping (currently using simple smoothing)
    juce::dsp::StateVariableTPTFilter<float> hpfL, hpfR; // HPF for DC blocking in feedback path
    float feedback = 0.0f;
    float depth = 0.0f;
    float baseDelaySamples = 100.0f;
    float delaySamplesL = 100.0f;
    float delaySamplesR = 100.0f;
    float currentDamping = 0.1f; // Current damping amount (0-1)
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

