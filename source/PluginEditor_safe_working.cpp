#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

// Minimal safe PluginEditor implementation to prevent crashes

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    setSize (800, 600);
    
    // Start timer for UI updates
    startTimer(50); // 20 Hz
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Stepper Plugin - Safe Mode", getLocalBounds(), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    // Minimal resizing - just ensure we have a valid size
    if (getWidth() < 100) setSize(800, 600);
}

void PluginEditor::timerCallback()
{
    // Minimal timer - just repaint occasionally
    repaint();
}

void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Safe parameter change handler - do nothing for now
    DBG("Parameter changed: " << parameterID << " = " << newValue);
}

// Minimal implementations to prevent linker errors
void PluginEditor::setupKnobs() {}
void PluginEditor::setupStepButtons() {}
void PluginEditor::initializeSnapshots() {}
void PluginEditor::saveSnapshot(int stepIndex) {}
void PluginEditor::restoreSnapshot(int stepIndex) {}
void PluginEditor::applyStepHighlight(int newStep) {}
void PluginEditor::clearSequencerUI() {}
void PluginEditor::refreshBarsFromStep(int stepToShow) {}

// CustomKnob implementation
CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 1.0, 0.01);
    setValue(0.5);
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::white);
    g.drawEllipse(bounds, 2.0f);
}

void CustomKnob::resized() {}
void CustomKnob::setRingImage(std::unique_ptr<juce::Drawable> ringImage) {}
void CustomKnob::setInnerImage(std::unique_ptr<juce::Drawable> innerImage) {}

// CustomStepButton implementation
CustomStepButton::CustomStepButton(int stepIndex) : juce::Button("step"), stepIndex(stepIndex) {}

void CustomStepButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::white);
    g.drawRect(bounds, 1.0f);
}

void CustomStepButton::resized() {}
void CustomStepButton::setActive(bool active) {}
void CustomStepButton::setInactive(bool inactive) {}
void CustomStepButton::setSequencerActive(bool sequencerActive) {}
void CustomStepButton::setActiveImage(std::unique_ptr<juce::Drawable> activeImage) {}
void CustomStepButton::setInactiveImage(std::unique_ptr<juce::Drawable> inactiveImage) {}
void CustomStepButton::setImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active) {}
