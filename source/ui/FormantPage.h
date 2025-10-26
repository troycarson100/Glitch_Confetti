#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Assets.h"

class FormantPage : public juce::Component
{
public:
    FormantPage(juce::AudioProcessorValueTreeState& apvts, UiAssets& assets);
    ~FormantPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refreshBarsFromSnapshot(int stepToShow);
    void animateBars(float deltaTime);
    
private:
    juce::AudioProcessorValueTreeState& apvts;
    UiAssets& assets;
    
    // Formant controls (8 knobs)
    LabeledKnobTile kVowelA, kVowelB, kMorph, kQ, kEmphasis, kGender, kVibrato, kMix;
    
    // Step sequencer
    StepSequencer stepSequencer;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> vowelAAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> vowelBAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> emphasisAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> genderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    
    // Vowel choice components
    juce::ComboBox vowelACombo, vowelBCombo;
    
    // Value labels for knobs
    std::array<juce::Label*, 8> valueLabels;
    
    // Indicator bars for step sequencer
    std::array<juce::Slider*, 8> indicatorBars;
    
    void setupKnobs();
    void setupComboBoxes();
    void setupValueLabels();
    void setupIndicatorBars();
    void updateValueLabels();
};
