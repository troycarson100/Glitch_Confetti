#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/StepSequencer.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    
    // GlitchConfetti parameter controls
    juce::Slider partySlider;
    juce::Label partyLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> partyAttachment;
    
    juce::Slider stepsSlider;
    juce::Label stepsLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttachment;
    
    juce::Slider densitySlider;
    juce::Label densityLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> densityAttachment;
    
    juce::Slider revPcSlider;
    juce::Label revPcLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revPcAttachment;
    
    juce::Slider flickPcSlider;
    juce::Label flickPcLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flickPcAttachment;
    
    juce::Slider humanizeSlider;
    juce::Label humanizeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humanizeAttachment;
    
    juce::Slider mixSlider;
    juce::Label mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    
    juce::Slider outDbSlider;
    juce::Label outDbLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outDbAttachment;
    
    // Peak meters
    struct PeakMeter : public juce::Component
    {
        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            
            // Background
            g.setColour(juce::Colour(0xff2a2a2a));
            g.fillRoundedRectangle(area, 2.0f);
            
            // Peak level
            if (level > 0.001f) // -60dB threshold
            {
                float dbLevel = juce::Decibels::gainToDecibels(level);
                float normalizedLevel = juce::jlimit(0.0f, 1.0f, (dbLevel + 60.0f) / 60.0f); // -60dB to 0dB
                
                auto levelArea = area.withHeight(area.getHeight() * normalizedLevel);
                levelArea = levelArea.withY(area.getBottom() - levelArea.getHeight());
                
                // Color based on level
                juce::Colour meterColour = juce::Colours::green;
                if (dbLevel > -6.0f) meterColour = juce::Colours::red;
                else if (dbLevel > -12.0f) meterColour = juce::Colours::yellow;
                
                g.setColour(meterColour);
                g.fillRoundedRectangle(levelArea, 2.0f);
            }
        }
        
        void setLevel(float newLevel) { level = newLevel; repaint(); }
        
    private:
        float level = 0.0f;
    };
    
    PeakMeter inputMeterL, inputMeterR, outputMeterL, outputMeterR;
    juce::Label inputLabel, outputLabel;
    
    // Preset selection
    juce::ComboBox presetComboBox;
    juce::Label presetLabel;
    juce::TextButton savePresetButton;
    
    // Step sequencer
    std::unique_ptr<StepSequencer> stepSequencer;
    
    // Timer callback for meter updates
    void timerCallback() override
    {
        inputMeterL.setLevel(processorRef.getInputPeakL());
        inputMeterR.setLevel(processorRef.getInputPeakR());
        outputMeterL.setLevel(processorRef.getOutputPeakL());
        outputMeterR.setLevel(processorRef.getOutputPeakR());
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
