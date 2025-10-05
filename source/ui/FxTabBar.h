#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

enum class FxPageID { SpaceDelay, Panner };

class FxTabBar : public juce::Component
{
public:
    FxTabBar(std::unique_ptr<juce::Drawable> tab1SVG,
             std::unique_ptr<juce::Drawable> tab2SVG,
             std::function<void(FxPageID)> onTab)
        : onTabClicked(std::move(onTab))
    {
        // Space Delay tab
        addAndMakeVisible(btnSpace);
        if (tab1SVG) btnSpace.setImages(tab1SVG.release());
        btnSpace.onClick = [this]{ if (onTabClicked) onTabClicked(FxPageID::SpaceDelay); };

        // Panner tab
        addAndMakeVisible(btnPanner);
        if (tab2SVG) btnPanner.setImages(tab2SVG.release());
        btnPanner.onClick = [this]{ if (onTabClicked) onTabClicked(FxPageID::Panner); };

        // Visual tweaks
        btnSpace.setTriggeredOnMouseDown(true);
        btnPanner.setTriggeredOnMouseDown(true);
        setInterceptsMouseClicks(true, false); // only tabs take clicks
    }

    void setActive(FxPageID id)
    {
        active = id;
        repaint();
    }

    void resized() override
    {
        // Position tabs exactly like your mockup
        // (numbers tuned to your screenshot)
        auto r = getLocalBounds();
        const int tabH = 44, tabW = 160;
        btnSpace.setBounds( 24, 0, tabW, tabH);   // left orange tab
        btnPanner.setBounds(210, 0, tabW, tabH);  // green tab behind it
    }

    void paint(juce::Graphics& g) override
    {
        // Optional: dim the inactive tab with a soft overlay
        if (active == FxPageID::SpaceDelay)
            g.setColour(juce::Colours::black.withAlpha(0.0f));
        else
            g.setColour(juce::Colours::black.withAlpha(0.0f));
        // (we keep tab SVGs as-is; dimming can be added if desired)
    }

private:
    class SvgButton : public juce::Button
    {
    public:
        SvgButton(const juce::String& name) : juce::Button(name) {}
        void setImages(juce::Drawable* svg) { image.reset(svg); }
        void paintButton(juce::Graphics& g, bool over, bool down) override
        {
            if (image) image->drawWithin(g, getLocalBounds().toFloat(),
                                         juce::RectanglePlacement::stretchToFit, 1.0f);
            if (over)  g.fillAll(juce::Colours::white.withAlpha(0.06f));
            if (down)  g.fillAll(juce::Colours::white.withAlpha(0.10f));
        }
    private:
        std::unique_ptr<juce::Drawable> image;
    };

    SvgButton btnSpace {"spaceTab"};
    SvgButton btnPanner{"pannerTab"};
    FxPageID active { FxPageID::SpaceDelay };
    std::function<void(FxPageID)> onTabClicked;
};
