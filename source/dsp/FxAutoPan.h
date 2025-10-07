#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class FxAutoPan
{
public:
    FxAutoPan();
    ~FxAutoPan() = default;

    void prepare(double sampleRate, int maxBlockSize);
    
    struct Targets {
        float rateHz = 1.0f;           // LFO rate in Hz
        float phaseDegrees = 180.0f;   // Phase offset in degrees
        int waveType = 0;              // 0=Sine, 1=Triangle, 2=Ramp Down, 3=Ramp Up, 4=Random
        float waveShape = 0.5f;        // Wave shape control (0-1)
        bool inverted = false;         // Invert left/right channels
        float amount = 0.5f;           // Pan amount (0-1)
        float width = 1.0f;            // Width: 0=keep original stereo, 1=fully replace with panned mono
        float mix = 1.0f;              // Dry/wet mix (0-1)
    };
    
    void setTargets(const Targets& t);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    double sampleRate = 44100.0;
    
    // LFO state
    double lfoPhase = 0.0;
    double lfoPhaseRadians = 0.0;
    float lastRate = 1.0f; // Track last rate to detect significant changes
    int bypassCounter = 0; // Counter to bypass processing during rate changes
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> rateSm, phaseSm, waveShapeSm, amountSm, widthSm, mixSm;
    juce::LinearSmoothedValue<int> waveTypeSm;
    juce::LinearSmoothedValue<bool> invertedSm;
    
    // Random number generator for random wave type
    juce::Random random;
    
    // Helper functions
    float generateLFOValue(int waveType, float waveShape, double phase);
    float applyPanAmount(float panValue, float amount);
};

