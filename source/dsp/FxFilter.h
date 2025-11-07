#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CombFilterFD.h"
#include "CrossfadeSwitcher.h"

// Filter parameter targets structure
struct FilterTargets {
    int type = 0;  // 0=LP, 1=HP, 2=BP, 3=Comb-, 4=Comb+
    float cutoff = 1200.0f;  // Hz for LP/HP/BP, Tune Hz for Comb
    float res = 0.35f;  // Q for SVF, Feedback for Comb
    int slope = 1;  // 0=12dB, 1=24dB for LP/HP/BP; Depth for Comb
    float drive = 6.0f;  // dB
    float spread = 0.0f;  // cents
    float keytrack = 0.0f;  // 0..1
    float mix = 1.0f;  // 0..1
};

// Main filter processor with clickless switching
class FxFilter
{
public:
    FxFilter();
    ~FxFilter() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void setTargets(const FilterTargets& targets);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);
    
    // For key tracking
    void setCurrentMIDINote(int note) { currentMIDINote = note; }
    
private:
    void switchFilterType(int newType);
    
    double fs = 48000.0;
    int block = 512;
    int channels = 2;
    
    int currentType = 0;
    int targetType = 0;
    int slope = 1;
    float drive = 6.0f;
    float spreadCents = 0.0f;
    float keytrack = 0.0f;
    float mix = 1.0f;
    int currentMIDINote = 60;  // Middle C
    
    // Smoothing for parameters (10-20 ms)
    juce::SmoothedValue<float> cutoffSm;
    juce::SmoothedValue<float> resSm;
    juce::SmoothedValue<float> driveSm;
    
    // JUCE State Variable TPT Filters for LP/HP/BP modes
    // Separate instances for left and right channels to support stereo spread
    juce::dsp::StateVariableTPTFilter<float> svfLP_L, svfLP_R;
    juce::dsp::StateVariableTPTFilter<float> svfHP_L, svfHP_R;
    juce::dsp::StateVariableTPTFilter<float> svfBP_L, svfBP_R;
    
    // Cascade filters for 24dB slope (two filters in series per channel)
    juce::dsp::StateVariableTPTFilter<float> svfLP2_L, svfLP2_R;
    juce::dsp::StateVariableTPTFilter<float> svfHP2_L, svfHP2_R;
    juce::dsp::StateVariableTPTFilter<float> svfBP2_L, svfBP2_R;
    
    // Comb filters for Comb- and Comb+ modes
    CombFilterFD combMinus;
    CombFilterFD combPlus;
    
    // Crossfade switcher for clickless mode changes
    CrossfadeSwitcher switcher;
    
    // Scratch buffers for crossfading
    juce::AudioBuffer<float> tmpA;
    juce::AudioBuffer<float> tmpB;
    juce::AudioBuffer<float> dryBuffer;
    
    // Helper for key tracking
    float applyKeyTracking(float baseCutoff, float keytrackVal, int midiNote);
    
    // Helper to map resonance (0-1) to Q (0.5-8.0 for LP/HP/BP with gentle musical curve)
    static float mapResToQ(float res, int filterType);
    
    // Helper to apply soft limiting (transparent at normal levels, protective at peaks)
    static inline float softLimit(float x) noexcept { return x / (1.0f + 0.5f * std::abs(x)); }
    
    // Helper to map resonance (0-1) to feedback (0-0.9) for comb filters
    // Uses quadratic curve to make resonance audible at lower values
    static float mapResToCombFeedback(float res);
    
    // Helper to apply drive with saturation
    float applyDrive(float sample, float driveDb);
    
    // Helper to process single filter mode with JUCE filter
    void processSVFMode(juce::AudioBuffer<float>& buffer, int type, float cutoff, float q, int slopeSel, float driveDb, float spreadCents, float res01);
    
    // Current parameters
    float currentCutoff = 1200.0f;
    float currentRes = 0.35f;
    float currentDrive = 6.0f;
    float currentSpread = 0.0f;
    
    // Process spec for JUCE filters
    juce::dsp::ProcessSpec processSpec;
    
    // BP-only HF damping state (per channel) for taming brittle hiss
    float bpHFPrev[2]{0.0f, 0.0f}; // per channel
    float bpHFAmount = 0.05f;      // Base amount (0.05-0.08 range, resonance-dependent)
};
