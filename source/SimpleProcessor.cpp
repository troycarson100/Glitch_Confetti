#include "SimpleProcessor.h"
#include "SimpleEditor.h"

SimpleProcessor::SimpleProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       valueTreeState (*this, nullptr, "Parameters", 
           juce::AudioProcessorValueTreeState::ParameterLayout {
               std::make_unique<juce::AudioParameterFloat> ("gain", "Gain", -60.0f, 20.0f, 0.0f)
           })
{
    gainParam = dynamic_cast<juce::AudioParameterFloat*>(valueTreeState.getParameter("gain"));
}

SimpleProcessor::~SimpleProcessor()
{
}

const juce::String SimpleProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleProcessor::getNumPrograms()
{
    return 1;
}

int SimpleProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleProcessor::getProgramName (int index)
{
    return {};
}

void SimpleProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void SimpleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void SimpleProcessor::releaseResources()
{
}

bool SimpleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void SimpleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    float gainValue = gainParam->get();
    float gainMultiplier = juce::Decibels::decibelsToGain(gainValue);
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.applyGain(channel, 0, buffer.getNumSamples(), gainMultiplier);
    }
}

bool SimpleProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SimpleProcessor::createEditor()
{
    return new SimpleEditor (*this);
}

void SimpleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = valueTreeState.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SimpleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (valueTreeState.state.getType()))
            valueTreeState.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleProcessor();
}