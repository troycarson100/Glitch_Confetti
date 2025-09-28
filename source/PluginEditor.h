#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include "StepSnapshot.h"
#include "ui/Assets.h"

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
    
        // UI Components
        std::array<std::unique_ptr<CustomKnob>, 8> knobs;
        std::array<std::unique_ptr<juce::Label>, 8> knobLabels;
        std::array<std::unique_ptr<juce::Label>, 8> valueLabels;
        std::array<std::unique_ptr<IndicatorBar>, 8> indicatorBars;
        std::array<std::unique_ptr<CustomDiceButton>, 8> knobDiceButtons;
    
    // Effects area components
    std::unique_ptr<juce::Label> effectsTitle;
    std::unique_ptr<CustomDiceButton> diceButton;
    
        // Helper methods
        void setupKnobs();
        void setupEffectsArea();
        void randomizeKnobValues();
        void randomizeIndividualKnob(int knobIndex);
    void drawGridOverlay(juce::Graphics& g);
    void drawMainAreas(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};