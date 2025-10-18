#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class CompressEngine
{
public:
    CompressEngine();
    ~CompressEngine();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    // Parameter setters
    void setDrive(float driveDb);
    void setThreshold(float thresholdDb);
    void setCrush(float crushValue);
    void setTilt(float tiltValue);
    void setNoise(float noiseLevel);
    void setNoiseDecay(float decayTime);
    void setNoiseTone(float toneFreq);
    void setWet(float wetLevel);
    void setEnabled(bool enabled);

    // Gain reduction meter (for UI feedback)
    float getGainReductionDb() const { return gainReductionDb.load(); }

private:
    // DSP processing methods
    void processCompressor(juce::AudioBuffer<float>& buffer);
    void processDrive(juce::AudioBuffer<float>& buffer);
    void processCrush(juce::AudioBuffer<float>& buffer);
    void processTilt(juce::AudioBuffer<float>& buffer);
    void processNoise(juce::AudioBuffer<float>& buffer);
    void processWetDry(juce::AudioBuffer<float>& buffer);

    // Compressor state
    float envelopeLevel = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    std::atomic<float> gainReductionDb { 0.0f };

    // Drive state
    float driveGain = 1.0f;

    // Crush state
    float crushValue = 0.0f;
    int sampleCounter = 0;
    float heldSample = 0.0f;
    int rateFactor = 1;
    int bitDepth = 24;

    // Tilt EQ state
    juce::dsp::IIR::Filter<float> tiltLowPass;
    juce::dsp::IIR::Coefficients<float>::Ptr tiltCoeffs;
    float tiltValue = 0.0f;

    // Noise state
    juce::Random random;
    float noiseLevel = 0.0f;
    float noiseEnv = 0.0f;
    float noiseDecayTime = 0.5f;
    juce::dsp::IIR::Filter<float> noiseFilter;
    juce::dsp::IIR::Coefficients<float>::Ptr noiseCoeffs;

    // Wet/Dry mix
    float wetLevel = 1.0f;

    // Oversampling for nonlinear stages
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Processing state
    bool enabled = false;
    double sampleRate = 44100.0;
    int blockSize = 512;

    // Dry buffer for wet/dry mixing
    juce::AudioBuffer<float> dryBuffer;
};
