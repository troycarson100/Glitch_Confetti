#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

// Step 2: Add back parameter system and basic knobs (no complex UI)

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    setSize (800, 600);
    
    // Initialize step snapshots
    initializeSnapshots();
    
    // Add parameter listeners for all delay parameters
    processorRef.getValueTreeState().addParameterListener("timeMs", this);
    processorRef.getValueTreeState().addParameterListener("feedback", this);
    processorRef.getValueTreeState().addParameterListener("wowDepth", this);
    processorRef.getValueTreeState().addParameterListener("wowRate", this);
    processorRef.getValueTreeState().addParameterListener("saturation", this);
    processorRef.getValueTreeState().addParameterListener("highCut", this);
    processorRef.getValueTreeState().addParameterListener("lowCut", this);
    processorRef.getValueTreeState().addParameterListener("mix", this);
    
    // Create simple knobs for testing
    setupBasicKnobs();
    
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
    g.drawFittedText ("Stepper Plugin - Step 2 (Parameters)", getLocalBounds().reduced(0, 200), juce::Justification::centred, 1);
    
    // Draw knob labels
    g.setFont (12.0f);
    const char* labels[] = {"Time", "Feedback", "Wow Depth", "Wow Rate", "Saturation", "Hi Cut", "Low Cut", "Mix"};
    for (int i = 0; i < 8; ++i)
    {
        if (i < static_cast<int>(knobs.size()) && knobs[i])
        {
            auto bounds = knobs[i]->getBounds();
            g.drawText(labels[i], bounds.getX(), bounds.getY() - 20, bounds.getWidth(), 15, juce::Justification::centred);
        }
    }
}

void PluginEditor::resized()
{
    if (getWidth() < 100) setSize(800, 600);
    
    // Layout knobs in a row
    const int knobSize = 80;
    const int spacing = 100;
    const int startX = 50;
    const int y = 300;
    
    for (int i = 0; i < static_cast<int>(knobs.size()); ++i)
    {
        if (knobs[i])
        {
            knobs[i]->setBounds(startX + i * spacing, y, knobSize, knobSize);
        }
    }
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

void PluginEditor::setupBasicKnobs()
{
    // Create 8 simple knobs using CustomKnob
    for (int i = 0; i < 8; ++i)
    {
        auto knob = std::make_unique<CustomKnob>();
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        knob->setRange(0.0, 1.0, 0.01);
        knob->setValue(0.5);
        
        // Connect to parameters
        switch (i)
        {
            case 0: knob->setRange(10.0, 2000.0, 1.0); knob->setValue(250.0); break; // Time
            case 1: knob->setRange(0.0, 1.0, 0.01); knob->setValue(0.2); break; // Feedback
            case 2: knob->setRange(0.0, 1.0, 0.01); knob->setValue(0.0); break; // Wow Depth
            case 3: knob->setRange(0.1, 8.0, 0.1); knob->setValue(1.0); break; // Wow Rate
            case 4: knob->setRange(0.0, 1.0, 0.01); knob->setValue(0.0); break; // Saturation
            case 5: knob->setRange(1000.0, 20000.0, 100.0); knob->setValue(20000.0); break; // Hi Cut
            case 6: knob->setRange(20.0, 2000.0, 10.0); knob->setValue(20.0); break; // Low Cut
            case 7: knob->setRange(0.0, 1.0, 0.01); knob->setValue(0.5); break; // Mix
        }
        
        addAndMakeVisible(knob.get());
        knobs.push_back(std::move(knob));
    }
}

// Minimal implementations for required methods
void PluginEditor::setupKnobs() {}
void PluginEditor::setupStepButtons() {}
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
