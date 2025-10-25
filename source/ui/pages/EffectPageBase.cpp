#include "EffectPageBase.h"
#include "PluginProcessor.h"
#include "RandomizationManager.h"
#include "ui/CustomKnob.h"
#include "ui/StepButton.h"
#include "ui/AllStepsToggleButton.h"
#include "ui/CircularToggleButton.h"
#include "ui/CustomDiceButton.h"

//==============================================================================
// EffectPageBase Implementation
//==============================================================================

EffectPageBase::EffectPageBase(PluginProcessor& processor, UiAssets& assets)
    : processorRef(processor), assets(assets)
{
    // Initialize randomization manager
    randomizationManager = std::make_unique<RandomizationManager>(processorRef, processorRef.getAPVTS(), this);
}

void EffectPageBase::setupCommonKnobs()
{
    // Create 8 knobs with common setup
    for (int i = 0; i < 8; ++i) {
        knobs[i] = std::make_unique<juce::Slider>();
        knobs[i]->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knobs[i]->setRange(0.0, 1.0, 0.001);
        knobs[i]->setValue(0.5);
        addAndMakeVisible(knobs[i].get());
        
        // Create labels
        knobLabels[i] = std::make_unique<juce::Label>();
        knobLabels[i]->setText("Knob " + juce::String(i + 1), juce::dontSendNotification);
        knobLabels[i]->setJustificationType(juce::Justification::centred);
        knobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(knobLabels[i].get());
        
        // Create value labels
        valueLabels[i] = std::make_unique<juce::Label>();
        valueLabels[i]->setText("0.50", juce::dontSendNotification);
        valueLabels[i]->setJustificationType(juce::Justification::centred);
        valueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(valueLabels[i].get());
        
        // Add to page group
        pageGroup.push_back(knobs[i].get());
        pageGroup.push_back(knobLabels[i].get());
        pageGroup.push_back(valueLabels[i].get());
    }
}

void EffectPageBase::setupCommonStepButtons()
{
    // Create 16 step buttons
    for (int i = 0; i < 16; ++i) {
        stepButtons[i] = std::make_unique<StepButton>(i);
        stepButtons[i]->onClick = [this, i]() {
            uiSelectedStep = i;
            updateSequencerUI();
            // Notify processor of step selection
            // This will be implemented by derived classes
        };
        addAndMakeVisible(stepButtons[i].get());
        pageGroup.push_back(stepButtons[i].get());
    }
}

void EffectPageBase::setupCommonPowerButtons()
{
    // FX Power Button
    fxPowerButton = std::make_unique<juce::DrawableButton>("FX Power", juce::DrawableButton::ButtonStyle::ImageFitted);
    fxPowerButton->onClick = [this]() {
        fxAreaEnabled = fxPowerButton->getToggleState();
        updateFxAreaVisibility();
        // Update processor parameter - to be implemented by derived classes
    };
    addAndMakeVisible(fxPowerButton.get());
    pageGroup.push_back(fxPowerButton.get());
    
    // Step Power Button
    stepPowerButton = std::make_unique<juce::DrawableButton>("Step Power", juce::DrawableButton::ButtonStyle::ImageFitted);
    stepPowerButton->onClick = [this]() {
        stepAreaEnabled = stepPowerButton->getToggleState();
        updateStepAreaVisibility();
        // Update processor parameter - to be implemented by derived classes
    };
    addAndMakeVisible(stepPowerButton.get());
    pageGroup.push_back(stepPowerButton.get());
}

void EffectPageBase::setupCommonAllStepsToggle()
{
    allStepsToggle = std::make_unique<AllStepsToggleButton>();
    allStepsToggle->onClick = [this]() {
        allStepsEnabled = allStepsToggle->getToggleState();
        // Update all steps - to be implemented by derived classes
    };
    addAndMakeVisible(allStepsToggle.get());
    pageGroup.push_back(allStepsToggle.get());
    
    allStepsLabel = std::make_unique<juce::Label>();
    allStepsLabel->setText("All Steps", juce::dontSendNotification);
    allStepsLabel->setJustificationType(juce::Justification::centred);
    allStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(allStepsLabel.get());
    pageGroup.push_back(allStepsLabel.get());
}

void EffectPageBase::setupCommonSequencerComponents()
{
    // Step Amount Label (TextEditor)
    stepAmountLabel = std::make_unique<juce::TextEditor>();
    stepAmountLabel->setText("16");
    stepAmountLabel->setJustificationType(juce::Justification::centred);
    stepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2B2D31));
    stepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    stepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF101113));
    addAndMakeVisible(stepAmountLabel.get());
    pageGroup.push_back(stepAmountLabel.get());
    
    // Rate Dropdown
    rateDropdown = std::make_unique<juce::ComboBox>();
    rateDropdown->addItem("1/4", 1);
    rateDropdown->addItem("1/8", 2);
    rateDropdown->addItem("1/16", 3);
    rateDropdown->addItem("1/32", 4);
    rateDropdown->setSelectedId(3);
    addAndMakeVisible(rateDropdown.get());
    pageGroup.push_back(rateDropdown.get());
    
    // STD Toggle
    stdToggle = std::make_unique<CircularToggleButton>();
    addAndMakeVisible(stdToggle.get());
    pageGroup.push_back(stdToggle.get());
    
    // Step Title
    stepTitle = std::make_unique<juce::Label>();
    stepTitle->setText("Steps", juce::dontSendNotification);
    stepTitle->setJustificationType(juce::Justification::centred);
    stepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(stepTitle.get());
    pageGroup.push_back(stepTitle.get());
    
    // Step Dice Button
    stepDiceButton = std::make_unique<CustomDiceButton>();
    stepDiceButton->onClick = [this]() {
        randomizeSequencer();
    };
    addAndMakeVisible(stepDiceButton.get());
    pageGroup.push_back(stepDiceButton.get());
}

void EffectPageBase::setupPowerButtons()
{
    setupCommonPowerButtons();
}

void EffectPageBase::setupStepPowerButton()
{
    // This is handled by setupCommonPowerButtons
}

void EffectPageBase::updatePowerButtonStates()
{
    if (fxPowerButton) {
        fxPowerButton->setToggleState(fxAreaEnabled, juce::dontSendNotification);
    }
    if (stepPowerButton) {
        stepPowerButton->setToggleState(stepAreaEnabled, juce::dontSendNotification);
    }
    if (allStepsToggle) {
        allStepsToggle->setToggleState(allStepsEnabled, juce::dontSendNotification);
    }
}

void EffectPageBase::setFxAreaEnabled(bool enabled)
{
    fxAreaEnabled = enabled;
    updateFxAreaVisibility();
    updatePowerButtonStates();
}

void EffectPageBase::setStepAreaEnabled(bool enabled)
{
    stepAreaEnabled = enabled;
    updateStepAreaVisibility();
    updatePowerButtonStates();
}

void EffectPageBase::setAllStepsEnabled(bool enabled)
{
    allStepsEnabled = enabled;
    updatePowerButtonStates();
}

void EffectPageBase::setVisibleVec(const std::vector<juce::Component*>& components, bool visible)
{
    for (auto* component : components) {
        if (component) {
            component->setVisible(visible);
        }
    }
}

void EffectPageBase::updateKnobValueLabel(int knobIndex, float value, const juce::String& suffix)
{
    if (knobIndex >= 0 && knobIndex < 8 && valueLabels[knobIndex]) {
        juce::String valueText = juce::String(value, 2) + suffix;
        valueLabels[knobIndex]->setText(valueText, juce::dontSendNotification);
    }
}

void EffectPageBase::updateSequencerStepButton(int stepIndex, bool selected, bool playing, bool enabled)
{
    if (stepIndex >= 0 && stepIndex < 16 && stepButtons[stepIndex]) {
        auto* stepButton = dynamic_cast<StepButton*>(stepButtons[stepIndex].get());
        if (stepButton) {
            stepButton->setSelected(selected);
            stepButton->setPlaying(playing);
            stepButton->setEnabledStep(enabled);
        }
    }
}
