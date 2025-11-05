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

// Comb filter processor (Comb- / Comb+)
struct CombProc : IFilter {
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
        
        // Internal LP for stability (12-16 kHz)
        lpL.prepare(spec);
        lpR.prepare(spec);
        lpL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        lpR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        lpL.setCutoffFrequency(14000.0f);
        lpR.setCutoffFrequency(14000.0f);
    }
    
    void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) override {
        // cutoff becomes Tune (Hz) for comb
        float tuneHz = juce::jlimit(40.0f, 8000.0f, cutoffHz);
        
        // res becomes Feedback
        feedback = juce::jlimit(0.0f, 0.95f, res);
        
        // slope becomes Depth (feed-forward tap)
        depth = (slopeSel == 1) ? slopeOrDepth : 0.0f;
        
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
        
        // Update LP if feedback/depth are high
        if (feedback > 0.8f || depth > 0.5f) {
            lpL.setCutoffFrequency(12000.0f);
            lpR.setCutoffFrequency(12000.0f);
        } else {
            lpL.setCutoffFrequency(16000.0f);
            lpR.setCutoffFrequency(16000.0f);
        }
    }
    
    void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) override {
        auto numChannels = in.getNumChannels();
        auto numSamples = in.getNumSamples();
        
        if (numChannels < 2) return;
        
        auto* inL = in.getChannelPointer(0);
        auto* inR = in.getChannelPointer(1);
        auto* outL = out.getChannelPointer(0);
        auto* outR = out.getChannelPointer(1);
        
        // Temporary buffers for LP processing
        juce::AudioBuffer<float> lpTempL(1, numSamples);
        juce::AudioBuffer<float> lpTempR(1, numSamples);
        float* lpTempLData = lpTempL.getWritePointer(0);
        float* lpTempRData = lpTempR.getWritePointer(0);
        
        for (int i = 0; i < numSamples; ++i) {
            // Left channel
            float delayedL = delayL->read(delaySamplesL);
            
            // Feedback path with LP
            float fbL = delayedL * feedback * combPolarity;
            lpTempLData[i] = fbL;
            
            // Feed-forward tap (depth) - use current input
            float ffL = inL[i] * depth;
            
            // Output: delayed + feed-forward
            outL[i] = delayedL + ffL;
            
            // Right channel
            float delayedR = delayR->read(delaySamplesR);
            
            float fbR = delayedR * feedback * combPolarity;
            lpTempRData[i] = fbR;
            
            float ffR = inR[i] * depth;
            
            outR[i] = delayedR + ffR;
        }
        
        // Process LP filters on feedback buffers
        juce::dsp::AudioBlock<float> lpBlockL(lpTempL);
        juce::dsp::AudioBlock<float> lpBlockR(lpTempR);
        lpL.process(juce::dsp::ProcessContextReplacing<float>(lpBlockL));
        lpR.process(juce::dsp::ProcessContextReplacing<float>(lpBlockR));
        
        // Now write filtered feedback to delay
        for (int i = 0; i < numSamples; ++i) {
            delayL->write(inL[i] + lpTempLData[i]);
            delayR->write(inR[i] + lpTempRData[i]);
        }
    }
    
private:
    int combPolarity; // -1 for Comb-, +1 for Comb+
    std::unique_ptr<FractionalDelay> delayL, delayR;
    juce::dsp::StateVariableTPTFilter<float> lpL, lpR;
    float feedback = 0.0f;
    float depth = 0.0f;
    float baseDelaySamples = 100.0f;
    float delaySamplesL = 100.0f;
    float delaySamplesR = 100.0f;
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

