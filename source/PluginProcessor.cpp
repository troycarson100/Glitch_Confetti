#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    valueTreeState(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    // Initialize sequencer state
    seq.enabled.store(false);
    seq.stepsUsed.store(16);
    seq.divisionIndex.store(3); // 1/8 default
    seq.playingStep.store(0);
    
    // Initialize UI state
    uiSelectedStep.store(0);
    
    // Initialize transport cache
    transportCache.valid.store(false);
    transportCache.playing.store(false);
    transportCache.bpm.store(120.0);
    transportCache.tsNum.store(4);
    transportCache.tsDen.store(4);
    transportCache.ppq.store(0.0);
    transportCache.barStartPpq.store(0.0);
    
    // Initialize DSP
    dspSampleRate = 44100.0;
    spaceDelay.prepare(dspSampleRate, 512);
}


juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("timeMs", "Time", 10.0f, 2000.0f, 250.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepth", "Wow Depth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("wowRate", "Wow Rate", 0.1f, 8.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("saturation", "Saturation", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("highCut", "High Cut", 1000.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lowCut", "Low Cut", 20.0f, 2000.0f, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 0.5f));
    
    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
}

const juce::String PluginProcessor::getProgramName (int index)
{
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dspSampleRate = sampleRate;
    spaceDelay.prepare(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals nd;
    
    // Update transport cache
    updateTransportCache(getPlayHead(), buffer.getNumSamples());
    
    // Update playing step from transport
    updatePlayingStepFromTransport();
    
#if GC_SAFE_DELAY_ONLY
    const FxType fx = FxType::Delay;
#else
    const FxType fx = currentFx;
#endif

    // Choose which step snapshot drives DSP (playing when seq ON, else selected)
    const int stepForDSP = (seq.enabled.load() && transportCache.playing.load())
                         ? seq.playingStep.load()
                         : uiSelectedStep.load();

    const StepSnapshot snap = getSafeSnapshot(stepForDSP);
    applySnapshotTargets(snap);

    // Clear any unused output channels
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

        // Process audio
        switch (fx)
        {
            case FxType::Delay:
                spaceDelay.process(buffer, buffer.getNumSamples());
                break;
        case FxType::Crunch:
            // fxCrunch.process(buffer, buffer.getNumSamples());
            break;
        case FxType::Pitch:
            // fxPitch.process(buffer, buffer.getNumSamples());
            break;
        case FxType::AutoPan:
            // fxPan.process(buffer, buffer.getNumSamples());
            break;
        default:
            break;
    }

    // Debug logging
    static int c = 0;
    if ((++c & 127) == 0) { // Log every 128 blocks
        DBG("[Delay] timeMs=" << snap.delay.timeMs << " fb=" << snap.delay.feedback << " mix=" << snap.delay.mix);
        DBG("[FX] fx=Delay  step=" << stepForDSP);
        DBG("[SEQ] on=" << (seq.enabled.load() ? "true" : "false") << " playing=" << (transportCache.playing.load() ? "true" : "false") << " step=" << seq.playingStep.load());
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

//==============================================================================
void PluginProcessor::updateTransportCache(juce::AudioPlayHead* playHead, int numSamples) noexcept
{
    if (playHead == nullptr) {
        transportCache.valid.store(false);
        return;
    }

    juce::AudioPlayHead::CurrentPositionInfo posInfo;
    if (playHead->getCurrentPosition(posInfo)) {
        transportCache.valid.store(true);
        transportCache.playing.store(posInfo.isPlaying);
        transportCache.bpm.store(posInfo.bpm);
        transportCache.tsNum.store(posInfo.timeSigNumerator);
        transportCache.tsDen.store(posInfo.timeSigDenominator);
        transportCache.ppq.store(posInfo.ppqPosition);
        transportCache.barStartPpq.store(posInfo.ppqPositionOfLastBarStart);
    } else {
        transportCache.valid.store(false);
    }
}

void PluginProcessor::updatePlayingStepFromTransport()
{
    if (!transportCache.valid.load() || !transportCache.playing.load()) {
        return;
    }

    // Get transport info
    double ppqPos = transportCache.ppq.load();
    double barStartPpq = transportCache.barStartPpq.load();
    double bpm = transportCache.bpm.load();
    int timeSigNum = transportCache.tsNum.load();
    int timeSigDen = transportCache.tsDen.load();
    int stepsUsed = seq.stepsUsed.load();
    int divisionIndex = seq.divisionIndex.load();

    // Calculate bar-relative PPQ position
    double barPpq = ppqPos - barStartPpq;
    
    // Map division index to steps per quarter note
    int stepsPerQuarter = 0;
    switch (divisionIndex) {
        case 0: stepsPerQuarter = 0; break;  // 1/1
        case 1: stepsPerQuarter = 0; break;  // 1/2  
        case 2: stepsPerQuarter = 1; break;  // 1/4
        case 3: stepsPerQuarter = 2; break;  // 1/8
        case 4: stepsPerQuarter = 4; break;  // 1/16
        case 5: stepsPerQuarter = 8; break;  // 1/32
        default: stepsPerQuarter = 4; break;
    }
    
    // Calculate current step
    int currentStep = 0;
    if (stepsPerQuarter > 0) {
        double beatsPerBar = timeSigNum;
        double stepsPerBar = beatsPerBar * stepsPerQuarter;
        if (stepsUsed < stepsPerBar) {
            stepsPerBar = stepsUsed;
        }
        
        double stepFloat = barPpq * stepsPerQuarter;
        currentStep = (int)std::floor(stepFloat) % (int)stepsPerBar;
        
        if (stepsUsed > 0 && stepsUsed < 16) {
            currentStep = currentStep % stepsUsed;
        }
    } else {
        // For 1/1 or 1/2, use simpler calculation
        double beatsPerBar = timeSigNum;
        double quartersIntoBar = barPpq;
        int quarterIndex = (int)std::floor(quartersIntoBar) % (int)beatsPerBar;
        
        int activeSteps = stepsUsed > 0 ? stepsUsed : 16;
        double perQuarter = (double)activeSteps / beatsPerBar;
        currentStep = (int)std::floor(quarterIndex * perQuarter) % activeSteps;
    }
    
    seq.playingStep.store(currentStep);
}

void PluginProcessor::getTransportSnapshot(TransportCache& t) const noexcept
{
    t.valid = transportCache.valid.load();
    t.playing = transportCache.playing.load();
    t.bpm = transportCache.bpm.load();
    t.tsNum = transportCache.tsNum.load();
    t.tsDen = transportCache.tsDen.load();
    t.ppq = transportCache.ppq.load();
    t.barStartPpq = transportCache.barStartPpq.load();
}



StepSnapshot PluginProcessor::getSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return stepSnapshots[step];
    }
    if (step < 0 || step >= 16) {
        step = 0; // final fallback
    }
    return stepSnapshots[step];
}

void PluginProcessor::applySnapshotTargets(const StepSnapshot& s)
{
    switch (currentFx) {
        case FxType::Delay:
        {
            DelayTargets t;
            // Use conservative ranges to prevent buzzy sound
            t.timeMs    = juce::jlimit(10.0f, 1500.0f, s.delay.timeMs);
            t.feedback  = juce::jlimit(0.0f, 0.85f,  s.delay.feedback / 100.0f);  // Reduced max feedback
            t.wowDepth  = juce::jlimit(0.0f, 1.0f,     s.delay.wowDepth / 100.0f);
            t.wowRate   = juce::jlimit(0.1f, 5.0f,   s.delay.wowRate);           // Reduced max rate
            t.drive     = juce::jlimit(0.0f, 1.0f,     s.delay.saturation / 100.0f);
            t.hiCutHz   = juce::jlimit(1000.0f, 20000.0f, s.delay.highCut);
            t.lowCutHz  = juce::jlimit(20.0f, 2000.0f, s.delay.lowCut);
            t.mix       = juce::jlimit(0.0f, 1.0f, s.delay.mix / 100.0f);
            spaceDelay.setTargets(t);
            break;
        }
        case FxType::Crunch:
            // Placeholder for crunch FX
            break;
        case FxType::Pitch:
            // Placeholder for pitch FX
            break;
        case FxType::AutoPan:
            // Placeholder for autopan FX
            break;
        default:
            break;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}