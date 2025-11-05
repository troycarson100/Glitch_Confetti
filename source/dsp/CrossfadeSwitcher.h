#pragma once

#include <juce_dsp/juce_dsp.h>

// Clickless Type switcher - run old & new processors in parallel and crossfade
struct CrossfadeSwitcher
{
    void prepare(double sampleRate, int maxBlock, int channels)
    {
        fs = sampleRate;
        maxN = maxBlock;
        a.setSize(channels, maxBlock);
        b.setSize(channels, maxBlock);
        a.clear();
        b.clear();
    }

    void start(float ms = 20.0f)
    {
        rampIdx = 0;
        rampN = (int)std::ceil(fs * (ms * 0.001f));
        active = true;
    }

    bool isActive() const { return active; }

    void render(juce::AudioBuffer<float>& out,
                const juce::AudioBuffer<float>& A,
                const juce::AudioBuffer<float>& B)
    {
        const int ch = juce::jmin(out.getNumChannels(), A.getNumChannels(), B.getNumChannels());
        const int n = out.getNumSamples();

        for (int c = 0; c < ch; ++c)
        {
            auto* o = out.getWritePointer(c);
            const float* pa = A.getReadPointer(c);
            const float* pb = B.getReadPointer(c);

            for (int i = 0; i < n; ++i)
            {
                float t = active ? (float)rampIdx / juce::jmax(1, rampN) : 1.0f;
                float g = std::sin(0.5f * juce::MathConstants<float>::pi * t); // Equal-power
                o[i] = (1.0f - g) * pa[i] + g * pb[i];

                if (active && ++rampIdx >= rampN)
                {
                    active = false;
                }
            }
        }
    }

    double fs = 48000.0;
    int maxN = 512;
    bool active = false;
    int rampIdx = 0;
    int rampN = 1;
    juce::AudioBuffer<float> a, b;
};

