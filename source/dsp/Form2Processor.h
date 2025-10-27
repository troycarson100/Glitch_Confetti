#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 * Form2Processor - Continuous vowel plane with 4-band SVF filtering,
 * dynamic emphasis, air/breath layer, and tempo-sync LFO motion.
 */
class Form2Processor
{
public:
    struct FF { float f1, f2, f3, f4; };
    
    Form2Processor();
    
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::dsp::AudioBlock<float>& block);
    
    // Parameter setters
    void setMorphX(float vowelIndex); // Maps discrete vowel (0-4) to XY positions
    void setMorphY(float y) { morphYSm.setTargetValue(y); }
    void setSharpness(float q) { sharpnessSm.setTargetValue(q); }
    void setEmphasis(float db) { emphasisSm.setTargetValue(db); }
    void setShift(float ratio) { shiftSm.setTargetValue(ratio); }
    void setMotion(float depth) { motionSm.setTargetValue(depth); }
    void setAir(float amt) { airSm.setTargetValue(amt); }
    void setMix(float m) { mixSm.setTargetValue(m); }
    
    void setHostTempo(double bpm, bool hasTempo);
    
private:
    // Formant anchor points (male baseline) - optimized for more distinct vowels
    static constexpr FF UL = {500.f, 1800.f, 2700.f, 3200.f}; // ~/E/ (more distinct E)
    static constexpr FF UR = {280.f, 2100.f, 3100.f, 3500.f}; // ~/I/ (sharper I)
    static constexpr FF LL = {320.f, 750.f, 2400.f, 3300.f};  // ~/U/ (clearer U)
    static constexpr FF LR = {720.f, 1100.f, 2600.f, 3400.f}; // ~/A/ (richer A)
    static constexpr FF UU = {320.f, 600.f, 2200.f, 3000.f};  // Pure /U/ vowel
    
    double sampleRate = 44100.0;
    double hostBpm = 120.0;
    bool hasHostTempo = false;
    
    // 4 IIR bandpass filters per channel (F1, F2, F3, F4)
    juce::dsp::IIR::Filter<float> filtersL[4], filtersR[4];
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> morphXSm, morphYSm;
    juce::LinearSmoothedValue<float> sharpnessSm, emphasisSm, shiftSm;
    juce::LinearSmoothedValue<float> motionSm, airSm, mixSm;
    
    // LFO state
    float lfoPhase = 0.0f;
    
    // RMS/env for dynamic emphasis & gate
    float envRms = 0.0f, envSlow = 0.0f, envFast = 0.0f;
    float envAlpha = 0.0f; // adapts to block size
    
    // Pink noise state
    float pink[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    juce::Random rng;
    
    // Q gain compensation (to prevent volume loss at high sharpness)
    float qGain = 1.0f;
    
    // Helper methods
    FF bilerp(float x, float y) const;
    void updateFormants(float x, float y, float shift, float q, bool isRight);
    float nextPink();
    void updateEnvState(float numSamples);
    void prepareSVFs();
};

