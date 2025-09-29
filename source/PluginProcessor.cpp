#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/DspFlags.h"
#include <chrono>

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       ),
    valueTreeState(*this, nullptr, "Parameters", createParameterLayout())
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
    
    // Initialize DSP variables (prepare will be called in prepareToPlay)
    dspSampleRate = 44100.0;
    
    // Verification log
    DBG("[Stepper] Built formats: VST3/AU/Standalone. BundleID=com.glitchcorp.stepper, Code=Stp1");
}


juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // RE-201 Delay Parameters - mapped to your 8 knobs
    params.push_back(std::make_unique<juce::AudioParameterFloat>("timeMs", "Time", 10.0f, 2000.0f, 250.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 0.95f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepth", "Wow Depth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("wowRate", "Wow Rate", 0.1f, 8.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hiCut", "Hi-Cut", 1000.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lowCut", "Low-Cut", 20.0f, 2000.0f, 20.0f));
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
    
    // Simplified processing to avoid crashes
    juce::ignoreUnused(midiMessages);
    
#if GC_SAFE_DELAY_ONLY
    const FxType fx = FxType::Delay;
#else
    const FxType fx = currentFx;
#endif

    // Update transport cache and compute playing step when host is playing
    updateTransportCache(getPlayHead(), buffer.getNumSamples());
    bool hostPlaying = transportCache.playing.load();
    bool sequencerEnabled = seq.enabled.load();
    
    if (hostPlaying && !prevHostPlaying)
    {
        // latch origin at transport start
        seq.originPPQ.store(transportCache.ppq.load());
        seq.haveOrigin.store(true);
    }
    
    // Handle sequencer enable/disable transitions
    static bool prevSequencerEnabled = false;
    if (sequencerEnabled && !prevSequencerEnabled)
    {
        // Sequencer just enabled - set origin based on current state
        if (hostPlaying) {
            // Use DAW transport origin
            seq.originPPQ.store(transportCache.ppq.load());
            seq.haveOrigin.store(true);
        } else {
            // Standalone mode - don't set origin yet, wait for play button
            seq.haveOrigin.store(false);
        }
        seq.playingStep.store(-1);
    }
    else if (!sequencerEnabled && prevSequencerEnabled)
    {
        // When sequencer is disabled, reset everything
        seq.originPPQ.store(0.0);
        seq.haveOrigin.store(false);
        seq.playingStep.store(-1);
    }
    prevSequencerEnabled = sequencerEnabled;
    prevHostPlaying = hostPlaying;
    
    // Update playing step if host is playing OR if internal sequencer is enabled AND has origin
    if (hostPlaying || (sequencerEnabled && seq.haveOrigin.load()))
    {
        updatePlayingStepFromTransport();
    }

    // Apply sequencer step snapshot only if sequencer is enabled, otherwise use APVTS parameters
    if (sequencerEnabled) {
        // Use playing step's snapshot for audio processing
        int playingStep = seq.playingStep.load();
        StepSnapshot playingSnapshot = getSafeSnapshot(playingStep);
        applySnapshotTargets(playingSnapshot);
    } else {
        // Use empty snapshot to read from APVTS parameters (manual control)
        applySnapshotTargets(StepSnapshot{});
    }

    // Clear any unused output channels
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Process delay effect
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) {
        if (fxEnabled.load())
            spaceDelay.process(buffer, buffer.getNumSamples());
    }

    // Debug logging
    static int c = 0;
    if ((++c & 127) == 0) { // Log every 128 blocks
        // Debug logging removed to prevent null pointer crashes
        DBG("[FX] fx=Delay");
        DBG("[SEQ] on=" << (seq.enabled.load() ? "true" : "false") << " playing=" << (transportCache.playing.load() ? "true" : "false") << " step=" << seq.playingStep.load());
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;  // Re-enable editor
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
    bool hostPlaying = transportCache.valid.load() && transportCache.playing.load();
    bool sequencerEnabled = seq.enabled.load();
    
    // Only proceed if host is playing OR internal sequencer is enabled
    if (!hostPlaying && !sequencerEnabled) {
        return;
    }

    // Get transport info
    double ppqPos, barStartPpq;
    int timeSigNum;
    
    if (hostPlaying) {
        // Use real transport data
        ppqPos = transportCache.ppq.load();
        barStartPpq = transportCache.barStartPpq.load();
        timeSigNum = transportCache.tsNum.load();
    } else {
        // Use simulated transport for standalone mode
        // Use a simple time-based approach for standalone
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(now - startTime).count();
        
        // Simulate PPQ at 120 BPM (2 beats per second)
        ppqPos = elapsed * 2.0; // 2 PPQ per second at 120 BPM
        barStartPpq = 0.0; // Start of bar
        timeSigNum = 4; // 4/4 time signature
    }

    const int stepsUsed = juce::jlimit(1, 16, seq.stepsUsed.load());
    const int divisionIndex = juce::jlimit(0, 7, seq.divisionIndex.load());
    const int stdMode = juce::jlimit(0, 2, seq.stdMode.load()); // 0 straight, 1 triplet, 2 dotted

    // Calculate bar-relative PPQ position
    double barPpq = ppqPos - barStartPpq;
    
    // Compute beats-per-step from division and STD mode
    double beatsPerStep = 0.0;
    switch (divisionIndex) {
        case 0: beatsPerStep = 16.0;   break; // 4 bars
        case 1: beatsPerStep = 8.0;    break; // 2 bars
        case 2: beatsPerStep = 4.0;    break; // 1 bar (renamed from 1/1 -> 1)
        case 3: beatsPerStep = 2.0;    break; // 1/2
        case 4: beatsPerStep = 1.0;    break; // 1/4
        case 5: beatsPerStep = 0.5;    break; // 1/8
        case 6: beatsPerStep = 0.25;   break; // 1/16
        case 7: beatsPerStep = 0.125;  break; // 1/32
        default: beatsPerStep = 0.25;  break;
    }
    if (stdMode == 1)      beatsPerStep *= (2.0 / 3.0); // triplet
    else if (stdMode == 2) beatsPerStep *= 1.5;         // dotted

    // Use an origin so stepping is free-running and rate-dependent (not bar-locked)
    double origin;
    if (hostPlaying) {
        origin = seq.haveOrigin.load() ? seq.originPPQ.load() : barStartPpq;
    } else {
        // For standalone mode, use 0 as origin (start from beginning)
        origin = 0.0;
    }
    const double beatsSinceOrigin = juce::jmax(0.0, ppqPos - origin);

    int currentStep = 0;
    if (beatsPerStep > 0.0 && stepsUsed > 0)
    {
        const double stepsElapsed = std::floor(beatsSinceOrigin / beatsPerStep);
        const double mod = std::fmod(stepsElapsed, (double) stepsUsed);
        currentStep = (int) (mod < 0.0 ? mod + stepsUsed : mod);
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

void PluginProcessor::setStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        stepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::randomizeAllStepSnapshots() noexcept
{
    DBG("[Processor] Randomizing all step snapshots");
    
    for (int step = 0; step < 16; ++step) {
        // Create more obvious differences for testing indicator bars
        float stepFactor = (float)step / 15.0f; // 0.0 to 1.0 across steps
        
        stepSnapshots[step].delay.timeMs = 100.0f + stepFactor * 1800.0f; // 100-1900ms across steps
        stepSnapshots[step].delay.feedback = stepFactor * 0.9f; // 0-0.9 across steps
        stepSnapshots[step].delay.wowDepth = stepFactor; // 0-1.0 across steps
        stepSnapshots[step].delay.wowRate = 0.5f + stepFactor * 7.5f; // 0.5-8.0Hz across steps
        stepSnapshots[step].delay.saturation = stepFactor; // 0-1.0 across steps
        stepSnapshots[step].delay.highCut = 2000.0f + stepFactor * 18000.0f; // 2k-20kHz across steps
        stepSnapshots[step].delay.lowCut = 50.0f + stepFactor * 1950.0f; // 50-2000Hz across steps
        stepSnapshots[step].delay.mix = stepFactor; // 0-1.0 across steps
    }
}

void PluginProcessor::applySnapshotTargets(const StepSnapshot& snapshot)
{
    switch (currentFx) {
        case FxType::Delay:
        {
            FxDelay::Targets t;
            
            // Check if we have a valid snapshot (sequencer mode) or should use APVTS (manual mode)
            bool useSnapshot = (snapshot.delay.timeMs > 0.0f); // Simple check for valid snapshot
            
            if (useSnapshot) {
                // Use sequencer step snapshot values
                t.timeMs    = snapshot.delay.timeMs;
                t.feedback  = juce::jlimit(0.0f, 0.85f, snapshot.delay.feedback / 100.0f); // Convert from percentage
                t.wowDepth  = snapshot.delay.wowDepth / 100.0f; // Convert from percentage
                t.wowRate   = snapshot.delay.wowRate;
                t.drive     = snapshot.delay.saturation / 100.0f; // Convert from percentage
                t.hiCutHz   = snapshot.delay.highCut;
                t.lowCutHz  = snapshot.delay.lowCut;
                t.mix       = snapshot.delay.mix;
            } else {
                // Read directly from APVTS parameters with null checks (manual mode)
                auto* timeMsParam = valueTreeState.getRawParameterValue("timeMs");
                auto* feedbackParam = valueTreeState.getRawParameterValue("feedback");
                auto* wowDepthParam = valueTreeState.getRawParameterValue("wowDepth");
                auto* wowRateParam = valueTreeState.getRawParameterValue("wowRate");
                auto* driveParam = valueTreeState.getRawParameterValue("drive");
                auto* hiCutParam = valueTreeState.getRawParameterValue("hiCut");
                auto* lowCutParam = valueTreeState.getRawParameterValue("lowCut");
                auto* mixParam = valueTreeState.getRawParameterValue("mix");
                
                t.timeMs    = timeMsParam ? timeMsParam->load() : 250.0f;
                t.feedback  = feedbackParam ? juce::jlimit(0.0f, 0.85f, feedbackParam->load()) : 0.2f;
                t.wowDepth  = wowDepthParam ? wowDepthParam->load() : 0.0f;
                t.wowRate   = wowRateParam ? wowRateParam->load() : 1.0f;
                t.drive     = driveParam ? driveParam->load() : 0.0f;
                t.hiCutHz   = hiCutParam ? hiCutParam->load() : 20000.0f;
                t.lowCutHz  = lowCutParam ? lowCutParam->load() : 20.0f;
                t.mix       = mixParam ? mixParam->load() : 0.5f;
            }
            
    if (fxEnabled.load())
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

void PluginProcessor::updateCurrentStepSnapshot(int knobIndex, float value)
{
    // Get the current step being edited
    int currentStep = uiSelectedStep.load();
    
    // Map knob index to both StepSnapshot and APVTS parameters
    switch (knobIndex) {
        case 0: // Time
            stepSnapshots[currentStep].delay.timeMs = value;
            valueTreeState.getParameter("timeMs")->setValueNotifyingHost(
                valueTreeState.getParameter("timeMs")->convertTo0to1(value));
            break;
        case 1: // Feedback
            stepSnapshots[currentStep].delay.feedback = value * 100.0f; // Convert to percentage
            valueTreeState.getParameter("feedback")->setValueNotifyingHost(
                valueTreeState.getParameter("feedback")->convertTo0to1(value));
            break;
        case 2: // Wow Depth
            stepSnapshots[currentStep].delay.wowDepth = value * 100.0f; // Convert to percentage
            valueTreeState.getParameter("wowDepth")->setValueNotifyingHost(
                valueTreeState.getParameter("wowDepth")->convertTo0to1(value));
            break;
        case 3: // Wow Rate
            stepSnapshots[currentStep].delay.wowRate = value;
            valueTreeState.getParameter("wowRate")->setValueNotifyingHost(
                valueTreeState.getParameter("wowRate")->convertTo0to1(value));
            break;
        case 4: // Drive
            stepSnapshots[currentStep].delay.saturation = value * 100.0f; // Convert to percentage
            valueTreeState.getParameter("drive")->setValueNotifyingHost(
                valueTreeState.getParameter("drive")->convertTo0to1(value));
            break;
        case 5: // Hi-Cut
            stepSnapshots[currentStep].delay.highCut = value;
            valueTreeState.getParameter("hiCut")->setValueNotifyingHost(
                valueTreeState.getParameter("hiCut")->convertTo0to1(value));
            break;
        case 6: // Low-Cut
            stepSnapshots[currentStep].delay.lowCut = value;
            valueTreeState.getParameter("lowCut")->setValueNotifyingHost(
                valueTreeState.getParameter("lowCut")->convertTo0to1(value));
            break;
        case 7: // Mix
            stepSnapshots[currentStep].delay.mix = value * 100.0f; // Convert to percentage
            valueTreeState.getParameter("mix")->setValueNotifyingHost(
                valueTreeState.getParameter("mix")->convertTo0to1(value));
            break;
        default:
            break;
    }
}

void PluginProcessor::resetSequencerState() noexcept
{
    // Reset sequencer state to prevent random playback when re-enabled
    seq.playingStep.store(-1);
    seq.originPPQ.store(0.0);
    seq.haveOrigin.store(false);
    
    DBG("[Processor] Sequencer state reset - playingStep: " << seq.playingStep.load() << ", originPPQ: " << seq.originPPQ.load());
}

void PluginProcessor::startStandalonePlayback() noexcept
{
    // Set origin for standalone playback using current time
    seq.originPPQ.store(0.0);
    seq.haveOrigin.store(true);
    seq.playingStep.store(-1);
    
    DBG("[Processor] Standalone playback started - originPPQ: " << seq.originPPQ.load() << ", haveOrigin: " << seq.haveOrigin.load());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}