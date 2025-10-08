// AutoPan.h - Proper left-right panning autopan
#pragma once
#include <juce_dsp/juce_dsp.h>
#include "PanSync.h"

// Get host BPM robustly
static inline float getHostBpm(juce::AudioPlayHead* ph, float fallback = 120.0f)
{
    if (ph == nullptr) return fallback;
    juce::AudioPlayHead::CurrentPositionInfo pos; 
    if (!ph->getCurrentPosition(pos) || pos.bpm <= 0.0) return fallback;
    return (float)pos.bpm;
}

// Shaped LFO function (sin ⇄ triangle ⇄ square) without nasty edges
static inline float shapedLFO(float phase01, float shape01)
{
    const float ph = phase01;                   // 0..1
    const float s  = juce::jlimit(0.0f, 1.0f, shape01);
    const float sinv = std::sin(juce::MathConstants<float>::twoPi * ph);

    // triangle via asin(sin) normalized to [-1,1]
    const float tri = (2.0f / juce::MathConstants<float>::pi) * std::asin(sinv);

    // soft square via tanh(k * sin); k from 0..~3 as shape increases
    const float k   = juce::jmap(s, 0.0f, 1.0f, 0.0f, 3.0f);
    const float sq  = std::tanh(k * sinv);

    // crossfade sin → tri → sq: first half sin→tri, second half tri→sq
    if (s < 0.5f)
        return juce::jmap(s * 2.0f, sinv, tri);
    else
        return juce::jmap((s - 0.5f) * 2.0f, tri, sq);
}

// Visual state for UI synchronization
struct PanVisualState {
    std::atomic<double> phaseAtPublish{ 0.0 };        // 0..1
    std::atomic<double> phaseIncPerSample{ 0.0 };     // cycles/sample
    std::atomic<uint64_t> audioSamplesAtPublish{ 0 }; // running sample counter
};

struct AutoPan
{
    void prepare(double sr, double smoothingMs = 30.0)
    {
        sampleRate = (sr > 0.0 ? sr : 44100.0);
        const double secs = juce::jmax(0.0, smoothingMs) / 1000.0;

        freqSmooth.reset(sampleRate, secs);
        depthSmooth.reset(sampleRate, secs);
        widthSmooth.reset(sampleRate, secs);
        mixSmooth.reset(sampleRate, secs);
        shapeSmooth.reset(sampleRate, secs);
        phaseOffSmooth.reset(sampleRate, secs);

        // Keep phase continuous - don't reset it
        phase = juce::jlimit(0.0, 1.0, phase); // Now using 0..1 range
        panSampleCounter = 0;
    }

    // Set targets each block (0..1 except freqHz)
    void setTargets(float freqHz, float depth01, float width01, float mix01, float shape01, float phaseOffset01)
    {
        freqSmooth.setTargetValue(juce::jmax(0.0f, freqHz));
        depthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, depth01));
        widthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, width01));
        mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix01));
        shapeSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, shape01));
        phaseOffSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, phaseOffset01)); // 0..1 maps to 0..2π
    }

    // Process with click-free mid/side rotation and visual state publishing
    void process(juce::AudioBuffer<float>& buffer, bool isPlaying = true, bool syncToTransport = false, double bpm = 120.0, double ppqPosition = 0.0)
    {
        const int N = buffer.getNumSamples();
        const int C = buffer.getNumChannels();
        if (C == 0 || N == 0) return;

        auto* L = buffer.getWritePointer(0);
        auto* R = (C > 1 ? buffer.getWritePointer(1) : nullptr);

        constexpr float invSqrt2 = 0.7071067811865475f;

        for (int n = 0; n < N; ++n)
        {
            // Read inputs first (avoid in-place hazards)
            const float inL = L[n];
            const float inR = (R ? R[n] : inL);

            // Get smoothed parameters per sample
            const float fHz  = freqSmooth.getNextValue();              // smoothed frequency
            const float dep  = depthSmooth.getNextValue();             // 0..1
            const float shp  = shapeSmooth.getNextValue();             // 0..1 morph
            const float phOf = phaseOffSmooth.getNextValue();          // 0..1
            const float width = widthSmooth.getNextValue();            // 0..1
            const float mix = mixSmooth.getNextValue();                // 0..1

            // Advance continuous phase (now in 0..1 range)
            const double inc = (double)fHz / sampleRate;  // cycles/sample
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;  // wrap 0..1

            // Compute phase with offset
            double phaseWithOffset = phase + phOf;
            phaseWithOffset -= std::floor(phaseWithOffset); // wrap to 0..1

            // Shaped LFO in [-1,1]
            const float lfo = shapedLFO((float)phaseWithOffset, shp);

            // Pan amount: x ∈ [-depth, depth]
            const float x = dep * lfo;

            // Convert to mid/side rotation angle φ in [-π/4, +π/4] scaled by x
            const float phi = (juce::MathConstants<float>::pi * 0.25f) * x;

            // Mid/Side conversion
            float M = (inL + inR) * invSqrt2;
            float S = (inL - inR) * invSqrt2;

            // Rotate the (M,S) vector by φ
            const float c = std::cos(phi), s = std::sin(phi);
            float M2 = c * M - s * S;
            float S2 = s * M + c * S;

            // Width control
            S2 *= width;

            // Back to L/R
            float wetL = (M2 + S2) * invSqrt2;
            float wetR = (M2 - S2) * invSqrt2;

            // True dry/wet crossfade
            L[n] = juce::jmap(mix, inL, wetL);
            if (R) R[n] = juce::jmap(mix, inR, wetR);
        }

        panSampleCounter += (uint64_t)N;

        // Publish visual state for UI (once per block)
        if (visualState) {
            visualState->phaseAtPublish.store(phase, std::memory_order_release);
            visualState->phaseIncPerSample.store(freqSmooth.getCurrentValue() / sampleRate, std::memory_order_release);
            visualState->audioSamplesAtPublish.store(panSampleCounter, std::memory_order_release);
        }
    }


    // Get current pan position for visualizer (-1 to +1)
    float getCurrentPanPosition() const noexcept
    {
        const float dep = depthSmooth.getCurrentValue();
        const float shp = shapeSmooth.getCurrentValue();
        
        // Compute current phase 0..1 with offset
        float phase01 = (float)(phase / juce::MathConstants<double>::twoPi);
        const float phOf = phaseOffSmooth.getCurrentValue();
        phase01 = std::fmod(phase01 + phOf, 1.0f);
        
        // Shaped LFO in [-1,1]
        const float lfo = shapedLFO(phase01, shp);
        
        // Pan amount: x ∈ [-depth, depth]
        return dep * lfo;
    }

    // Get current pan position for visualizer with sync mode support
    float getCurrentPanPosition(bool syncToTransport, bool isPlaying, double bpm, double ppqPosition) const noexcept
    {
        const float dep = depthSmooth.getCurrentValue();
        const float shp = shapeSmooth.getCurrentValue();
        
        double currentPhase = phase;
        if (syncToTransport && isPlaying) {
            const float fHz = freqSmooth.getCurrentValue();
            const double currentBeat = ppqPosition;
            const double beatsPerCycle = 60.0 / bpm * fHz; // Convert Hz back to beats per cycle
            currentPhase = juce::MathConstants<double>::twoPi * std::fmod(currentBeat / beatsPerCycle, 1.0);
        }
        
        // Compute current phase 0..1 with offset
        float phase01 = (float)(currentPhase / juce::MathConstants<double>::twoPi);
        const float phOf = phaseOffSmooth.getCurrentValue();
        phase01 = std::fmod(phase01 + phOf, 1.0f);
        
        // Shaped LFO in [-1,1]
        const float lfo = shapedLFO(phase01, shp);
        
        // Pan amount: x ∈ [-depth, depth]
        return dep * lfo;
    }

    // Optional: call between blocks to keep phase a sane magnitude
    void wrapPhase() noexcept
    {
        if (phase >= juce::MathConstants<double>::twoPi)
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
    }

    // Set visual state pointer for UI synchronization
    void setVisualState(PanVisualState* vs) { visualState = vs; }
    
    // Publish clock data for PanManBar visualizer
    void publishClockData(double phase01, double incPerSample, double sr) {
        if (visualState) {
            visualState->phaseAtPublish.store(phase01, std::memory_order_release);
            visualState->phaseIncPerSample.store(incPerSample, std::memory_order_release);
            visualState->audioSamplesAtPublish.store(panSampleCounter, std::memory_order_release);
        }
    }

    // Members
    double sampleRate { 44100.0 };
    double phase { 0.0 };                    // 0..1 cycles
    uint64_t panSampleCounter { 0 };         // running sample counter

    juce::SmoothedValue<float> freqSmooth;     // Hz
    juce::SmoothedValue<float> depthSmooth;    // 0..1 travel
    juce::SmoothedValue<float> widthSmooth;    // 0..1 stereo->mono(panned)
    juce::SmoothedValue<float> mixSmooth;      // 0..1 dry/wet
    juce::SmoothedValue<float> shapeSmooth;    // 0..1 wave shape morphing (sin⇄tri⇄square)
    juce::SmoothedValue<float> phaseOffSmooth; // 0..1 phase offset

    PanVisualState* visualState { nullptr };   // pointer to visual state for UI
};