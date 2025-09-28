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

// CustomStepButton implementation
CustomStepButton::CustomStepButton(int stepIndex) : juce::Button("Step" + juce::String(stepIndex)), stepIndex(stepIndex)
{
    setClickingTogglesState(false);
}

void CustomStepButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Save the graphics state
    g.saveState();
    
    // Apply 70% opacity if button is inactive
    if (inactive)
    {
        g.setOpacity(0.7f);
    }
    
    // Draw the appropriate SVG based on active state
    if (active && activeImage != nullptr)
    {
        activeImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else if (!active && inactiveImage != nullptr)
    {
        inactiveImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback drawing if SVGs not loaded
        g.setColour(active ? juce::Colours::orange : juce::Colours::darkgrey);
        g.fillRoundedRectangle(bounds, 4.0f);
    }
    
    // Restore graphics state
    g.restoreState();
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
    
    // Update step button visibility
    updateStepButtonVisibility();
    
    // Add parameter listeners for all delay parameters
    processorRef.getValueTreeState().addParameterListener("timeMs", this);
    processorRef.getValueTreeState().addParameterListener("feedback", this);
    processorRef.getValueTreeState().addParameterListener("wowDepth", this);
    processorRef.getValueTreeState().addParameterListener("wowRate", this);
    processorRef.getValueTreeState().addParameterListener("saturation", this);
    processorRef.getValueTreeState().addParameterListener("highCut", this);
    processorRef.getValueTreeState().addParameterListener("lowCut", this);
    processorRef.getValueTreeState().addParameterListener("mix", this);
    
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
    // Draw background
    if (backgroundImage != nullptr)
    {
        backgroundImage->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);
    }
    else
    {
        g.fillAll(juce::Colours::darkgrey);
    }
    
    // Draw step area background
    auto stepArea = getStepArea();
    if (stepBackgroundImage != nullptr)
    {
        stepBackgroundImage->drawWithin(g, stepArea.toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);
    }
    
    // Draw effect area background
    auto effectArea = getEffectArea();
    if (effectBackgroundImage != nullptr)
    {
        effectBackgroundImage->drawWithin(g, effectArea.toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);
    }
}

void PluginEditor::resized()
{
    // Step area positioning
    auto stepArea = getStepArea();
    if (stepAmountBox != nullptr)
    {
        stepAmountBox->setBounds(stepArea.getX() + 140, stepArea.getY() - 30, 40, 25);
    }
    
    if (rateTypeButton != nullptr)
    {
        rateTypeButton->setBounds(stepArea.getX() + 280, stepArea.getY() - 30, 25, 25);
    }
    
    if (rateBox != nullptr)
    {
        rateBox->setBounds(stepArea.getX() + 320, stepArea.getY() - 30, 60, 25);
    }
    
    // Position step buttons
    const int buttonSize = 40;
    const int buttonSpacing = 50;
    const int startX = stepArea.getX() + 50;
    const int startY = stepArea.getY() + 50;
    
    for (int i = 0; i < 16; ++i)
    {
        int x = startX + (i % 8) * buttonSpacing;
        int y = startY + (i / 8) * buttonSpacing;
        stepButtons[i]->setBounds (x, y, buttonSize, buttonSize);
    }
    
    // Position knobs in effect area
    auto effectArea = getEffectArea();
    const int knobSize = 60;
    const int knobSpacing = 80;
    const int knobStartX = effectArea.getX() + 50;
    const int knobStartY = effectArea.getY() + 80;
    
    for (int i = 0; i < 8; ++i)
    {
        int x = knobStartX + (i % 4) * knobSpacing;
        int y = knobStartY + (i / 4) * knobSpacing;
        knobs[i]->setBounds (x, y, knobSize, knobSize);
        knobs[i]->setVisible(true);
        knobs[i]->setEnabled(true);
        
        // Position labels
        if (labels[i] != nullptr)
        {
            labels[i]->setBounds (x, y - 20, knobSize, 15);
        }
        
        // Position value labels
        if (valueLabels[i] != nullptr)
        {
            valueLabels[i]->setBounds (x, y + knobSize + 5 - 8, knobSize, 15);
        }
    }
}

void PluginEditor::timerCallback()
{
    // Update value labels
    for (int i = 0; i < 8; ++i)
    {
        if (valueLabels[i] != nullptr && knobs[i] != nullptr)
        {
            auto value = knobs[i]->getValue();
            valueLabels[i]->setText(juce::String(value, 1), juce::dontSendNotification);
        }
    }
    
    // Update step sequencer
    updateStepSequencer();
}

void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
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

void PluginEditor::loadSVGAssets()
{
    // Load background images
    backgroundImage = juce::Drawable::createFromImageData(BinaryData::Background_Mustard_svg, BinaryData::Background_Mustard_svgSize);
    stepBackgroundImage = juce::Drawable::createFromImageData(BinaryData::Step_Background_Plate_svg, BinaryData::Step_Background_Plate_svgSize);
    effectBackgroundImage = juce::Drawable::createFromImageData(BinaryData::Effect_Background_Plate_svg, BinaryData::Effect_Background_Plate_svgSize);
    
    // Load step button images
    auto stepActiveImage = juce::Drawable::createFromImageData(BinaryData::Step_Active_svg, BinaryData::Step_Active_svgSize);
    auto stepInactiveImage = juce::Drawable::createFromImageData(BinaryData::Step_Inactive_svg, BinaryData::Step_Inactive_svgSize);
    
    // Load knob images
    auto knobRingImage = juce::Drawable::createFromImageData(BinaryData::Knob_Basic_Ring_svg, BinaryData::Knob_Basic_Ring_svgSize);
    auto knobInnerImage = juce::Drawable::createFromImageData(BinaryData::Knob_Basic_Inside_svg, BinaryData::Knob_Basic_Inside_svgSize);
    
    // Set images for step buttons
    for (int i = 0; i < 16; ++i)
    {
        stepButtons[i]->setImages(
            juce::Drawable::createCopy(*stepInactiveImage),
            juce::Drawable::createCopy(*stepActiveImage)
        );
    }
    
    // Set images for knobs
    for (int i = 0; i < 8; ++i)
    {
        knobs[i]->setRingImage(juce::Drawable::createCopy(*knobRingImage));
        knobs[i]->setInnerImage(juce::Drawable::createCopy(*knobInnerImage));
    }
}

void PluginEditor::setupKnobs()
{
    // Create knob attachments
    auto& apvts = processorRef.getValueTreeState();
    
    knobAttachments.clear();
    for (int i = 0; i < 8; ++i)
    {
        knobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(knobs[i].get());
        
        // Create parameter attachment based on knob index
        juce::String parameterID;
        switch (i) {
            case 0: parameterID = "timeMs"; break;
            case 1: parameterID = "feedback"; break;
            case 2: parameterID = "wowDepth"; break;
            case 3: parameterID = "wowRate"; break;
            case 4: parameterID = "saturation"; break;
            case 5: parameterID = "highCut"; break;
            case 6: parameterID = "lowCut"; break;
            case 7: parameterID = "mix"; break;
        }
        
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, parameterID, *knobs[i]);
        knobAttachments.push_back (std::move(attachment));
    }
}

void PluginEditor::setupLabels()
{
    const juce::String labelTexts[] = {
        "Time", "Feedback", "Wow Depth", "Wow Rate",
        "Drive", "Hi-Cut", "Lo-Cut", "Mix"
    };
    
    for (int i = 0; i < 8; ++i)
    {
        labels[i] = std::make_unique<juce::Label>();
        labels[i]->setText(labelTexts[i], juce::dontSendNotification);
        labels[i]->setFont(FontManager::getFont("Akira Expanded", 12.0f));
        labels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        labels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(labels[i].get());
    }
}

void PluginEditor::setupValueLabels()
{
    for (int i = 0; i < 8; ++i)
    {
        valueLabels[i] = std::make_unique<juce::Label>();
        valueLabels[i]->setText("50.0", juce::dontSendNotification);
        valueLabels[i]->setFont(FontManager::getFont("Akira Expanded", 10.0f));
        valueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        valueLabels[i]->setJustificationType(juce::Justification::centred);
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

void PluginEditor::updateStepButtonVisibility()
{
    // For now, show all buttons - this can be enhanced later
    for (int i = 0; i < 16; ++i)
    {
        stepButtons[i]->setVisible(true);
        stepButtons[i]->setInactive(false);
    }
}

void PluginEditor::updateStepSequencer()
{
    // Simple sequencer update - can be enhanced later
    static int currentStep = 0;
    static int counter = 0;
    
    counter++;
    if (counter >= 20) // Update every 20 timer calls
    {
        counter = 0;
        
        // Clear previous step
        if (currentStep >= 0 && currentStep < 16)
        {
            stepButtons[currentStep]->setActive(false);
        }
        
        // Set new active step
        currentStep = (currentStep + 1) % 16;
        if (currentStep < 16)
        {
            stepButtons[currentStep]->setActive(true);
        }
    }
}

// Helper methods
juce::Rectangle<int> PluginEditor::getStepArea() const
{
    return juce::Rectangle<int>(50, 50, 450, 200);
}

juce::Rectangle<int> PluginEditor::getEffectArea() const
{
    return juce::Rectangle<int>(550, 50, 350, 200);
}

void PluginEditor::saveSnapshot(int stepIndex) {}
void PluginEditor::restoreSnapshot(int stepIndex) {}
void PluginEditor::applyStepHighlight(int newStep) {}
void PluginEditor::clearSequencerUI() {}
void PluginEditor::refreshBarsFromStep(int stepToShow) {}
void PluginEditor::setupBasicKnobs() {}
void PluginEditor::setupVisibleKnobs() {}
