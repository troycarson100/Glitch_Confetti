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

// Wave Type enum matching the parameter choice
enum class WaveType {
    Sine = 0,
    Triangle = 1,
    RampDown = 2,
    RampUp = 3,
    Random = 4
};

// Full LFO function with wave type and shape support
static inline float shapedLFO(float phase01, float shape01, WaveType waveType = WaveType::Sine, bool inverted = false)
{
    const float ph = phase01;  // 0..1
    float lfo = 0.0f;
    
    switch (waveType) {
        case WaveType::Sine: {
            // Sine with optional shape morphing to triangle/square
            const float s = juce::jlimit(0.0f, 1.0f, shape01);
            const float sinv = std::sin(juce::MathConstants<float>::twoPi * ph);
            const float tri = (2.0f / juce::MathConstants<float>::pi) * std::asin(sinv);
            const float k = juce::jmap(s, 0.0f, 1.0f, 0.0f, 3.0f);
            const float sq = std::tanh(k * sinv);
            
            if (s < 0.5f)
                lfo = juce::jmap(s * 2.0f, sinv, tri);
            else
                lfo = juce::jmap((s - 0.5f) * 2.0f, tri, sq);
            break;
        }
        
        case WaveType::Triangle: {
            // Pure triangle wave (shape morphs to pulse width)
            const float s = juce::jlimit(0.0f, 1.0f, shape01);
            const float pw = juce::jmap(s, 0.25f, 0.75f); // pulse width 25%-75%
            
            if (ph < pw)
                lfo = juce::jmap(ph, 0.0f, pw, -1.0f, 1.0f);
            else
                lfo = juce::jmap(ph, pw, 1.0f, 1.0f, -1.0f);
            break;
        }
        
        case WaveType::RampDown: {
            // Sawtooth ramp down (shape adjusts slope curve)
            const float s = juce::jlimit(0.0f, 1.0f, shape01);
            const float linear = 1.0f - 2.0f * ph;  // 1 to -1
            const float curved = std::pow(1.0f - ph, 1.0f + s * 2.0f) * 2.0f - 1.0f;
            lfo = juce::jmap(s, linear, curved);
            break;
        }
        
        case WaveType::RampUp: {
            // Sawtooth ramp up (shape adjusts slope curve)
            const float s = juce::jlimit(0.0f, 1.0f, shape01);
            const float linear = -1.0f + 2.0f * ph;  // -1 to 1
            const float curved = std::pow(ph, 1.0f + s * 2.0f) * 2.0f - 1.0f;
            lfo = juce::jmap(s, linear, curved);
            break;
        }
        
        case WaveType::Random: {
            // Sample & hold random (shape controls smoothness)
            static float lastRandom = 0.0f;
            static float lastPhase = 0.0f;
            
            // Trigger new random value at phase wrap
            if (ph < lastPhase) {
                lastRandom = (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            }
            lastPhase = ph;
            
            const float s = juce::jlimit(0.0f, 1.0f, shape01);
            // Shape controls how smooth the random is (0=stepped, 1=smoothed)
            lfo = lastRandom;
            break;
        }
    }
    
    // Apply inversion if requested
    return inverted ? -lfo : lfo;
}

// Visual state for UI synchronization
struct PanVisualState {
    std::atomic<double> phaseAtPublish{ 0.0 };        // 0..1
    std::atomic<double> phaseIncPerSample{ 0.0 };     // cycles/sample
    std::atomic<uint64_t> audioSamplesAtPublish{ 0 }; // running sample counter
};

struct AutoPan
{
    void prepare(double sr, double smoothingMs = 120.0)
    {
        sampleRate = (sr > 0.0 ? sr : 44100.0);
        phase = std::fmod(phase, 1.0); // Keep phase continuous - don't reset it
        
        // Smooth the PHASE INCREMENT (cycles/sample), not frequency - this is the key!
        const double glideSec = juce::jmax(0.0, smoothingMs) / 1000.0;
        phaseIncSmooth.reset(sampleRate, glideSec); // 120ms feels great
        
        // Shorter smoothing for other params for more responsive feel
        const double shortSecs = 50.0 / 1000.0;
        depthSmooth.reset(sampleRate, shortSecs);
        widthSmooth.reset(sampleRate, shortSecs);
        mixSmooth.reset(sampleRate, shortSecs);
        shapeSmooth.reset(sampleRate, shortSecs);
        phaseOffSmooth.reset(sampleRate, shortSecs);
        
        // Initialize motion detection and LFO output poles
        lastRateTarget = 0.0;
        motionHoldSamples = 0;
        lfoOutZ = 0.0f;
        lfoOutZRight = 0.0f;
        
        panSampleCounter = 0;
    }

    // Set targets each block (0..1 except freqHz)
    void setTargets(float freqHz, float depth01, float width01, float mix01, float shape01, float phaseOffset01, WaveType wType = WaveType::Sine, bool inv = false)
    {
        freqHz = juce::jlimit(0.0f, 20.0f, freqHz); // Cap at reasonable rate
        
        // Convert frequency to phase increment (cycles/sample) and smooth THAT
        const double incTarget = (double)freqHz / sampleRate;
        phaseIncSmooth.setTargetValue(incTarget);
        
        // Detect active knob motion for shape softening
        const double delta = std::abs((double)freqHz - lastRateTarget);
        lastRateTarget = (double)freqHz;
        if (delta > 0.005) { // Turning quickly
            motionHoldSamples = (int)std::round(0.06 * sampleRate); // Keep soft for 60ms
        } else if (motionHoldSamples > 0) {
            --motionHoldSamples;
        }
        
        // Set other parameter targets
        depthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, depth01));
        widthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, width01));
        mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix01));
        shapeSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, shape01));
        phaseOffSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, phaseOffset01));
        waveType = wType;
        inverted = inv;
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
            const float dep  = depthSmooth.getNextValue();             // 0..1
            float shp  = shapeSmooth.getNextValue();                   // 0..1 morph
            const float phOf = phaseOffSmooth.getNextValue();          // 0..1
            const float width = widthSmooth.getNextValue();            // 0..1
            const float mix = mixSmooth.getNextValue();                // 0..1

            // Get SMOOTHED phase increment (cycles/sample) - this prevents clicks!
            const double inc = phaseIncSmooth.getNextValue();
            
            if (syncToTransport && isPlaying) {
                // True transport sync: calculate phase from PPQ position
                // Use current increment to calculate quarter notes per cycle
                const double freqHz = inc * sampleRate;
                const double quarterNotesPerCycle = (freqHz > 0.0) ? ((double)bpm / (60.0 * freqHz)) : 1.0;
                const double transportPhase = std::fmod(ppqPosition / quarterNotesPerCycle, 1.0);
                phase = transportPhase;
            } else if (syncToTransport && !isPlaying) {
                // Transport sync enabled but not playing - freeze at current position
                // Don't advance phase
            } else {
                // Free-running mode: advance phase with smoothed increment
                phase += inc;
                if (phase >= 1.0) phase -= 1.0;  // wrap 0..1
            }

            // Soften shape while user is actively turning the rate knob
            if (motionHoldSamples > 0) {
                shp = juce::jmap(0.65f, shp, 0.0f); // Bias toward sine during motion
            }

            // DUAL LFO APPROACH (like Ableton AutoPan):
            // Left channel uses base phase, right channel has phase offset
            
            // Left LFO (master phase)
            float leftLfo = shapedLFO((float)phase, shp, waveType, inverted);
            
            // Right LFO (master phase + offset)
            // phOf is 0-1, convert to phase offset
            double rightPhase = phase + phOf;
            rightPhase -= std::floor(rightPhase); // wrap to 0..1
            float rightLfo = shapedLFO((float)rightPhase, shp, waveType, inverted);
            
            // Tiny output pole (2ms) to erase micro-zipper on hard shapes
            const double lfoOutTauMs = 2.0;
            const float poleA = (float)std::exp(-1.0 / ((lfoOutTauMs * 0.001) * sampleRate));
            lfoOutZ = poleA * lfoOutZ + (1.0f - poleA) * leftLfo;
            leftLfo = lfoOutZ;
            
            // Apply same pole to right LFO
            lfoOutZRight = poleA * lfoOutZRight + (1.0f - poleA) * rightLfo;
            rightLfo = lfoOutZRight;

            // Equal-power panning (proper stereo panning):
            // LFO in [-1,1], depth scales the pan amount
            // At phase=0°: both channels modulate together (tremolo)
            // At phase=180°: opposite modulation (stereo panning)
            
            // Map LFO to pan position: -dep (full left) to +dep (full right)
            float panPosL = dep * leftLfo;   // -dep..+dep
            float panPosR = dep * rightLfo;  // -dep..+dep
            
            // Equal-power panning law (constant energy, -3dB in center)
            // panPos = 0 (center): both = 0.707 (-3dB)
            // panPos = -1 (left): left = 1.0, right = 0.0
            // panPos = +1 (right): left = 0.0, right = 1.0
            const float leftGainL  = std::cos((panPosL + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            const float rightGainL = std::sin((panPosL + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            const float leftGainR  = std::cos((panPosR + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            const float rightGainR = std::sin((panPosR + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

            // Apply panning
            float wetL = inL * leftGainL;
            float wetR = inR * rightGainR;

            // True dry/wet crossfade
            L[n] = juce::jmap(mix, inL, wetL);
            if (R) R[n] = juce::jmap(mix, inR, wetR);
        }

        panSampleCounter += (uint64_t)N;

        // Publish visual state for UI (once per block)
        if (visualState) {
            visualState->phaseAtPublish.store(phase, std::memory_order_release);
            visualState->phaseIncPerSample.store(phaseIncSmooth.getCurrentValue(), std::memory_order_release);
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
            const double inc = phaseIncSmooth.getCurrentValue();
            const double fHz = inc * sampleRate;
            const double currentBeat = ppqPosition;
            const double beatsPerCycle = (fHz > 0.0) ? (60.0 / bpm * fHz) : 1.0; // Convert Hz back to beats per cycle
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

    // Smooth the PHASE INCREMENT (cycles/sample) - this is the key to click-free rate changes!
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> phaseIncSmooth;
    
    juce::SmoothedValue<float> depthSmooth;    // 0..1 travel
    juce::SmoothedValue<float> widthSmooth;    // 0..1 stereo->mono(panned)
    juce::SmoothedValue<float> mixSmooth;      // 0..1 dry/wet
    juce::SmoothedValue<float> shapeSmooth;    // 0..1 wave shape morphing (sin⇄tri⇄square)
    juce::SmoothedValue<float> phaseOffSmooth; // 0..1 phase offset

    // Motion detection for shape softening during knob movement
    double lastRateTarget { 0.0 };
    int motionHoldSamples { 0 };
    float lfoOutZ { 0.0f };                    // Tiny output pole for left LFO micro-zipper removal
    float lfoOutZRight { 0.0f };               // Tiny output pole for right LFO micro-zipper removal
    
    WaveType waveType { WaveType::Sine };      // Wave type (Sine, Triangle, Ramp, etc.)
    bool inverted { false };                   // LFO inversion

    PanVisualState* visualState { nullptr };   // pointer to visual state for UI
};