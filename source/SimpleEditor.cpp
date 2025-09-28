#include "SimpleProcessor.h"
#include "SimpleEditor.h"

SimpleEditor::SimpleEditor (SimpleProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set up the gain slider
    gainSlider.setSliderStyle(juce::Slider::Rotary);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(gainSlider);
    
    gainLabel.setText("GAIN", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainLabel);
    
    // Attach the slider to the parameter
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "gain", gainSlider);

    setSize (400, 300);
}

SimpleEditor::~SimpleEditor()
{
}

void SimpleEditor::paint (juce::Graphics& g)
{
    // Dark background
    g.fillAll (juce::Colour(0xFF1E1E1E));
    
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawFittedText ("STEPPER", getLocalBounds().removeFromTop(60), 
                      juce::Justification::centred, 1);
}

void SimpleEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(80); // Title area
    
    auto sliderArea = bounds.removeFromTop(120);
    gainSlider.setBounds(sliderArea.reduced(40));
    
    gainLabel.setBounds(bounds.removeFromTop(30));
}