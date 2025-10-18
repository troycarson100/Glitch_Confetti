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
    void setLofi(float lofiLevel);
    void setMakeupGain(float makeupGainDb);
    void setWet(float wetLevel);
    void setEnabled(bool enabled);

    // Gain reduction meter (for UI feedback)
    float getGainReductionDb() const { return gainReductionDb.load(); }

private:
    // DSP processing methods
    void processCompressor(juce::AudioBuffer<float>& buffer);
    void processDrive(juce::AudioBuffer<float>& buffer);
    void processLofi(juce::AudioBuffer<float>& buffer);
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

    // Lofi state
    float lofiLevel = 0.0f;
    float sampleCounter = 0.0f;
    float heldSample = 0.0f;
    
    // Makeup gain state
    float makeupGainDb = 0.0f;

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
