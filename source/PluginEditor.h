#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include "StepSnapshot.h"
#include "ui/Assets.h"
#include "ui/BigComboWithSvgLNF.h"
#include "ui/CompressSlider.h"
#include "DualBarMeter.h"
#include "ui/PanManBar.h"
#include "EffectRouter.h"
#include "ui/OutputSpectrumView.h"
#include "ui/SpectrumFilterSlider.h"
#include "ui/GumroadLicenseDialog.h"

// Forward declarations
struct RandomizationManager;
class PresetManager;
class PresetBrowserOverlay;
#include "ui/StepSequencer.h"

// Tab system enum
enum class FxPageID { SpaceDelay, Panner, Dirt, Chorus, Reverb, Granular, Slicer, Redux, PhaseBloom, Formant, Form2 };

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
    
    static constexpr int defaultSize = 10;
    static constexpr int defaultIconInset = 0;
        
        void paintButton(juce::Graphics& g, bool over, bool down) override;
        void setImages(std::unique_ptr<juce::Drawable> unlocked, std::unique_ptr<juce::Drawable> locked);
        void setAlpha(float alpha);
    void setIconInset(int inset) { iconInset = inset; repaint(); }
        
    private:
        std::unique_ptr<juce::Drawable> unlockedImage;
        std::unique_ptr<juce::Drawable> lockedImage;
        std::unique_ptr<juce::Drawable> originalUnlockedImage;
        std::unique_ptr<juce::Drawable> originalLockedImage;
        float buttonAlpha = 1.0f;
    int iconInset = defaultIconInset;
        
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
// ResonanceKnobLNF - Custom LookAndFeel for small resonance knobs
//==============================================================================
class ResonanceKnobLNF : public juce::LookAndFeel_V4
{
public:
    ResonanceKnobLNF() {}
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPosProportional, float rotaryStartAngle,
                         float rotaryEndAngle, juce::Slider& slider) override;
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
// SyncToggleButton class (for "S" button on sync-able parameters)
//==============================================================================
class SyncToggleButton : public juce::Button
{
public:
    SyncToggleButton() : juce::Button("SyncToggle") {}
    ~SyncToggleButton() override = default;
    
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        juce::ignoreUnused(over, down);
        auto r = getLocalBounds().toFloat();
        const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
        auto centre = r.getCentre();
        g.setColour(juce::Colours::white);
        if (getToggleState()) {
            g.fillEllipse(centre.x - radius, centre.y - radius, radius*2, radius*2);
            g.setColour(juce::Colours::black);
        } else {
            g.drawEllipse(centre.x - radius, centre.y - radius, radius*2, radius*2, 2.0f);
        }
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("S", r, juce::Justification::centred);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SyncToggleButton)
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
// PresetSelectorButton class
//==============================================================================
class PresetSelectorButton : public juce::Button
{
public:
    PresetSelectorButton();
    ~PresetSelectorButton() override = default;
    
    void paintButton(juce::Graphics& g, bool over, bool down) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void setPresetName(const juce::String& name);
    void setCarrotImage(std::unique_ptr<juce::Drawable> carrot);
    void setDiceImage(std::unique_ptr<juce::Drawable> dice);
    void setSaveIcon(std::unique_ptr<juce::Drawable> save);
    
    std::function<void()> onSaveClick;
    std::function<void()> onDiceClick;
    
private:
    juce::String presetName = "Default Preset";
    std::unique_ptr<juce::Drawable> carrotImage;
    std::unique_ptr<juce::Drawable> diceImage;
    std::unique_ptr<juce::Drawable> saveIcon;
    juce::Rectangle<float> saveIconBounds;
    juce::Rectangle<float> diceBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetSelectorButton)
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
        
    // Preset management system
    std::unique_ptr<PresetManager> presetManager;
    std::unique_ptr<PresetBrowserOverlay> presetBrowser;
    std::unique_ptr<PresetSelectorButton> presetBrowserButton;
    std::unique_ptr<juce::DrawableButton> compCrushTabButton;
    
    // COMPRESS+ gain reduction meter with professional exponential ballistics
    class GainReductionMeter : public juce::Component, public juce::Timer
    {
    public:
        GainReductionMeter() 
        {
            // Calculate exponential coefficients for ballistics
            // Using formula: coeff = exp(-1.0 / (timeMs * updateHz / 1000.0))
            const float updateHz = 60.0f; // 60Hz update rate
            attackCoeff = std::exp(-1.0f / (attackTimeMs * updateHz / 1000.0f));
            releaseCoeff = std::exp(-1.0f / (releaseTimeMs * updateHz / 1000.0f));
            
            startTimerHz(60); // 60Hz for smooth visual updates
        }
        
        ~GainReductionMeter()
        {
            stopTimer();
        }
        
        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const float cornerRadius = 2.0f;
            
            // Background (white)
            g.setColour(juce::Colour(0xFFFFFFFF));
            g.fillRoundedRectangle(bounds, cornerRadius);
            
            // Gain reduction bar (orange) - use smoothed value directly
            if (currentDisplayValue > 0.01f) {
                float normalized = currentDisplayValue / 30.0f; // 0-30dB range
                float fillWidth = bounds.getWidth() * normalized;
                fillWidth = juce::jlimit(0.0f, bounds.getWidth(), fillWidth);
                
                auto fillRect = bounds.removeFromRight(fillWidth);
                g.setColour(juce::Colour(0xFFE96A3E)); // Orange #E96A3E
                g.fillRoundedRectangle(fillRect, cornerRadius);
            }
        }
        
        void setGainReduction(float newDb)
        {
            targetValue = juce::jlimit(0.0f, 30.0f, newDb);
        }
        
        void timerCallback() override
        {
            // Apply asymmetric exponential smoothing (VU-style ballistics)
            float coeff;
            if (targetValue > currentDisplayValue) {
                // Fast attack for rises
                coeff = attackCoeff;
            } else {
                // Very slow release for smooth decay
                coeff = releaseCoeff;
            }
            
            // Exponential smoothing: y[n] = coeff * y[n-1] + (1-coeff) * x[n]
            currentDisplayValue = coeff * currentDisplayValue + (1.0f - coeff) * targetValue;
            
            // Update peak hold with smooth decay
            if (targetValue > peakValue) {
                peakValue = targetValue;
                peakHoldCounter = peakHoldTimeMs;
            } else {
                peakHoldCounter = juce::jmax(0.0f, peakHoldCounter - (1000.0f / 60.0f));
                if (peakHoldCounter <= 0.0f) {
                    // Smooth peak decay
                    peakValue = juce::jmax(currentDisplayValue, peakValue * 0.98f);
                }
            }
            
            repaint();
        }
        
    private:
        // Exponential ballistics for smooth movement
        float currentDisplayValue = 0.0f;
        float targetValue = 0.0f;
        
        // Time constants for professional VU-style ballistics
        // Very smooth release for butter-smooth decay
        const float attackTimeMs = 10.0f;    // Fast attack (10ms)
        const float releaseTimeMs = 500.0f;  // Very slow release (500ms)
        
        // Calculated coefficients (set in constructor)
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        
        // Peak hold for transient display
        float peakValue = 0.0f;
        float peakHoldCounter = 0.0f;
        const float peakHoldTimeMs = 1000.0f; // Hold peaks for 1 second
    };
    
    // Smaller duplicate gain reduction meter for top area
    class SmallGainReductionMeter : public juce::Component, public juce::Timer
    {
    public:
        SmallGainReductionMeter() 
        {
            // Calculate exponential coefficients for ballistics
            // Using formula: coeff = exp(-1.0 / (timeMs * updateHz / 1000.0))
            const float updateHz = 60.0f; // 60Hz update rate
            attackCoeff = std::exp(-1.0f / (attackTimeMs * updateHz / 1000.0f));
            releaseCoeff = std::exp(-1.0f / (releaseTimeMs * updateHz / 1000.0f));
            
            startTimerHz(60); // 60Hz for smooth visual updates
        }
        
        ~SmallGainReductionMeter()
        {
            stopTimer();
        }
        
        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const float cornerRadius = 1.0f; // Smaller corner radius for small bar
            
            // Background (white)
            g.setColour(juce::Colour(0xFFFFFFFF));
            g.fillRoundedRectangle(bounds, cornerRadius);
            
            // Gain reduction bar (orange) - use smoothed value directly
            if (currentDisplayValue > 0.01f) {
                float normalized = currentDisplayValue / 30.0f; // 0-30dB range
                float fillWidth = bounds.getWidth() * normalized;
                fillWidth = juce::jlimit(0.0f, bounds.getWidth(), fillWidth);
                
                auto fillRect = bounds.removeFromRight(fillWidth);
                g.setColour(juce::Colour(0xFFE96A3E)); // Orange #E96A3E
                g.fillRoundedRectangle(fillRect, cornerRadius);
            }
        }
        
        void setGainReduction(float newDb)
        {
            targetValue = juce::jlimit(0.0f, 30.0f, newDb);
        }
        
        void timerCallback() override
        {
            // Apply asymmetric exponential smoothing (VU-style ballistics)
            float coeff;
            if (targetValue > currentDisplayValue) {
                // Fast attack for rises
                coeff = attackCoeff;
            } else {
                // Very slow release for smooth decay
                coeff = releaseCoeff;
            }
            
            // Exponential smoothing: y[n] = coeff * y[n-1] + (1-coeff) * x[n]
            currentDisplayValue = coeff * currentDisplayValue + (1.0f - coeff) * targetValue;
            
            // Update peak hold with smooth decay
            if (targetValue > peakValue) {
                peakValue = targetValue;
                peakHoldCounter = peakHoldTimeMs;
            } else {
                peakHoldCounter = juce::jmax(0.0f, peakHoldCounter - (1000.0f / 60.0f));
                if (peakHoldCounter <= 0.0f) {
                    // Smooth peak decay
                    peakValue = juce::jmax(currentDisplayValue, peakValue * 0.98f);
                }
            }
            
            repaint();
        }
        
    private:
        // Exponential ballistics for smooth movement
        float currentDisplayValue = 0.0f;
        float targetValue = 0.0f;
        
        // Time constants for professional VU-style ballistics
        // Very smooth release for butter-smooth decay
        const float attackTimeMs = 10.0f;    // Fast attack (10ms)
        const float releaseTimeMs = 500.0f;  // Very slow release (500ms)
        
        // Calculated coefficients (set in constructor)
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        
        // Peak hold for transient display
        float peakValue = 0.0f;
        float peakHoldCounter = 0.0f;
        const float peakHoldTimeMs = 1000.0f; // Hold peaks for 1 second
    };
    
    // Custom overlay component with black background
    class CompCrushOverlay : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF131313)); // Same color as preset area background
        }
    };
    std::unique_ptr<CompCrushOverlay> compCrushOverlay;
    bool compCrushEnabled = false;
    
    // COMPRESS+ gain reduction meter
    std::unique_ptr<GainReductionMeter> gainReductionMeter;
    
    // Small gain reduction meter for top area (80px wide, 6px tall)
    std::unique_ptr<SmallGainReductionMeter> smallGainReductionMeter;
    
    // COMPRESS+ audio visualizer
    
    // COMPRESS+ Sliders - 8 sliders in 2 rows of 4 (Top: Threshold, Attack, Release, Ratio; Bottom: Drive, Lofi, Makeup Gain, Wet)
    std::unique_ptr<CompressSlider> compressThresholdSlider;
    std::unique_ptr<CompressSlider> compressAttackSlider;
    std::unique_ptr<CompressSlider> compressReleaseSlider;
    std::unique_ptr<CompressSlider> compressRatioSlider;
    std::unique_ptr<CompressSlider> compressDriveSlider;
    std::unique_ptr<CompressSlider> compressLofiSlider;
    std::unique_ptr<CompressSlider> compressMakeupGainSlider;
    std::unique_ptr<CompressSlider> compressWetSlider;
    
    // COMPRESS+ slider attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressThresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressReleaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressDriveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressLofiAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressMakeupGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressWetAttachment;
    
    // COMPRESS+ Slider Labels
    std::unique_ptr<juce::Label> compressThresholdLabel;
    std::unique_ptr<juce::Label> compressAttackLabel;
    std::unique_ptr<juce::Label> compressReleaseLabel;
    std::unique_ptr<juce::Label> compressRatioLabel;
    std::unique_ptr<juce::Label> compressDriveLabel;
    std::unique_ptr<juce::Label> compressLofiLabel;
    std::unique_ptr<juce::Label> compressMakeupGainLabel;
    std::unique_ptr<juce::Label> compressWetLabel;
    
    // COMPRESS+ Value Labels
    std::unique_ptr<juce::Label> compressThresholdValueLabel;
    std::unique_ptr<juce::Label> compressAttackValueLabel;
    std::unique_ptr<juce::Label> compressReleaseValueLabel;
    std::unique_ptr<juce::Label> compressRatioValueLabel;
    std::unique_ptr<juce::Label> compressDriveValueLabel;
    std::unique_ptr<juce::Label> compressLofiValueLabel;
    std::unique_ptr<juce::Label> compressMakeupGainValueLabel;
    std::unique_ptr<juce::Label> compressWetValueLabel;
        
        // Flag to prevent onValueChange from saving snapshots during randomization reload
        std::atomic<bool> isLoadingFromSnapshot { false };
        
        // UI Assets
        UiAssets assets;
        ResonanceKnobLNF resonanceKnobLNF;
        
        // Tab system
        FxPageID currentPage = FxPageID::SpaceDelay;
        
    // Tab buttons (SVG)
    std::unique_ptr<juce::DrawableButton> tabSpaceDelay;
    std::unique_ptr<juce::DrawableButton> tabPanner;
    std::unique_ptr<juce::DrawableButton> tabDirt;
    std::unique_ptr<juce::DrawableButton> tabChorus;
    std::unique_ptr<juce::DrawableButton> tabDubDelay;
    std::unique_ptr<juce::DrawableButton> tabPhaseBloom;
    std::unique_ptr<juce::DrawableButton> tabFormant;
    
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
    std::unique_ptr<juce::Slider> hpResonanceKnob;
    std::unique_ptr<juce::Slider> lpResonanceKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpResonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpResonanceAttachment;
    
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
    
    // Space Delay All Steps toggle
    std::unique_ptr<AllStepsToggleButton> spaceDelayAllStepsToggle;
    std::unique_ptr<juce::Label> spaceDelayAllStepsLabel;
    bool spaceDelayAllStepsEnabled = false;
    
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
    std::array<std::unique_ptr<LockButton>, 6> slicerLockButtons;
    std::array<bool, 6> slicerKnobLocked { false, false, false, false, false, false };
    
    // Slicer effects area
    std::unique_ptr<juce::Label> slicerEffectsTitle;
    std::unique_ptr<juce::DrawableButton> slicerFxPowerButton;
    bool slicerFxAreaEnabled = true;
    
    // Slicer sync toggle removed - division is always tempo-synced
    
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
    
    // Dub Delay page components (8 knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> dubdelayKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> dubdelayAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> dubdelayKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> dubdelayValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> dubdelayIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> dubdelayDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> dubdelayLockButtons;
    std::array<bool, 8> dubdelayKnobLocked { false, false, false, false, false, false, false, false };
    
    // Dub Delay effects area
    std::unique_ptr<juce::Label> dubdelayEffectsTitle;
    std::unique_ptr<CustomDiceButton> dubdelayDiceButton;
    std::unique_ptr<juce::DrawableButton> dubdelayFxPowerButton;
    std::unique_ptr<juce::Button> dubdelaySyncToggle; // S circle toggle for time sync
    bool dubdelaySyncEnabled = false;
    bool dubdelayFxAreaEnabled = true;
    
    // Dub Delay step sequencer area
    std::array<std::unique_ptr<StepButton>, 16> dubdelayStepButtons;
    int dubdelayUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> dubdelayStepAmountLabel;
    std::unique_ptr<juce::ComboBox> dubdelayRateDropdown;
    std::unique_ptr<CircularToggleButton> dubdelayStdToggle;
    std::unique_ptr<juce::Label> dubdelayStepTitle;
    std::unique_ptr<CustomDiceButton> dubdelayStepDiceButton;
    std::unique_ptr<juce::DrawableButton> dubdelayStepPowerButton;
    bool dubdelayStepAreaEnabled = true;
    
    // Dub Delay All Steps toggle
    std::unique_ptr<AllStepsToggleButton> dubdelayAllStepsToggle;
    std::unique_ptr<juce::Label> dubdelayAllStepsLabel;
    bool dubdelayAllStepsEnabled = false;
    
    std::vector<juce::Component*> dubdelayGroup; // All Dub Delay UI components for visibility toggling
    
    // Redux page components (8 knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> reduxKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> reduxAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> reduxKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> reduxValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> reduxIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> reduxDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> reduxLockButtons;
    std::array<bool, 8> reduxKnobLocked { false, false, false, false, false, false, false, false };
    
    // Redux effects area
    std::unique_ptr<juce::Label> reduxEffectsTitle;
    std::unique_ptr<CustomDiceButton> reduxDiceButton;
    std::unique_ptr<juce::DrawableButton> reduxFxPowerButton;
    bool reduxFxAreaEnabled = true;
    
    // Redux step sequencer area
    std::array<std::unique_ptr<StepButton>, 16> reduxStepButtons;
    int reduxUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> reduxStepAmountLabel;
    std::unique_ptr<juce::ComboBox> reduxRateDropdown;
    std::unique_ptr<CircularToggleButton> reduxStdToggle;
    std::unique_ptr<juce::Label> reduxStepTitle;
    std::unique_ptr<CustomDiceButton> reduxStepDiceButton;
    std::unique_ptr<juce::DrawableButton> reduxStepPowerButton;
    bool reduxStepAreaEnabled = true;
    
    // Redux All Steps toggle
    std::unique_ptr<AllStepsToggleButton> reduxAllStepsToggle;
    std::unique_ptr<juce::Label> reduxAllStepsLabel;
    bool reduxAllStepsEnabled = false;
    
    // PhaseBloom page components (8 sliders - not knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> phaseBloomKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> phaseBloomAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> phaseBloomKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> phaseBloomValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> phaseBloomIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> phaseBloomDiceButtons;
    
    // Formant page components (5 knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> formantKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> formantAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> formantKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> formantValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> formantIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> formantDiceButtons;
    std::array<std::unique_ptr<LockButton>, 8> formantLockButtons;
    std::array<bool, 8> formantKnobLocked { false, false, false, false, false, false, false, false };
    
    // Formant power buttons
    std::unique_ptr<juce::DrawableButton> formantFxPowerButton;
    std::unique_ptr<juce::DrawableButton> formantStepPowerButton;
    bool formantFxAreaEnabled = true;
    bool formantStepAreaEnabled = true;
    
        // Formant sequencer UI
        std::unique_ptr<StepSequencer> formantStepSequencer;
        std::atomic<int> formantUiSelectedStep { 0 };
        
        // Formant step buttons
        std::array<std::unique_ptr<StepButton>, 16> formantStepButtons;
        
        // Formant titles and controls
        std::unique_ptr<juce::Label> formantEffectsTitle;
        std::unique_ptr<juce::Label> formantStepTitle;
        std::unique_ptr<CustomDiceButton> formantDiceButton;
        std::unique_ptr<CustomDiceButton> formantStepDiceButton;
        std::unique_ptr<juce::Label> formantStepAmountLabel;
        std::unique_ptr<juce::ComboBox> formantRateDropdown;
        std::unique_ptr<CircularToggleButton> formantStdToggle;
        std::unique_ptr<AllStepsToggleButton> formantAllStepsToggle;
        std::unique_ptr<juce::Label> formantAllStepsLabel;
        bool formantAllStepsEnabled = false;
    
    // Form 2 page components (8 knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> form2Knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> form2Attachments;
    std::array<std::unique_ptr<juce::Label>, 8> form2KnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> form2ValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> form2IndicatorBars;
    std::array<std::unique_ptr<LockButton>, 8> form2LockButtons;
    std::array<bool, 8> form2KnobLocked { false, false, false, false, false, false, false, false };
    
    // Form 2 power buttons
    std::unique_ptr<juce::DrawableButton> form2FxPowerButton;
    std::unique_ptr<juce::DrawableButton> form2StepPowerButton;
    bool form2FxAreaEnabled = true;
    bool form2StepAreaEnabled = true;
    
    // Form 2 sequencer UI
    std::unique_ptr<StepSequencer> form2StepSequencer;
    std::atomic<int> form2UiSelectedStep { 0 };
    
    // Form 2 step buttons
    std::array<std::unique_ptr<StepButton>, 16> form2StepButtons;
    
    // Form 2 titles and controls
    std::unique_ptr<juce::Label> form2EffectsTitle;
    std::unique_ptr<juce::Label> form2StepTitle;
    std::unique_ptr<CustomDiceButton> form2DiceButton;
    std::unique_ptr<CustomDiceButton> form2StepDiceButton;
    std::unique_ptr<juce::TextEditor> form2StepAmountLabel;
    std::unique_ptr<juce::ComboBox> form2RateDropdown;
    std::unique_ptr<CircularToggleButton> form2StdToggle;
    std::unique_ptr<AllStepsToggleButton> form2AllStepsToggle;
    std::unique_ptr<juce::Label> form2AllStepsLabel;
    bool form2AllStepsEnabled = false;
    std::array<std::unique_ptr<LockButton>, 8> phaseBloomLockButtons;
    std::array<bool, 8> phaseBloomKnobLocked { false, false, false, false, false, false, false, false };
    
    // PhaseBloom effects area
    std::unique_ptr<juce::Label> phaseBloomEffectsTitle;
    std::unique_ptr<CustomDiceButton> phaseBloomDiceButton;
    std::unique_ptr<juce::DrawableButton> phaseBloomFxPowerButton;
    bool phaseBloomFxAreaEnabled = true;
    
    // PhaseBloom step sequencer area
    std::array<std::unique_ptr<StepButton>, 16> phaseBloomStepButtons;
    int phaseBloomUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> phaseBloomStepAmountLabel;
    std::unique_ptr<juce::ComboBox> phaseBloomRateDropdown;
    std::unique_ptr<CircularToggleButton> phaseBloomStdToggle;
    std::unique_ptr<juce::Label> phaseBloomStepTitle;
    std::unique_ptr<CustomDiceButton> phaseBloomStepDiceButton;
    std::unique_ptr<juce::DrawableButton> phaseBloomStepPowerButton;
    bool phaseBloomStepAreaEnabled = true;
    
    // PhaseBloom All Steps toggle
    std::unique_ptr<AllStepsToggleButton> phaseBloomAllStepsToggle;
    std::unique_ptr<juce::Label> phaseBloomAllStepsLabel;
    bool phaseBloomAllStepsEnabled = false;
    
    std::vector<juce::Component*> reduxGroup; // All Redux UI components for visibility toggling
    std::vector<juce::Component*> phaseBloomGroup; // All PhaseBloom UI components for visibility toggling
    std::vector<juce::Component*> formantGroup; // All Formant UI components for visibility toggling
    
    // Filter page components (8 knobs: Type, Cutoff, Res, Slope, Drive, Spread, Key Track, Mix)
    std::unique_ptr<CustomKnob> filterTypeKnob;
    std::unique_ptr<juce::Label> filterTypeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterTypeAttachment;
    std::array<std::unique_ptr<CustomKnob>, 5> filterKnobs; // Cutoff, Res, Drive, Key Track, Mix (Spread removed)
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> filterAttachments;
    std::unique_ptr<CustomKnob> filterSlopeKnob;
    std::unique_ptr<juce::Label> filterSlopeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterSlopeAttachment;
    std::array<std::unique_ptr<juce::Label>, 8> filterKnobLabels; // Labels for all 8 controls (Type, Cutoff, Res, Slope, Drive, Spread, Key Track, Mix)
    std::array<std::unique_ptr<juce::Label>, 8> filterValueLabels; // Value labels for all 8 knobs
    std::array<std::unique_ptr<IndicatorBar>, 8> filterIndicatorBars; // Indicators for all 8 knobs
    std::array<std::unique_ptr<LockButton>, 8> filterLockButtons; // Lock buttons for all 8 knobs
    std::array<bool, 8> filterKnobLocked { false, false, false, false, false, false, false, false };
    std::unique_ptr<juce::Label> filterEffectsTitle;
    std::unique_ptr<CustomDiceButton> filterDiceButton;
    std::unique_ptr<juce::DrawableButton> filterFxPowerButton;
    bool filterFxAreaEnabled = true;
    
    // Filter step sequencer area
    std::array<std::unique_ptr<StepButton>, 16> filterStepButtons;
    int filterUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> filterStepAmountLabel;
    std::unique_ptr<juce::ComboBox> filterRateDropdown;
    std::unique_ptr<CircularToggleButton> filterStdToggle;
    std::unique_ptr<juce::Label> filterStepTitle;
    std::unique_ptr<CustomDiceButton> filterStepDiceButton;
    std::unique_ptr<juce::DrawableButton> filterStepPowerButton;
    bool filterStepAreaEnabled = true;
    
    // Filter All Steps toggle
    std::unique_ptr<AllStepsToggleButton> filterAllStepsToggle;
    std::unique_ptr<juce::Label> filterAllStepsLabel;
    bool filterAllStepsEnabled = false;
    
    std::vector<juce::Component*> filterGroup; // All Filter UI components for visibility toggling
    
    // Saturate page components (8 knobs)
    std::array<std::unique_ptr<CustomKnob>, 8> saturateKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> saturateAttachments;
    std::array<std::unique_ptr<juce::Label>, 8> saturateKnobLabels;
    std::array<std::unique_ptr<juce::Label>, 8> saturateValueLabels;
    std::array<std::unique_ptr<IndicatorBar>, 8> saturateIndicatorBars;
    std::array<std::unique_ptr<CustomDiceButton>, 8> saturateDiceButtons;
    std::array<std::unique_ptr<LockButton>, 7> saturateLockButtons; // 7 knobs (oversample removed)
    std::array<bool, 7> saturateKnobLocked { false, false, false, false, false, false, false };
    
    // Saturate effects area
    std::unique_ptr<juce::Label> saturateEffectsTitle;
    std::unique_ptr<CustomDiceButton> saturateDiceButton;
    std::unique_ptr<juce::DrawableButton> saturateFxPowerButton;
    bool saturateFxAreaEnabled = true; // Default to enabled like other effects
    
    // Saturate step sequencer area
    std::array<std::unique_ptr<StepButton>, 16> saturateStepButtons;
    int saturateUiSelectedStep = 0;
    std::unique_ptr<juce::TextEditor> saturateStepAmountLabel;
    std::unique_ptr<juce::ComboBox> saturateRateDropdown;
    std::unique_ptr<CircularToggleButton> saturateStdToggle;
    std::unique_ptr<juce::Label> saturateStepTitle;
    std::unique_ptr<CustomDiceButton> saturateStepDiceButton;
    std::unique_ptr<juce::DrawableButton> saturateStepPowerButton;
    bool saturateStepAreaEnabled = true;
    
    // Saturate All Steps toggle
    std::unique_ptr<AllStepsToggleButton> saturateAllStepsToggle;
    std::unique_ptr<juce::Label> saturateAllStepsLabel;
    bool saturateAllStepsEnabled = false;
    
    std::vector<juce::Component*> saturateGroup; // All Saturate UI components for visibility toggling
    std::vector<juce::Component*> form2Group; // All Form 2 UI components for visibility toggling
    
    // Redux page container for isolation
    std::unique_ptr<juce::Component> reduxPage;
    
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
        
    // Unified effect handling methods
    void randomizeEffectStepSnapshot(FxPageID effect, int step);
    void loadSelectedStepIntoKnobs(FxPageID effect);
    void onUnifiedStepButtonClicked(int stepIndex);
    void updateUnifiedAllStepSnapshots(int knobIndex);
    void updateUnifiedParameterFromKnob(int knobIndex);
    void saveCurrentStepSnapshot();
    void updateSelectedStepInProcessor(int stepIndex);
    void updateSnapshotValue(StepSnapshot& snapshot, int knobIndex, float value);
    juce::String getParameterIdForKnob(int knobIndex);
        void setupUIToggle();
        void toggleUIVisibility();
        void setupPlayButton();
        
        // AutoPan page helper methods
        void setupAutoPanKnobs();
        void setupAutoPanEffectsArea();
        void setupAutoPanSequencerArea();
        void setupAutoPanAllStepsToggle();
        void setupSpaceDelayAllStepsToggle();
        
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
        
        // Dub Delay page helper methods
        void setupDubDelayKnobs();
        void setupDubDelayEffectsArea();
        void setupDubDelaySequencerArea();
        void setupDubDelayAllStepsToggle();
        
        // Saturate page helper methods
        void setupSaturateKnobs();
        void setupSaturateEffectsArea();
        void setupSaturateSequencerArea();
        void setupFilterSequencerArea();
        void setupSaturateAllStepsToggle();
        void updateSaturateKnobLabels(int type);
        void updateSaturateFxAreaVisibility();
        void updateFilterFxAreaVisibility();
        void updateSaturateStepAreaVisibility();
        void updateFilterStepAreaVisibility();
        void updateSaturateSequencerUI();
        void onSaturateStepButtonClicked(int stepIndex);
        
        void updateFilterSequencerUI();
        void onFilterStepButtonClicked(int stepIndex);
        void updateFilterParameterFromKnob(int knobIndex);
        void updateSaturateParameterFromKnob(int knobIndex);
        void randomizeSaturateKnobValues();
        void randomizeIndividualSaturateKnob(int knobIndex);
        void updateDubDelayFxAreaVisibility();
        void updateDubDelayStepAreaVisibility();
        void randomizeDubDelayKnobValues();
        void randomizeIndividualDubDelayKnob(int knobIndex);
        void updateDubDelayParameterFromKnob(int knobIndex);
        void updateDubDelaySequencerUI();
        void updateSpaceDelayTimeLabel();
    void updateDubDelayTimeLabel(); // Update Time knob label based on sync mode
        void onDubDelayStepButtonClicked(int stepIndex);
        void updateDubDelayCurrentStepSnapshot(int knobIndex, float value);
        
        // Gumroad License management methods
        void checkLicenseOnStartup();
        void showLicenseDialog();
        
        // Redux page helper methods
        void setupReduxKnobs();
        void setupCompressSliders();
    void updateCompressValueLabels();
        void setupReduxEffectsArea();
        void setupReduxSequencerArea();
        void setupReduxAllStepsToggle();
        void updateReduxFxAreaVisibility();
        void updateReduxStepAreaVisibility();
        void randomizeReduxKnobValues();
        void randomizeIndividualReduxKnob(int knobIndex);
        void updateReduxParameterFromKnob(int knobIndex);
        void updateReduxSequencerUI();
        void onReduxStepButtonClicked(int stepIndex);
        void ensureReduxAttachments();
        void rebindReduxAttachments();
        
        // PhaseBloom page helper methods
        void setupPhaseBloomKnobs();
        void setupPhaseBloomEffectsArea();
        void setupPhaseBloomSequencerArea();
        void setupPhaseBloomAllStepsToggle();
        void updatePhaseBloomFxAreaVisibility();
        void updatePhaseBloomStepAreaVisibility();
        void randomizePhaseBloomKnobValues();
        void randomizeIndividualPhaseBloomKnob(int knobIndex);
        void updatePhaseBloomParameterFromKnob(int knobIndex);
        void updatePhaseBloomSequencerUI();
        
        // Formant page helper methods
        void setupFormantKnobs();
        void setupFilterKnobs();
        void setupFilterEffectsArea();
        void setupFilterAllStepsToggle();
        void populateFilterGroup();
        void setupFormantEffectsArea();
        void setupFormantSequencerArea();
        void setupFormantAllStepsToggle();
        
        void setupForm2Knobs();
        void setupForm2EffectsArea();
        void setupForm2SequencerArea();
        void setupForm2AllStepsToggle();
        void updateForm2FxAreaVisibility();
        void updateForm2StepAreaVisibility();
        void randomizeForm2KnobValues();
        void randomizeIndividualForm2Knob(int knobIndex);
        void updateForm2ParameterFromKnob(int knobIndex);
        void updateForm2SequencerUI();
        void onForm2StepButtonClicked(int stepIndex);
        void updateFormantFxAreaVisibility();
        void updateFormantStepAreaVisibility();
        void randomizeFormantKnobValues();
        void updateFormantOverlay();
        void randomizeIndividualFormantKnob(int knobIndex);
        void updateFormantParameterFromKnob(int knobIndex);
        void updateFormantSequencerUI();
        void onPhaseBloomStepButtonClicked(int stepIndex);
        void ensurePhaseBloomAttachments();
        void rebindPhaseBloomAttachments();
        
        void togglePlayback();
        
        // Tab system helpers
        void setupTabSystem();
        void showPage(FxPageID id);
    void drawGridOverlay(juce::Graphics& g);
    void drawMainAreas(juce::Graphics& g);
    
    // Effect router UI helpers
    void onEffectSelectorChanged(int slotIndex);
    void updateAllEffectSelectors();
    void updateAllEffectSelectors(int skipSlot);
    void updateBackgroundsAfterSwap();
    void updateTabButtonImages();
    juce::ComboBox* getEffectSelectorForSlot(int slotIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};