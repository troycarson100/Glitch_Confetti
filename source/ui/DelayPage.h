#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Assets.h"

class LabeledKnobTile : public juce::Component
{
public:
    juce::Slider knob;
    juce::Label label;
    
    LabeledKnobTile(const juce::String& name);
    void resized() override;
};

class StepSequencer : public juce::Component
{
public:
    StepSequencer();
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    int getSelectedStep() const { return selectedStep; }
    void setSelectedStep(int step);
    
private:
    int selectedStep = 0;
    std::vector<std::unique_ptr<juce::Button>> stepButtons;
};

class DelayPage : public juce::Component
{
public:
    DelayPage(juce::AudioProcessorValueTreeState& apvts, UiAssets& assets);
    ~DelayPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refreshBarsFromSnapshot(int stepToShow);
    void animateBars(float deltaTime);
    
private:
    juce::AudioProcessorValueTreeState& apvts;
    UiAssets& assets;
    
    // controls (destroyed last)
    LabeledKnobTile kTime, kFeedback, kWowDepth, kWowRate, kDrive, kHighCut, kLowCut, kMix;
public:
    StepSequencer stepSequencer;
    
    // attachments (destroyed first)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAtt, feedbackAtt, wowDepthAtt, wowRateAtt, driveAtt, hiCutAtt, lowCutAtt, mixAtt;
};
