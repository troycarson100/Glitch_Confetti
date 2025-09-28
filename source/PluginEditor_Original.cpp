#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Setup GlitchConfetti parameter controls
    auto& apvts = processorRef.getAPVTS();
    
    // Party slider
    addAndMakeVisible(partySlider);
    partySlider.setSliderStyle(juce::Slider::Rotary);
    partySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    partyLabel.setText("Party", juce::dontSendNotification);
    partyLabel.attachToComponent(&partySlider, false);
    partyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "party", partySlider);
    
    // Steps slider
    addAndMakeVisible(stepsSlider);
    stepsSlider.setSliderStyle(juce::Slider::Rotary);
    stepsSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    stepsLabel.setText("Steps", juce::dontSendNotification);
    stepsLabel.attachToComponent(&stepsSlider, false);
    stepsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "steps", stepsSlider);
    
    // Density slider
    addAndMakeVisible(densitySlider);
    densitySlider.setSliderStyle(juce::Slider::Rotary);
    densitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    densityLabel.setText("Density", juce::dontSendNotification);
    densityLabel.attachToComponent(&densitySlider, false);
    densityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "density", densitySlider);
    
    // Reverse % slider
    addAndMakeVisible(revPcSlider);
    revPcSlider.setSliderStyle(juce::Slider::Rotary);
    revPcSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    revPcLabel.setText("Reverse", juce::dontSendNotification);
    revPcLabel.attachToComponent(&revPcSlider, false);
    revPcAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "rev_pc", revPcSlider);
    
    // Flick % slider
    addAndMakeVisible(flickPcSlider);
    flickPcSlider.setSliderStyle(juce::Slider::Rotary);
    flickPcSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    flickPcLabel.setText("Flick", juce::dontSendNotification);
    flickPcLabel.attachToComponent(&flickPcSlider, false);
    flickPcAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "flick_pc", flickPcSlider);
    
    // Humanize slider
    addAndMakeVisible(humanizeSlider);
    humanizeSlider.setSliderStyle(juce::Slider::Rotary);
    humanizeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    humanizeLabel.setText("Humanize", juce::dontSendNotification);
    humanizeLabel.attachToComponent(&humanizeSlider, false);
    humanizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "humanize", humanizeSlider);
    
    // Mix slider
    addAndMakeVisible(mixSlider);
    mixSlider.setSliderStyle(juce::Slider::Rotary);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.attachToComponent(&mixSlider, false);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider);
    
    // Output dB slider
    addAndMakeVisible(outDbSlider);
    outDbSlider.setSliderStyle(juce::Slider::Rotary);
    outDbSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    outDbLabel.setText("Output", juce::dontSendNotification);
    outDbLabel.attachToComponent(&outDbSlider, false);
    outDbAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "out_db", outDbSlider);

    // Setup peak meters
    addAndMakeVisible(inputMeterL);
    addAndMakeVisible(inputMeterR);
    addAndMakeVisible(outputMeterL);
    addAndMakeVisible(outputMeterR);
    addAndMakeVisible(inputLabel);
    addAndMakeVisible(outputLabel);
    
    inputLabel.setText("IN", juce::dontSendNotification);
    inputLabel.setJustificationType(juce::Justification::centred);
    outputLabel.setText("OUT", juce::dontSendNotification);
    outputLabel.setJustificationType(juce::Justification::centred);
    
    // Setup preset combo box
    addAndMakeVisible(presetComboBox);
    addAndMakeVisible(presetLabel);
    
    presetLabel.setText("Presets", juce::dontSendNotification);
    presetLabel.attachToComponent(&presetComboBox, true);
    
    // Populate preset combo box
    auto presetNames = processorRef.getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetComboBox.addItem(presetNames[i], i + 1);
    }
    
    presetComboBox.onChange = [this]() {
        int selectedIndex = presetComboBox.getSelectedItemIndex();
        if (selectedIndex >= 0)
        {
            processorRef.loadPreset(selectedIndex);
        }
    };
    
    // Setup save preset button
    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("Save Preset");
    savePresetButton.onClick = [this]() {
        // Create a custom AlertWindow with text input
        auto alertWindow = std::make_unique<juce::AlertWindow>("Save Preset", 
                                                               "Enter preset name:", 
                                                               juce::MessageBoxIconType::NoIcon);
        alertWindow->addTextEditor("presetName", "My Preset", "Preset name:");
        alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        
        auto* window = alertWindow.release();
        window->enterModalState(true, 
            juce::ModalCallbackFunction::create([this, window](int result) {
                if (result == 1) // Save button
                {
                    auto presetName = window->getTextEditorContents("presetName");
                    if (presetName.isNotEmpty())
                    {
                        processorRef.saveCurrentAsUserPreset(presetName);
                        
                        // Refresh the preset combo box
                        presetComboBox.clear();
                        auto newPresetNames = processorRef.getPresetNames();
                        for (int i = 0; i < newPresetNames.size(); ++i)
                        {
                            presetComboBox.addItem(newPresetNames[i], i + 1);
                        }
                    }
                }
                delete window;
            }), true);
    };
    
    // Create step sequencer
    stepSequencer = std::make_unique<StepSequencer>(apvts, [this]() {
        return processorRef.getCurrentStep();
    }, [this]() {
        processorRef.requestSequencerReset();
    });
    addAndMakeVisible(*stepSequencer);
    
    DBG("StepSequencer created and made visible");
    
    // Start meter timer at 30fps
    startTimer(33);

    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (800, 550);
    setResizable(true, true);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Fill background with dark color for glitch aesthetic
    g.fillAll (juce::Colour (0xff1e1e1e));

    auto area = getLocalBounds();
    
    // Title area
    auto titleArea = area.removeFromTop(50);
    g.setColour (juce::Colours::lightgreen);
    g.setFont (24.0f);
    g.drawText ("GLITCH CONFETTI", titleArea, juce::Justification::centred, false);
    
    // Subtitle
    auto subtitleArea = area.removeFromTop(30);
    g.setColour (juce::Colours::lightgrey);
    g.setFont (12.0f);
    g.drawText ("Step-Synced Glitch Effects", subtitleArea, juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Meters at the sides first
    const int meterWidth = 12;
    const int meterHeight = 120;
    const int meterMargin = 20;
    
    // Input meters (left side)
    auto inputArea = bounds.removeFromLeft(meterWidth * 2 + 4 + meterMargin);
    inputArea.removeFromTop(80); // Skip title area
    auto inputMeterArea = inputArea.removeFromTop(meterHeight);
    inputMeterArea.removeFromLeft(10); // Left margin
    inputMeterL.setBounds(inputMeterArea.removeFromLeft(meterWidth));
    inputMeterArea.removeFromLeft(4); // Small gap
    inputMeterR.setBounds(inputMeterArea.removeFromLeft(meterWidth));
    inputLabel.setBounds(inputArea.removeFromTop(20).withTrimmedLeft(10));
    
    // Output meters (right side) 
    auto outputArea = bounds.removeFromRight(meterWidth * 2 + 4 + meterMargin);
    outputArea.removeFromTop(80); // Skip title area
    auto outputMeterArea = outputArea.removeFromTop(meterHeight);
    outputMeterL.setBounds(outputMeterArea.removeFromLeft(meterWidth));
    outputMeterArea.removeFromLeft(4); // Small gap
    outputMeterR.setBounds(outputMeterArea.removeFromLeft(meterWidth));
    outputLabel.setBounds(outputArea.removeFromTop(20));
    
    // Main control area
    auto area = bounds;
    
    // Skip title area
    area.removeFromTop(80);
    
    // Create a grid layout for controls
    const int sliderSize = 80;
    const int margin = 20;
    
    // Top row: Party, Steps, Density, Reverse
    auto topRow = area.removeFromTop(sliderSize + 40);
    topRow.removeFromLeft(margin);
    topRow.removeFromRight(margin);
    
    auto topSliderWidth = topRow.getWidth() / 4;
    partySlider.setBounds(topRow.removeFromLeft(topSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    stepsSlider.setBounds(topRow.removeFromLeft(topSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    densitySlider.setBounds(topRow.removeFromLeft(topSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    revPcSlider.setBounds(topRow.removeFromLeft(topSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    
    // Bottom row: Flick, Humanize, Mix, Output
    area.removeFromTop(margin);
    auto bottomRow = area.removeFromTop(sliderSize + 40);
    bottomRow.removeFromLeft(margin);
    bottomRow.removeFromRight(margin);
    
    auto bottomSliderWidth = bottomRow.getWidth() / 4;
    flickPcSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    humanizeSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    mixSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    outDbSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).withSizeKeepingCentre(sliderSize, sliderSize));
    
    // Step sequencer in middle with proper proportional sizing
    area.removeFromTop(margin);
    auto sequencerArea = area.removeFromTop(140);
    sequencerArea = sequencerArea.reduced(margin, 0);
    stepSequencer->setBounds(sequencerArea);
    
    // Preset area at bottom
    area.removeFromTop(margin);
    auto presetArea = area.removeFromBottom(25);
    auto saveButtonArea = presetArea.removeFromRight(100);
    savePresetButton.setBounds(saveButtonArea.withSizeKeepingCentre(90, 25));
    presetArea.removeFromRight(10); // Small gap
    presetComboBox.setBounds(presetArea.withSizeKeepingCentre(200, 25));
    
    // Inspector button at bottom
    area.removeFromTop(margin);
    inspectButton.setBounds(area.removeFromBottom(30).withSizeKeepingCentre(120, 25));
}
