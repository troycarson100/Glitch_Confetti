#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Envelope node for phase-based shaping
struct EnvelopeNode {
    float phase;  // 0..1
    float value;  // 0..1
};

// One-pole slew filter for Release smoothing
struct SlewFilter {
    float z = 0.f;
    
    void reset(float v = 0.f) { z = v; }
    
    inline float process(float target, float attackMs, float releaseMs, double sr) {
        const float aA = std::exp(-1.f / (float)((attackMs * 0.001) * sr));
        const float aR = std::exp(-1.f / (float)((releaseMs * 0.001) * sr));
        float a = (target > z) ? aA : aR;
        z = target - (target - z) * a;
        return z;
    }
};

struct RhythmGateEngine
{
    void prepare(double sr, int maxBlock, int numCh);
    void reset();

    void setTempoInfo(bool playing, double bpm, double ppq, int tsNum);
    void setParameters(int patternIdx, int divisionIdx, float offset01, float shape01,
                       float releaseMs, float mix01, bool syncOn);

    void process(juce::AudioBuffer<float>& buffer);

private:
    // Timing
    double sampleRate = 44100.0;
    bool   isPlaying = false, sync = true;
    bool   wasPlaying = false; // For play edge detection
    double bpm = 120.0, ppqPos = 0.0;
    int    tsNum = 4;
    int    division = 3; // Unused in new system (uses PPQ directly)
    int    patternIndex = 0;
    int    queuedPatternIndex = 0;
    
    // Free-run phase for when transport is stopped
    double freeRunPhase = 0.0;
    double lastPpqPos = -1.0;

    // Smoothing
    juce::SmoothedValue<float> shapeSm, releaseSm, offsetSm, mixSm;
    
    // Envelope slew filter (per-channel for stereo micro-offset)
    SlewFilter envSlewL, envSlewR;
    
    // Envelope nodes (built from pattern selection)
    std::vector<EnvelopeNode> envelopeNodes;
    
    // PRNG for humanize jitter
    uint32_t rng = 0x1234567u;
    
    inline float rand01() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (rng & 0x7FFFFFFF) / 2147483647.0f;
    }
    
    // Shape curvature (bipolar -1..+1)
    inline float applyShape(float frac, float shape) const {
        const float kMin = 0.25f, kMax = 4.0f;
        float s = std::clamp(shape, -1.0f, 1.0f);
        float k = (s >= 0.f)
                  ? s * (kMax - 1.0f) + 1.0f              // 1.0 -> 4.0 (concave)
                  : (s + 1.0f) * (1.0f - kMin) + kMin;   // 0.25 -> 1.0 (convex)
        return std::pow(std::clamp(frac, 0.0f, 1.0f), k);
    }
    
    // Evaluate envelope at phase with shape curvature
    float evalEnvelope(float phase, float shapeParam) const;
    
    // Build envelope from pattern index
    void buildEnvelopeFromPattern(int patternIdx);
    
    // Patterns (16 steps, 0..1 per step) - convert to envelope nodes
    static constexpr float P_STRAIGHT_8TH[16]   = {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0};
    static constexpr float P_OFFBEAT[16]        = {0,1,0,1, 0,1,0,1, 0,1,0,1, 0,1,0,1};
    static constexpr float P_HALF_TIME[16]      = {1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0};
    static constexpr float P_SYNCOP[16]         = {1,0,1,1, 0,1,0,1, 1,0,1,1, 0,1,0,1};
    static constexpr float P_TRIPLET_12[16]     = {1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,1};
    static constexpr float P_BUILD[16]          = {0,0,0,0, 0,0,1,0, 0,1,0,1, 1,1,1,1};
    static constexpr float P_CHOKE_16[16]       = {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0};
    static constexpr float P_GALLOP[16]         = {1,0,1,0, 0,1,0,1, 1,0,1,0, 0,1,0,1};

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
};

