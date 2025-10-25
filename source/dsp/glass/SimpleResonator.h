#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// Simple modal resonator (dependency-free, JUCE-only)
// 6-mode glass/bell physical model using resonant band-pass filters

struct SimpleResonatorParams {
    float f0Hz       = 440.0f;  // 20-12000 Hz
    float brightness = 0.6f;    // 0-1 (tilt toward high modes)
    float damping    = 0.5f;    // 0-1 (higher = shorter decay)
    float spread     = 0.35f;   // 0-1 (stereo width)
};

class SimpleResonator {
public:
    void prepare(double sampleRate, int blockSize, int numChannels);
    void reset();
    void setParams(const SimpleResonatorParams& p);
    
    // Mono exciter → stereo wet out
    void process(const float* excMono, int numSamples, float* outL, float* outR);
    
private:
    double sampleRate = 48000.0;
    
    // 6 modal resonators (band-pass filters)
    static constexpr int kNumModes = 6;
    juce::dsp::IIR::Filter<float> modesL[kNumModes];
    juce::dsp::IIR::Filter<float> modesR[kNumModes];
    
    // Cached parameters
    float currentF0 = -1.0f;
    float currentBrightness = -1.0f;
    float currentDamping = -1.0f;
    float currentSpread = -1.0f;
    
    void updateModes();
};
