#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct UiAssets {
    // one instance owned by PluginEditor
    std::unique_ptr<juce::Drawable> backgroundMustard;
    std::unique_ptr<juce::Drawable> spaceDelayBackgroundTab1;
    std::unique_ptr<juce::Drawable> pannerBackgroundTab2;
    std::unique_ptr<juce::Drawable> effectPlate;
    std::unique_ptr<juce::Drawable> stepActive, stepInactive, stepTopActive, stepTopInactive;
    std::unique_ptr<juce::Drawable> buttonStepTopActive, buttonStepTopInactive;
    std::unique_ptr<juce::Drawable> knobRing, knobInside, knobMasterRing, knobMasterInside;
    std::unique_ptr<juce::Drawable> tabTitleSpaceDelay;
    std::unique_ptr<juce::Drawable> tabTitleAutoPan;
    std::unique_ptr<juce::Drawable> fxPowerOn, stepPowerOn, knobDice, diceLarge, macroAssign;
    std::unique_ptr<juce::Drawable> macro1AssignButton, macro2AssignButton;
    std::unique_ptr<juce::Drawable> lockedIcon, unlockedIcon;
    std::unique_ptr<juce::Drawable> fxTypeCarrotInactive, fxTypeCarrotActive;
    // … add only what Delay page & Sequencer need

    bool loadAll();
};
