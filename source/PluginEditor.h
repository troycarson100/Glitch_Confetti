#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include "StepSnapshot.h"
#include "ui/Assets.h"
#include "ui/BigComboWithSvgLNF.h"
#include "DualBarMeter.h"
#include "ui/PanManBar.h"
#include "EffectRouter.h"
#include "ui/OutputSpectrumView.h"
#include "ui/SpectrumFilterSlider.h"

// Forward declaration
struct RandomizationManager;

// Tab system enum
enum class FxPageID { SpaceDelay, Panner, Dirt, Chorus, Reverb, Granular, Slicer };

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
    friend class RandomizationManager;
    
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
    // Allow step amount TextEditors to receive keyboard input
    bool keyPressed(const juce::KeyPress& key) override;

    private:
        PluginProcessor& processorRef;
        
        // Randomization manager (thread-safe)
        std::unique_ptr<RandomizationManager> randomizationManager;
        
        // Flag to prevent onValueChange from saving snapshots during randomization reload
        std::atomic<bool> isLoadingFromSnapshot { false };
        
        // UI Assets
        UiAssets assets;
        
        // Tab system
        FxPageID currentPage = FxPageID::SpaceDelay;
        
    // Tab buttons (SVG)
    std::unique_ptr<juce::DrawableButton> tabSpaceDelay;
    std::unique_ptr<juce::DrawableButton> tabPanner;
    std::unique_ptr<juce::DrawableButton> tabDirt;
    std::unique_ptr<juce::DrawableButton> tabChorus;
    
    // Effect selector dropdowns (one per page/slot)
    std::unique_ptr<juce::ComboBox> effectSelector1;
    std::unique_ptr<juce::ComboBox> effectSelector2;
    std::unique_ptr<juce::ComboBox> effectSelector3;
    std::unique_ptr<juce::ComboBox> effectSelector4;
        
        // Groups: we will only toggle visibility; we DO NOT reparent anything.
        juce::OwnedArray<juce::Component> dummyKeepAlive; // (unused, but handy if Claude tries to delete)
        std::vector<juce::Component*> spaceDelayGroup;    // pointers to existing delay UI components
        std::vector<juce::Component*> pannerGroup;        // pointers to panner UI components
        std::vector<juce::Component*> dirtGroup;          // pointers to dirt UI components
        std::vector<juce::Component*> chorusGroup;        // pointers to chorus UI components
        std::vector<juce::Component*> reverbGroup;        // pointers to reverb UI components
    
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
    
    // Master area title and controls
    std::unique_ptr<juce::Label> masterTitle;
    std::unique_ptr<CustomDiceButton> masterDiceButton;
    
    // Macro knobs
    std::array<std::unique_ptr<CustomKnob>, 2> macroKnobs;
    std::array<std::unique_ptr<juce::Label>, 2> macroLabels;
    std::array<std::unique_ptr<juce::DrawableButton>, 2> macroAssignButtons;
    
    // Modern dual-bar meters (pre-fx and post-fx)
    std::unique_ptr<DualBarMeter> inMeter, outMeter;
    
    // L/C/R pan indicator
    std::unique_ptr<PanManBar> panBar;
    std::unique_ptr<OutputSpectrumView> outputSpectrumView;
    std::unique_ptr<SpectrumFilterSlider> spectrumFilterSlider;
    
    // Effects area components
    std::unique_ptr<juce::Label> effectsTitle;
    std::unique_ptr<juce::ComboBox> effectTypeDropdown;
    std::unique_ptr<BigComboWithSvgLNF> fxComboLNF;
    std::unique_ptr<class RouterComboLookAndFeel> routerComboLNF;
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
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> autopanAttachments;
    std::array<std::unique_ptr<juce::Label>, 6> autopanKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 6> autopanValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 6> autopanIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 6> autopanDiceButtons;
    std::array<std::unique_ptr<LockButton>, 6> autopanLockButtons;
    std::array<bool, 6> autopanKnobLocked { false, false, false, false, false, false };
    
    // AutoPan effects area components
    std::unique_ptr<juce::Label> autopanEffectsTitle;
    std::unique_ptr<BigComboWithSvgLNF> autopanFxComboLNF;
    std::unique_ptr<CustomDiceButton> autopanDiceButton;
    std::unique_ptr<juce::Button> autopanTimeSyncToggle; // S circle toggle for Rate knob
    bool autopanTimeSyncEnabled = false;
    int autopanTimeSyncStdMode = 0; // 0 straight, 1 triplet, 2 dotted
    std::unique_ptr<juce::DrawableButton> autopanFxPowerButton;
    bool autopanFxAreaEnabled = true;
    
    // AutoPan sequencer components (independent from delay page)
    std::array<std::unique_ptr<StepButton>, 16> autopanStepButtons;
    int autopanUiSelectedStep = 0;  // UI selected step for editing (0-15)
    std::unique_ptr<juce::TextEditor> autopanStepAmountLabel;
    std::unique_ptr<juce::ComboBox> autopanRateDropdown;
    std::unique_ptr<CircularToggleButton> autopanStdToggle;
    std::unique_ptr<juce::Label> autopanStepTitle;
    std::unique_ptr<CustomDiceButton> autopanStepDiceButton;
    std::unique_ptr<juce::DrawableButton> autopanStepPowerButton;
    bool autopanStepAreaEnabled = true; // Sequencer enabled by default (matches Delay behavior)
    
    // AutoPan All Steps toggle
    std::unique_ptr<AllStepsToggleButton> autopanAllStepsToggle;
    std::unique_ptr<juce::Label> autopanAllStepsLabel;
    bool autopanAllStepsEnabled = false;
    
    // Dirt page components (clone of Delay page layout)
    std::array<std::unique_ptr<CustomKnob>, 8> dirtKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> dirtAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> dirtKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> dirtValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> dirtIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> dirtDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> dirtLockButtons;
    std::array<bool, 8> dirtKnobLocked { false, false, false, false, false, false, false, false };
    
    // Dirt effects area components
    std::unique_ptr<juce::Label> dirtEffectsTitle;
    std::unique_ptr<CustomDiceButton> dirtDiceButton;
    std::unique_ptr<juce::DrawableButton> dirtFxPowerButton;
    bool dirtFxAreaEnabled = true;
    
    // Dirt sequencer components (independent sequencer)
    std::array<std::unique_ptr<StepButton>, 16> dirtStepButtons;
    int dirtUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> dirtStepAmountLabel;
    std::unique_ptr<juce::ComboBox> dirtRateDropdown;
    std::unique_ptr<CircularToggleButton> dirtStdToggle;
    std::unique_ptr<juce::Label> dirtStepTitle;
    std::unique_ptr<CustomDiceButton> dirtStepDiceButton;
    std::unique_ptr<juce::DrawableButton> dirtStepPowerButton;
    bool dirtStepAreaEnabled = true; // Sequencer enabled by default
    
    // Dirt All Steps toggle
    std::unique_ptr<AllStepsToggleButton> dirtAllStepsToggle;
    std::unique_ptr<juce::Label> dirtAllStepsLabel;
    bool dirtAllStepsEnabled = false;
    
    // Chorus page components (clone of Dirt page layout)
    std::array<std::unique_ptr<CustomKnob>, 8> chorusKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> chorusAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> chorusKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> chorusValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> chorusIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> chorusDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> chorusLockButtons;
    std::array<bool, 8> chorusKnobLocked { false, false, false, false, false, false, false, false };
    
    // Chorus effects area components
    std::unique_ptr<juce::Label> chorusEffectsTitle;
    std::unique_ptr<CustomDiceButton> chorusDiceButton;
    std::unique_ptr<juce::DrawableButton> chorusFxPowerButton;
    bool chorusFxAreaEnabled = true;
    
    // Chorus sequencer components (independent sequencer)
    std::array<std::unique_ptr<StepButton>, 16> chorusStepButtons;
    int chorusUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> chorusStepAmountLabel;
    std::unique_ptr<juce::ComboBox> chorusRateDropdown;
    std::unique_ptr<CircularToggleButton> chorusStdToggle;
    std::unique_ptr<juce::Label> chorusStepTitle;
    std::unique_ptr<CustomDiceButton> chorusStepDiceButton;
    std::unique_ptr<juce::DrawableButton> chorusStepPowerButton;
    bool chorusStepAreaEnabled = true;
    
    // Chorus All Steps toggle
    std::unique_ptr<AllStepsToggleButton> chorusAllStepsToggle;
    std::unique_ptr<juce::Label> chorusAllStepsLabel;
    bool chorusAllStepsEnabled = false;
    
    // Reverb page components (clone of Chorus page layout)
    std::array<std::unique_ptr<CustomKnob>, 8> reverbKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> reverbAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> reverbKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> reverbValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> reverbIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> reverbDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> reverbLockButtons;
    std::array<bool, 8> reverbKnobLocked { false, false, false, false, false, false, false, false };
    
    // Reverb effects area components
    std::unique_ptr<juce::Label> reverbEffectsTitle;
    std::unique_ptr<CustomDiceButton> reverbDiceButton;
    std::unique_ptr<juce::DrawableButton> reverbFxPowerButton;
    bool reverbFxAreaEnabled = true;
    
    // Reverb sequencer components (independent sequencer)
    std::array<std::unique_ptr<StepButton>, 16> reverbStepButtons;
    int reverbUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> reverbStepAmountLabel;
    std::unique_ptr<juce::ComboBox> reverbRateDropdown;
    std::unique_ptr<CircularToggleButton> reverbStdToggle;
    std::unique_ptr<juce::Label> reverbStepTitle;
    std::unique_ptr<CustomDiceButton> reverbStepDiceButton;
    std::unique_ptr<juce::DrawableButton> reverbStepPowerButton;
    bool reverbStepAreaEnabled = true;
    
    // Reverb All Steps toggle
    std::unique_ptr<AllStepsToggleButton> reverbAllStepsToggle;
    std::unique_ptr<juce::Label> reverbAllStepsLabel;
    bool reverbAllStepsEnabled = false;
    
    // Granular page components (with sequencer)
    std::array<std::unique_ptr<CustomKnob>, 8> granularKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> granularAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> granularKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> granularValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> granularIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> granularDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> granularLockButtons;
    std::array<bool, 8> granularKnobLocked { false, false, false, false, false, false, false, false };
    
    // Granular effects area (power button)
    std::unique_ptr<juce::Label> granularEffectsTitle;
    std::unique_ptr<CustomDiceButton> granularDiceButton;
    std::unique_ptr<juce::DrawableButton> granularFxPowerButton;
    bool granularFxAreaEnabled = true;
    
    // Granular step sequencer components
    std::array<std::unique_ptr<StepButton>, 16> granularStepButtons;
    int granularUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> granularStepAmountLabel;
    std::unique_ptr<juce::ComboBox> granularRateDropdown;
    std::unique_ptr<CircularToggleButton> granularStdToggle;
    std::unique_ptr<juce::Button> granularDensitySyncToggle;
    bool granularDensitySyncEnabled = false;
    int granularDensitySyncStdMode = 0; // 0 straight, 1 triplet, 2 dotted
    std::unique_ptr<juce::Label> granularStepTitle;
    std::unique_ptr<CustomDiceButton> granularStepDiceButton;
    std::unique_ptr<juce::DrawableButton> granularStepPowerButton;
    bool granularStepAreaEnabled = true;
    
    // Granular All Steps toggle
    std::unique_ptr<AllStepsToggleButton> granularAllStepsToggle;
    std::unique_ptr<juce::Label> granularAllStepsLabel;
    bool granularAllStepsEnabled = false;
    
    std::vector<juce::Component*> granularGroup; // All Granular UI components for visibility toggling
    
    // Slicer page components (6 knobs + sync toggle + LED strip)
    std::array<std::unique_ptr<CustomKnob>, 6> slicerKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> slicerAttachments;
    std::array<std::unique_ptr<juce::Label>, 6> slicerKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 6> slicerValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 6> slicerIndicatorBars;
    
    // Slicer effects area
    std::unique_ptr<juce::Label> slicerEffectsTitle;
    std::unique_ptr<juce::DrawableButton> slicerFxPowerButton;
    bool slicerFxAreaEnabled = true;
    
    // Slicer sync toggle (styled like granular density sync)
    std::unique_ptr<juce::Button> slicerSyncToggle;
    bool slicerSyncEnabled = true;
    
    // LED strip for 16-step pattern visualization
    std::array<std::unique_ptr<juce::Component>, 16> slicerLEDStrip;
    int slicerCurrentStep = 0; // For LED playhead indicator
    
    // Slicer step sequencer area (matching other pages)
    std::array<std::unique_ptr<StepButton>, 16> slicerStepButtons;
    int slicerUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> slicerStepAmountLabel;
    std::unique_ptr<juce::ComboBox> slicerRateDropdown;
    std::unique_ptr<CircularToggleButton> slicerStdToggle;
    std::unique_ptr<juce::Label> slicerStepTitle;
    std::unique_ptr<CustomDiceButton> slicerStepDiceButton;
    std::unique_ptr<juce::DrawableButton> slicerStepPowerButton;
    bool slicerStepAreaEnabled = true;
    
    // Slicer All Steps toggle
    std::unique_ptr<AllStepsToggleButton> slicerAllStepsToggle;
    std::unique_ptr<juce::Label> slicerAllStepsLabel;
    bool slicerAllStepsEnabled = false;
    
    // Slicer main dice button
    std::unique_ptr<CustomDiceButton> slicerDiceButton;
    
    std::vector<juce::Component*> slicerGroup; // All Slicer UI components for visibility toggling
    
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
        
        // Dirt page setup methods
        void setupDirtKnobs();
        void setupDirtEffectsArea();
        void setupDirtSequencerArea();
        void setupDirtAllStepsToggle();
        void setupAutoPanStepPowerButton();
        void updateAutoPanFxAreaVisibility();
        void updateAutoPanStepAreaVisibility();
        void updateDirtFxAreaVisibility();
        void updateDirtStepAreaVisibility();
        void randomizeAutoPanKnobValues();
        void randomizeIndividualAutoPanKnob(int knobIndex);
        void updateAutoPanParameterFromKnob(int knobIndex);
        void onAutoPanStepButtonClicked(int stepIndex);
        void updateAutoPanSequencerUI();
        
        // Dirt page helper methods
        void randomizeDirtKnobValues();
        void randomizeIndividualDirtKnob(int knobIndex);
        void updateDirtParameterFromKnob(int knobIndex);
        void onDirtStepButtonClicked(int stepIndex);
        void updateDirtSequencerUI();
        
        // Chorus page setup methods
        void setupChorusKnobs();
        void setupChorusEffectsArea();
        void setupChorusSequencerArea();
        void setupChorusAllStepsToggle();
        void updateChorusFxAreaVisibility();
        void updateChorusStepAreaVisibility();
        void randomizeChorusKnobValues();
        void randomizeIndividualChorusKnob(int knobIndex);
        void updateChorusParameterFromKnob(int knobIndex);
        void onChorusStepButtonClicked(int stepIndex);
        void updateChorusSequencerUI();
        
        // Reverb page setup methods
        void setupReverbKnobs();
        void setupReverbEffectsArea();
        void setupReverbSequencerArea();
        void setupReverbAllStepsToggle();
        void updateReverbFxAreaVisibility();
        void updateReverbStepAreaVisibility();
        void randomizeReverbKnobValues();
        void randomizeIndividualReverbKnob(int knobIndex);
        void updateReverbParameterFromKnob(int knobIndex);
        void onReverbStepButtonClicked(int stepIndex);
        void updateReverbSequencerUI();
        
        // Granular page setup methods
        void setupGranularKnobs();
        void setupGranularEffectsArea();
        void setupGranularSequencerArea();
        void setupGranularAllStepsToggle();
        void updateGranularFxAreaVisibility();
        void updateGranularStepAreaVisibility();
        void randomizeGranularKnobValues();
        void randomizeIndividualGranularKnob(int knobIndex);
        void updateGranularParameterFromKnob(int knobIndex);
        void onGranularStepButtonClicked(int stepIndex);
        void updateGranularSequencerUI();
        
        // Slicer page helper methods
        void setupSlicerKnobs();
        void setupSlicerEffectsArea();
        void setupSlicerSequencerArea();
        void setupSlicerAllStepsToggle();
        void updateSlicerFxAreaVisibility();
        void updateSlicerStepAreaVisibility();
        void randomizeSlicerKnobValues();
        void randomizeIndividualSlicerKnob(int knobIndex); // Helper for randomization
        void updateSlicerParameterFromKnob(int knobIndex);
        void updateSlicerSequencerUI();
        void onSlicerStepButtonClicked(int stepIndex);
        void updateSlicerLEDStrip();
        
        void togglePlayback();
        
        // Tab system helpers
        void setupTabSystem();
        void showPage(FxPageID id);
    void drawGridOverlay(juce::Graphics& g);
    void drawMainAreas(juce::Graphics& g);
    
    // Effect router UI helpers
    void onEffectSelectorChanged(int slotIndex);
    void updateAllEffectSelectors();
    void updateBackgroundsAfterSwap();
    void updateTabButtonImages();
    juce::ComboBox* getEffectSelectorForSlot(int slotIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};