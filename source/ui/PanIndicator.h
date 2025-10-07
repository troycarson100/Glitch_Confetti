#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PanIndicator : public juce::Component
{
public:
    PanIndicator();
    ~PanIndicator() = default;
    
    void setPanPosition(float position); // -1.0 (full left) to +1.0 (full right)
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    float panPosition = 0.0f; // -1.0 to +1.0
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanIndicator)
};

