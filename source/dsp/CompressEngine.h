#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <memory>

class CompressEngine
{
public:
    CompressEngine();
    ~CompressEngine();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    // Parameter setters
    void setThreshold(float thresholdDb);
    void setAttack(float attackMs);
    void setRelease(float releaseMs);
    void setRatio(float ratio);
    void setDrive(float driveDb);
    void setNoise(float noiseLevel);
    void setNoiseTone(float toneFreq);
    void setWet(float wetLevel);
    void setEnabled(bool enabled);

    // Gain reduction meter (for UI feedback)
    float getGainReductionDb() const { return gainReductionDb.load(); }

private:
    // DSP processing methods
    void processCompressor(juce::AudioBuffer<float>& buffer);
    void processDrive(juce::AudioBuffer<float>& buffer);
    void processNoise(juce::AudioBuffer<float>& buffer);
    void processWetDry(juce::AudioBuffer<float>& buffer);

    // JUCE Compressor
    juce::dsp::Compressor<float> juceCompressor;

    // Compressor parameters
    float thresholdDb = -20.0f;
    float attackMs = 5.0f;
    float releaseMs = 50.0f;
    float ratio = 4.0f;

    // Drive state
    float driveGain = 1.0f;

    // Noise state (fixed decay at 0.6s)
    juce::Random random;
    float noiseLevel = 0.0f;
    float noiseEnv = 0.0f;
    float noiseDecayTime = 0.6f; // Fixed at 0.6s
    juce::dsp::IIR::Filter<float> noiseFilter;
    juce::dsp::IIR::Coefficients<float>::Ptr noiseCoeffs;

    // Wet/Dry mix
    float wetLevel = 1.0f;

    // Drive oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> driveOversampler;

    // Processing state
    bool enabled = false;
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    // Smooth gain for makeup gain compensation
    juce::SmoothedValue<float> smoothedGain;
    
    // Gain reduction meter (thread-safe)
    std::atomic<float> gainReductionDb{0.0f};

    // Dry buffer for wet/dry mixing
    juce::AudioBuffer<float> dryBuffer;
};
