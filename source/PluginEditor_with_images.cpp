#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

// Version with SVG image loading to test if that's the crash cause

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Set the size to match our desired dimensions (fixed size)
    setSize (974, 532);
    
    // Lock the size - no resizing
    setResizable (false, false);
    
    // Add parameter listeners for all delay parameters
    processorRef.getValueTreeState().addParameterListener("timeMs", this);
    processorRef.getValueTreeState().addParameterListener("feedback", this);
    processorRef.getValueTreeState().addParameterListener("wowDepth", this);
    processorRef.getValueTreeState().addParameterListener("wowRate", this);
    processorRef.getValueTreeState().addParameterListener("saturation", this);
    processorRef.getValueTreeState().addParameterListener("highCut", this);
    processorRef.getValueTreeState().addParameterListener("lowCut", this);
    processorRef.getValueTreeState().addParameterListener("mix", this);
    
    // Try to initialize images - this might be the crash cause
    initializeImages();
    
    // Initialize snapshots
    initializeSnapshots();
    
    // Start timer for UI updates
    startTimer(50); // 20 Hz
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    
    // Remove parameter listeners
    processorRef.getValueTreeState().removeParameterListener("timeMs", this);
    processorRef.getValueTreeState().removeParameterListener("feedback", this);
    processorRef.getValueTreeState().removeParameterListener("wowDepth", this);
    processorRef.getValueTreeState().removeParameterListener("wowRate", this);
    processorRef.getValueTreeState().removeParameterListener("saturation", this);
    processorRef.getValueTreeState().removeParameterListener("highCut", this);
    processorRef.getValueTreeState().removeParameterListener("lowCut", this);
    processorRef.getValueTreeState().removeParameterListener("mix", this);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Stepper Plugin - Testing Image Loading", getLocalBounds().reduced(0, 200), juce::Justification::centred, 1);
    
    // Try to draw a simple test image to see if image loading works
    if (backgroundImage)
    {
        g.setColour(juce::Colours::green);
        g.drawFittedText("Images loaded successfully!", getLocalBounds().reduced(0, 150), juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(juce::Colours::red);
        g.drawFittedText("Image loading failed!", getLocalBounds().reduced(0, 150), juce::Justification::centred, 1);
    }
}

void PluginEditor::resized()
{
    // Basic resizing - no complex layout
}

void PluginEditor::timerCallback()
{
    repaint();
}

void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    DBG("Parameter changed: " << parameterID << " = " << newValue);
    
    // Get the currently selected step (default to 0 for now)
    const int selectedStep = 0;
    
    // Update the snapshot for this step
    if (selectedStep >= 0 && selectedStep < 16)
    {
        auto& snap = stepSnapshots[selectedStep];
        
        if (parameterID == "timeMs") {
            snap.delay.timeMs = newValue;
        } else if (parameterID == "feedback") {
            snap.delay.feedback = newValue * 100.0f; // Convert to percentage
        } else if (parameterID == "wowDepth") {
            snap.delay.wowDepth = newValue * 100.0f;
        } else if (parameterID == "wowRate") {
            snap.delay.wowRate = newValue;
        } else if (parameterID == "saturation") {
            snap.delay.saturation = newValue * 100.0f;
        } else if (parameterID == "highCut") {
            snap.delay.highCut = newValue;
        } else if (parameterID == "lowCut") {
            snap.delay.lowCut = newValue;
        } else if (parameterID == "mix") {
            snap.delay.mix = newValue * 100.0f;
        }
        
        // Save back to processor (using direct assignment for now)
        // processorRef.setStepSnapshot(selectedStep, snap);
    }
}

void PluginEditor::initializeImages()
{
    // Try to load background image - this might be the crash cause
    try {
        backgroundImage = juce::Drawable::createFromImageData(BinaryData::Background_Mustard_svg, BinaryData::Background_Mustard_svgSize);
        DBG("Background image loaded successfully");
    }
    catch (const std::exception& e) {
        DBG("Failed to load background image: " << e.what());
        backgroundImage.reset();
    }
    catch (...) {
        DBG("Unknown exception loading background image");
        backgroundImage.reset();
    }
}

// Minimal implementations for required methods
void PluginEditor::setupKnobs() {}
void PluginEditor::setupStepButtons() {}
void PluginEditor::setupBasicKnobs() {}
void PluginEditor::setupVisibleKnobs() {}
void PluginEditor::initializeSnapshots()
{
    for (int step = 0; step < 16; ++step)
    {
        stepSnapshots[step] = StepSnapshot(); // Uses default constructor values
    }
}
void PluginEditor::saveSnapshot(int stepIndex) {}
void PluginEditor::restoreSnapshot(int stepIndex) {}
void PluginEditor::applyStepHighlight(int newStep) {}
void PluginEditor::clearSequencerUI() {}
void PluginEditor::refreshBarsFromStep(int stepToShow) {}

// CustomKnob implementation (minimal)
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

// CustomStepButton implementation (minimal)
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
