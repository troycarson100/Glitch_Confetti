#pragma once

#include "SimpleProcessor.h"

class SimpleEditor : public juce::AudioProcessorEditor
{
public:
    SimpleEditor (SimpleProcessor&);
    ~SimpleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SimpleProcessor& audioProcessor;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEditor)
};