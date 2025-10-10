#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

/**
 * Musical Granular Synthesis Engine
 * Original implementation inspired by granular synthesis concepts
 * No GPL code reuse - clean room implementation
 * 
 * Features:
 * - Overlap-aware grain density (more musical)
 * - Multi-window morphing (Sine→Hann→Tukey→Blackman-Harris)
 * - Gaussian position spray (natural texture)
 * - Micro stereo decorrelation
 * - Adaptive gain control for consistent loudness
 * - Soft limiting for headroom
 * - 5-point Lagrange interpolation
 */
class GranularEngine
{
public:
    GranularEngine();
    ~GranularEngine() = default;
    
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    
    void setParameters(float sizeMs, float densityHz, float position01, float sprayMs,
                      float pitchSemi, float randomAmt, float texture01, float mix01);
    
    void process(juce::AudioBuffer<float>& buffer);
    
private:
    // Configuration
    static constexpr float kCaptureSec = 8.0f;  // 8 second buffer for long textures
    static constexpr int kMaxVoices = 128;      // 128 voices for dense clouds
    static constexpr float kTwoPi = 6.28318530718f;
    
    // Ring buffer
    juce::AudioBuffer<float> ringBuffer;
    int ringWritePos = 0;
    int ringSize = 0;
    
    // Voice structure
    struct Voice {
        bool active = false;
        float phase = 0.0f;
        float readPos = 0.0f;
        float increment = 1.0f;
        float duration = 2048.0f;
        float panL = 0.707f;
        float panR = 0.707f;
        float envLevel = 0.0f;
        float microDelay = 0.0f; // Stereo decorrelation delay (samples)
        float windowType = 0.2f; // Cached texture for this grain
    };
    
    std::array<Voice, kMaxVoices> voices;
    
    // Grain spawning
    float spawnAccumulator = 0.0f;
    
    // Smoothed parameters (slower for musical modulation)
    juce::SmoothedValue<float> sizeSmooth;
    juce::SmoothedValue<float> pitchSmooth;
    juce::SmoothedValue<float> densitySmooth;
    juce::SmoothedValue<float> mixSmooth;
    juce::SmoothedValue<float> textureSmooth;
    
    // Current parameters
    float currentSizeMs = 80.0f;
    float currentDensityHz = 8.0f;
    float currentPosition01 = 1.0f;
    float currentSprayMs = 20.0f;
    float currentPitchSemi = 0.0f;
    float currentRandomAmt = 0.15f;
    float currentTexture01 = 0.2f;
    
    // AGC state
    float wetRms = 0.0f;
    float agcGain = 1.0f;
    
    // PRNG
    uint32_t rngState = 0x12345678u;
    
    double sr = 44100.0;
    int channels = 2;
    
    // Helpers
    inline float rand01();
    inline float lagrangeInterp(const float* buf, int N, float rp);
    inline float computeMusicalWindow(float phase, float morphParam);
    int findFreeVoice();
    void spawnGrain(float baseReadPos, float size, float pitch, float texture);
};
