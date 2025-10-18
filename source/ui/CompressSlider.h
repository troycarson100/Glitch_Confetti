#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class CompressSlider : public juce::Slider
{
public:
    CompressSlider() : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
        setColour(juce::Slider::trackColourId, juce::Colours::white);
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF666666));
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw background track (grey)
        g.setColour(juce::Colour(0xFF666666));
        g.fillRoundedRectangle(bounds, 2.0f);
        
        // Draw filled portion (white)
        float normalizedValue = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
        float fillWidth = bounds.getWidth() * normalizedValue;
        
        if (fillWidth > 0.0f)
        {
            auto fillBounds = bounds.withWidth(fillWidth);
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(fillBounds, 2.0f);
        }
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressSlider)
};
