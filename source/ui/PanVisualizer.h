// PanVisualizer.h - PanMan-style visualizer with jitter-free interpolation
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PanVisualizer : public juce::Component, private juce::Timer
{
public:
    struct Reader {
        std::function<double()> getPhaseAtPublish;       // 0..1
        std::function<double()> getPhaseIncPerSample;    // cycles/sample
        std::function<uint64_t()> getAudioSamplesAtPublish;
        std::function<double()> getSampleRate;
    };

    PanVisualizer(Reader r) : reader(std::move(r))
    {
        start = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60); // 60 fps
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float h = r.getHeight();
        const float w = r.getWidth();

        // Base bar background (dark to light gradient)
        juce::ColourGradient grad(juce::Colour(0xFF3A3A3A), r.getX(), r.getY(),
                                  juce::Colour(0xFF1E1E1E), r.getRight(), r.getY(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r.reduced(0, h * 0.35f), h * 0.18f);

        // Ticks
        g.setColour(juce::Colour(0x33FFFFFF));
        for (int i = 0; i <= 32; ++i)
        {
            const float x = r.getX() + (w * (float)i / 32.0f);
            const float th = (i % 8 == 0 ? 10.f : (i % 4 == 0 ? 6.f : 3.f));
            g.fillRect(x, r.getCentreY() - th * 0.5f, 1.0f, th);
        }

        // Labels L C R
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(12.0f);
        g.drawFittedText("L", r.removeFromLeft(20).translated(-4, 10).toNearestInt(), juce::Justification::centred, 1);
        g.drawFittedText("C", getLocalBounds().withTrimmedLeft((int)w / 2 - 10).withWidth(20).translated(0, 10), juce::Justification::centred, 1);
        g.drawFittedText("R", getLocalBounds().withTrimmedLeft((int)w - 20).translated(4, 10), juce::Justification::centred, 1);

        // Moving "oval" (PanMan vibe)
        const float xNorm = recentX; // 0..1
        const float cx = r.getX() + xNorm * w;
        const float cy = getLocalBounds().toFloat().getCentreY();
        juce::Rectangle<float> blob(0, 0, 26, 14); // tune size
        blob = blob.withCentre({ cx, cy });
        g.setColour(juce::Colours::red.withAlpha(0.9f));
        g.fillEllipse(blob);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(blob, 1.2f);
    }

private:
    void timerCallback() override
    {
        // Interpolate phase from audio-published state
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double dtSec = (nowMs - start) * 0.001;

        const double sr = reader.getSampleRate();
        const double ph0 = reader.getPhaseAtPublish();          // 0..1
        const double incS = reader.getPhaseIncPerSample();       // cycles/sample
        const uint64_t aS = reader.getAudioSamplesAtPublish();

        // Estimate samples elapsed since publish using our wall clock
        // (good enough for smooth UI)
        static double lastSr = 44100.0;
        lastSr = (sr > 0.0 ? sr : lastSr);
        const double estSamplesSince = dtSec * lastSr;

        double ph = ph0 + incS * estSamplesSince; // cycles
        ph -= std::floor(ph);                     // wrap 0..1

        // Convert phase to pan position (0..1 where 0.5 = center)
        // Phase 0..1 maps to pan position -1..1, then scaled to 0..1 for display
        const float panPos = (float)((ph + 0.5) - std::floor(ph + 0.5)); // center=0.5 at C
        
        // Optional: low-pass filter the visual position slightly to kill timer jitter
        const float alpha = 0.25f;
        recentX = juce::jlimit(0.0f, 1.0f, panPos);
        recentX = alpha * recentX + (1.0f - alpha) * recentXPrev;
        recentXPrev = recentX;

        repaint();
    }

    Reader reader;
    double start = 0.0;
    float recentX = 0.5f, recentXPrev = 0.5f;
};
