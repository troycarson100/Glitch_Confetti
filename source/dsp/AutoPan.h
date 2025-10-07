// AutoPan.h - Proper left-right panning autopan
#pragma once
#include <juce_dsp/juce_dsp.h>

struct AutoPan
{
    void prepare(double sr, double smoothingMs = 30.0)
    {
        sampleRate = (sr > 0.0 ? sr : 44100.0);
        const double secs = juce::jmax(0.0, smoothingMs) / 1000.0;

        rateSmooth.reset(sampleRate, secs);
        depthSmooth.reset(sampleRate, secs);
        widthSmooth.reset(sampleRate, secs);
        mixSmooth.reset(sampleRate, secs);
        waveShapeSmooth.reset(sampleRate, secs);
        phaseOffsetSmooth.reset(sampleRate, secs);

        // Keep phase continuous - don't reset it
        phase = juce::jlimit(0.0, juce::MathConstants<double>::twoPi, phase);
    }

    // set targets each block (0..1 except rate)
    void set(float rateHzTarget, float depth01, float width01, float mix01, 
             int waveType = 0, float waveShape = 0.5f, float phaseOffset = 0.0f, bool inverted = false)
    {
        rateSmooth.setTargetValue(juce::jmax(0.0f, rateHzTarget)); // Hz
        depthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, depth01));
        widthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, width01));
        mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix01));
        
        // Wave parameters
        waveTypeParam = juce::jlimit(0, 4, waveType); // 0-4 for wave types
        waveShapeSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, waveShape));
        phaseOffsetSmooth.setTargetValue(juce::jlimit(0.0f, 360.0f, phaseOffset));
        invertedParam = inverted;
    }

    // Generate different wave types with shape morphing
    float generateWave(float phase, int waveType, float waveShape, bool inverted) const
    {
        // Normalize phase to 0-2π
        phase = std::fmod(phase, juce::MathConstants<float>::twoPi);
        if (phase < 0) phase += juce::MathConstants<float>::twoPi;
        
        float wave = 0.0f;
        
        switch (waveType) {
            case 0: // Sine
                wave = std::sin(phase);
                break;
                
            case 1: // Triangle
                if (phase < juce::MathConstants<float>::pi) {
                    wave = 2.0f * phase / juce::MathConstants<float>::pi - 1.0f;
                } else {
                    wave = 3.0f - 2.0f * phase / juce::MathConstants<float>::pi;
                }
                break;
                
            case 2: // Ramp Down (Sawtooth)
                wave = 1.0f - 2.0f * phase / juce::MathConstants<float>::twoPi;
                break;
                
            case 3: // Ramp Up (Reverse Sawtooth)
                wave = 2.0f * phase / juce::MathConstants<float>::twoPi - 1.0f;
                break;
                
            case 4: // Random (Sample & Hold)
                // For random, we'll use a pseudo-random approach based on phase
                // This creates a stepped random pattern
                {
                    int step = (int)(phase * 8.0f / juce::MathConstants<float>::twoPi); // 8 steps per cycle
                    float rand = std::sin(step * 1.234f) * 0.5f + std::cos(step * 2.345f) * 0.3f + std::sin(step * 3.456f) * 0.2f;
                    wave = juce::jlimit(-1.0f, 1.0f, rand);
                }
                break;
        }
        
        // Apply wave shape morphing (interpolate between wave types)
        if (waveShape != 0.5f) {
            float morphedWave = 0.0f;
            
            if (waveShape < 0.5f) {
                // Morph towards sine (0.0 = pure sine, 0.5 = original wave)
                float sineWave = std::sin(phase);
                float t = waveShape * 2.0f; // 0 to 1
                morphedWave = sineWave + t * (wave - sineWave);
            } else {
                // Morph towards triangle (0.5 = original wave, 1.0 = pure triangle)
                float triangleWave = 0.0f;
                if (phase < juce::MathConstants<float>::pi) {
                    triangleWave = 2.0f * phase / juce::MathConstants<float>::pi - 1.0f;
                } else {
                    triangleWave = 3.0f - 2.0f * phase / juce::MathConstants<float>::pi;
                }
                float t = (waveShape - 0.5f) * 2.0f; // 0 to 1
                morphedWave = wave + t * (triangleWave - wave);
            }
            wave = morphedWave;
        }
        
        // Apply inversion
        if (inverted) {
            wave = -wave;
        }
        
        return juce::jlimit(-1.0f, 1.0f, wave);
    }

    void process(juce::AudioBuffer<float>& buffer, bool isPlaying = true, bool syncToTransport = false, double bpm = 120.0, double ppqPosition = 0.0)
    {
        const int N = buffer.getNumSamples();
        const int C = buffer.getNumChannels();
        if (C == 0 || N == 0) return;

        auto* L = buffer.getWritePointer(0);
        auto* R = (C > 1 ? buffer.getWritePointer(1) : nullptr);

        for (int n = 0; n < N; ++n)
        {
            // Read inputs first (avoid in-place hazards)
            const float inL = L[n];
            const float inR = (R ? R[n] : inL);

            // Smooth parameters per-sample
            const float rateHz = rateSmooth.getNextValue();
            const float depth = depthSmooth.getNextValue();   // 0..1
            const float width = widthSmooth.getNextValue();   // 0..1
            const float mix = mixSmooth.getNextValue();       // 0..1

            // Calculate current phase based on sync mode
            double currentPhase = phase;
            if (syncToTransport && isPlaying) {
                // In sync mode, calculate phase from DAW position
                // rateHz is actually the musical division (e.g., 0.25 for 1/4 note)
                const double beatsPerCycle = rateHz; // rateHz contains the division value
                const double currentBeat = ppqPosition + (double)n * bpm / (60.0 * sampleRate);
                currentPhase = juce::MathConstants<double>::twoPi * std::fmod(currentBeat / beatsPerCycle, 1.0);
            } else if (isPlaying) {
                // Free-running mode
                currentPhase = phase;
            }
            // If not playing, keep current phase (no change)

            // Generate LFO value (-1 to +1) based on wave type
            const float waveShape = waveShapeSmooth.getNextValue();
            const float phaseOffset = phaseOffsetSmooth.getNextValue() * juce::MathConstants<double>::pi / 180.0; // Convert to radians
            const float lfo = generateWave((float)currentPhase + phaseOffset, waveTypeParam, waveShape, invertedParam);

            // Convert LFO to pan position (-1 = full left, +1 = full right)
            const float panPosition = lfo * depth;

            // Equal-power panning: convert pan position to gains
            // panPosition: -1 (full left) to +1 (full right)
            const float panNormalized = (panPosition + 1.0f) * 0.5f; // 0 to 1
            const float leftGain = std::cos(panNormalized * juce::MathConstants<float>::halfPi);
            const float rightGain = std::sin(panNormalized * juce::MathConstants<float>::halfPi);

            // Create mono source to avoid stereo peak doubling
            const float mono = 0.5f * (inL + inR);

            // Apply panning to mono source
            const float pannedL = mono * leftGain;
            const float pannedR = mono * rightGain;

            // Width control: blend between original stereo and panned mono
            const float wetL = inL + width * (pannedL - inL);
            const float wetR = inR + width * (pannedR - inR);

            // True dry/wet crossfade
            L[n] = inL + mix * (wetL - inL);
            if (R) R[n] = inR + mix * (wetR - inR);

            // Advance continuous phase with smoothed rate ONLY when playing and NOT in sync mode
            if (isPlaying && !syncToTransport) {
                phase += juce::MathConstants<double>::twoPi * (double)rateHz / sampleRate;
                if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
            }
        }
    }

    // Get current pan position for visualizer (-1 to +1)
    float getCurrentPanPosition() const noexcept
    {
        const float lfo = std::sin((float)phase);
        const float depth = depthSmooth.getCurrentValue();
        return lfo * depth;
    }
    
    // Get current pan position for visualizer with sync mode support
    float getCurrentPanPosition(bool syncToTransport = false, bool isPlaying = false, double bpm = 120.0, double ppqPosition = 0.0) const noexcept
    {
        double currentPhase = phase;
        if (syncToTransport && isPlaying) {
            const float rateHz = rateSmooth.getCurrentValue();
            const double beatsPerCycle = rateHz;
            const double currentBeat = ppqPosition;
            currentPhase = juce::MathConstants<double>::twoPi * std::fmod(currentBeat / beatsPerCycle, 1.0);
        }
        
        const float lfo = std::sin((float)currentPhase);
        const float depth = depthSmooth.getCurrentValue();
        return lfo * depth;
    }

    // Optional: call between blocks to keep phase a sane magnitude
    void wrapPhase() noexcept
    {
        if (phase >= juce::MathConstants<double>::twoPi)
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
    }

    // Members
    double sampleRate { 44100.0 };
    double phase { 0.0 };

    juce::SmoothedValue<float> rateSmooth;  // Hz
    juce::SmoothedValue<float> depthSmooth; // 0..1 travel
    juce::SmoothedValue<float> widthSmooth; // 0..1 stereo->mono(panned)
    juce::SmoothedValue<float> mixSmooth;   // 0..1 dry/wet
    juce::SmoothedValue<float> waveShapeSmooth; // 0..1 wave shape morphing
    juce::SmoothedValue<float> phaseOffsetSmooth; // 0..360 degrees phase offset
    
    int waveTypeParam { 0 }; // 0=Sine, 1=Triangle, 2=RampDown, 3=RampUp, 4=Random
    bool invertedParam { false }; // wave inversion
};