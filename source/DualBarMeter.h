#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "MeterTheme.h"

class DualBarMeter : public juce::Component, private juce::Timer {
public:
    struct Source {
        std::function<float()> getRmsDbL;
        std::function<float()> getRmsDbR;
        std::function<float()> getPeakDbL;
        std::function<float()> getPeakDbR;
    };

    explicit DualBarMeter(Source s): src(std::move(s)) { startTimerHz(30); }

    void paint(juce::Graphics& g) override {
        using namespace MeterTheme;
        auto r = getLocalBounds().toFloat();

        // Background (mock style): charcoal with divider
        g.fillAll(juce::Colours::black.withBrightness(0.07f));

        const float gap = 2.0f;  // gap between left and right bars
        const float totalWidth = r.getWidth();
        const float channelWidth = (totalWidth - gap) * 0.5f;  // equal width for both channels
        
        // Calculate positions for equal-width channels with gap
        auto left = juce::Rectangle<float>(0.0f, 0.0f, channelWidth, r.getHeight());
        auto right = juce::Rectangle<float>(channelWidth + gap, 0.0f, channelWidth, r.getHeight());

        // No central divider - just use the background

        paintBar(g, left,  src.getRmsDbL(),  src.getPeakDbL());
        paintBar(g, right, src.getRmsDbR(),  src.getPeakDbR());
    }

private:
    Source src;

    static float dbToNorm(float db) {
        // Map [-60 dB .. 0 dB] -> [0 .. 1]
        const float t = juce::jlimit(MeterTheme::kFloorDb, 0.0f, db);
        return juce::jmap(t, MeterTheme::kFloorDb, 0.0f, 0.0f, 1.0f);
    }

    static juce::Colour bandColour(float db) {
        using namespace MeterTheme;
        if (db >= kRedStartDb)     return red();
        if (db >= kYellowStartDb)  return yellow();
        return green();
    }

    void paintBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                  float rmsDb, float peakDb) {
        // Track background (subtle inner track)
        auto track = bounds;
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRect(track);  // No border radius - box shape

        // Fill from bottom according to RMS
        const float h = track.getHeight();
        const float norm = dbToNorm(rmsDb);
        const float fillH = juce::jlimit(0.0f, h, h * norm);
        auto fill = track.withY(track.getBottom() - fillH).withHeight(fillH);

        // Colour by zone - only draw fill if there's something to show
        if (fillH > 0.0f) {
            g.setColour(bandColour(rmsDb));
            g.fillRect(fill);  // No border radius - box shape
        }

        // Peak hold tick (thin white line) - only draw if peak is above floor
        if (peakDb > MeterTheme::kFloorDb) {
            const float peakY = track.getBottom() - h * dbToNorm(peakDb);
            if (peakY >= track.getY() && peakY <= track.getBottom()) {
                juce::Rectangle<float> tick(track.getX(), peakY - 1.0f, track.getWidth(), 2.0f);
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.fillRect(tick);
            }
        }

        // Clip indication is now handled by the red bar color when peaking
    }

    void timerCallback() override { repaint(); }
};
