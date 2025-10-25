#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>
#include "StepSnapshot.h"
#include "ui/Assets.h"
#include "ui/BigComboWithSvgLNF.h"
#include "ui/CompressSlider.h"
#include "EffectRouter.h"
#include "ui/CustomKnob.h"
#include "ui/StepButton.h"
#include "ui/AllStepsToggleButton.h"
#include "ui/CircularToggleButton.h"
#include "ui/CustomDiceButton.h"
#include "ui/IndicatorBar.h"
#include "ui/LockButton.h"

// Forward declarations
class PluginProcessor;
class RandomizationManager;

//==============================================================================
// EffectPageBase - Abstract base class for all effect pages
//==============================================================================
class EffectPageBase : public juce::Component
{
public:
    EffectPageBase(PluginProcessor& processor, UiAssets& assets);
    virtual ~EffectPageBase() = default;
    
    // Pure virtual methods that each effect page must implement
    virtual void setupKnobs() = 0;
    virtual void setupEffectsArea() = 0;
    virtual void setupSequencerArea() = 0;
    virtual void setupAllStepsToggle() = 0;
    virtual void updateKnobValues() = 0;
    virtual void updateSequencerUI() = 0;
    virtual void randomizeKnobs() = 0;
    virtual void randomizeSequencer() = 0;
    virtual void randomizeAll() = 0;
    virtual void updateFxAreaVisibility() = 0;
    virtual void updateStepAreaVisibility() = 0;
    virtual void showPage() = 0;
    virtual void hidePage() = 0;
    
    // Common methods that can be overridden
    virtual void setupPowerButtons();
    virtual void setupStepPowerButton();
    virtual void updatePowerButtonStates();
    
    // Getters for common state
    bool isFxAreaEnabled() const { return fxAreaEnabled; }
    bool isStepAreaEnabled() const { return stepAreaEnabled; }
    bool isAllStepsEnabled() const { return allStepsEnabled; }
    
    // Setters for common state
    void setFxAreaEnabled(bool enabled);
    void setStepAreaEnabled(bool enabled);
    void setAllStepsEnabled(bool enabled);
    
    // Common utility methods
    void setVisibleVec(const std::vector<juce::Component*>& components, bool visible);
    void updateKnobValueLabel(int knobIndex, float value, const juce::String& suffix = "");
    void updateSequencerStepButton(int stepIndex, bool selected, bool playing, bool enabled = true);
    
    // Common component access
    std::vector<juce::Component*>& getPageGroup() { return pageGroup; }
    const std::vector<juce::Component*>& getPageGroup() const { return pageGroup; }
    
protected:
    // Common member variables
    PluginProcessor& processorRef;
    UiAssets& assets;
    
    // Common UI state
    bool fxAreaEnabled = true;
    bool stepAreaEnabled = true;
    bool allStepsEnabled = false;
    
    // Common UI components
    std::vector<juce::Component*> pageGroup;
    
    // Common knobs (8 per effect)
    std::array<std::unique_ptr<juce::Slider>, 8> knobs;
    std::array<std::unique_ptr<juce::Label>, 8> knobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> valueLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> attachments;
    
    // Common step buttons (16 per effect)
    std::array<std::unique_ptr<juce::Button>, 16> stepButtons;
    int uiSelectedStep = 0;
    
    // Common power buttons
    std::unique_ptr<juce::DrawableButton> fxPowerButton;
    std::unique_ptr<juce::DrawableButton> stepPowerButton;
    
    // Common All Steps toggle
    std::unique_ptr<juce::Button> allStepsToggle;
    std::unique_ptr<juce::Label> allStepsLabel;
    
    // Common sequencer components
    std::unique_ptr<juce::TextEditor> stepAmountLabel;
    std::unique_ptr<juce::ComboBox> rateDropdown;
    std::unique_ptr<juce::Button> stdToggle;
    std::unique_ptr<juce::Label> stepTitle;
    std::unique_ptr<juce::Button> stepDiceButton;
    
    // Common randomization
    std::unique_ptr<RandomizationManager> randomizationManager;
    
    // Common utility methods
    void setupCommonKnobs();
    void setupCommonStepButtons();
    void setupCommonPowerButtons();
    void setupCommonAllStepsToggle();
    void setupCommonSequencerComponents();
    
    // Common parameter access
    juce::AudioProcessorValueTreeState& getAPVTS() { return processorRef.getAPVTS(); }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return processorRef.getAPVTS(); }
    
    // Common effect ID (to be set by derived classes)
    EffectID effectID = EffectID::SpaceDelay;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectPageBase)
};
