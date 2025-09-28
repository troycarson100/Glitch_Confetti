#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "FontManager.h"

//==============================================================================
// CustomKnob Implementation
//==============================================================================

CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 100.0, 1.0);
    setValue(50.0);
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw ring image if available
    if (ringImage != nullptr)
    {
        ringImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    
    // Draw inner image if available
    if (innerImage != nullptr)
    {
        innerImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    
    // Draw value indicator line
    auto centre = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
    auto angle = (getValue() / getMaximum()) * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
    
    auto endX = centre.x + radius * std::cos(angle);
    auto endY = centre.y + radius * std::sin(angle);
    
    g.setColour(juce::Colours::white);
    g.drawLine(centre.x, centre.y, endX, endY, 3.0f);
}

void CustomKnob::resized()
{
}

void CustomKnob::setRingImage(std::unique_ptr<juce::Drawable> ring)
{
    ringImage = std::move(ring);
    repaint();
}

void CustomKnob::setInnerImage(std::unique_ptr<juce::Drawable> inner)
{
    innerImage = std::move(inner);
    repaint();
}

//==============================================================================
// CustomStepButton Implementation
//==============================================================================

CustomStepButton::CustomStepButton(int stepIndex) : juce::Button("Step" + juce::String(stepIndex)), stepIndex(stepIndex)
{
}

void CustomStepButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw inactive image if available
    if (inactiveImage != nullptr && !active)
    {
        inactiveImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    
    // Draw active image if available
    if (activeImage != nullptr && active)
    {
        activeImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    
    // Draw highlight if sequencer is active
    if (sequencerActive)
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.7f));
        g.fillRoundedRectangle(bounds, 10.0f);
    }
}

void CustomStepButton::resized()
{
}

void CustomStepButton::setActive(bool isActive)
{
    if (active != isActive)
    {
        active = isActive;
        repaint();
    }
}

void CustomStepButton::setInactive(bool isInactive)
{
    if (inactive != isInactive)
    {
        inactive = isInactive;
        repaint();
    }
}

void CustomStepButton::setSequencerActive(bool isSequencerActive)
{
    if (sequencerActive != isSequencerActive)
    {
        sequencerActive = isSequencerActive;
        repaint();
    }
}

void CustomStepButton::setActiveImage(std::unique_ptr<juce::Drawable> image)
{
    activeImage = std::move(image);
    repaint();
}

void CustomStepButton::setInactiveImage(std::unique_ptr<juce::Drawable> image)
{
    inactiveImage = std::move(image);
    repaint();
}

void CustomStepButton::setImages(std::unique_ptr<juce::Drawable> inactiveImg, std::unique_ptr<juce::Drawable> activeImg)
{
    inactiveImage = std::move(inactiveImg);
    activeImage = std::move(activeImg);
    repaint();
}

//==============================================================================
// PluginEditor Implementation
//==============================================================================

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Set the size to match our desired dimensions
    setSize (974, 532);
    
    // Lock the size - no resizing
    setResizable (false, false);
    
    // Load SVG assets
    loadSVGAssets();
    
    // Setup UI components
    setupLabels();
    setupValueLabels();
    setupStepButtons();
    setupKnobs();
    
    // Initialize snapshots
    initializeSnapshots();
    
    // Start timer for UI updates
    startTimer(50); // 20 Hz
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Fill background
    g.fillAll (juce::Colours::darkgrey);
    
    // Draw background images if available
    if (backgroundImage != nullptr)
    {
        backgroundImage->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    }
}

void PluginEditor::resized()
{
    // Layout components
    layoutComponents();
}

void PluginEditor::timerCallback()
{
    // Update UI based on processor state
    updateStepSequencer();
}

void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Save current knob values to the active step snapshot
    saveSnapshot(currentActiveStep);
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
        knobs[i]->setVisible(true);
        knobs[i]->setEnabled(true);
        
        // Position labels above knobs
        if (labels[i] != nullptr)
        {
            labels[i]->setBounds (x, y - 20, knobSize, 15);
        }
        
        // Position value labels below knobs
        if (valueLabels[i] != nullptr)
        {
            valueLabels[i]->setBounds (x, y + knobSize + 5 - 8, knobSize, 15);
        }
    }
    
    // Update value labels
    updateValueLabels();
}

void PluginEditor::updateValueLabels()
{
    for (int i = 0; i < 8; ++i)
    {
        if (valueLabels[i] != nullptr && knobs[i] != nullptr)
        {
            auto value = knobs[i]->getValue();
            valueLabels[i]->setText(juce::String(value, 1), juce::dontSendNotification);
        }
    }
}

void PluginEditor::loadSVGAssets()
{
    // Load SVG images from BinaryData
    stepActiveImage = juce::Drawable::createFromImageData(BinaryData::Step_Active_svg, BinaryData::Step_Active_svgSize);
    stepInactiveImage = juce::Drawable::createFromImageData(BinaryData::Step_Inactive_svg, BinaryData::Step_Inactive_svgSize);
    knobRingImage = juce::Drawable::createFromImageData(BinaryData::Knob_Basic_Ring_svg, BinaryData::Knob_Basic_Ring_svgSize);
    knobInnerImage = juce::Drawable::createFromImageData(BinaryData::Knob_Basic_Inside_svg, BinaryData::Knob_Basic_Inside_svgSize);
    
    // Set images for step buttons
    for (int i = 0; i < 16; ++i)
    {
        stepButtons[i]->setImages(
            stepInactiveImage->createCopy(),
            stepActiveImage->createCopy()
        );
    }
    
    // Set images for knobs
    for (int i = 0; i < 8; ++i)
    {
        knobs[i]->setRingImage(knobRingImage->createCopy());
        knobs[i]->setInnerImage(knobInnerImage->createCopy());
    }
}

void PluginEditor::setupKnobs()
{
    // Create knob attachments
    std::vector<juce::String> parameterIDs = {
        "delayTime", "feedback", "mix", "drive",
        "lowCut", "highCut", "wowRate", "wowDepth"
    };
    
    // Create knobs
    for (int i = 0; i < 8; ++i)
    {
        knobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(knobs[i].get());
        
        // Create parameter attachment
        knobAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, parameterIDs[i], *knobs[i]));
    }
}

void PluginEditor::setupLabels()
{
    std::vector<juce::String> labelTexts = {
        "Time", "Feedback", "Mix", "Drive",
        "Low Cut", "High Cut", "Wow Rate", "Wow Depth"
    };
    
    for (int i = 0; i < 8; ++i)
    {
        labels[i] = std::make_unique<juce::Label>();
        labels[i]->setText(labelTexts[i], juce::dontSendNotification);
        labels[i]->setFont(FontManager::getInstance().getFont("Akira Expanded", 12.0f));
        labels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        labels[i]->setJustificationType(juce::Justification::centred);
        labels[i]->setVisible(true);
        addAndMakeVisible(labels[i].get());
    }
}

void PluginEditor::setupValueLabels()
{
    for (int i = 0; i < 8; ++i)
    {
        valueLabels[i] = std::make_unique<juce::Label>();
        valueLabels[i]->setText("50.0", juce::dontSendNotification);
        valueLabels[i]->setFont(FontManager::getInstance().getFont("Akira Expanded", 10.0f));
        valueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        valueLabels[i]->setJustificationType(juce::Justification::centred);
        valueLabels[i]->setVisible(true);
        addAndMakeVisible(valueLabels[i].get());
    }
}

void PluginEditor::setupStepButtons()
{
    for (int i = 0; i < 16; ++i)
    {
        stepButtons[i] = std::make_unique<CustomStepButton>(i);
        addAndMakeVisible(stepButtons[i].get());
    }
}

void PluginEditor::initializeSnapshots()
{
    for (int step = 0; step < 16; ++step)
    {
        stepSnapshots[step] = StepSnapshot(); // Uses default constructor values
    }
}

void PluginEditor::updateStepSequencer()
{
    // Update step button visibility based on stepsUsed
    updateStepButtonVisibility();
    
    // Update active step highlight
    int currentStep = processorRef.getCurrentStep();
    if (currentStep != currentActiveStep)
    {
        applyStepHighlight(currentStep);
        currentActiveStep = currentStep;
    }
}

void PluginEditor::updateStepButtonVisibility()
{
    int stepsUsed = processorRef.getStepsUsed();
    
    for (int i = 0; i < 16; ++i)
    {
        stepButtons[i]->setVisible(true);
        stepButtons[i]->setInactive(false);
        
        // Grey out steps beyond stepsUsed
        if (i >= stepsUsed)
        {
            stepButtons[i]->setInactive(true);
        }
    }
}

void PluginEditor::applyStepHighlight(int newStep)
{
    // Clear previous highlight
    if (currentActiveStep >= 0 && currentActiveStep < 16)
    {
        stepButtons[currentActiveStep]->setActive(false);
    }
    
    // Set new highlight
    if (newStep >= 0 && newStep < 16)
    {
        stepButtons[currentActiveStep]->setActive(true);
    }
}

void PluginEditor::clearSequencerUI() {}

void PluginEditor::saveSnapshot(int stepIndex) {}

void PluginEditor::restoreSnapshot(int stepIndex) {}

void PluginEditor::refreshBarsFromStep(int stepToShow) {}
