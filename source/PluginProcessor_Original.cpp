#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetLoader.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout()),
       stutterRng(std::random_device{}())
{
    // Initialize preset loader
    presetLoader = std::make_unique<PresetLoader>();
    
    // init atomics from APVTS
    for (int i = 0; i < 16; ++i)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("seq_step" + juce::String(i+1))))
            stepModes[i].store(p->getIndex());
    }
    auto* divP = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("seq_division"));
    divisionIndex = divP ? divP->getIndex() : 2; // 0:1/4,1:1/8,2:1/16,3:1/32,4:Free
    freeRateHz    =        *apvts.getRawParameterValue ("seq_rate_hz");
    activeSteps   = (int) *apvts.getRawParameterValue ("seq_steps");
}

PluginProcessor::~PluginProcessor()
{
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
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    stepScheduler.prepare(sampleRate, samplesPerBlock);
    circularBuffer.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    stepSync.prepare(sampleRate);
    
    lastSampleRate = sampleRate;
    stepIndex = 0;
    reseedStepTimer (lastSampleRate, lastBpm);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();

    const bool followHost = (*apvts.getRawParameterValue ("seq_follow_host") > 0.5f);

    if (followHost && !wasFollowingHost) {
        // We just turned Follow Host ON → realign to current transport position
        stepSync.requestReset();  // will set originPPQ on next beginBlock()
    }
    wasFollowingHost = followHost;

    auto* divP = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("seq_division"));
    divisionIndex = divP ? divP->getIndex() : 2; // 0:1/4,1:1/8,2:1/16,3:1/32,4:Free
    activeSteps   = juce::jlimit (1, 16, (int) *apvts.getRawParameterValue ("seq_steps"));
    for (int i = 0; i < 16; ++i)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("seq_step" + juce::String(i+1))))
            stepModes[i].store(p->getIndex());
    }

    if (followHost && stepSync.beginBlock (getPlayHead(), numSamples) && divisionIndex != 4) // Skip Free mode
    {
        const double stepBeats = divisionToBeats (divisionIndex);

        // Step at block start (UI mirror)
        const int s0 = StepSync::stepAtPPQ (stepSync.ppqStart, stepSync.originPPQ, stepBeats, activeSteps);
        currentStep.store (s0);
        currentMode.store (stepModes[s0].load());

        // Emit each boundary in this buffer with sample offsets
        stepSync.emitBoundaries (stepBeats, activeSteps, numSamples,
            [this](int sampleOffset, int newStep)
            {
                // Fire per-step actions here at 'sampleOffset' if needed
                currentStep.store (newStep);
                currentMode.store (stepModes[newStep].load());
            });

        // proceed with audio processing...
    }
    else
    {
        // --- FREE (internal) mode fallback (not following host)
        const bool seqRun = (*apvts.getRawParameterValue ("seq_run") > 0.5f);
        if (seqRun)
        {
            freeRateHz = *apvts.getRawParameterValue ("seq_rate_hz");
            
            // samples per step from Hz
            if (divisionIndex == 4) // Free
            {
                const int sps = (int) juce::jmax (1.0, std::round (lastSampleRate / juce::jlimit (0.5, 32.0, (double) freeRateHz)));
                int processed = 0;
                while (processed < numSamples)
                {
                    if (samplesToNext <= 0) {            // fire next step
                        stepIndex = (stepIndex + 1) % activeSteps;
                        currentStep.store (stepIndex);
                        currentMode.store (stepModes[stepIndex].load());
                        samplesToNext = sps;
                    }
                    const int n = std::min (samplesToNext, numSamples - processed);
                    samplesToNext -= n;
                    processed += n;
                }
            }
            else
            {
                // tempo-sync but not following host (rare): just use your old sps timer
                const double beats = divisionToBeats (divisionIndex);
                const double sec   = beats * (60.0 / juce::jmax (1.0, stepSync.bpm));
                const int sps      = (int) std::max (1.0, std::round (sec * lastSampleRate));
                int processed = 0;
                while (processed < numSamples)
                {
                    if (samplesToNext <= 0) {
                        stepIndex = (stepIndex + 1) % activeSteps;
                        currentStep.store (stepIndex);
                        currentMode.store (stepModes[stepIndex].load());
                        samplesToNext = sps;
                    }
                    const int n = std::min (samplesToNext, numSamples - processed);
                    samplesToNext -= n;
                    processed += n;
                }
            }
        }
    }

    // Snapshot host playhead info to atomics (non-allocating)
    if (auto* playHead = getPlayHead())
    {
        if (auto positionInfo = playHead->getPosition())
        {
            double bpm = positionInfo->getBpm().orFallback(120.0);
            double ppq = positionInfo->getPpqPosition().orFallback(0.0);
            bool isPlaying = positionInfo->getIsPlaying();
            
            auto timeSig = positionInfo->getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{});
            int numerator = timeSig.numerator;
            int denominator = timeSig.denominator;
            
            stepScheduler.updateHostInfo(bpm, ppq, isPlaying, numerator, denominator);
        }
    }
    
    // Update step scheduler settings from parameters
    auto* stepsParam = apvts.getRawParameterValue("steps");
    auto* humanizeParam = apvts.getRawParameterValue("humanize");
    
    if (stepsParam && humanizeParam)
    {
        int steps = static_cast<int>(*stepsParam);
        float humanize = *humanizeParam / 100.0f; // Convert 0-100% to 0.0-1.0
        stepScheduler.updateSettings(steps, humanize);
    }
    
    // Capture input peaks for metering
    float inputL = 0.0f, inputR = 0.0f;
    if (buffer.getNumChannels() > 0)
        inputL = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() > 1)
        inputR = buffer.getMagnitude(1, 0, buffer.getNumSamples());
    else
        inputR = inputL; // Mono to stereo
    
    inputPeakL.store(inputL);
    inputPeakR.store(inputR);
    
    // Write input to circular buffer (always capture audio)
    circularBuffer.writeBlock(buffer);
    
    // Process step timing
    auto stepInfo = stepScheduler.processBlock(buffer.getNumSamples());
    
    // Get effect parameters
    auto* densityParam = apvts.getRawParameterValue("density");
    auto* partyParam = apvts.getRawParameterValue("party");
    auto* revPcParam = apvts.getRawParameterValue("rev_pc");
    auto* flickPcParam = apvts.getRawParameterValue("flick_pc");
    auto* mixParam = apvts.getRawParameterValue("mix");
    
    // Check if we should trigger effects on step boundary
    if (stepInfo.isStepBoundary && densityParam && partyParam && revPcParam && flickPcParam)
    {
        float density = *densityParam; // 0-100%
        float party = *partyParam;     // 0-100%
        float revPc = *revPcParam;     // 0-100%
        float flickPc = *flickPcParam; // 0-100%
        
        // Get current step mode: 0=Straight, 1=Reverse, 2=Glitch
        int currentStepMode = currentMode.load();
        
        // Priority: reverse > flick > stutter (if multiple could trigger, highest priority wins)
        bool shouldTriggerReverse = false;
        bool shouldTriggerFlick = false;
        bool shouldTriggerStutter = false;
        
        // Only trigger effects if NOT on a Straight step (mode 0)
        if (currentStepMode != 0)
        {
            // Check reverse first (highest priority) - only if step is Reverse mode
            if (currentStepMode == 1 && revPc > 0.0f && !reverseInfo.isActive && !stutterInfo.isActive && !flickInfo.isActive)
            {
                float reverseChanceValue = reverseChance(stutterRng) * 100.0f;
                shouldTriggerReverse = (reverseChanceValue <= revPc);
            }
            
            // Check flick and stutter if step is Glitch mode
            if (currentStepMode == 2)
            {
                // Check flick if reverse didn't trigger (second priority)
                if (!shouldTriggerReverse && flickPc > 0.0f && !flickInfo.isActive && !stutterInfo.isActive && !reverseInfo.isActive)
                {
                    float flickChanceValue = flickChance(stutterRng) * 100.0f;
                    shouldTriggerFlick = (flickChanceValue <= flickPc);
                }
                
                // Check stutter if reverse and flick didn't trigger (lowest priority)
                if (!shouldTriggerReverse && !shouldTriggerFlick && density > 0.0f && !stutterInfo.isActive && !reverseInfo.isActive && !flickInfo.isActive)
                {
                    float stutterChanceValue = stutterChance(stutterRng) * 100.0f;
                    shouldTriggerStutter = (stutterChanceValue <= density);
                }
            }
        }
        
        if (shouldTriggerReverse)
        {
            // Calculate reverse slice parameters (80-200ms)
            double sampleRate = getSampleRate();
            float reverseLengthMs = 80.0f + (party / 100.0f) * (200.0f - 80.0f); // 80-200ms based on party
            int reverseLengthSamples = static_cast<int>(reverseLengthMs * 0.001 * sampleRate);
            
            // Copy the slice to scratch buffer
            circularBuffer.copyReverseSliceToScratch(reverseLengthSamples);
            
            // Set up reverse
            reverseInfo.isActive = true;
            reverseInfo.reverseLengthSamples = reverseLengthSamples;
            reverseInfo.playbackPosition = 0;
            reverseInfo.reverseDurationSamples = stepInfo.samplesUntilNextStep;
            reverseInfo.needsHannWindow = true;
        }
        else if (shouldTriggerFlick)
        {
            // Calculate flick duration parameters (40-120ms)
            double sampleRate = getSampleRate();
            float flickLengthMs = 40.0f + (party / 100.0f) * (120.0f - 40.0f); // 40-120ms based on party
            int flickDurationSamples = static_cast<int>(flickLengthMs * 0.001 * sampleRate);
            
            // Set up flick
            flickInfo.isActive = true;
            flickInfo.flickDurationSamples = flickDurationSamples;
            flickInfo.playbackPosition = 0;
            flickInfo.readPosition = 0.0;
            flickInfo.currentRate = 1.0f;
            flickInfo.needsFades = true;
        }
        else if (shouldTriggerStutter)
        {
            // Calculate stutter slice parameters
            double sampleRate = getSampleRate();
            float sliceLengthMs = 8.0f + (party / 100.0f) * (60.0f - 8.0f); // 8-60ms based on party
            int sliceLengthSamples = static_cast<int>(sliceLengthMs * 0.001 * sampleRate);
            
            // Calculate how far back to read (up to 4 seconds)
            int maxLookbackSamples = static_cast<int>(sampleRate * 4.0);
            float lookbackMs = 50.0f + (party / 100.0f) * 500.0f; // 50-550ms based on party
            int lookbackSamples = static_cast<int>(lookbackMs * 0.001 * sampleRate);
            lookbackSamples = juce::jmin(lookbackSamples, maxLookbackSamples);
            
            // Set up stutter
            stutterInfo.isActive = true;
            stutterInfo.sliceLengthSamples = sliceLengthSamples;
            stutterInfo.sliceStartSample = lookbackSamples; // Start position relative to current write
            stutterInfo.playbackPosition = 0;
            stutterInfo.stutterDurationSamples = stepInfo.samplesUntilNextStep;
            stutterInfo.needsCrossfade = true;
        }
    }
    
    // Create a copy of the dry signal for mixing
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);
    
    // Process active effects (priority: reverse > flick > stutter)
    bool effectProcessed = false;
    
    if (reverseInfo.isActive)
    {
        circularBuffer.processReverse(buffer, buffer.getNumSamples(), reverseInfo);
        effectProcessed = true;
    }
    else if (flickInfo.isActive)
    {
        circularBuffer.processFlick(buffer, buffer.getNumSamples(), flickInfo);
        effectProcessed = true;
    }
    else if (stutterInfo.isActive)
    {
        circularBuffer.processStutter(buffer, buffer.getNumSamples(), stutterInfo);
        effectProcessed = true;
    }
    
    // Equal-power dry/wet mixing
    if (mixParam)
    {
        float mixValue = *mixParam / 100.0f; // 0-1
        
        if (effectProcessed)
        {
            // Equal-power crossfade: dry*sqrt(1-m) + wet*sqrt(m)
            float wetGain = std::sqrt(mixValue);
            float dryGain = std::sqrt(1.0f - mixValue);
            
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* wetData = buffer.getWritePointer(channel);
                const auto* dryData = dryBuffer.getReadPointer(channel);
                
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    wetData[sample] = wetData[sample] * wetGain + dryData[sample] * dryGain;
                }
            }
        }
        // If no effect processed, mix is essentially dry gain
        else if (mixValue < 1.0f)
        {
            float dryGain = std::sqrt(1.0f - mixValue);
            buffer.applyGain(dryGain);
        }
    }
    
    // Apply output trim (post-effects, post-mix)
    auto* outDbParam = apvts.getRawParameterValue("out_db");
    if (outDbParam)
    {
        float outDbValue = *outDbParam;
        float outGain = juce::Decibels::decibelsToGain(outDbValue);
        
        // Apply gain to the entire buffer
        buffer.applyGain(outGain);
    }
    
    // Capture output peaks for metering
    float outputL = 0.0f, outputR = 0.0f;
    if (buffer.getNumChannels() > 0)
        outputL = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() > 1)
        outputR = buffer.getMagnitude(1, 0, buffer.getNumSamples());
    else
        outputR = outputL; // Mono to stereo
    
    outputPeakL.store(outputL);
    outputPeakR.store(outputR);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Party parameter (0-100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "party", "Party", 0.0f, 100.0f, 0.0f));
    
    // Steps parameter (1-16)
    layout.add (std::make_unique<juce::AudioParameterInt> (
        "steps", "Steps", 1, 16, 8));
    
    // Density parameter (0-100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "density", "Density", 0.0f, 100.0f, 50.0f));
    
    // Rev_pc parameter (0-100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rev_pc", "Reverse %", 0.0f, 100.0f, 0.0f));
    
    // Flick_pc parameter (0-100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "flick_pc", "Flicker %", 0.0f, 100.0f, 0.0f));
    
    // Humanize parameter (0-100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "humanize", "Humanize", 0.0f, 100.0f, 0.0f));
    
    // Mix parameter (0-100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix", 0.0f, 100.0f, 50.0f));
    
    // Out_db parameter (-60 to +20 dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "out_db", "Output dB", -60.0f, 20.0f, 0.0f));
    
    // --- Sequencer params ---
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        "seq_division", "Step Division",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32", "Free" }, 2)); // default 1/16

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "seq_rate_hz", "Free Rate (Hz)",
        juce::NormalisableRange<float> (0.5f, 32.0f, 0.001f, 0.5f), 8.0f));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        "seq_steps", "Steps Used", 1, 16, 16));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        "seq_follow_host", "Follow Host Transport", true));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        "seq_run", "Run", false));

    // 16 per-step mode params (0..2)
    for (int i = 0; i < 16; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice>(
            "seq_step" + juce::String (i+1),
            "Step " + juce::String (i+1),
            kStepModeNames, 0));
    }
    
    return layout;
}

//==============================================================================
// Preset management methods

void PluginProcessor::loadPreset(int presetIndex)
{
    if (presetLoader)
    {
        presetLoader->loadPreset(presetIndex, apvts);
    }
}

void PluginProcessor::saveCurrentAsUserPreset(const juce::String& presetName)
{
    if (presetLoader)
    {
        presetLoader->saveCurrentAsUserPreset(presetName, apvts);
    }
}

juce::StringArray PluginProcessor::getPresetNames() const
{
    if (presetLoader)
        return presetLoader->getPresetNames();
    return {};
}

int PluginProcessor::getNumPresets() const
{
    if (presetLoader)
        return presetLoader->getNumPresets();
    return 0;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

int PluginProcessor::computeSamplesPerStep (double sr, double bpm) const
{
    if (divisionIndex == 4) // Free
        return (int) juce::jmax (1.0, std::round (sr / juce::jlimit (0.5, 32.0, (double) freeRateHz)));

    const double beatDurSec = 60.0 / juce::jmax (1.0, bpm);
    double beats = 1.0; // quarter
    if      (divisionIndex == 0) beats = 1.0;    // 1/4
    else if (divisionIndex == 1) beats = 0.5;    // 1/8
    else if (divisionIndex == 2) beats = 0.25;   // 1/16
    else if (divisionIndex == 3) beats = 0.125;  // 1/32
    const double dur = beats * beatDurSec;
    return (int) std::max (1.0, std::round (dur * sr));
}

void PluginProcessor::reseedStepTimer (double sr, double bpm)
{
    samplesToNext = computeSamplesPerStep (sr, bpm);
}
