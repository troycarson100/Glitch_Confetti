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
        int vowelA = 0;           // 0-4 (A,E,I,O,U)
        int vowelB = 1;           // 0-4 (A,E,I,O,U)
        float morph = 0.0f;       // 0-1 crossfade between A and B
        float q = 6.0f;           // 0.3-20 Q factor for bandwidth
        float emphasis = 6.0f;    // 0-12 dB emphasis gain
        float gender = 1.0f;      // 0.5-2.0 gender shift ratio
        float vibDepth = 5.0f;    // 0-30 cents vibrato depth
        float mix = 0.5f;         // 0-1 dry/wet mix
    };
    
    void setTargets(const Targets& t);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    // Vowel formant data (F1, F2, F3 in Hz)
    enum Vowel { A_ = 0, E_ = 1, I_ = 2, O_ = 3, U_ = 4 };
    struct Formants { float F1, F2, F3; };
    static constexpr Formants kVowelTable[5] = {
        /*A*/ { 730.f, 1090.f, 2440.f },
        /*E*/ { 530.f, 1840.f, 2480.f },
        /*I*/ { 270.f, 2290.f, 3010.f },
        /*O*/ { 570.f,  840.f, 2410.f },
        /*U*/ { 300.f,  870.f, 2240.f },
    };
    
    // Get interpolated formants between two vowels
    Formants getInterpolatedFormants(int vowelA, int vowelB, float morph, float gender);
    
    // Update filter coefficients
    void updateCoeffs(float vibratoMod = 0.0f);
    
    // Process parameters
    void processParams();
    
    double sampleRate = 44100.0;
    
    // 3 parallel band-pass filters per channel (6 total)
    juce::dsp::IIR::Filter<float> filtersL[3], filtersR[3];
    
    // LFO for vibrato
    juce::dsp::Oscillator<float> lfo { [](float x){ return std::sin(x); } };
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> morphSm, qSm, emphasisSm, genderSm, vibDepthSm, mixSm;
    juce::LinearSmoothedValue<int> vowelASm, vowelBSm;
    
    // Current parameters
    Targets currentTargets;
    Formants currentFormants;
    
    // LFO state
    double lfoPhase = 0.0;
    static constexpr double kLfoRate = 5.5; // Hz
};
