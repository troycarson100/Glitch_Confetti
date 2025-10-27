#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class FormantProcessor
{
public:
    FormantProcessor();
    ~FormantProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    
    struct Targets {
        int vowel = 0;            // 0-4 (A,E,I,O,U)
        float resonance = 12.0f;   // 0.5-20 Q factor for bandwidth
        float intensity = 6.0f;   // 0-12 dB emphasis gain
        float mix = 0.8f;         // 0-1 dry/wet mix
    };
    
    void setTargets(const Targets& t);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);
    
    // Access methods for UI visualization
    struct CurrentState {
        float f1Hz, f2Hz, f3Hz;
        float q, emphasisDb;
        bool enabled;
    };
    
    CurrentState getCurrentState() const;

private:
    // Vowel formant data (F1, F2, F3 in Hz)
    enum Vowel { A_ = 0, E_ = 1, I_ = 2, O_ = 3, U_ = 4 };
    struct Formants { float F1, F2, F3; };
    static constexpr Formants kVowelTable[5] = {
        /*A*/ { 730.f, 1090.f, 2440.f },   // "Ah" - open vowel
        /*E*/ { 530.f, 1840.f, 2480.f },   // "Eh" - mid-front vowel  
        /*I*/ { 270.f, 2290.f, 3010.f },   // "Ee" - high-front vowel
        /*O*/ { 570.f,  840.f, 2410.f },   // "Oh" - mid-back vowel
        /*U*/ { 300.f,  870.f, 2240.f },   // "Oo" - high-back vowel
    };
    
    // Get formants for selected vowel
    Formants getFormantsForVowel(int vowel) const;
    
    // Update filter coefficients
    void updateCoeffs();
    
    // Process parameters
    void processParams();
    
    double sampleRate = 44100.0;
    
    // 3 parallel bandpass filters per channel (6 total)
    juce::dsp::IIR::Filter<float> filtersL[3], filtersR[3];
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> resonanceSm, intensitySm, mixSm;
    juce::LinearSmoothedValue<int> vowelSm;
    
    // Current parameters
    Targets currentTargets;
    Formants currentFormants;
    
    // Cache for optimization
    int lastVowel = -1;
    float lastQ = 0.0f;
    float lastIntensity = 0.0f;
    
    // Prepared flag to prevent processing before initialization
    bool isPrepared = false;
};
