#include "DelayPage.h"
#include "UiFlags.h"
#include <juce_audio_processors/juce_audio_processors.h>

LabeledKnobTile::LabeledKnobTile(const juce::String& name)
{
    addAndMakeVisible(knob);
    addAndMakeVisible(label);
    
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 100.0, 0.1);
    knob.setValue(50.0);
    
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&knob, false);
}

void LabeledKnobTile::resized()
{
    auto bounds = getLocalBounds();
    knob.setBounds(bounds.reduced(10));
}

StepSequencer::StepSequencer()
{
    for (int i = 0; i < 16; ++i)
    {
        auto button = std::make_unique<juce::TextButton>("Step " + juce::String(i + 1));
        button->setClickingTogglesState(true);
        button->setToggleState(i == 0, juce::dontSendNotification);
        addAndMakeVisible(*button);
        stepButtons.push_back(std::move(button));
    }
}

void StepSequencer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkblue);
    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 2);
}

void StepSequencer::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    int buttonSize = 30;
    int spacing = 5;
    
    for (int i = 0; i < 16; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        int x = bounds.getX() + col * (buttonSize + spacing);
        int y = bounds.getY() + row * (buttonSize + spacing);
        
        if (stepButtons[i])
            stepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
    }
}

void StepSequencer::setSelectedStep(int step)
{
    if (step >= 0 && step < 16)
    {
        selectedStep = step;
        for (int i = 0; i < 16; ++i)
        {
            if (stepButtons[i])
                stepButtons[i]->setToggleState(i == step, juce::dontSendNotification);
        }
    }
}

DelayPage::DelayPage(juce::AudioProcessorValueTreeState& apvts, UiAssets& assets)
    : apvts(apvts), assets(assets),
      kTime("Time"), kFeedback("Feedback"), kWowDepth("Wow Depth"), kWowRate("Wow Rate"),
      kDrive("Drive"), kHighCut("High Cut"), kLowCut("Low Cut"), kMix("Mix")
{
    // Add all controls
    addAndMakeVisible(kTime);
    addAndMakeVisible(kFeedback);
    addAndMakeVisible(kWowDepth);
    addAndMakeVisible(kWowRate);
    addAndMakeVisible(kDrive);
    addAndMakeVisible(kHighCut);
    addAndMakeVisible(kLowCut);
    addAndMakeVisible(kMix);
    addAndMakeVisible(stepSequencer);
    
    // Set initial bounds to make sure components are visible
    setBounds(0, 0, 800, 600);
    
    DBG("[DelayPage] Created with " << getNumChildComponents() << " child components");
    
    // Create attachments
    timeAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "timeMs",     kTime.knob);
    feedbackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "feedback",   kFeedback.knob);
    wowDepthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wowDepth",   kWowDepth.knob);
    wowRateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wowRate",    kWowRate.knob);
    driveAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "saturation", kDrive.knob);
    hiCutAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "highCut",    kHighCut.knob);
    lowCutAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lowCut",     kLowCut.knob);
    mixAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix",        kMix.knob);
}

DelayPage::~DelayPage()
{
    // Attachments destroyed first (before sliders)
    timeAtt.reset();
    feedbackAtt.reset();
    wowDepthAtt.reset();
    wowRateAtt.reset();
    driveAtt.reset();
    hiCutAtt.reset();
    lowCutAtt.reset();
    mixAtt.reset();
}

void DelayPage::paint(juce::Graphics& g)
{
    if (assets.effectPlate)
        assets.effectPlate->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);
    else {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::darkgrey); 
        g.drawRect(getLocalBounds(), 2);
    }
}

void DelayPage::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    
    // Layout knobs in 2 rows of 4
    int knobSize = 80;
    int spacing = 20;
    int startY = bounds.getY() + 20;
    
    // First row
    kTime.setBounds(bounds.getX(), startY, knobSize, knobSize + 20);
    kFeedback.setBounds(bounds.getX() + knobSize + spacing, startY, knobSize, knobSize + 20);
    kWowDepth.setBounds(bounds.getX() + 2 * (knobSize + spacing), startY, knobSize, knobSize + 20);
    kWowRate.setBounds(bounds.getX() + 3 * (knobSize + spacing), startY, knobSize, knobSize + 20);
    
    // Second row
    kDrive.setBounds(bounds.getX(), startY + knobSize + 40, knobSize, knobSize + 20);
    kHighCut.setBounds(bounds.getX() + knobSize + spacing, startY + knobSize + 40, knobSize, knobSize + 20);
    kLowCut.setBounds(bounds.getX() + 2 * (knobSize + spacing), startY + knobSize + 40, knobSize, knobSize + 20);
    kMix.setBounds(bounds.getX() + 3 * (knobSize + spacing), startY + knobSize + 40, knobSize, knobSize + 20);
    
    // Step sequencer at bottom
    stepSequencer.setBounds(bounds.getX(), startY + 2 * (knobSize + 40) + 20, bounds.getWidth(), 150);
    
    // Force repaint to ensure visibility
    repaint();
    
    DBG("[DelayPage] Resized to " << getWidth() << "x" << getHeight() << " with " << getNumChildComponents() << " children");
}

void DelayPage::refreshBarsFromSnapshot(int stepToShow)
{
    // TODO: Implement bar refresh from step snapshot
    stepSequencer.setSelectedStep(stepToShow);
}

void DelayPage::animateBars(float deltaTime)
{
    // TODO: Implement bar animation
}
