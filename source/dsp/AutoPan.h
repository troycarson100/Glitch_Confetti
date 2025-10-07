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

        // Keep phase continuous - don't reset it
        phase = juce::jlimit(0.0, juce::MathConstants<double>::twoPi, phase);
    }

    // set targets each block (0..1 except rate)
    void set(float rateHzTarget, float depth01, float width01, float mix01)
    {
        rateSmooth.setTargetValue(juce::jmax(0.0f, rateHzTarget)); // Hz
        depthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, depth01));
        widthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, width01));
        mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix01));
    }

    void process(juce::AudioBuffer<float>& buffer, bool isPlaying = true)
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

            // Generate LFO value (-1 to +1)
            const float lfo = std::sin((float)phase);

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

            // Advance continuous phase with smoothed rate ONLY when playing
            if (isPlaying) {
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
};