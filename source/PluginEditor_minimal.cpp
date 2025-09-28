#include "PluginEditor.h"
#include "PluginProcessor.h"

// Minimal PluginEditor implementation focused on knob functionality

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Set the size to match our desired dimensions (fixed size)
    setSize (974, 532);
    
    // Lock the size - no resizing
    setResizable (false, false);
    
    // Add parameter listener
    processorRef.getValueTreeState().addParameterListener("timeMs", this);
    processorRef.getValueTreeState().addParameterListener("feedback", this);
    processorRef.getValueTreeState().addParameterListener("wowDepth", this);
    processorRef.getValueTreeState().addParameterListener("wowRate", this);
    processorRef.getValueTreeState().addParameterListener("saturation", this);
    processorRef.getValueTreeState().addParameterListener("highCut", this);
    processorRef.getValueTreeState().addParameterListener("lowCut", this);
    processorRef.getValueTreeState().addParameterListener("mix", this);
    
    // Initialize basic components
    initializeImages();
    setupKnobs();
    setupStepButtons();
    
    // Initialize snapshots
    initializeSnapshots();
    
    // Start timer for sequencer updates
    startTimerHz(30);
}

PluginEditor::~PluginEditor()
{
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
    // Draw background
    g.fillAll(juce::Colour(0xFF1A1A1A));
    
    // Draw step indicator bars
    for (size_t i = 0; i < stepIndicatorAreas.size() && i < currentStepProgress.size(); ++i)
    {
        auto barArea = stepIndicatorAreas[i].toFloat();
        
        // Draw bar background (stroke)
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(barArea, 4.0f, 2.0f);
        
        // Draw fill based on current step progress
        if (i < static_cast<int>(currentStepProgress.size()) && currentStepProgress[i] > 0.0f)
        {
            auto fillArea = barArea.reduced(2.0f, 2.0f);
            g.reduceClipRegion(stepIndicatorAreas[i]);
            
            g.setColour(juce::Colours::white);
            float fillWidth = fillArea.getWidth() * currentStepProgress[i];
            auto filledArea = fillArea.withWidth(fillWidth);
            g.fillRoundedRectangle(filledArea, 2.0f);
        }
    }
}

void PluginEditor::resized()
{
    layoutComponents();
}

void PluginEditor::timerCallback()
{
    // Get transport and sequencer state
    TransportCache t;
    processorRef.getTransportSnapshot(t);
    const bool playing = (t.valid && t.playing);
    const bool seqOn = processorRef.isSequencerEnabled();

    const int stepToShow = (seqOn && playing)
                         ? processorRef.getPlayingStep()
                         : currentActiveStep; // UI selection

    if (seqOn && playing && stepToShow >= 0) {
        applyStepHighlight(stepToShow);
    } else {
        clearSequencerUI();
    }

    // Update step progress for all indicator bars based on SAVED step values
    refreshBarsFromStep(stepToShow);

    // Trigger repaint to update the indicator bar
    repaint();
}

void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Save the parameter change to the current step's snapshot
    saveSnapshot(currentActiveStep);
}

void PluginEditor::initializeImages()
{
    // Load SVG assets using BinaryData
    backgroundImage = juce::Drawable::createFromImageData(BinaryData::Background_Mustard_svg, BinaryData::Background_Mustard_svgSize);
    stepActiveImage = juce::Drawable::createFromImageData(BinaryData::Step_Active_svg, BinaryData::Step_Active_svgSize);
    stepInactiveImage = juce::Drawable::createFromImageData(BinaryData::Step_Inactive_svg, BinaryData::Step_Inactive_svgSize);
    knobRingImage = juce::Drawable::createFromImageData(BinaryData::Knob_Basic_Ring_svg, BinaryData::Knob_Basic_Ring_svgSize);
    knobInnerImage = juce::Drawable::createFromImageData(BinaryData::Knob_Basic_Inside_svg, BinaryData::Knob_Basic_Inside_svgSize);
}

void PluginEditor::setupKnobs()
{
    const juce::StringArray knobNames = {
        "timeMs", "feedback", "wowDepth", "wowRate", 
        "saturation", "highCut", "lowCut", "mix"
    };
    
    const juce::StringArray knobLabels = {
        "Time", "Feedback", "Wow Depth", "Wow Rate", 
        "Drive", "Hi Cut", "Low Cut", "Mix"
    };
    
    for (const auto& name : knobNames)
    {
        auto knob = std::make_unique<CustomKnob>();
        
        // Set the SVG images for this knob
        if (knobRingImage != nullptr)
        {
            auto ringCopy = knobRingImage->createCopy();
            knob->setRingImage(std::move(ringCopy));
        }
        
        if (knobInnerImage != nullptr)
        {
            auto innerCopy = knobInnerImage->createCopy();
            knob->setInnerImage(std::move(innerCopy));
        }
        
        // Create parameter attachment
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getValueTreeState(), name, *knob);
        
        addAndMakeVisible(knob.get());
        knobs.push_back(std::move(knob));
    }
    
    // Create labels for knobs
    for (const auto& label : knobLabels)
    {
        auto labelComp = std::make_unique<juce::Label>();
        labelComp->setText(label, juce::dontSendNotification);
        labelComp->setFont(juce::Font(12.0f, juce::Font::bold));
        labelComp->setColour(juce::Label::textColourId, juce::Colours::white);
        labelComp->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(labelComp.get());
        labels.push_back(std::move(labelComp));
    }
    
    // Initialize step indicator areas
    stepIndicatorAreas.resize(8);
    currentStepProgress.resize(8, 0.0f);
}

void PluginEditor::setupStepButtons()
{
    for (int i = 0; i < 16; ++i)
    {
        auto button = std::make_unique<CustomStepButton>(i);
        
        // Set SVG images
        if (stepActiveImage != nullptr && stepInactiveImage != nullptr)
        {
            auto activeCopy = stepActiveImage->createCopy();
            auto inactiveCopy = stepInactiveImage->createCopy();
            button->setImages(std::move(inactiveCopy), std::move(activeCopy));
        }
        
        button->onClick = [this, i]() {
            // Deactivate previous step
            if (currentActiveStep >= 0 && currentActiveStep < static_cast<int>(stepButtons.size()))
                stepButtons[currentActiveStep]->setActive(false);
            
            // Activate new step
            currentActiveStep = i;
            stepButtons[currentActiveStep]->setActive(true);
            
            // Restore snapshot for this step
            restoreSnapshot(currentActiveStep);
        };
        
        addAndMakeVisible(button.get());
        stepButtons.push_back(std::move(button));
    }
    
    // Activate first step by default
    if (!stepButtons.empty())
    {
        currentActiveStep = 0;
        stepButtons[0]->setActive(true);
    }
}

void PluginEditor::layoutComponents()
{
    // Layout knobs in 2 rows of 4
    const int knobSize = 84;
    const int knobSpacing = 20;
    const int startX = 50;
    const int startY = 80;
    
    for (int i = 0; i < 8; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        
        int x = startX + col * (knobSize + knobSpacing);
        int y = startY + row * (knobSize + 60); // 60px spacing between rows
        
        knobs[i]->setBounds(x, y, knobSize, knobSize);
        knobs[i]->setVisible(true);
        knobs[i]->setEnabled(true);
        
        // Position labels above knobs
        labels[i]->setBounds(x, y - 18, knobSize, 15);
        
        // Position step indicator bars below knobs
        stepIndicatorAreas[i] = juce::Rectangle<int>(
            x + (knobSize - 63) / 2, // Center the 63px wide bar
            y + knobSize + 25, // 25px below knob
            63, 10
        );
    }
    
    // Layout step buttons
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int stepStartX = 50;
    const int stepStartY = 300;
    
    for (int i = 0; i < 16; ++i)
    {
        int row = i / 8;
        int col = i % 8;
        
        int x = stepStartX + col * (buttonSize + buttonSpacing);
        int y = stepStartY + row * (buttonSize + buttonSpacing);
        
        stepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
    }
}

void PluginEditor::initializeSnapshots()
{
    // Initialize 16 steps with default StepSnapshot values
    for (int step = 0; step < 16; ++step)
    {
        stepSnapshots[step] = StepSnapshot(); // Uses default constructor values
    }
}

void PluginEditor::saveSnapshot(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < 16 && stepIndex < static_cast<int>(knobs.size()))
    {
        auto& snap = stepSnapshots[stepIndex];
        // Map knob values to delay parameters
        snap.delay.timeMs = knobs[0] ? knobs[0]->getValue() * 2000.0f : 250.0f;
        snap.delay.feedback = knobs[1] ? knobs[1]->getValue() * 100.0f : 20.0f;
        snap.delay.wowDepth = knobs[2] ? knobs[2]->getValue() * 100.0f : 0.0f;
        snap.delay.wowRate = knobs[3] ? knobs[3]->getValue() * 8.0f : 1.0f;
        snap.delay.saturation = knobs[4] ? knobs[4]->getValue() * 100.0f : 0.0f;
        snap.delay.highCut = knobs[5] ? 1000.0f + knobs[5]->getValue() * 19000.0f : 20000.0f;
        snap.delay.lowCut = knobs[6] ? 20.0f + knobs[6]->getValue() * 1980.0f : 20.0f;
        snap.delay.mix = knobs[7] ? knobs[7]->getValue() * 100.0f : 50.0f;
    }
}

void PluginEditor::restoreSnapshot(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < 16 && stepIndex < static_cast<int>(knobs.size()))
    {
        const auto& snap = stepSnapshots[stepIndex];
        // Map delay parameters to knob values
        if (knobs[0]) knobs[0]->setValue(snap.delay.timeMs / 2000.0f, juce::dontSendNotification);
        if (knobs[1]) knobs[1]->setValue(snap.delay.feedback / 100.0f, juce::dontSendNotification);
        if (knobs[2]) knobs[2]->setValue(snap.delay.wowDepth / 100.0f, juce::dontSendNotification);
        if (knobs[3]) knobs[3]->setValue(snap.delay.wowRate / 8.0f, juce::dontSendNotification);
        if (knobs[4]) knobs[4]->setValue(snap.delay.saturation / 100.0f, juce::dontSendNotification);
        if (knobs[5]) knobs[5]->setValue((snap.delay.highCut - 1000.0f) / 19000.0f, juce::dontSendNotification);
        if (knobs[6]) knobs[6]->setValue((snap.delay.lowCut - 20.0f) / 1980.0f, juce::dontSendNotification);
        if (knobs[7]) knobs[7]->setValue(snap.delay.mix / 100.0f, juce::dontSendNotification);
    }
}

void PluginEditor::applyStepHighlight(int newStep)
{
    // Clear previous highlight
    if (lastPaintedStep >= 0 && lastPaintedStep < static_cast<int>(stepButtons.size()))
    {
        if (auto* b = stepButtons[lastPaintedStep].get()) { 
            b->setSequencerActive(false); 
        }
    }
    
    // Set new highlight
    if (newStep >= 0 && newStep < static_cast<int>(stepButtons.size()))
    {
        if (auto* b = stepButtons[newStep].get()) { 
            b->setSequencerActive(true); 
        }
    }
    
    lastPaintedStep = newStep;
}

void PluginEditor::clearSequencerUI()
{
    if (lastPaintedStep >= 0 && lastPaintedStep < static_cast<int>(stepButtons.size()))
    {
        if (auto* b = stepButtons[lastPaintedStep].get()) { 
            b->setSequencerActive(false); 
        }
    }
    lastPaintedStep = -1;
}

void PluginEditor::refreshBarsFromStep(int stepToShow)
{
    // Clear all bars first
    for (int i = 0; i < 8; ++i)
    {
        if (i < static_cast<int>(currentStepProgress.size()))
        {
            currentStepProgress[i] = 0.0f;
        }
    }

    // If step is valid, update bars with snapshot values
    if (stepToShow >= 0 && stepToShow < 16)
    {
        const auto& snap = stepSnapshots[stepToShow];

        // Map delay snapshot fields to bars (0..1)
        if (static_cast<int>(currentStepProgress.size()) >= 8)
        {
            currentStepProgress[0] = snap.delay.timeMs / 2000.0f;      // Time (0-2000ms)
            currentStepProgress[1] = snap.delay.feedback / 100.0f;     // Feedback (0-100%)
            currentStepProgress[2] = snap.delay.wowDepth / 100.0f;     // Wow Depth (0-100%)
            currentStepProgress[3] = snap.delay.wowRate / 8.0f;        // Wow Rate (0-8Hz)
            currentStepProgress[4] = snap.delay.saturation / 100.0f;   // Drive (0-100%)
            currentStepProgress[5] = (snap.delay.highCut - 1000.0f) / 19000.0f;  // Hi Cut (1k-20k Hz)
            currentStepProgress[6] = (snap.delay.lowCut - 20.0f) / 1980.0f;      // Low Cut (20-2k Hz)
            currentStepProgress[7] = snap.delay.mix / 100.0f;          // Mix (0-100%)
        }
    }
}

// Add missing member variable
int lastPaintedStep = -1;
