#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Delay parameter targets structure
struct DelayTargets {
    float timeMs = 250.0f;
    float feedback = 0.2f;
    float wowDepth = 0.0f;
    float wowRate = 1.0f;
    float drive = 0.0f;
    float hiCutHz = 20000.0f;
    float lowCutHz = 20.0f;
    float mix = 0.5f;
};

class FxDelay
{
public:
    FxDelay();
    ~FxDelay() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void setTargets(const DelayTargets& targets);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    // Core delay line - RE-201 style tape loop simulation
    std::vector<float> tapeBufferL, tapeBufferR;
    int bufferSize = 0;
    int mask = 0;
    int writePos = 0;
    double currentSampleRate = 44100.0;

    // Parameter smoothers for smooth changes
    juce::LinearSmoothedValue<float> timeMsSm;
    juce::LinearSmoothedValue<float> feedbackSm;
    juce::LinearSmoothedValue<float> mixSm;
    juce::LinearSmoothedValue<float> driveSm;
    juce::LinearSmoothedValue<float> hiCutSm;
    juce::LinearSmoothedValue<float> loCutSm;

    // Wow and flutter modulation
    float lfoPhase = 0.0f;
    juce::LinearSmoothedValue<float> wowDepthSm;
    juce::LinearSmoothedValue<float> wowRateSm;

    // RE-201 style feedback filters
    struct StateVariableFilter {
        float s1 = 0.0f, s2 = 0.0f;
        float cutoff = 1000.0f;
        
        void setCutoff(float freq) {
            cutoff = juce::jlimit(20.0f, 20000.0f, freq);
        }
        
        float processLowPass(float input, double sampleRate) {
            float f = 2.0f * std::sin(juce::MathConstants<float>::pi * juce::jmin(0.25f, cutoff / (float)sampleRate));
            s1 += f * input;
            s2 += f * s1;
            return s2;
        }
        
        float processHighPass(float input, double sampleRate) {
            float f = 2.0f * std::sin(juce::MathConstants<float>::pi * juce::jmin(0.25f, cutoff / (float)sampleRate));
            s1 += f * input;
            s2 += f * s1;
            return input - s1 - s2;
        }
    };
    
    StateVariableFilter hiCutFilterL, hiCutFilterR;
    StateVariableFilter loCutFilterL, loCutFilterR;

    // RE-201 style tape saturation
    struct TapeSaturator {
        float process(float input, float driveAmount) {
            // RE-201 style soft saturation
            float gain = 1.0f + 12.0f * driveAmount;
            float saturated = input * gain;
            
            // Soft clipping with slight asymmetry (tape-like)
            if (saturated > 0.0f) {
                saturated = std::tanh(saturated * 0.8f) / 0.8f;
            } else {
                saturated = std::tanh(saturated * 1.2f) / 1.2f;
            }
            
            // Normalize to maintain consistent levels
            return saturated / (1.0f + 0.1f * driveAmount);
        }
    };
    
    TapeSaturator tapeSatL, tapeSatR;

    // Helper functions
    inline float wrapBuffer(float index) const {
        while (index < 0) index += bufferSize;
        while (index >= bufferSize) index -= bufferSize;
        return index;
    }
    
    inline float readDelay(const std::vector<float>& buffer, float readIndex) const {
        // 4-point Lagrange interpolation for smooth tape reading
        int i = (int)readIndex;
        float frac = readIndex - i;
        
        float y0 = buffer[(i - 1) & mask];
        float y1 = buffer[i & mask];
        float y2 = buffer[(i + 1) & mask];
        float y3 = buffer[(i + 2) & mask];
        
        // Lagrange interpolation coefficients
        float c0 = (-1.0f/6.0f) * y0 + 0.5f * y1 - 0.5f * y2 + (1.0f/6.0f) * y3;
        float c1 = 0.5f * y0 - y1 + 0.5f * y2;
        float c2 = (-1.0f/3.0f) * y0 + 0.5f * y1 + 0.5f * y2 - (1.0f/6.0f) * y3;
        float c3 = y1;
        
        return ((c0 * frac + c1) * frac + c2) * frac + c3;
    }
    
    float generateWowFlutter();
};