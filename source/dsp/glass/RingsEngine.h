#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

struct RingsParams
{
    float f0Hz = 440.0f;     // mapped from pitch_semitones
    float brightness = 0.6f; // 0..1
    float damping   = 0.5f;  // 0..1 (maps from decay_sec)
    float structure = 0.35f; // 0..1 (glass/inharm tilt)
    int   poly       = 1;     // 1,2,4 voices (keep 1 for CPU)
};

class RingsEngine
{
public:
    RingsEngine();
    ~RingsEngine();
    
    void prepare (double sr, int blockSize, int numChannels);
    void reset();
    void setParams (const RingsParams& p);

    // process a mono exciter buffer -> stereo wet out
    // excMono can be nullptr (interpreted as silence)
    void process (const float* excMono, int numSamples, float* outL, float* outR);

    int getLatencySamples() const noexcept { return 0; } // Rings is effectively zero-latency
    
private:
    double sampleRate = 48000.0;
    // rings structs (forward-declare in cpp; include rings headers there)
    struct Impl;
    std::unique_ptr<Impl> impl; // heap once in prepare; no per-block allocs
};
