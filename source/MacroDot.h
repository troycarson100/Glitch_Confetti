#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Forward declarations
class PluginProcessor;

//==============================================================================
// MacroDot - Thermal-style macro assignment indicator
//==============================================================================
class MacroDot : public juce::Component
{
public:
    MacroDot(PluginProcessor& p, const juce::String& pID, int mIndex);
    ~MacroDot() override = default;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    
    // Update dot based on current route
    void updateFromRoute();
    
    // Static positioning helpers
    static juce::Point<int> getPositionForMacro(int macroIndex, const juce::Rectangle<int>& knobBounds);
    static int getSizeForMacro(int macroIndex, const juce::Rectangle<int>& knobBounds);
    
private:
    PluginProcessor& processor;
    juce::String paramID;
    int macroIndex;
    
    // Colors for Macro 1 and 2
    static const juce::Colour macro1Color;
    static const juce::Colour macro2Color;
    
    // Drag state
    bool isDragging = false;
    float startDepth = 0.0f;
    int startY = 0;
    
    // Show right-click menu
    void showContextMenu();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroDot)
};
