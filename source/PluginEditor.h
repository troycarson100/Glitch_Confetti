#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include "StepSnapshot.h"
#include "ui/Assets.h"
#include "ui/BigComboWithSvgLNF.h"
#include "DualBarMeter.h"

// Tab system enum
enum class FxPageID { SpaceDelay, Panner };

// Forward declaration
class PluginProcessor;

//==============================================================================
// IndicatorBar class
//==============================================================================
class IndicatorBar : public juce::Component
{
public:
    IndicatorBar();
    ~IndicatorBar() override = default;
    
    void paint(juce::Graphics& g) override;
    void setValue(float value); // 0.0 to 1.0
    
private:
    float currentValue = 0.5f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndicatorBar)
};

    //==============================================================================
    // AllStepsToggleButton class
    //==============================================================================
    class AllStepsToggleButton : public juce::Button
    {
    public:
        AllStepsToggleButton();
        ~AllStepsToggleButton() override = default;
        
        void paintButton(juce::Graphics& g, bool over, bool down) override;
        void setImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active);
        
    private:
        std::unique_ptr<juce::Drawable> inactiveImage;
        std::unique_ptr<juce::Drawable> activeImage;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AllStepsToggleButton)
    };

    //==============================================================================
    // LockButton class
    //==============================================================================
    class LockButton : public juce::Button
    {
    public:
        LockButton();
        ~LockButton() override = default;
        
        void paintButton(juce::Graphics& g, bool over, bool down) override;
        void setImages(std::unique_ptr<juce::Drawable> unlocked, std::unique_ptr<juce::Drawable> locked);
        void setAlpha(float alpha);
        
    private:
        std::unique_ptr<juce::Drawable> unlockedImage;
        std::unique_ptr<juce::Drawable> lockedImage;
        std::unique_ptr<juce::Drawable> originalUnlockedImage;
        std::unique_ptr<juce::Drawable> originalLockedImage;
        float buttonAlpha = 1.0f;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LockButton)
    };

//==============================================================================
// CustomDiceButton class
//==============================================================================
class CustomDiceButton : public juce::Button
{
public:
    CustomDiceButton();
    ~CustomDiceButton() override = default;
    
    void paintButton(juce::Graphics& g, bool over, bool down) override;
    void setDiceImage(std::unique_ptr<juce::Drawable> dice);
    
private:
    std::unique_ptr<juce::Drawable> diceImage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomDiceButton)
};

//==============================================================================
// CustomKnob class
//==============================================================================
class CustomKnob : public juce::Slider
{
public:
    CustomKnob();
    ~CustomKnob() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setRingImage(std::unique_ptr<juce::Drawable> ring);
    void setInnerImage(std::unique_ptr<juce::Drawable> inner);
    
private:
    std::unique_ptr<juce::Drawable> ringImage;
    std::unique_ptr<juce::Drawable> innerImage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomKnob)
};

//==============================================================================
// CircularToggleButton class
//==============================================================================
class CircularToggleButton : public juce::Button
{
public:
    CircularToggleButton();
    ~CircularToggleButton() override = default;
    
    void paintButton(juce::Graphics& g, bool over, bool down) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CircularToggleButton)
};

//==============================================================================
// PlayButton class
//==============================================================================
class PlayButton : public juce::Button
{
public:
    PlayButton();
    ~PlayButton() override = default;
    
    void paintButton(juce::Graphics& g, bool over, bool down) override;
    void setPlaying(bool playing) { isPlaying = playing; repaint(); }
    bool isPlayingState() const { return isPlaying; }

private:
    bool isPlaying = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayButton)
};




//==============================================================================
// StepButton class for sequencer
//==============================================================================
class StepButton : public juce::Button
{
public:
    StepButton(int stepIndex);
    ~StepButton() override = default;
    
    void paintButton(juce::Graphics& g, bool over, bool down) override;
    void setActiveImage(std::unique_ptr<juce::Drawable> active);
    void setInactiveImage(std::unique_ptr<juce::Drawable> inactive);
    void setSelected(bool selected);
    void setPlaying(bool playing);
    void setEnabledStep(bool enabled) { isEnabledStep = enabled; repaint(); }
    bool getEnabledStep() const { return isEnabledStep; }
    
private:
    int stepIndex;
    bool isSelected = false;
    bool isPlaying = false;
    bool isEnabledStep = true;
    std::unique_ptr<juce::Drawable> activeImage;
    std::unique_ptr<juce::Drawable> inactiveImage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepButton)
};


//==============================================================================
// PluginEditor class
//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    private:
        PluginProcessor& processorRef;
        
        // UI Assets
        UiAssets assets;
        
        // Tab system
        FxPageID currentPage = FxPageID::SpaceDelay;
        
    // Tab buttons (SVG)
    std::unique_ptr<juce::DrawableButton> tabSpaceDelay;
    std::unique_ptr<juce::DrawableButton> tabPanner;
        
        // Groups: we will only toggle visibility; we DO NOT reparent anything.
        juce::OwnedArray<juce::Component> dummyKeepAlive; // (unused, but handy if Claude tries to delete)
        std::vector<juce::Component*> spaceDelayGroup;    // pointers to existing delay UI components
        std::vector<juce::Component*> pannerGroup;        // pointers to panner UI components (later)
    
        // UI Components
        std::array<std::unique_ptr<CustomKnob>, 8> knobs;
        std::array<std::unique_ptr<juce::Label>, 8> knobLabels;
        std::array<std::unique_ptr<juce::Label>, 8> valueLabels;
        std::array<std::unique_ptr<IndicatorBar>, 8> indicatorBars;
        std::array<std::unique_ptr<CustomDiceButton>, 8> knobDiceButtons; // kept but hidden
        std::array<std::unique_ptr<LockButton>, 8> knobLockButtons;
        std::array<bool, 8> knobLocked { false, false, false, false, false, false, false, false };
    
    // Master knobs
    std::array<std::unique_ptr<CustomKnob>, 3> masterKnobs;
    std::array<std::unique_ptr<juce::Label>, 3> masterLabels;
    std::array<std::unique_ptr<juce::Label>, 3> masterValueLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> masterAttachments;
    
    // Macro knobs
    std::array<std::unique_ptr<CustomKnob>, 2> macroKnobs;
    std::array<std::unique_ptr<juce::Label>, 2> macroLabels;
    std::array<std::unique_ptr<juce::DrawableButton>, 2> macroAssignButtons;
    
    // Modern dual-bar meters (pre-fx and post-fx)
    std::unique_ptr<DualBarMeter> inMeter, outMeter;
    
    // Effects area components
    std::unique_ptr<juce::Label> effectsTitle;
    std::unique_ptr<juce::DrawableButton> spaceDelayTitle;
    std::unique_ptr<juce::ComboBox> effectTypeDropdown;
    std::unique_ptr<BigComboWithSvgLNF> fxComboLNF;
    std::unique_ptr<CustomDiceButton> diceButton;
    std::unique_ptr<juce::Button> timeSyncToggle; // S circle toggle
    bool timeSyncEnabled = false;
    int timeSyncStdMode = 0; // 0 straight, 1 triplet, 2 dotted
    std::unique_ptr<juce::DrawableButton> fxPowerButton;
    bool fxAreaEnabled = true;
    
    // All Steps toggle
    std::unique_ptr<juce::Button> allStepsToggle;
    std::unique_ptr<juce::Label> allStepsLabel;
    bool allStepsEnabled = false;
    
    // Sequencer area components
    std::array<std::unique_ptr<StepButton>, 16> stepButtons;
    std::unique_ptr<juce::Label> stepAmountLabel;
    std::unique_ptr<juce::ComboBox> rateDropdown;
    std::unique_ptr<CircularToggleButton> stdToggle;
    std::unique_ptr<juce::Label> stepTitle;
    std::unique_ptr<CustomDiceButton> stepDiceButton;
    std::unique_ptr<juce::DrawableButton> stepPowerButton;
    bool stepAreaEnabled = true; // Default to enabled
    
    // UI visibility toggle
    std::unique_ptr<juce::ToggleButton> uiToggleButton;
    bool uiVisible = false; // Default to hidden
    
    // Play button
    std::unique_ptr<PlayButton> playButton;
    
    // AutoPan page components
    std::array<std::unique_ptr<CustomKnob>, 6> autopanKnobs; // 6 knobs instead of 8
    std::array<std::unique_ptr<juce::Label>, 6> autopanKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 6> autopanValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 6> autopanIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 6> autopanDiceButtons;
    std::array<std::unique_ptr<LockButton>, 6> autopanLockButtons;
    std::array<bool, 6> autopanKnobLocked { false, false, false, false, false, false };
    
    // AutoPan effects area components
    std::unique_ptr<juce::Label> autopanEffectsTitle;
    std::unique_ptr<juce::ComboBox> autopanEffectTypeDropdown;
    std::unique_ptr<BigComboWithSvgLNF> autopanFxComboLNF;
    std::unique_ptr<CustomDiceButton> autopanDiceButton;
    std::unique_ptr<CircularToggleButton> autopanTimeSyncToggle; // S circle toggle for Rate knob
    bool autopanTimeSyncEnabled = false;
    int autopanTimeSyncStdMode = 0; // 0 straight, 1 triplet, 2 dotted
    std::unique_ptr<juce::DrawableButton> autopanFxPowerButton;
    bool autopanFxAreaEnabled = true;
    
    // AutoPan sequencer components (independent from delay page)
    std::array<std::unique_ptr<StepButton>, 16> autopanStepButtons;
    std::unique_ptr<juce::Label> autopanStepAmountLabel;
    std::unique_ptr<juce::ComboBox> autopanRateDropdown;
    std::unique_ptr<CircularToggleButton> autopanStdToggle;
    std::unique_ptr<juce::Label> autopanStepTitle;
    std::unique_ptr<CustomDiceButton> autopanStepDiceButton;
    std::unique_ptr<juce::DrawableButton> autopanStepPowerButton;
    bool autopanStepAreaEnabled = true;
    
    // AutoPan All Steps toggle
    std::unique_ptr<CircularToggleButton> autopanAllStepsToggle;
    std::unique_ptr<juce::Label> autopanAllStepsLabel;
    bool autopanAllStepsEnabled = false;
    
        // Helper methods
        void setupKnobs();
        void setupMasterKnobs();
        void setupMacroKnobs();
        void setupEffectsArea();
        void setupSpaceDelayUI();
        void setupFxPowerButton();
        void updateFxAreaVisibility();
        void setupAllStepsToggle();
        void setupSequencerArea();
        void setupStepPowerButton();
        void updateStepAreaVisibility();
        void randomizeKnobValues();
        void randomizeIndividualKnob(int knobIndex);
        void updateParameterFromKnob(int knobIndex);
        void updateAllStepSnapshots(int knobIndex);
        void onStepButtonClicked(int stepIndex);
        void updateSequencerUI();
        void setupUIToggle();
        void toggleUIVisibility();
        void setupPlayButton();
        
        // AutoPan page helper methods
        void setupAutoPanKnobs();
        void setupAutoPanEffectsArea();
        void setupAutoPanSequencerArea();
        void setupAutoPanAllStepsToggle();
        void setupAutoPanStepPowerButton();
        void updateAutoPanFxAreaVisibility();
        void updateAutoPanStepAreaVisibility();
        void randomizeAutoPanKnobValues();
        void randomizeIndividualAutoPanKnob(int knobIndex);
        void updateAutoPanParameterFromKnob(int knobIndex);
        void onAutoPanStepButtonClicked(int stepIndex);
        void updateAutoPanSequencerUI();
        void togglePlayback();
        
        // Tab system helpers
        void setupTabSystem();
        void showPage(FxPageID id);
    void drawGridOverlay(juce::Graphics& g);
    void drawMainAreas(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};