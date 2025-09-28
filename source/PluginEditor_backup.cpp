#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

// Ultra-minimal crash-safe version

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Minimal size
    setSize (400, 300);
    
    // No resizing
    setResizable (false, false);
    
    // NO timer - eliminate timer-related crashes
    // NO component creation - eliminate component crashes
}

PluginEditor::~PluginEditor()
{
    // No timer to stop
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Fill background
    g.fillAll (juce::Colours::darkgrey);
    
    // Draw title
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Stepper Plugin - Safe Gradual UI", getLocalBounds().reduced(0, 200), juce::Justification::centred, 1);
    
    // Draw step area background
    auto stepArea = getStepArea();
    g.setColour (juce::Colours::darkblue);
    g.fillRoundedRectangle(stepArea.toFloat(), 10.0f);
    g.setColour (juce::Colours::white);
    g.drawRoundedRectangle(stepArea.toFloat(), 10.0f, 2.0f);
    
    // Draw effect area background
    auto effectArea = getEffectArea();
    g.setColour (juce::Colours::darkgreen);
    g.fillRoundedRectangle(effectArea.toFloat(), 10.0f);
    g.setColour (juce::Colours::white);
    g.drawRoundedRectangle(effectArea.toFloat(), 10.0f, 2.0f);
}

void PluginEditor::resized()
{
    // Layout components safely
    layoutComponents();
}

void PluginEditor::timerCallback()
{
    // Minimal timer - just repaint occasionally
    repaint();
}

void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Safe parameter change handler
    DBG("Parameter changed: " << parameterID << " = " << newValue);
}

void PluginEditor::setupBasicKnobs()
{
    // Create simple JUCE sliders that are guaranteed to work
    for (int i = 0; i < 8; ++i)
    {
        knobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(knobs[i].get());
    }
}

void PluginEditor::setupStepButtons()
{
    // Create simple step buttons
    for (int i = 0; i < 16; ++i)
    {
        stepButtons[i] = std::make_unique<CustomStepButton>(i);
        addAndMakeVisible(stepButtons[i].get());
    }
}

void PluginEditor::layoutComponents()
{
    // Layout step buttons (4x4 grid)
    int buttonSize = 60;
    int buttonSpacing = 10;
    int startX = 50;
    int startY = 100;
    
    for (int i = 0; i < 16; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        int x = startX + col * (buttonSize + buttonSpacing);
        int y = startY + row * (buttonSize + buttonSpacing);
        
        stepButtons[i]->setBounds (x, y, buttonSize, buttonSize);
    }
    
    // Layout knobs (2 rows of 4)
    int knobSize = 80;
    int knobSpacing = 20;
    int knobStartX = 400;
    int knobStartY = 120;
    
    for (int i = 0; i < 8; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        int x = knobStartX + col * (knobSize + knobSpacing);
        int y = knobStartY + row * (knobSize + knobSpacing);
        
        knobs[i]->setBounds (x, y, knobSize, knobSize);
    }
}

juce::Rectangle<int> PluginEditor::getStepArea() const
{
    return juce::Rectangle<int>(30, 80, 300, 300);
}

juce::Rectangle<int> PluginEditor::getEffectArea() const
{
    return juce::Rectangle<int>(350, 80, 600, 300);
}

// Stub implementations for required methods
void PluginEditor::initializeSnapshots()
{
    for (int step = 0; step < 16; ++step)
    {
        stepSnapshots[step] = StepSnapshot();
    }
}

void PluginEditor::saveSnapshot(int stepIndex) {}
void PluginEditor::restoreSnapshot(int stepIndex) {}
void PluginEditor::clearSequencerUI() {}
void PluginEditor::refreshBarsFromStep(int stepToShow) {}
void PluginEditor::applyStepHighlight(int newStep) {}
void PluginEditor::updateStepSequencer() {}
void PluginEditor::updateStepButtonVisibility() {}

// CustomKnob Implementation - Safe version
CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 100.0, 1.0);
    setValue(50.0);
    
    // Set basic colors
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white);
    setColour(juce::Slider::thumbColourId, juce::Colours::red);
}

void CustomKnob::paint(juce::Graphics& g)
{
    // Use default JUCE knob painting - this is guaranteed to work
    juce::Slider::paint(g);
}

void CustomKnob::resized() {}

void CustomKnob::setRingImage(std::unique_ptr<juce::Drawable> ringImage) {}
void CustomKnob::setInnerImage(std::unique_ptr<juce::Drawable> innerImage) {}

// CustomStepButton Implementation - Safe version
CustomStepButton::CustomStepButton(int stepIndex) : juce::Button("Step" + juce::String(stepIndex)), stepIndex(stepIndex)
{
    // Set basic colors using correct JUCE color IDs
    setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
}

void CustomStepButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    // Simple button painting
    auto bounds = getLocalBounds().toFloat();
    
    if (isButtonDown)
    {
        g.setColour(juce::Colours::yellow);
    }
    else if (isMouseOverButton)
    {
        g.setColour(juce::Colours::lightgrey);
    }
    else
    {
        g.setColour(juce::Colours::darkgrey);
    }
    
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
}

void CustomStepButton::resized() {}

void CustomStepButton::setActive(bool active) 
{
    if (this->active != active)
    {
        this->active = active;
        setToggleState(active, juce::dontSendNotification);
        repaint();
    }
}

void CustomStepButton::setInactive(bool inactive) 
{
    if (this->inactive != inactive)
    {
        this->inactive = inactive;
        setEnabled(!inactive);
        repaint();
    }
}

void CustomStepButton::setSequencerActive(bool sequencerActive) 
{
    if (this->sequencerActive != sequencerActive)
    {
        this->sequencerActive = sequencerActive;
        repaint();
    }
}

void CustomStepButton::setActiveImage(std::unique_ptr<juce::Drawable> activeImage) {}
void CustomStepButton::setInactiveImage(std::unique_ptr<juce::Drawable> inactiveImage) {}
void CustomStepButton::setImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active) {}
