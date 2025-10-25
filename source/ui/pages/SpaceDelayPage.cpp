#include "SpaceDelayPage.h"
#include "PluginProcessor.h"
#include "RandomizationManager.h"
#include "ui/CustomKnob.h"
#include "ui/StepButton.h"
#include "ui/AllStepsToggleButton.h"
#include "ui/CircularToggleButton.h"
#include "ui/CustomDiceButton.h"
#include "ui/IndicatorBar.h"
#include "ui/LockButton.h"

//==============================================================================
// SpaceDelayPage Implementation
//==============================================================================

SpaceDelayPage::SpaceDelayPage(PluginProcessor& processor, UiAssets& assets)
    : EffectPageBase(processor, assets)
{
    effectID = EffectID::SpaceDelay;
    setupSpaceDelayUI();
}

void SpaceDelayPage::setupSpaceDelayUI()
{
    DBG("[UI] Setting up Space Delay UI...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create effect type dropdown (DEPRECATED - replaced by router dropdowns)
    effectTypeDropdown = std::make_unique<juce::ComboBox>();
    // Don't add to UI - replaced by new router effect selectors
    
    // Create and configure BigComboWithSvgLNF for larger popup menus with SVG caret
    fxComboLNF = std::make_unique<BigComboWithSvgLNF>();
    fxComboLNF->popupFontPx   = 18.0f;  // Larger font
    fxComboLNF->rowHeightPx   = 32;     // Taller rows
    fxComboLNF->minPopupWidth = 300;    // Wider minimum width
    fxComboLNF->closedHeight  = 36;     // Closed control height
    
    // Load carrot SVGs into the LookAndFeel
    if (assets.fxTypeCarrotInactive != nullptr) {
        fxComboLNF->setCaretSVG(assets.fxTypeCarrotInactive->createCopy());
    }
    
    // Apply the custom LookAndFeel to the FX ComboBox only
    effectTypeDropdown->setLookAndFeel(fxComboLNF.get());
    
    // Configure ComboBox colors for dark theme
    effectTypeDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2B2D31));
    effectTypeDropdown->setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xFF101113));
    effectTypeDropdown->setColour(juce::ComboBox::textColourId,       juce::Colours::white);
    
    // Add effect types
    effectTypeDropdown->addItem("Space Delay", 1);
    effectTypeDropdown->addItem("Auto Pan", 2);
    effectTypeDropdown->addItem("Dirt", 3);
    effectTypeDropdown->addItem("Chorus", 4);
    effectTypeDropdown->addItem("Hall", 5);
    effectTypeDropdown->addItem("Grain", 6);
    effectTypeDropdown->addItem("Slicer", 7);
    effectTypeDropdown->addItem("Dub Echo", 8);
    effectTypeDropdown->addItem("Redux", 9);
    effectTypeDropdown->addItem("PhaseBloom", 10);
    effectTypeDropdown->setSelectedId(1, juce::dontSendNotification);
    
    // Position dropdown with proper height for closed control
    effectTypeDropdown->setBounds(effectArea.getX() + 80 - 85 - 10, effectArea.getY() - 37 - 5 - 5, 120, fxComboLNF->closedHeight);
    effectTypeDropdown->setJustificationType(juce::Justification::centredLeft);
    
    // Set up dropdown change handler
    effectTypeDropdown->onChange = [this]() {
        int selectedId = effectTypeDropdown->getSelectedId();
        DBG("[UI] Effect type changed to: " << selectedId);
        // TODO: Implement effect type switching logic
    };
    
    // Setup all Space Delay specific components
    setupSpaceDelayKnobs();
    setupSpaceDelayStepButtons();
    setupSpaceDelaySequencerComponents();
    setupSpaceDelayPowerButtons();
    setupSpaceDelayAllStepsToggle();
    
    // Add all components to page group for visibility management
    pageGroup.clear();
    
    // Add knobs and related components
    for (int i = 0; i < 8; ++i) {
        if (spaceDelayKnobs[i]) pageGroup.push_back(spaceDelayKnobs[i].get());
        if (spaceDelayKnobLabels[i]) pageGroup.push_back(spaceDelayKnobLabels[i].get());
        if (spaceDelayValueLabels[i]) pageGroup.push_back(spaceDelayValueLabels[i].get());
        if (spaceDelayIndicatorBars[i]) pageGroup.push_back(spaceDelayIndicatorBars[i].get());
        if (spaceDelayKnobDiceButtons[i]) pageGroup.push_back(spaceDelayKnobDiceButtons[i].get());
        if (spaceDelayKnobLockButtons[i]) pageGroup.push_back(spaceDelayKnobLockButtons[i].get());
    }
    
    // Add step buttons
    for (int i = 0; i < 16; ++i) {
        if (spaceDelayStepButtons[i]) pageGroup.push_back(spaceDelayStepButtons[i].get());
    }
    
    // Add sequencer components
    if (spaceDelayStepAmountLabel) pageGroup.push_back(spaceDelayStepAmountLabel.get());
    if (spaceDelayRateDropdown) pageGroup.push_back(spaceDelayRateDropdown.get());
    if (spaceDelayStdToggle) pageGroup.push_back(spaceDelayStdToggle.get());
    if (spaceDelayStepTitle) pageGroup.push_back(spaceDelayStepTitle.get());
    if (spaceDelayStepDiceButton) pageGroup.push_back(spaceDelayStepDiceButton.get());
    if (spaceDelayStepPowerButton) pageGroup.push_back(spaceDelayStepPowerButton.get());
    
    // Add power buttons
    if (spaceDelayFxPowerButton) pageGroup.push_back(spaceDelayFxPowerButton.get());
    
    // Add All Steps toggle
    if (spaceDelayAllStepsToggle) pageGroup.push_back(spaceDelayAllStepsToggle.get());
    if (spaceDelayAllStepsLabel) pageGroup.push_back(spaceDelayAllStepsLabel.get());
    
    DBG("[UI] Space Delay UI setup complete");
}

void SpaceDelayPage::setupSpaceDelayKnobs()
{
    DBG("[UI] Setting up Space Delay knobs...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;
    
    for (int i = 0; i < 8; ++i) {
        // Create knob
        spaceDelayKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(spaceDelayKnobs[i].get());
        
        // Set knob properties
        spaceDelayKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        spaceDelayKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        spaceDelayKnobs[i]->setRange(0.0, 1.0, 0.001);
        spaceDelayKnobs[i]->setValue(0.5, juce::dontSendNotification);
        
        // Position knob
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + knobSpacing);
        spaceDelayKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label
        spaceDelayKnobLabels[i] = std::make_unique<juce::Label>();
        spaceDelayKnobLabels[i]->setText(spaceDelayKnobNames[i], juce::dontSendNotification);
        spaceDelayKnobLabels[i]->setJustificationType(juce::Justification::centred);
        spaceDelayKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        spaceDelayKnobLabels[i]->setBounds(x, y + knobSize + 5, knobSize, 20);
        addAndMakeVisible(spaceDelayKnobLabels[i].get());
        
        // Create value label
        spaceDelayValueLabels[i] = std::make_unique<juce::Label>();
        spaceDelayValueLabels[i]->setText("0.50", juce::dontSendNotification);
        spaceDelayValueLabels[i]->setJustificationType(juce::Justification::centred);
        spaceDelayValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        spaceDelayValueLabels[i]->setBounds(x, y + knobSize + 25, knobSize, 20);
        addAndMakeVisible(spaceDelayValueLabels[i].get());
        
        // Create indicator bar
        spaceDelayIndicatorBars[i] = std::make_unique<IndicatorBar>();
        spaceDelayIndicatorBars[i]->setBounds(x + 10, y + knobSize + 45, knobSize - 20, 8);
        addAndMakeVisible(spaceDelayIndicatorBars[i].get());
        
        // Create dice button
        spaceDelayKnobDiceButtons[i] = std::make_unique<CustomDiceButton>();
        spaceDelayKnobDiceButtons[i]->setBounds(x + knobSize - 20, y + knobSize + 55, 16, 16);
        addAndMakeVisible(spaceDelayKnobDiceButtons[i].get());
        
        // Create lock button
        spaceDelayKnobLockButtons[i] = std::make_unique<LockButton>();
        spaceDelayKnobLockButtons[i]->setBounds(x + 4, y + knobSize + 55, 16, 16);
        addAndMakeVisible(spaceDelayKnobLockButtons[i].get());
        
        // Set up knob value change handler
        spaceDelayKnobs[i]->onValueChange = [this, i]() {
            updateSpaceDelayKnobValueLabel(i, spaceDelayKnobs[i]->getValue());
            // Update processor parameter
            auto* param = processorRef.getAPVTS().getParameter(spaceDelayParameterIds[i]);
            if (param) {
                param->setValueNotifyingHost(spaceDelayKnobs[i]->getValue());
            }
        };
        
        // Set up dice button click handler
        spaceDelayKnobDiceButtons[i]->onClick = [this, i]() {
            // Randomize this specific knob
            float randomValue = juce::Random::getSystemRandom().nextFloat();
            spaceDelayKnobs[i]->setValue(randomValue, juce::dontSendNotification);
            updateSpaceDelayKnobValueLabel(i, randomValue);
        };
        
        // Set up lock button click handler
        spaceDelayKnobLockButtons[i]->onClick = [this, i]() {
            spaceDelayKnobLocked[i] = spaceDelayKnobLockButtons[i]->getToggleState();
        };
    }
    
    DBG("[UI] Space Delay knobs setup complete");
}

void SpaceDelayPage::setupSpaceDelayStepButtons()
{
    DBG("[UI] Setting up Space Delay step buttons...");
    
    // Step area bounds
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);
    const int buttonSize = 24;
    const int buttonSpacing = 4;
    const int startX = stepArea.getX() + 20;
    const int startY = stepArea.getY() + 20;
    
    for (int i = 0; i < 16; ++i) {
        spaceDelayStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(spaceDelayStepButtons[i].get());
        
        // Position button
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        spaceDelayStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set up click handler
        spaceDelayStepButtons[i]->onClick = [this, i]() {
            spaceDelayUiSelectedStep = i;
            updateSpaceDelaySequencerUI();
            processorRef.setSpaceDelaySelectedStep(i);
        };
    }
    
    DBG("[UI] Space Delay step buttons setup complete");
}

void SpaceDelayPage::setupSpaceDelaySequencerComponents()
{
    DBG("[UI] Setting up Space Delay sequencer components...");
    
    // Step area bounds
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Step Amount Label (TextEditor)
    spaceDelayStepAmountLabel = std::make_unique<juce::TextEditor>();
    spaceDelayStepAmountLabel->setText("16");
    spaceDelayStepAmountLabel->setJustificationType(juce::Justification::centred);
    spaceDelayStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2B2D31));
    spaceDelayStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    spaceDelayStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF101113));
    spaceDelayStepAmountLabel->setBounds(stepArea.getX() + 20, stepArea.getY() + 60, 40, 24);
    addAndMakeVisible(spaceDelayStepAmountLabel.get());
    
    // Rate Dropdown
    spaceDelayRateDropdown = std::make_unique<juce::ComboBox>();
    spaceDelayRateDropdown->addItem("1/4", 1);
    spaceDelayRateDropdown->addItem("1/8", 2);
    spaceDelayRateDropdown->addItem("1/16", 3);
    spaceDelayRateDropdown->addItem("1/32", 4);
    spaceDelayRateDropdown->setSelectedId(3);
    spaceDelayRateDropdown->setBounds(stepArea.getX() + 80, stepArea.getY() + 60, 60, 24);
    addAndMakeVisible(spaceDelayRateDropdown.get());
    
    // STD Toggle
    spaceDelayStdToggle = std::make_unique<CircularToggleButton>();
    spaceDelayStdToggle->setBounds(stepArea.getX() + 160, stepArea.getY() + 60, 24, 24);
    addAndMakeVisible(spaceDelayStdToggle.get());
    
    // Step Title
    spaceDelayStepTitle = std::make_unique<juce::Label>();
    spaceDelayStepTitle->setText("Steps", juce::dontSendNotification);
    spaceDelayStepTitle->setJustificationType(juce::Justification::centred);
    spaceDelayStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    spaceDelayStepTitle->setBounds(stepArea.getX() + 20, stepArea.getY() + 90, 60, 20);
    addAndMakeVisible(spaceDelayStepTitle.get());
    
    // Step Dice Button
    spaceDelayStepDiceButton = std::make_unique<CustomDiceButton>();
    spaceDelayStepDiceButton->setBounds(stepArea.getX() + 200, stepArea.getY() + 60, 24, 24);
    addAndMakeVisible(spaceDelayStepDiceButton.get());
    
    // Set up dice button click handler
    spaceDelayStepDiceButton->onClick = [this]() {
        randomizeSpaceDelaySequencer();
    };
    
    DBG("[UI] Space Delay sequencer components setup complete");
}

void SpaceDelayPage::setupSpaceDelayPowerButtons()
{
    DBG("[UI] Setting up Space Delay power buttons...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // FX Power Button
    spaceDelayFxPowerButton = std::make_unique<juce::DrawableButton>("fxPowerButton", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(spaceDelayFxPowerButton.get());
    
    const int buttonSize = 46;
    spaceDelayFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    
    spaceDelayFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    spaceDelayFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn != nullptr) {
        spaceDelayFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }
    
    spaceDelayFxPowerButton->setClickingTogglesState(true);
    spaceDelayFxPowerButton->setToggleState(spaceDelayFxAreaEnabled, juce::dontSendNotification);
    spaceDelayFxPowerButton->onClick = [this]() {
        spaceDelayFxAreaEnabled = spaceDelayFxPowerButton->getToggleState();
        updateSpaceDelayFxAreaVisibility();
        
        // Update the delayEnabled parameter
        auto* delayEnabledParam = processorRef.getAPVTS().getParameter("delayEnabled");
        if (delayEnabledParam) {
            delayEnabledParam->setValueNotifyingHost(spaceDelayFxAreaEnabled ? 1.0f : 0.0f);
        }
    };
    
    // Step Power Button
    spaceDelayStepPowerButton = std::make_unique<juce::DrawableButton>("stepPowerButton", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(spaceDelayStepPowerButton.get());
    
    const int stepButtonSize = 40;
    spaceDelayStepPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - stepButtonSize - 8, effectArea.getY() + effectArea.getHeight() - stepButtonSize - 8, stepButtonSize, stepButtonSize);
    
    spaceDelayStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    spaceDelayStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn != nullptr) {
        spaceDelayStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    spaceDelayStepPowerButton->setClickingTogglesState(true);
    spaceDelayStepPowerButton->setToggleState(spaceDelayStepAreaEnabled, juce::dontSendNotification);
    spaceDelayStepPowerButton->onClick = [this]() {
        spaceDelayStepAreaEnabled = spaceDelayStepPowerButton->getToggleState();
        updateSpaceDelayStepAreaVisibility();
        
        // Update processor sequencer state
        processorRef.setSpaceDelaySequencerEnabled(spaceDelayStepAreaEnabled);
    };
    
    DBG("[UI] Space Delay power buttons setup complete");
}

void SpaceDelayPage::setupSpaceDelayAllStepsToggle()
{
    DBG("[UI] Setting up Space Delay All Steps toggle...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "All Steps" toggle button
    spaceDelayAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(spaceDelayAllStepsToggle.get());
    
    // Position button
    const int buttonSize = 29;
    spaceDelayAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, effectArea.getY() - 1, buttonSize, buttonSize);
    
    // Set up images
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr) {
        spaceDelayAllStepsToggle->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    // Set up click handler
    spaceDelayAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    spaceDelayAllStepsToggle->onClick = [this]() {
        spaceDelayAllStepsEnabled = spaceDelayAllStepsToggle->getToggleState();
        DBG("[UI] Space Delay All Steps toggle: " + juce::String(spaceDelayAllStepsEnabled ? "ON" : "OFF"));
    };
    
    // Create "All Steps" label
    spaceDelayAllStepsLabel = std::make_unique<juce::Label>();
    spaceDelayAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    spaceDelayAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    spaceDelayAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    spaceDelayAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    spaceDelayAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, effectArea.getY() + 1, 80, 24);
    addAndMakeVisible(spaceDelayAllStepsLabel.get());
    
    DBG("[UI] Space Delay All Steps toggle setup complete");
}

// Override base class methods
void SpaceDelayPage::setupKnobs() { setupSpaceDelayKnobs(); }
void SpaceDelayPage::setupEffectsArea() { /* Handled by setupSpaceDelayUI */ }
void SpaceDelayPage::setupSequencerArea() { setupSpaceDelayStepButtons(); setupSpaceDelaySequencerComponents(); }
void SpaceDelayPage::setupAllStepsToggle() { setupSpaceDelayAllStepsToggle(); }

void SpaceDelayPage::updateKnobValues() { updateSpaceDelayKnobValues(); }
void SpaceDelayPage::updateSequencerUI() { updateSpaceDelaySequencerUI(); }
void SpaceDelayPage::randomizeKnobs() { randomizeSpaceDelayKnobs(); }
void SpaceDelayPage::randomizeSequencer() { randomizeSpaceDelaySequencer(); }
void SpaceDelayPage::randomizeAll() { randomizeSpaceDelayAll(); }
void SpaceDelayPage::updateFxAreaVisibility() { updateSpaceDelayFxAreaVisibility(); }
void SpaceDelayPage::updateStepAreaVisibility() { updateSpaceDelayStepAreaVisibility(); }

void SpaceDelayPage::showPage()
{
    setVisibleVec(pageGroup, true);
    DBG("[ROUTER] Showing Space Delay UI");
    
    // Restore UI state from processor/APVTS parameters
    auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("delayEnabled");
    spaceDelayFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true;
    if (spaceDelayFxPowerButton) {
        spaceDelayFxPowerButton->setToggleState(spaceDelayFxAreaEnabled, juce::dontSendNotification);
    }
    
    // Step power reflects processor sequencer enabled state
    spaceDelayStepAreaEnabled = processorRef.getSpaceDelaySeqState().enabled.load();
    if (spaceDelayStepPowerButton) {
        spaceDelayStepPowerButton->setToggleState(spaceDelayStepAreaEnabled, juce::dontSendNotification);
    }
    
    // Update All Steps toggle state
    if (spaceDelayAllStepsToggle) {
        spaceDelayAllStepsToggle->setToggleState(spaceDelayAllStepsEnabled, juce::dontSendNotification);
    }
    
    updateSpaceDelayFxAreaVisibility();
    updateSpaceDelayStepAreaVisibility();
    
    // Trigger initial value label updates
    for (int i = 0; i < 8; ++i) {
        if (spaceDelayKnobs[i]) {
            spaceDelayKnobs[i]->onValueChange();
        }
    }
    
    // Update sequencer UI to show first step as selected
    processorRef.setSpaceDelaySelectedStep(0);
    updateSpaceDelaySequencerUI();
}

void SpaceDelayPage::hidePage()
{
    setVisibleVec(pageGroup, false);
    DBG("[ROUTER] Hiding Space Delay UI");
}

// Implementation of specific methods
void SpaceDelayPage::updateSpaceDelayKnobValues()
{
    // Update knob values from processor parameters
    for (int i = 0; i < 8; ++i) {
        auto* param = processorRef.getAPVTS().getRawParameterValue(spaceDelayParameterIds[i]);
        if (param && spaceDelayKnobs[i]) {
            float value = param->load();
            spaceDelayKnobs[i]->setValue(value, juce::dontSendNotification);
            updateSpaceDelayKnobValueLabel(i, value);
        }
    }
}

void SpaceDelayPage::updateSpaceDelaySequencerUI()
{
    // Update step buttons based on current state
    for (int i = 0; i < 16; ++i) {
        bool selected = (i == spaceDelayUiSelectedStep);
        bool playing = processorRef.getSpaceDelaySeqState().currentStep.load() == i;
        bool enabled = true; // TODO: Get from processor
        updateSpaceDelaySequencerStepButton(i, selected, playing, enabled);
    }
}

void SpaceDelayPage::randomizeSpaceDelayKnobs()
{
    for (int i = 0; i < 8; ++i) {
        if (!spaceDelayKnobLocked[i] && spaceDelayKnobs[i]) {
            float randomValue = juce::Random::getSystemRandom().nextFloat();
            spaceDelayKnobs[i]->setValue(randomValue, juce::dontSendNotification);
            updateSpaceDelayKnobValueLabel(i, randomValue);
        }
    }
}

void SpaceDelayPage::randomizeSpaceDelaySequencer()
{
    // Randomize step states
    for (int i = 0; i < 16; ++i) {
        if (spaceDelayStepButtons[i]) {
            bool randomEnabled = juce::Random::getSystemRandom().nextBool();
            spaceDelayStepButtons[i]->setEnabledStep(randomEnabled);
        }
    }
}

void SpaceDelayPage::randomizeSpaceDelayAll()
{
    randomizeSpaceDelayKnobs();
    randomizeSpaceDelaySequencer();
}

void SpaceDelayPage::updateSpaceDelayFxAreaVisibility()
{
    float alpha = spaceDelayFxAreaEnabled ? 1.0f : 0.3f;
    
    // Grey knobs, labels, values, indicators, locks
    for (int i = 0; i < 8; ++i) {
        if (spaceDelayKnobs[i]) { spaceDelayKnobs[i]->setAlpha(alpha); spaceDelayKnobs[i]->setEnabled(spaceDelayFxAreaEnabled); }
        if (spaceDelayKnobLabels[i]) spaceDelayKnobLabels[i]->setAlpha(alpha);
        if (spaceDelayValueLabels[i]) spaceDelayValueLabels[i]->setAlpha(alpha);
        if (spaceDelayIndicatorBars[i]) spaceDelayIndicatorBars[i]->setAlpha(alpha);
        if (spaceDelayKnobLockButtons[i]) { 
            spaceDelayKnobLockButtons[i]->setEnabled(spaceDelayFxAreaEnabled);
            spaceDelayKnobLockButtons[i]->setAlpha(alpha);
        }
    }
    
    // Grey the power button itself when off
    if (spaceDelayFxPowerButton) spaceDelayFxPowerButton->setAlpha(spaceDelayFxAreaEnabled ? 1.0f : 0.3f);
}

void SpaceDelayPage::updateSpaceDelayStepAreaVisibility()
{
    float alpha = spaceDelayStepAreaEnabled ? 1.0f : 0.3f;
    
    // Grey step buttons
    for (int i = 0; i < 16; ++i) {
        if (spaceDelayStepButtons[i]) { 
            spaceDelayStepButtons[i]->setAlpha(alpha); 
            spaceDelayStepButtons[i]->setEnabled(spaceDelayStepAreaEnabled); 
        }
    }
    
    // Grey sequencer components
    if (spaceDelayStepAmountLabel) { spaceDelayStepAmountLabel->setAlpha(alpha); spaceDelayStepAmountLabel->setEnabled(spaceDelayStepAreaEnabled); }
    if (spaceDelayRateDropdown) { spaceDelayRateDropdown->setAlpha(alpha); spaceDelayRateDropdown->setEnabled(spaceDelayStepAreaEnabled); }
    if (spaceDelayStdToggle) { spaceDelayStdToggle->setAlpha(alpha); spaceDelayStdToggle->setEnabled(spaceDelayStepAreaEnabled); }
    if (spaceDelayStepTitle) spaceDelayStepTitle->setAlpha(alpha);
    if (spaceDelayStepDiceButton) { spaceDelayStepDiceButton->setAlpha(alpha); spaceDelayStepDiceButton->setEnabled(spaceDelayStepAreaEnabled); }
    
    // Grey the power button itself when off
    if (spaceDelayStepPowerButton) spaceDelayStepPowerButton->setAlpha(spaceDelayStepAreaEnabled ? 1.0f : 0.3f);
}

void SpaceDelayPage::updateSpaceDelayKnobValueLabel(int knobIndex, float value, const juce::String& suffix)
{
    if (knobIndex >= 0 && knobIndex < 8 && spaceDelayValueLabels[knobIndex]) {
        juce::String valueText = juce::String(value, 2) + suffix;
        spaceDelayValueLabels[knobIndex]->setText(valueText, juce::dontSendNotification);
    }
}

void SpaceDelayPage::updateSpaceDelaySequencerStepButton(int stepIndex, bool selected, bool playing, bool enabled)
{
    if (stepIndex >= 0 && stepIndex < 16 && spaceDelayStepButtons[stepIndex]) {
        auto* stepButton = dynamic_cast<StepButton*>(spaceDelayStepButtons[stepIndex].get());
        if (stepButton) {
            stepButton->setSelected(selected);
            stepButton->setPlaying(playing);
            stepButton->setEnabledStep(enabled);
        }
    }
}
