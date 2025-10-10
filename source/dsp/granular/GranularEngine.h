#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

/**
 * Live-input granular synthesis engine.
 * Original implementation - no GPL code reuse.
 * 
 * Features:
 * - 8 second stereo circular buffer
 * - 128 voice polyphony with stealing
 * - Hermite interpolation for pitch shifting
 * - Morphable window (Hann → Blackman → Rect)
 * - Position spray and global randomization
 * - External wet/dry mix
 */
class GranularEngine
{
public:
    GranularEngine();
    ~GranularEngine() = default;
    
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset(); // Clear ring buffer and voices
    
    // Update parameters (called once per block from UI)
    void setParameters(float sizeMs, float densityHz, float position01, float sprayMs,
                      float pitchSemi, float randomAmt, float texture01, float mix01);
    
    // Process audio (in-place, stereo)
    void process(juce::AudioBuffer<float>& buffer);
    
private:
    // Configuration
    static constexpr float kCaptureSec = 8.0f;
    static constexpr int kMaxVoices = 128;
    static constexpr float kTwoPi = 6.28318530718f;
    
    // Ring buffer for live input capture
    juce::AudioBuffer<float> ringBuffer;
    int ringWritePos = 0;
    int ringSize = 0;
    
    // Voice structure
    struct Voice {
        bool active = false;
        float phase = 0.0f;          // 0..1 grain playback position
        float readPos = 0.0f;        // Read position in ring buffer (samples)
        float increment = 1.0f;      // Pitch ratio (1.0 = original speed)
        float duration = 2048.0f;    // Grain length in samples
        float panL = 0.707f;         // Left pan coefficient
        float panR = 0.707f;         // Right pan coefficient
        float envLevel = 0.0f;       // Current envelope level (for stealing)
    };
    
    std::array<Voice, kMaxVoices> voices;
    
    // Grain spawning
    float spawnAccumulator = 0.0f;
    
    // Smoothed parameters
    juce::SmoothedValue<float> sizeSmooth;
    juce::SmoothedValue<float> pitchSmooth;
    juce::SmoothedValue<float> densitySmooth;
    juce::SmoothedValue<float> mixSmooth;
    
    // Current raw parameters (for spawning)
    float currentSizeMs = 40.0f;
    float currentDensityHz = 20.0f;
    float currentPosition01 = 1.0f;
    float currentSprayMs = 35.0f;
    float currentPitchSemi = 0.0f;
    float currentRandomAmt = 0.25f;
    float currentTexture01 = 0.3f;
    
    // Fast PRNG (xorshift32)
    uint32_t rngState = 0x12345678u;
    
    double sr = 44100.0;
    int channels = 2;
    
    // Helpers
    inline float rand01();
    inline float hermite4(const float* buf, int N, float rp);
    inline float computeWindow(float phase, float texture);
    int findFreeVoice();
    void spawnGrain(float baseReadPos);
    void renderVoices(juce::AudioBuffer<float>& wetBuffer);
};

