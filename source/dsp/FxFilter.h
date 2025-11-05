#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Filter parameter targets structure
struct FilterTargets {
    int type = 0;  // 0=LP, 1=HP, 2=BP, 3=Comb-, 4=Comb+
    float cutoff = 1200.0f;  // Hz for LP/HP/BP, Tune Hz for Comb
    float res = 0.35f;  // Q for SVF, Feedback for Comb
    int slope = 1;  // 0=12dB, 1=24dB for LP/HP/BP; Depth for Comb
    float drive = 6.0f;  // dB
    float spread = 0.0f;  // cents
    float keytrack = 0.0f;  // 0..1
    float mix = 1.0f;  // 0..1
};

// Crossfade ramp helper for pop-free switching
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
    
    int n = 0, i = 0;
    bool active = false;
};

// Filter processor interface
struct IFilter {
    virtual ~IFilter() = default;
    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) = 0;
    virtual void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) = 0;
};

// State-Variable Filter processor (LP/HP/BP)
struct SVFProc : public IFilter {
    SVFProc(int filterType);  // 0=LP, 1=HP, 2=BP
    ~SVFProc() override = default;
    
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) override;
    void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) override;
    
private:
    int type;  // 0=LP, 1=HP, 2=BP
    bool use24dB;
    juce::dsp::StateVariableTPTFilter<float> svf1, svf2;
    juce::SmoothedValue<float> cutoffSm;
    float currentQ = 0.35f;
    float currentRes = 0.35f;  // Store resonance value for BP mode dry/wet mixing
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec spec;
    
    float mapResToQ(float res);
    float getGainTrim(float q);
};

// Comb filter processor (Comb- / Comb+)
struct CombProc : public IFilter {
    CombProc(int combSign);  // -1 for Comb-, +1 for Comb+
    ~CombProc() override = default;
    
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs) override;
    void process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out) override;
    
private:
    int combSign;  // -1 for Comb-, +1 for Comb+
    std::vector<float> delayBufferL, delayBufferR;
    float readPosL = 0.0f, readPosR = 0.0f;
    int writePos = 0;
    int bufferSize = 0;
    int mask = 0;
    double sampleRate = 44100.0;
    float currentTuneHz = 1000.0f;  // Store tune in Hz
    float currentDelay = 0.0f;  // Store delay in samples
    float feedback = 0.0f;
    float depth = 0.0f;
    float spreadCents = 0.0f;
    
    // Internal LP for stability
    juce::dsp::IIR::Filter<float> stabilityLP;
    juce::dsp::IIR::Coefficients<float>::Ptr lpCoeffs;
    
    float readDelay(const std::vector<float>& buffer, float readIndex) const;
    void updateDelayLine(float tuneHz, float spread);
};

// Main filter processor with clickless switching
class FxFilter
{
public:
    FxFilter();
    ~FxFilter() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void setTargets(const FilterTargets& targets);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);
    
    // For key tracking
    void setCurrentMIDINote(int note) { currentMIDINote = note; }
    
private:
    void makeFilter(int type);
    
    double fs = 48000.0;
    int block = 512;
    juce::dsp::ProcessSpec specCached;
    
    int currentType = 0;
    int targetType = 0;
    int slope = 1;
    float drive = 6.0f;
    float spreadCents = 0.0f;
    float keytrack = 0.0f;
    float mix = 1.0f;
    int currentMIDINote = 60;  // Middle C
    
    juce::SmoothedValue<float> cutoffSm;
    juce::SmoothedValue<float> resSm;
    
    std::unique_ptr<IFilter> cur;
    std::unique_ptr<IFilter> newF;
    
    juce::AudioBuffer<float> tmpA;
    juce::AudioBuffer<float> tmpB;
    
    CrossfadeRamp ramp;
    
    // Helper for key tracking
    float applyKeyTracking(float baseCutoff, float keytrackVal, int midiNote);
};

