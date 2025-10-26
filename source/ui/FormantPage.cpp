#include "FormantPage.h"
#include "UiFlags.h"
#include <juce_audio_processors/juce_audio_processors.h>

FormantPage::FormantPage(juce::AudioProcessorValueTreeState& apvts, UiAssets& assets)
    : apvts(apvts), assets(assets),
      kVowelA("Vowel A"), kVowelB("Vowel B"), kMorph("Morph"), kQ("Q"),
      kEmphasis("Emphasis"), kGender("Gender"), kVibrato("Vibrato"), kMix("Mix")
{
    setupKnobs();
    setupComboBoxes();
    setupValueLabels();
    setupIndicatorBars();
    
    addAndMakeVisible(stepSequencer);
    
    // Add all knob tiles
    addAndMakeVisible(kVowelA);
    addAndMakeVisible(kVowelB);
    addAndMakeVisible(kMorph);
    addAndMakeVisible(kQ);
    addAndMakeVisible(kEmphasis);
    addAndMakeVisible(kGender);
    addAndMakeVisible(kVibrato);
    addAndMakeVisible(kMix);
    
    // Add combo boxes
    addAndMakeVisible(vowelACombo);
    addAndMakeVisible(vowelBCombo);
    
    // Create parameter attachments
    vowelAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "vowelA", vowelACombo);
    vowelBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "vowelB", vowelBCombo);
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "morph", kMorph.knob);
    qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "q", kQ.knob);
    emphasisAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "emphasis", kEmphasis.knob);
    genderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "gender", kGender.knob);
    vibratoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "vibDepth", kVibrato.knob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "mix", kMix.knob);
}

FormantPage::~FormantPage() = default;

void FormantPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    
    // Draw background layers
    if (assets.formBackgroundTab1.isValid())
    {
        g.drawImage(assets.formBackgroundTab1, getLocalBounds().toFloat());
    }
    
    // Draw title
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("Formant", getLocalBounds().removeFromTop(50), juce::Justification::centred);
    
    // Draw step sequencer title
    g.setFont(16.0f);
    g.drawText("Formant Mod", stepSequencer.getBounds().removeFromTop(30), juce::Justification::centred);
}

void FormantPage::resized()
{
    auto bounds = getLocalBounds();
    
    // Title area
    auto titleArea = bounds.removeFromTop(50);
    
    // Knob grid (2 rows of 4 knobs)
    auto knobArea = bounds.removeFromTop(200);
    int knobSize = 80;
    int spacing = 20;
    
    // Top row
    auto topRow = knobArea.removeFromTop(100);
    kVowelA.setBounds(topRow.removeFromLeft(knobSize).reduced(spacing/2));
    kVowelB.setBounds(topRow.removeFromLeft(knobSize).reduced(spacing/2));
    kMorph.setBounds(topRow.removeFromLeft(knobSize).reduced(spacing/2));
    kQ.setBounds(topRow.removeFromLeft(knobSize).reduced(spacing/2));
    
    // Bottom row
    auto bottomRow = knobArea.removeFromTop(100);
    kEmphasis.setBounds(bottomRow.removeFromLeft(knobSize).reduced(spacing/2));
    kGender.setBounds(bottomRow.removeFromLeft(knobSize).reduced(spacing/2));
    kVibrato.setBounds(bottomRow.removeFromLeft(knobSize).reduced(spacing/2));
    kMix.setBounds(bottomRow.removeFromLeft(knobSize).reduced(spacing/2));
    
    // Combo boxes (overlay on Vowel knobs)
    vowelACombo.setBounds(kVowelA.getBounds().reduced(10));
    vowelBCombo.setBounds(kVowelB.getBounds().reduced(10));
    
    // Step sequencer
    stepSequencer.setBounds(bounds.reduced(20));
    
    // Update value labels
    updateValueLabels();
}

void FormantPage::refreshBarsFromSnapshot(int stepToShow)
{
    // This would be called to update the indicator bars based on step sequencer data
    // Implementation depends on how the step sequencer data is structured
}

void FormantPage::animateBars(float deltaTime)
{
    // This would be called to animate the indicator bars
    // Implementation depends on the animation requirements
}

void FormantPage::setupKnobs()
{
    // Set up knob ranges and styles
    kMorph.knob.setRange(0.0, 1.0, 0.01);
    kMorph.knob.setValue(0.0);
    
    kQ.knob.setRange(0.3, 20.0, 0.1);
    kQ.knob.setValue(6.0);
    
    kEmphasis.knob.setRange(0.0, 12.0, 0.1);
    kEmphasis.knob.setValue(6.0);
    
    kGender.knob.setRange(0.5, 2.0, 0.01);
    kGender.knob.setValue(1.0);
    
    kVibrato.knob.setRange(0.0, 30.0, 0.1);
    kVibrato.knob.setValue(5.0);
    
    kMix.knob.setRange(0.0, 1.0, 0.01);
    kMix.knob.setValue(0.5);
    
    // Vowel knobs are handled by combo boxes, but set up the knobs for display
    kVowelA.knob.setRange(0.0, 4.0, 1.0);
    kVowelA.knob.setValue(0.0);
    kVowelA.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    kVowelA.knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    
    kVowelB.knob.setRange(0.0, 4.0, 1.0);
    kVowelB.knob.setValue(1.0);
    kVowelB.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    kVowelB.knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

void FormantPage::setupComboBoxes()
{
    // Set up vowel choice combo boxes
    juce::StringArray vowelChoices = {"A", "E", "I", "O", "U"};
    
    vowelACombo.addItemList(vowelChoices, 1);
    vowelACombo.setSelectedId(1); // A
    vowelACombo.setColour(juce::ComboBox::backgroundColourId, juce::Colours::darkgrey);
    vowelACombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    
    vowelBCombo.addItemList(vowelChoices, 1);
    vowelBCombo.setSelectedId(2); // E
    vowelBCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colours::darkgrey);
    vowelBCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
}

void FormantPage::setupValueLabels()
{
    // Create value labels for each knob
    for (int i = 0; i < 8; ++i)
    {
        valueLabels[i] = new juce::Label();
        valueLabels[i]->setJustificationType(juce::Justification::centred);
        valueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(valueLabels[i]);
    }
}

void FormantPage::setupIndicatorBars()
{
    // Create indicator bars for step sequencer
    for (int i = 0; i < 8; ++i)
    {
        indicatorBars[i] = new juce::Slider();
        indicatorBars[i]->setSliderStyle(juce::Slider::LinearVertical);
        indicatorBars[i]->setRange(0.0, 1.0, 0.01);
        indicatorBars[i]->setValue(0.5);
        indicatorBars[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(indicatorBars[i]);
    }
}

void FormantPage::updateValueLabels()
{
    // Update value labels based on current knob values
    if (valueLabels[0]) valueLabels[0]->setText("A", juce::dontSendNotification);
    if (valueLabels[1]) valueLabels[1]->setText("E", juce::dontSendNotification);
    if (valueLabels[2]) valueLabels[2]->setText(juce::String(kMorph.knob.getValue(), 2), juce::dontSendNotification);
    if (valueLabels[3]) valueLabels[3]->setText(juce::String(kQ.knob.getValue(), 1), juce::dontSendNotification);
    if (valueLabels[4]) valueLabels[4]->setText(juce::String(kEmphasis.knob.getValue(), 1) + "dB", juce::dontSendNotification);
    if (valueLabels[5]) valueLabels[5]->setText(juce::String(kGender.knob.getValue(), 2), juce::dontSendNotification);
    if (valueLabels[6]) valueLabels[6]->setText(juce::String(kVibrato.knob.getValue(), 1) + "c", juce::dontSendNotification);
    if (valueLabels[7]) valueLabels[7]->setText(juce::String(kMix.knob.getValue() * 100, 0) + "%", juce::dontSendNotification);
}
