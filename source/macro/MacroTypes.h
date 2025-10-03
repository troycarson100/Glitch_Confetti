#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

enum class MacroMode { Unipolar, Bipolar };
enum class MacroDirection { Forward, Reverse }; // Unipolar only

struct MacroRoute
{
    juce::String   paramID;         // APVTS destination id
    int            macroIndex = 0;  // 0 = macro1, 1 = macro2
    MacroMode      mode       = MacroMode::Unipolar;
    MacroDirection direction  = MacroDirection::Forward; // if Unipolar
    float          depth      = 0.20f; // normalized span [0..1]
};