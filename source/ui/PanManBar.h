// PanManBar.h - PanMan-style visualizer with moving white boxes
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PanManBar : public juce::Component, private juce::Timer
{
public:
    struct Reader {
        std::function<double()> getPhase01;        // 0..1 (from audio)
        std::function<double()> getIncPerSample;   // cycles/sample (from audio)
        std::function<double()> getSampleRate;     // (from audio)
        std::function<float()>  getDepth01;        // read APVTS depth (0..1)
        std::function<float()>  getPhaseOffset01;  // read APVTS phase offset (0..1)
        std::function<float()>  getShape01;        // optional, if you morph waves
    };

    PanManBar(Reader r, int bins = 64)
        : reader(std::move(r)), numBins(juce::jlimit(16, 256, bins))
    {
        startTimeMs = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60); // smooth UI
    }

    // Styling
    void setColours(juce::Colour track, juce::Colour bin) { trackColour = track; binColour = bin; repaint(); }
    void setFalloff(float sigma) { falloffSigma = juce::jlimit(0.01f, 0.5f, sigma); } // how wide the glow is

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float radius = juce::jmin(r.getHeight() * 0.5f, 10.0f);

        // Track
        g.setColour(trackColour);
        g.fillRoundedRectangle(r, radius);

        // Ticks (subtle)
        g.setColour(trackColour.brighter(0.15f).withAlpha(0.35f));
        const int bigTicks = 4; // L | | C | | R
        for (int i = 0; i <= bigTicks; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) bigTicks;
            g.fillRect(x, r.getCentreY() - 8.0f, 1.0f, 16.0f);
        }

        // L C R labels
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(12.0f);
        g.drawFittedText("L", getLocalBounds().removeFromLeft(20), juce::Justification::centredLeft, 1);
        g.drawFittedText("C", getLocalBounds().withTrimmedLeft(getWidth()/2 - 10).withWidth(20), juce::Justification::centred, 1);
        g.drawFittedText("R", getLocalBounds().removeFromRight(20), juce::Justification::centredRight, 1);

        // Compute bin rectangles area (shrink inside the track)
        auto binsArea = r.reduced(8.0f, r.getHeight() * 0.35f);

        const float w = binsArea.getWidth();
        const float h = binsArea.getHeight();

        // Current position 0..1 from timer interpolation
        const float xNorm = currentX; // 0..1

        // Draw bins
        const float dx = w / (float) numBins;
        for (int i = 0; i < numBins; ++i)
        {
            const float cx = (i + 0.5f) / (float) numBins; // bin center in 0..1
            const float dist = std::abs(cx - xNorm);

            // Gaussian-ish falloff → opacity
            const float a = std::exp(- (dist * dist) / (2.0f * falloffSigma * falloffSigma));
            const float alpha = juce::jlimit(0.08f, 1.0f, a); // keep a faint floor

            juce::Rectangle<float> bin(binsArea.getX() + i * dx, binsArea.getY(), dx - 1.0f, h);
            g.setColour(binColour.withAlpha(alpha));
            g.fillRoundedRectangle(bin, 2.0f);
        }
    }

private:
    void timerCallback() override
    {
        // Interpolate phase from the audio-published clock
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double dtSec = (nowMs - startTimeMs) * 0.001;

        const double sr   = reader.getSampleRate();
        const double ph0  = reader.getPhase01();
        const double incS = reader.getIncPerSample();

        double ph = ph0 + incS * dtSec * (sr > 0.0 ? sr : 44100.0); // cycles
        ph -= std::floor(ph); // wrap 0..1

        // Depth & phase offset from APVTS (safe to read on UI)
        const float depth = reader.getDepth01 ? reader.getDepth01() : 1.0f;
        const float phOff = reader.getPhaseOffset01 ? reader.getPhaseOffset01() : 0.0f;

        // Simple sine visual (stable, smooth). If you morph shapes, reuse your same UI-safe function.
        const float lfo = std::sin(juce::MathConstants<float>::twoPi * (float) std::fmod(ph + phOff, 1.0));
        const float x = 0.5f + 0.5f * (depth * lfo); // map [-1,1] -> [0,1]

        // Small visual smoothing to remove timer jitter
        const float alpha = 0.25f;
        currentX = alpha * x + (1.0f - alpha) * currentX;

        repaint();
    }

    Reader reader;
    int numBins = 64;
    float falloffSigma = 0.06f; // 6% of width

    juce::Colour trackColour = juce::Colour(0xFF2F3237); // dark grey track
    juce::Colour binColour   = juce::Colours::white;     // WHITE boxes (your request)

    double startTimeMs = 0.0;
    float currentX = 0.5f;
};
