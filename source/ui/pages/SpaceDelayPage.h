#pragma once

#include "EffectPageBase.h"

//==============================================================================
// SpaceDelayPage - Space Delay effect page implementation
//==============================================================================
class SpaceDelayPage : public EffectPageBase
{
public:
    SpaceDelayPage(PluginProcessor& processor, UiAssets& assets);
    ~SpaceDelayPage() override = default;
    
    // Override pure virtual methods from EffectPageBase
    void setupKnobs() override;
    void setupEffectsArea() override;
    void setupSequencerArea() override;
    void setupAllStepsToggle() override;
    void updateKnobValues() override;
    void updateSequencerUI() override;
    void randomizeKnobs() override;
    void randomizeSequencer() override;
    void randomizeAll() override;
    void updateFxAreaVisibility() override;
    void updateStepAreaVisibility() override;
    void showPage() override;
    void hidePage() override;
    
    // Space Delay specific methods
    void setupSpaceDelayUI();
    void setupSpaceDelayAllStepsToggle();
    void updateSpaceDelayKnobValues();
    void updateSpaceDelaySequencerUI();
    void randomizeSpaceDelayKnobs();
    void randomizeSpaceDelaySequencer();
    void randomizeSpaceDelayAll();
    
    // Override common methods for Space Delay specific behavior
    void setupPowerButtons() override;
    void setupStepPowerButton() override;
    void updatePowerButtonStates() override;
    
private:
    // Space Delay specific UI components
    std::unique_ptr<juce::Label> effectsTitle;
    std::unique_ptr<juce::ComboBox> effectTypeDropdown;
    std::unique_ptr<BigComboWithSvgLNF> fxComboLNF;
    std::unique_ptr<CustomDiceButton> diceButton;
    std::unique_ptr<juce::Button> timeSyncToggle;
    bool timeSyncEnabled = false;
    int timeSyncStdMode = 0; // 0 straight, 1 triplet, 2 dotted
    
    // Space Delay specific knobs (8 knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> spaceDelayKnobs;
    std::array<std::unique_ptr<juce::Label>, 8> spaceDelayKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> spaceDelayValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> spaceDelayIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> spaceDelayKnobDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> spaceDelayKnobLockButtons;
    std::array<bool, 8> spaceDelayKnobLocked { false, false, false, false, false, false, false, false };
    
    // Space Delay specific step buttons (16 steps)
    std::array<std::unique_ptr<StepButton>, 16> spaceDelayStepButtons;
    int spaceDelayUiSelectedStep = 0;
    
    // Space Delay specific sequencer components
    std::unique_ptr<juce::TextEditor> spaceDelayStepAmountLabel;
    std::unique_ptr<juce::ComboBox> spaceDelayRateDropdown;
    std::unique_ptr<CircularToggleButton> spaceDelayStdToggle;
    std::unique_ptr<juce::Label> spaceDelayStepTitle;
    std::unique_ptr<CustomDiceButton> spaceDelayStepDiceButton;
    std::unique_ptr<juce::DrawableButton> spaceDelayStepPowerButton;
    bool spaceDelayStepAreaEnabled = true;
    
    // Space Delay specific All Steps toggle
    std::unique_ptr<AllStepsToggleButton> spaceDelayAllStepsToggle;
    std::unique_ptr<juce::Label> spaceDelayAllStepsLabel;
    bool spaceDelayAllStepsEnabled = false;
    
    // Space Delay specific power button
    std::unique_ptr<juce::DrawableButton> spaceDelayFxPowerButton;
    bool spaceDelayFxAreaEnabled = true;
    
    // Space Delay specific parameter IDs
    std::vector<juce::String> spaceDelayParameterIds = {
        "timeMs", "feedback", "wowDepth", "wowRate",
        "saturation", "highCut", "lowCut", "mix"
    };
    
    // Space Delay specific knob names
    std::vector<juce::String> spaceDelayKnobNames = {
        "Time", "Feedback", "Wow Depth", "Wow Rate",
        "Drive", "Hi-Cut", "Low-Cut", "Mix"
    };
    
    // Helper methods
    void setupSpaceDelayKnobs();
    void setupSpaceDelayStepButtons();
    void setupSpaceDelaySequencerComponents();
    void setupSpaceDelayPowerButtons();
    void updateSpaceDelayFxAreaVisibility();
    void updateSpaceDelayStepAreaVisibility();
    void updateSpaceDelayKnobValueLabel(int knobIndex, float value, const juce::String& suffix = "");
    void updateSpaceDelaySequencerStepButton(int stepIndex, bool selected, bool playing, bool enabled = true);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceDelayPage)
};
