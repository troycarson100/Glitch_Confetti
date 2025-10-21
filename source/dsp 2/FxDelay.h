#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class FxDelay
{
public:
    FxDelay();
    ~FxDelay() = default;

    void prepare(double sampleRate, int maxBlockSize);
    
    struct Targets {
        float timeMs = 250.0f;
        float feedback = 0.2f;
        float wowDepth = 0.0f;
        float wowRate = 1.0f;
        float drive = 0.0f;
        float hiCutHz = 20000.0f;
        float lowCutHz = 20.0f;
        float mix = 0.5f;
    };
    
    void setTargets(const Targets& t);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    double sampleRate = 44100.0;
    
    // Power-of-two ring buffer
    std::vector<float> ringL, ringR;
    int mask = 0, writePos = 0;

    // Core delay time with slope limiting (samples)
    float delaySampCurrent = 0.0f;
    float delaySampMaxStep = 0.0f;  // max change per sample for slope limiting

    // Smoothed parameters
    juce::LinearSmoothedValue<float> timeMsSm, fbSm, mixSm, driveSm;
    juce::LinearSmoothedValue<float> hiCutHzSm, lowCutHzSm;
    juce::LinearSmoothedValue<float> wowDepthSm, wowRateSm;

    // Wow/flutter LFO state
    double lfoPhase = 0.0, lfoPhase2 = 0.0;
    float wowLP = 0.0f;  // one-pole LP for tape-like smoothness

    // Feedback processing chain
    struct DCBlock { 
        float z = 0.0f; 
        float process(float x) { 
            float y = x - z + 0.995f * yPrev; 
            z = x; 
            yPrev = y; 
            return y; 
        } 
    private: 
        float yPrev = 0.0f; 
    } dcL, dcR;
    
    struct SVF {
        void prepare(double sr);
        void setHiCut(float hz); 
        void setLowCut(float hz);
        float lp(float x); 
        float hp(float x);
    private: 
        double sr = 44100.0; 
        float gLP = 1.0f, gHP = 1.0f; 
        float z1LP = 0, z1HP = 0;
    } svf;
    
    // 4-point Lagrange interpolation helper
    inline float lagrange4(const std::vector<float>& v, float idx) const;
    
    // Ring buffer wrapping helper
    inline float wrapF(float i) const { 
        float s = (float)(mask + 1); 
        float x = std::fmod(i, s); 
        if (x < 0) x += s; 
        return x; 
    }
};