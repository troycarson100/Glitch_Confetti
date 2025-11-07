#include "FilterPage.h"
#include "DelayPage.h"  // For LabeledKnobTile and StepSequencer
#include "UiFlags.h"
#include <juce_audio_processors/juce_audio_processors.h>

FilterPage::FilterPage(juce::AudioProcessorValueTreeState& apvts, UiAssets& assets)
    : apvts(apvts), assets(assets),
      kCutoff("Cutoff"), kRes("Resonance"),
      kDrive("Drive"), kSpread("Spread"), kKeyTrack("Key Track"), kMix("Mix")
{
    // Setup Type ComboBox
    typeCombo.addItem("LP", 1);
    typeCombo.addItem("HP", 2);
    typeCombo.addItem("BP", 3);
    typeCombo.addItem("Comb-", 4);
    typeCombo.addItem("Comb+", 5);
    typeCombo.setSelectedId(1); // Default LP
    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.attachToComponent(&typeCombo, false);
    
    // Setup Slope ComboBox
    slopeCombo.addItem("12dB", 1);
    slopeCombo.addItem("24dB", 2);
    slopeCombo.setSelectedId(2); // Default 24dB
    slopeLabel.setText("Slope", juce::dontSendNotification);
    slopeLabel.setJustificationType(juce::Justification::centred);
    slopeLabel.attachToComponent(&slopeCombo, false);
    
    // Add all controls
    addAndMakeVisible(typeCombo);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(kCutoff);
    addAndMakeVisible(kRes);
    addAndMakeVisible(slopeCombo);
    addAndMakeVisible(slopeLabel);
    addAndMakeVisible(kDrive);
    addAndMakeVisible(kSpread);
    addAndMakeVisible(kKeyTrack);
    addAndMakeVisible(kMix);
    addAndMakeVisible(stepSequencer);
    
    // Set initial bounds to make sure components are visible
    setBounds(0, 0, 800, 600);
    
    // Update knob labels based on initial type (default LP)
    updateKnobLabels(0); // 0 = LP
    
    // Listen for type changes to update labels
    typeCombo.onChange = [this] {
        int type = typeCombo.getSelectedId() - 1; // 0-4
        updateKnobLabels(type);
    };
    
    DBG("[FilterPage] Created with " << getNumChildComponents() << " child components");
    
    // Create attachments
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "fType", typeCombo);
    cutoffAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "cutoff", kCutoff.knob);
    resAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "res", kRes.knob);
    slopeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "slope", slopeCombo);
    driveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "filterDrive", kDrive.knob);
    spreadAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "spread", kSpread.knob);
    keytrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "keytrack", kKeyTrack.knob);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "filterMix", kMix.knob);
}

FilterPage::~FilterPage()
{
    // Attachments destroyed first (before sliders)
    typeAtt.reset();
    cutoffAtt.reset();
    resAtt.reset();
    slopeAtt.reset();
    driveAtt.reset();
    spreadAtt.reset();
    keytrackAtt.reset();
    mixAtt.reset();
}

void FilterPage::paint(juce::Graphics& g)
{
    if (assets.effectPlate)
        assets.effectPlate->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);
    else {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::darkgrey); 
        g.drawRect(getLocalBounds(), 2);
    }
}

void FilterPage::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    
    // Layout knobs in 2 rows of 4 (same as DelayPage)
    int knobSize = 80;
    int spacing = 20;
    int startY = bounds.getY() + 20;
    
    // First row - Type ComboBox, then knobs
    typeCombo.setBounds(bounds.getX(), startY, knobSize, knobSize);
    kCutoff.setBounds(bounds.getX() + knobSize + spacing, startY, knobSize, knobSize + 20);
    kRes.setBounds(bounds.getX() + 2 * (knobSize + spacing), startY, knobSize, knobSize + 20);
    slopeCombo.setBounds(bounds.getX() + 3 * (knobSize + spacing), startY, knobSize, knobSize);
    
    // Second row
    kDrive.setBounds(bounds.getX(), startY + knobSize + 40, knobSize, knobSize + 20);
    kSpread.setBounds(bounds.getX() + knobSize + spacing, startY + knobSize + 40, knobSize, knobSize + 20);
    kKeyTrack.setBounds(bounds.getX() + 2 * (knobSize + spacing), startY + knobSize + 40, knobSize, knobSize + 20);
    kMix.setBounds(bounds.getX() + 3 * (knobSize + spacing), startY + knobSize + 40, knobSize, knobSize + 20);
    
    // Step sequencer at bottom
    stepSequencer.setBounds(bounds.getX(), startY + 2 * (knobSize + 40) + 20, bounds.getWidth(), 150);
    
    // Force repaint to ensure visibility
    repaint();
    
    DBG("[FilterPage] Resized to " << getWidth() << "x" << getHeight() << " with " << getNumChildComponents() << " children");
}

void FilterPage::refreshBarsFromSnapshot(int stepToShow)
{
    // TODO: Implement bar refresh from step snapshot
    stepSequencer.setSelectedStep(stepToShow);
}

void FilterPage::animateBars(float deltaTime)
{
    // TODO: Implement bar animation
}

void FilterPage::updateKnobLabels(int filterType)
{
    // filterType: 0=LP, 1=HP, 2=BP, 3=Comb-, 4=Comb+
    if (filterType <= 2) {
        // LP/HP/BP modes
        kCutoff.label.setText("Cutoff", juce::dontSendNotification);
        kRes.label.setText("Resonance", juce::dontSendNotification);
        slopeLabel.setText("Slope", juce::dontSendNotification);
    } else {
        // Comb modes
        kCutoff.label.setText("Tune", juce::dontSendNotification);
        kRes.label.setText("Feedback", juce::dontSendNotification);
        slopeLabel.setText("Depth", juce::dontSendNotification);
    }
    repaint();
}

