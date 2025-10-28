#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <array>

/**
 * SaturateProcessor - Airwindows-style saturation with 8 models
 * Uses strategy pattern for model switching
 */

// Base interface for saturation models
struct ISat {
    virtual ~ISat() = default;
    virtual void setSampleRate(double fs) { (void)fs; }
    virtual void setParams(float param1, float param2, float param3, float param4) {
        (void)param1; (void)param2; (void)param3; (void)param4;
    }
    virtual float process(float x) = 0;
    virtual void reset() {}
};

// Forward declarations for concrete models
struct SatSpiral2;
struct SatDensity2;
struct SatDrive;
struct SatPurestDrive;
struct SatMojo;
struct SatConsole;
struct SatCoils;
struct SatTubey;

class SaturateProcessor {
public:
    SaturateProcessor();
    ~SaturateProcessor() = default;
    
    void prepare(double sampleRate, int maxBlockSize);
    void process(juce::AudioBuffer<float>& buffer, int numSamples, juce::AudioProcessorValueTreeState& apvts);
    void processWithSnapshot(juce::AudioBuffer<float>& buffer, int numSamples, float type, float drive, float color, float shape, float bias, float output, float mix);
    
private:
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    
    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentOsFactor = 4; // 0=1×, 1=2×, 2=4×, 3=8×
    
    // Input HPF (20-30 Hz)
    juce::dsp::IIR::Filter<float> inputHPF;
    
    // Output LPF for anti-aliasing (if needed)
    juce::dsp::IIR::Filter<float> outputLPF;
    
    // Current saturation model
    std::unique_ptr<ISat> currentModel;
    int currentType = 0;
    
    // Model instances (we'll create these in prepare)
    std::array<std::unique_ptr<ISat>, 8> models;
    
    // Parameter smoothing
    juce::LinearSmoothedValue<float> driveSm;
    juce::LinearSmoothedValue<float> colorSm;
    juce::LinearSmoothedValue<float> shapeSm;
    juce::LinearSmoothedValue<float> biasSm;
    juce::LinearSmoothedValue<float> outSm;
    juce::LinearSmoothedValue<float> mixSm;
    
    // Model switching crossfade
    float crossfadeValue = 1.0f;
    float crossfadeTarget = 1.0f;
    static constexpr float crossfadeTime = 0.01f; // 10ms
    
    // Helper to create models
    void createModels();
    void switchModel(size_t type);
};

