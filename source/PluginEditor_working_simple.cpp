#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

// Simple working PluginEditor with basic sliders

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    setSize (800, 600);
    
    // Create simple working sliders
    setupSimpleWorkingSliders();
    
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
    g.drawFittedText ("Stepper Plugin - Working Simple UI", getLocalBounds(), juce::Justification::centred, 1);
    
    // Draw labels for sliders
    g.setColour (juce::Colours::yellow);
    g.setFont (12.0f);
    g.drawText ("Time", 50, 150, 100, 20, juce::Justification::centred);
    g.drawText ("Feedback", 200, 150, 100, 20, juce::Justification::centred);
    g.drawText ("Mix", 350, 150, 100, 20, juce::Justification::centred);
}

void PluginEditor::resized()
{
    // Position sliders
    if (timeSlider != nullptr)
        timeSlider->setBounds(50, 180, 100, 100);
    if (feedbackSlider != nullptr)
        feedbackSlider->setBounds(200, 180, 100, 100);
    if (mixSlider != nullptr)
        mixSlider->setBounds(350, 180, 100, 100);
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

void PluginEditor::setupSimpleWorkingSliders()
{
    // Create simple JUCE sliders that will definitely work
    timeSlider = std::make_unique<juce::Slider>();
    timeSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    timeSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    timeSlider->setRange(0.0, 100.0, 1.0);
    timeSlider->setValue(50.0);
    timeSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
    timeSlider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white);
    timeSlider->setColour(juce::Slider::thumbColourId, juce::Colours::red);
    addAndMakeVisible(timeSlider.get());
    
    feedbackSlider = std::make_unique<juce::Slider>();
    feedbackSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    feedbackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    feedbackSlider->setRange(0.0, 100.0, 1.0);
    feedbackSlider->setValue(30.0);
    feedbackSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
    feedbackSlider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white);
    feedbackSlider->setColour(juce::Slider::thumbColourId, juce::Colours::red);
    addAndMakeVisible(feedbackSlider.get());
    
    mixSlider = std::make_unique<juce::Slider>();
    mixSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    mixSlider->setRange(0.0, 100.0, 1.0);
    mixSlider->setValue(70.0);
    mixSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
    mixSlider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white);
    mixSlider->setColour(juce::Slider::thumbColourId, juce::Colours::red);
    addAndMakeVisible(mixSlider.get());
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
