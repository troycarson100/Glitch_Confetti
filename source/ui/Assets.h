#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct UiAssets {
    // one instance owned by PluginEditor
    std::unique_ptr<juce::Drawable> backgroundMustard;
    
    // Background variants: effect × tab position (4 effects × 4 positions = 16 total)
    std::unique_ptr<juce::Drawable> spaceDelayBackgroundTab1;
    std::unique_ptr<juce::Drawable> spaceDelayBackgroundTab2;
    std::unique_ptr<juce::Drawable> spaceDelayBackgroundTab3;
    std::unique_ptr<juce::Drawable> spaceDelayBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> pannerBackgroundTab1;
    std::unique_ptr<juce::Drawable> pannerBackgroundTab2;
    std::unique_ptr<juce::Drawable> pannerBackgroundTab3;
    std::unique_ptr<juce::Drawable> pannerBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> dirtBackgroundTab1;
    std::unique_ptr<juce::Drawable> dirtBackgroundTab2;
    std::unique_ptr<juce::Drawable> dirtBackgroundTab3;
    std::unique_ptr<juce::Drawable> dirtBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> chorusBackgroundTab1;
    std::unique_ptr<juce::Drawable> chorusBackgroundTab2;
    std::unique_ptr<juce::Drawable> chorusBackgroundTab3;
    std::unique_ptr<juce::Drawable> chorusBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> reverbBackgroundTab1;
    std::unique_ptr<juce::Drawable> reverbBackgroundTab2;
    std::unique_ptr<juce::Drawable> reverbBackgroundTab3;
    std::unique_ptr<juce::Drawable> reverbBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> granularBackgroundTab1;
    std::unique_ptr<juce::Drawable> granularBackgroundTab2;
    std::unique_ptr<juce::Drawable> granularBackgroundTab3;
    std::unique_ptr<juce::Drawable> granularBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> slicerBackgroundTab1;
    std::unique_ptr<juce::Drawable> slicerBackgroundTab2;
    std::unique_ptr<juce::Drawable> slicerBackgroundTab3;
    std::unique_ptr<juce::Drawable> slicerBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> effectPlate;
    std::unique_ptr<juce::Drawable> stepActive, stepInactive, stepTopActive, stepTopInactive;
    std::unique_ptr<juce::Drawable> buttonStepTopActive, buttonStepTopInactive;
    std::unique_ptr<juce::Drawable> knobRing, knobInside, knobMasterRing, knobMasterInside;
    std::unique_ptr<juce::Drawable> tabTitleSpaceDelay;
    std::unique_ptr<juce::Drawable> tabTitleAutoPan;
    std::unique_ptr<juce::Drawable> tabDirtIcon;
    std::unique_ptr<juce::Drawable> tabChorusIcon;
    std::unique_ptr<juce::Drawable> tabVerbIcon;
    std::unique_ptr<juce::Drawable> tabGranularIcon;
    
    // New consistent icons with uniform containing boxes
    std::unique_ptr<juce::Drawable> tabSpaceIcon;      // Space_Icon.svg
    std::unique_ptr<juce::Drawable> tabAutoPanIcon;    // AutoPan_Icon.svg
    std::unique_ptr<juce::Drawable> tabDirtIconNew;    // Dirt_Icon.svg (rename to avoid conflict)
    std::unique_ptr<juce::Drawable> tabChorusIconNew;  // Chorus_Icon.svg (rename to avoid conflict)
    std::unique_ptr<juce::Drawable> tabHallIcon;       // Hall_Icon.svg (was reverb)
    std::unique_ptr<juce::Drawable> tabGrainIcon;      // Grain_Icon.svg
    std::unique_ptr<juce::Drawable> tabSlicerIcon;     // Slice_Icon.svg
    std::unique_ptr<juce::Drawable> tabDubDelayIcon;   // DubDelay_Icon.svg
    std::unique_ptr<juce::Drawable> tabReduxIcon;      // Redux_Icon.svg
    std::unique_ptr<juce::Drawable> tabPhaseBloomIcon; // PhaseBloom_Icon.svg
    
    // Formant assets
    std::unique_ptr<juce::Drawable> tabFormantIcon; // Form_Icon.svg
    std::unique_ptr<juce::Drawable> formBackgroundTab1, formBackgroundTab2, formBackgroundTab3, formBackgroundTab4;
    
    // Form 2 assets
    std::unique_ptr<juce::Drawable> tabForm2Icon; // Form_Icon.svg (shared)
    std::unique_ptr<juce::Drawable> form2BackgroundTab1, form2BackgroundTab2, form2BackgroundTab3, form2BackgroundTab4;
    
    // Saturate assets
    std::unique_ptr<juce::Drawable> tabSaturateIcon; // Saturate_Icon.svg
    std::unique_ptr<juce::Drawable> saturateBackgroundTab1, saturateBackgroundTab2, saturateBackgroundTab3, saturateBackgroundTab4;
    
    // Dub Delay backgrounds
    std::unique_ptr<juce::Drawable> dubdelayBackgroundTab1, dubdelayBackgroundTab2, dubdelayBackgroundTab3, dubdelayBackgroundTab4;
    
    // Redux backgrounds
    std::unique_ptr<juce::Drawable> reduxBackgroundTab1, reduxBackgroundTab2, reduxBackgroundTab3, reduxBackgroundTab4;
    
    // PhaseBloom backgrounds
    std::unique_ptr<juce::Drawable> phasebloomBackgroundTab1, phasebloomBackgroundTab2, phasebloomBackgroundTab3, phasebloomBackgroundTab4;
    
    std::unique_ptr<juce::Drawable> fxPowerOn, stepPowerOn, knobDice, diceLarge, macroAssign;
    std::unique_ptr<juce::Drawable> macro1AssignButton, macro2AssignButton;
    std::unique_ptr<juce::Drawable> lockedIcon, unlockedIcon;
    std::unique_ptr<juce::Drawable> fxTypeCarrotInactive, fxTypeCarrotActive;
    std::unique_ptr<juce::Drawable> presetMenuBackground;
    std::unique_ptr<juce::Drawable> presetMenuCarrot;
    std::unique_ptr<juce::Drawable> saveIcon;
    std::unique_ptr<juce::Drawable> compCrushTabInactive;
    std::unique_ptr<juce::Drawable> compCrushTabActive;
    
    // Category menu tabs (PNGs)
    juce::Image favoritesMenuTab;
    juce::Image rhythmicMenuTab;
    juce::Image distortMenuTab;
    juce::Image lofiMenuTab;
    juce::Image bassMenuTab;
    juce::Image guitarSynthMenuTab;
    juce::Image userMenuTab;
    // … add only what Delay page & Sequencer need

    bool loadAll();
};
