#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class FxPageLayer : public juce::Component
{
public:
    FxPageLayer(std::unique_ptr<juce::Drawable> backgroundSVG)
        : background(std::move(backgroundSVG))
    {
        addAndMakeVisible(content); // where the page's left-side controls live
        setPaintingIsUnclipped(false); // we paint only inside bounds
    }

    // Adopt an existing component (keeps attachments)
    void adopt(juce::Component& c)
    {
        if (c.getParentComponent() != nullptr)
            c.getParentComponent()->removeChildComponent(&c);
        content.addAndMakeVisible(&c);
    }

    juce::Component& getContent() { return content; }

    void paint(juce::Graphics& g) override
    {
        if (background != nullptr)
            background->drawWithin(g, getLocalBounds().toFloat(),
                                   juce::RectanglePlacement::stretchToFit, 1.0f);
    }

    void resized() override
    {
        content.setBounds(getLocalBounds()); // your left controls use absolute layout already
    }

private:
    std::unique_ptr<juce::Drawable> background;
    juce::Component content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPageLayer)
};
