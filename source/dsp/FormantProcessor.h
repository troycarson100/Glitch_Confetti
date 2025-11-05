#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

/**
 * FormantProcessor - RBJ biquad formant filter
 * Stable formant filtering using IIR biquad filters with coefficient interpolation
 */
class FormantProcessor
{
public:
    FormantProcessor();
    ~FormantProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void process(juce::AudioBuffer<float>& buffer, int numSamples, juce::AudioProcessorValueTreeState& apvts);
    void setHostTempo(double bpm, bool hasTempo);
    
    // Access methods for UI visualization
    struct CurrentState {
        float f1Hz, f2Hz, f3Hz;
        float q, emphasisDb;
        bool enabled;
    };
    
    CurrentState getCurrentState() const;

private:
    // Vowel formant data (F1, F2, F3 in Hz)
    struct Formants { float F1, F2, F3; };
    static constexpr Formants kVowels[5] = {
        { 730.f, 1090.f, 2440.f }, // A (0)
        { 530.f, 1840.f, 2480.f }, // E (1)
        { 270.f, 2290.f, 3010.f }, // I (2)
        { 570.f,  840.f, 2410.f }, // O (3)
        { 300.f,  870.f, 2240.f }  // U (4)
    };
    
    // Formant bandwidths in Hz (narrower for more pronounced formants)
    static constexpr float kBandwidths[4] = {
        80.0f,  // F1 (narrower for more pronounced)
        70.0f,  // F2 (narrower for more pronounced)
        120.0f, // F3 (narrower for more pronounced)
        150.0f  // F4 (narrower for more pronounced)
    };
    
    // Get formants for continuous vowel (0-4)
    Formants getFormantsForVowel(float vowel) const;
    
    // Calculate F4 from F2 and F3
    float computeF4(float f2, float f3) const;
    
    // Create RBJ bandpass coefficients
    juce::dsp::IIR::Coefficients<float>::Ptr makeBandpassCoefficients(float centerFreq, float bandwidth) const;
    
    double sampleRate = 44100.0;
    
    // Single filter bank with 4 IIR biquads per channel
    std::array<juce::dsp::IIR::Filter<float>, 4> filtersL;  // F1, F2, F3, F4
    std::array<juce::dsp::IIR::Filter<float>, 4> filtersR;  // F1, F2, F3, F4
    
    // Parameters (all 8 for complete control)
    juce::LinearSmoothedValue<float> vowelSm;       // 0-4
    juce::LinearSmoothedValue<float> qSm;           // 0.4-18 (sharpness multiplier)
    juce::LinearSmoothedValue<float> emphasisSm;    // -6..+18 dB
    juce::LinearSmoothedValue<float> shiftSm;       // 0.5-2.0
    juce::LinearSmoothedValue<float> brightnessSm;  // -12..+12 dB (F4)
    juce::LinearSmoothedValue<float> motionSm;      // 0-1 (morph depth)
    juce::LinearSmoothedValue<float> airSm;          // 0-1 (not used yet)
    juce::LinearSmoothedValue<float> mixSm;          // 0-1
    
    bool isPrepared = false;
    int maxBlockSize = 512;
};
