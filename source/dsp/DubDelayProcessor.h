#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class DubDelayProcessor
{
public:
    DubDelayProcessor();
    ~DubDelayProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    
    struct Targets {
        float timeMs = 450.0f;
        float feedback = 0.45f;
        float toneHz = 6500.0f;
        float drive = 0.15f;
        bool pingPong = true;
        float wowFlutterDepth = 0.35f;
        float regenDamp = 0.25f;
        float mix = 0.35f;
    };
    
    void setTargets(const Targets& t);
    void setTargetDelaySec(float seconds); // For tempo-synced time updates
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    double sr = 44100.0;
    
    // Ring buffers (power-of-two size for wrapping efficiency)
    std::vector<float> delayBufferL, delayBufferR;
    int bufferMask = 0;
    int writePos = 0;
    
    // Smoothed parameters (15ms time constant for smooth transitions)
    juce::SmoothedValue<float> timeMsSmooth;
    juce::SmoothedValue<float> feedbackSmooth;
    juce::SmoothedValue<float> toneCutoffSmooth;
    juce::SmoothedValue<float> driveSmooth;
    juce::SmoothedValue<float> wowFlutterSmooth;
    juce::SmoothedValue<float> regenDampSmooth;
    juce::SmoothedValue<float> mixSmooth;
    
    // Additional smoothing for delay samples to prevent read position jumps
    juce::SmoothedValue<float> delaySampsSmoothL;
    juce::SmoothedValue<float> delaySampsSmoothR;
    
    // Crossfade state for big time jumps
    bool isCrossfading = false;
    int crossfadeSamplesRemaining = 0;
    int crossfadeTotalSamples = 0;
    float crossfadeDelaySampsA_L = 0.0f, crossfadeDelaySampsA_R = 0.0f; // Frozen delay times (in samples)
    float crossfadeDelaySampsB_L = 0.0f, crossfadeDelaySampsB_R = 0.0f; // Frozen delay times (in samples)
    float previousTimeSec = 0.45f; // Track last time for jump detection
    
    // Ping-pong state
    bool pingPongEnabled = true;
    
    // Wow/Flutter modulation
    double wowPhase = 0.0;      // Slow LFO (0.1-0.4 Hz)
    double flutterPhase = 0.0;  // Fast LFO (3-8 Hz)
    double flutterPhaseR = 0.0; // Phase-shifted for stereo
    juce::Random random;
    float randomWalk = 0.0f;
    float randomWalkSmooth = 0.0f;
    
    // Feedback path filters
    struct OnePoleHP {
        float z = 0.0f;
        float coeff = 0.0f;
        void setCutoff(float hz, double sampleRate) {
            float rc = 1.0f / (juce::MathConstants<float>::twoPi * hz);
            coeff = rc / (rc + 1.0f / static_cast<float>(sampleRate));
        }
        float process(float x) {
            float y = coeff * (z + x - prevX);
            prevX = x;
            z = y;
            return y;
        }
    private:
        float prevX = 0.0f;
    } hpfL, hpfR;
    
    // Tone LPF (12dB/oct state variable)
    juce::dsp::StateVariableTPTFilter<float> toneLPF_L, toneLPF_R;
    
    // Regen damp shelf (high shelf cut)
    struct HighShelf {
        void prepare(double sampleRate);
        void setParams(float freqHz, float gainDb);
        float process(float x);
    private:
        double sr = 44100.0;
        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
        float b1 = 0.0f, b2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
    } shelfL, shelfR;
    
    // Auto-gain compensation
    float computeLoopAttenuation(float toneCutoff, float regenDamp);
    float compensatedFeedback = 0.45f;
    
    // Lagrange interpolation for fractional delay
    inline float lagrange3(const std::vector<float>& buffer, float readPos);
    
    // Helper functions
    inline float softClip(float x, float drive);
    inline float equalPowerCrossfade(float dry, float wet, float mix);
};

