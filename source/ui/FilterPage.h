#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Assets.h"
#include "DelayPage.h"  // For LabeledKnobTile and StepSequencer

class FilterPage : public juce::Component
{
public:
    FilterPage(juce::AudioProcessorValueTreeState& apvts, UiAssets& assets);
    ~FilterPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refreshBarsFromSnapshot(int stepToShow);
    void animateBars(float deltaTime);
    
    // Update knob labels based on filter type
    void updateKnobLabels(int filterType);
    
private:
    juce::AudioProcessorValueTreeState& apvts;
    UiAssets& assets;
    
    // controls (destroyed last) - 8 knobs matching DelayPage layout
    // Type and Slope use ComboBox (Choice params), others use Sliders
    juce::ComboBox typeCombo;
    juce::Label typeLabel;
    juce::ComboBox slopeCombo;
    juce::Label slopeLabel;
    LabeledKnobTile kCutoff, kRes, kDrive, kSpread, kKeyTrack, kMix;
public:
    StepSequencer stepSequencer;
    
    // attachments (destroyed first)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAtt, resAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeAtt;  // Choice parameter
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAtt, spreadAtt, keytrackAtt, mixAtt;
};

