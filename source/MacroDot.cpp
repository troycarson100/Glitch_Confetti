#include "MacroDot.h"
#include "PluginProcessor.h"

// Color definitions - exact colors as specified
const juce::Colour MacroDot::macro1Color = juce::Colour(0xFFF6A22D);  // #F6A22D
const juce::Colour MacroDot::macro2Color = juce::Colour(0xFFEC571B);  // #EC571B

MacroDot::MacroDot(PluginProcessor& p, const juce::String& pID, int mIndex)
    : processor(p), paramID(pID), macroIndex(mIndex)
{
    jassert(macroIndex == 0 || macroIndex == 1);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void MacroDot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto centre = bounds.getCentre();
    auto radius = (bounds.getWidth() * 0.5f) - 1.0f; // 1px smaller radius
    
    // Choose color based on macro index
    auto color = (macroIndex == 0) ? macro1Color : macro2Color;
    
    // Draw stroked circle instead of filled
    g.setColour(color);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
}

void MacroDot::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        showContextMenu();
        return;
    }
    
    if (event.mods.isLeftButtonDown())
    {
        isDragging = true;
        startY = event.getPosition().y;
        
        if (processor.hasMacroRoute(paramID))
        {
            auto route = processor.getMacroRoute(paramID);
            startDepth = route.depth;
        }
        else
        {
            startDepth = 0.2f; // Default depth
        }
    }
}

void MacroDot::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDragging) return;
    
    // Vertical drag adjusts depth
    const int deltaY = event.getPosition().y - startY;
    const float sensitivity = 0.005f; // More sensitive for smoother control
    const float newDepth = juce::jlimit(0.0f, 1.0f, startDepth - deltaY * sensitivity);
    
    // Check for mode toggle with Shift key
    bool shouldBeBipolar = event.mods.isShiftDown(); // Shift for bipolar, default to unipolar
    
    // Update mode if it has changed
    if (processor.hasMacroRoute(paramID))
    {
        auto currentRoute = processor.getMacroRoute(paramID);
        auto newMode = shouldBeBipolar ? MacroMode::Bipolar : MacroMode::Unipolar;
        
        if (currentRoute.mode != newMode)
        {
            processor.updateMacroMode(paramID, newMode);
        }
    }
    
    // Update the route depth
    processor.updateMacroDepth(paramID, newDepth);
    
    // Trigger immediate visual update of the parent knob's range ring
    if (auto* parentKnob = getParentComponent())
    {
        parentKnob->repaint();
    }
    
    DBG("[MacroDot] Depth updated for " << paramID << ": " << newDepth 
        << " Mode: " << (shouldBeBipolar ? "Bipolar" : "Unipolar"));
}

void MacroDot::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    isDragging = false;
}

void MacroDot::updateFromRoute()
{
    // This will be called when the route changes
    // Trigger repaint of both the dot and the parent knob's range ring
    repaint();
    
    if (auto* parentKnob = getParentComponent())
    {
        parentKnob->repaint();
    }
}

juce::Point<int> MacroDot::getPositionForMacro(int macroIndex, const juce::Rectangle<int>& knobBounds)
{
    const int inset = 4; // Pixels inset from edge
    const int radius = getSizeForMacro(macroIndex, knobBounds) / 2;
    
    if (macroIndex == 0) // Macro 1 - top-left
    {
        return juce::Point<int>(knobBounds.getX() + inset, 
                               knobBounds.getY() + inset);
    }
    else // Macro 2 - top-right
    {
        return juce::Point<int>(knobBounds.getRight() - inset - radius * 2,
                               knobBounds.getY() + inset);
    }
}

int MacroDot::getSizeForMacro(int macroIndex, const juce::Rectangle<int>& knobBounds)
{
    // Dot radius relative to knob size (about 10-12px on an 84px knob)
    const float baseRatio = 12.0f / 84.0f; // ~14.3% of knob size
    int baseSize = static_cast<int>(knobBounds.getWidth() * baseRatio);
    
    if (macroIndex == 0) // Macro 1
    {
        return baseSize;
    }
    else // Macro 2 - 15% smaller
    {
        return static_cast<int>(baseSize * 0.85f);
    }
}

void MacroDot::showContextMenu()
{
    juce::PopupMenu menu;
    
    // Get current route to show current mode
    bool hasRoute = processor.hasMacroRoute(paramID);
    auto currentMode = MacroMode::Unipolar;
    
    if (hasRoute)
    {
        auto route = processor.getMacroRoute(paramID);
        currentMode = route.mode;
    }
    
    // Add menu items with checkmarks for current mode
    menu.addItem("Macro Unipolar", true, currentMode == MacroMode::Unipolar, [this]()
    {
        processor.updateMacroMode(paramID, MacroMode::Unipolar);
        updateFromRoute();
    });

    menu.addItem("Macro Bipolar", true, currentMode == MacroMode::Bipolar, [this]()
    {
        processor.updateMacroMode(paramID, MacroMode::Bipolar);
        updateFromRoute();
    });
    
    menu.addSeparator();
    
    menu.addItem("Remove macro", true, false, [this]()
    {
        processor.removeMacroRoute(paramID);
        // Parent component should handle removing this dot from the UI
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
    });
    
    menu.showMenuAsync(juce::PopupMenu::Options());
}
