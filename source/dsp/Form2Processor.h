#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 * Form2Processor - Musical vowel filter using parallel formant banks.
 * Clear, obvious vowel character with State-Variable TPT filters.
 */
class Form2Processor
{
public:
    Form2Processor();
    
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::dsp::AudioBlock<float>& block);
    
    // Parameter setters
    void setVowel(float v) { vowelSm.setTargetValue(v); }
    void setEmphasis(float db) { emphasisSm.setTargetValue(db); }
    void setSharpness(float q) { sharpnessSm.setTargetValue(q); }
    void setShift(float ratio) { shiftSm.setTargetValue(ratio); }
    void setBrightness(float db) { brightnessSm.setTargetValue(db); }
    void setMotion(float depth) { motionSm.setTargetValue(depth); }
    void setAir(float amt) { airSm.setTargetValue(amt); }
    void setMix(float m) { mixSm.setTargetValue(m); }
    
    void setHostTempo(double bpm, bool hasTempo);
    
private:
    // Vowel formant table (F1, F2, F3)
    struct F123 { float f1, f2, f3; };
    static constexpr F123 kVowels[5] = {
        {730.f, 1090.f, 2440.f}, // A (0)
        {530.f, 1840.f, 2480.f}, // E (1)
        {270.f, 2290.f, 3010.f}, // I (2)
        {570.f,  840.f, 2410.f}, // O (3)
        {300.f,  870.f, 2240.f}  // U (4)
    };
    
    double sampleRate = 44100.0;
    double hostBpm = 120.0;
    bool hasHostTempo = false;
    
    // 4 State-Variable filters per channel (F1, F2, F3, F4)
    juce::dsp::StateVariableTPTFilter<float> filtersL[4], filtersR[4];
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> vowelSm;
    juce::LinearSmoothedValue<float> emphasisSm, sharpnessSm, shiftSm, brightnessSm;
    juce::LinearSmoothedValue<float> motionSm, airSm, mixSm;
    
    // LFO state
    float lfoPhase = 0.0f;
    
    // Envelope state for air gate
    float envFast = 0.0f, envSlow = 0.0f;
    
    // Pink noise state for air
    float pink[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    juce::Random rng;
    
    // Helper methods
    float interpolateVowel(float v) const;
    float computeF4(float f2, float f3) const;
    void updateFormants(float v, float shift, float q);
    float nextPink();
    void updateEnvelope(float sample);
};