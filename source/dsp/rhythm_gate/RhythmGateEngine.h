#pragma once
#include <juce_dsp/juce_dsp.h>

struct RhythmGateEngine
{
    void prepare(double sr, int maxBlock, int numCh);
    void reset();

    void setTempoInfo(bool playing, double bpm, double ppq, int tsNum);
    void setParameters(int patternIdx, int divisionIdx, float offset01, float shape01,
                       float pitchSemi, float reverse01, float glitch01, float mix01, bool syncOn);

    void process(juce::AudioBuffer<float>& buffer);

private:
    // Timing
    double sampleRate = 44100.0;
    bool   isPlaying = false, sync = true;
    double bpm = 120.0, ppqPos = 0.0;
    int    tsNum = 4;
    int    division = 3; // 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, 5=1/32
    int    patternIndex = 0;
    int    queuedPatternIndex = 0;
    float  offset = 0.0f;

    // Smoothing
    juce::SmoothedValue<float> shapeSm, pitchSemiSm, glitchSm, mixSm;

    // Ring buffer for reverse/pitch
    juce::AudioBuffer<float> ring;
    int ringWrite = 0, ringLen = 1;

    // Step/playhead state
    static constexpr int stepCount = 16;
    float stepPhase = 0.0f;    // 0..1 within step
    int   curStep = 0;
    int   samplesIntoStep = 0;
    int   samplesPerStep = 2048;
    
    // Free-running phase for non-sync mode
    float freePhase = 0.0f;
    float lastPpqPos = -1.0;

    // Cached per-block
    float probReverse = 0.0f;
    bool  stepReversed[16] = {false};
    
    // Glitch state
    int glitchRetriggerCount = 0;
    int glitchSamplesUntilRetrigger = 0;

    // PRNG
    uint32_t rng = 0x1234567u;
    inline float rand01() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (rng & 0x7FFFFFFF) / 2147483647.0f;
    }

    // Patterns (16 steps, 0..1 per step)
    static constexpr float P_STRAIGHT_8TH[16]   = {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0};
    static constexpr float P_OFFBEAT[16]        = {0,1,0,1, 0,1,0,1, 0,1,0,1, 0,1,0,1};
    static constexpr float P_HALF_TIME[16]      = {1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0};
    static constexpr float P_SYNCOP[16]         = {1,0,1,1, 0,1,0,1, 1,0,1,1, 0,1,0,1};
    static constexpr float P_TRIPLET_12[16]     = {1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,1};
    static constexpr float P_BUILD[16]          = {0,0,0,0, 0,0,1,0, 0,1,0,1, 1,1,1,1};
    static constexpr float P_CHOKE_16[16]       = {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0};
    static constexpr float P_GALLOP[16]         = {1,0,1,0, 0,1,0,1, 1,0,1,0, 0,1,0,1};

    // Pattern table
    struct Pattern {
        const char* name;
        const float* data;
    };
    
    static constexpr Pattern patterns[] = {
        {"Straight 8th", P_STRAIGHT_8TH},
        {"Offbeat", P_OFFBEAT},
        {"Half Time", P_HALF_TIME},
        {"Syncopated", P_SYNCOP},
        {"Triplet 12", P_TRIPLET_12},
        {"Build Up", P_BUILD},
        {"Choke 16", P_CHOKE_16},
        {"Gallop", P_GALLOP}
    };
    
    static constexpr int numPatterns = 8;

    // Helpers
    int   divisionToSamplesPerStep() const;
    float patternValueAt(int step, float localPhase) const;
    float shapedGate(float stepAmp, float localPhase) const;
    void  writeRing(const float* L, const float* R, int n);
    void  readRingVarispeed(float* outL, float* outR, int n, float startPos, float ratio, bool reverse,
                            float fadeInMs, float fadeOutMs);
    float hermite(const float* buf, int len, float rp) const;
    
    // Soft limiter
    void softLimit(float& sample, float knee, float ceiling);
};

