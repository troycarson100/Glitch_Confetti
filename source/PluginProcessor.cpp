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
    // Initialize delay sequencer state
    seq.enabled.store(false);
    seq.stepsUsed.store(16);
    seq.divisionIndex.store(3); // 1/8 default
    seq.playingStep.store(0);
    
    // Initialize AutoPan sequencer state (independent)
    autopanSeq.enabled.store(false);
    autopanSeq.stepsUsed.store(16);
    autopanSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    autopanSeq.playingStep.store(0);
    
    // Initialize UI state
    uiSelectedStep.store(0);
    autopanUiSelectedStep.store(0);
    
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
    
    // Initialize standalone start time
    standaloneStartTime = std::chrono::high_resolution_clock::now();
    
    // Initialize AutoPan step snapshots with default values
    for (int i = 0; i < 16; ++i) {
        autopanStepSnapshots[i].autopan.rate = 0.43f;      // ~1/4 note (default)
        autopanStepSnapshots[i].autopan.phase = 180.0f;    // 180° default
        autopanStepSnapshots[i].autopan.waveType = 0;      // Sine
        autopanStepSnapshots[i].autopan.waveShape = 0.5f;  // Middle
        autopanStepSnapshots[i].autopan.inverted = false;  // Not inverted
        autopanStepSnapshots[i].autopan.amount = 1.0f;     // Full amount
    }
    DBG("[Stepper] Initialized AutoPan step snapshots with default values");
    
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
    
    // AutoPan Parameters - 6 knobs
    params.push_back(std::make_unique<juce::AudioParameterFloat>("autopanRate", "AutoPan Rate", 0.0f, 1.0f, 0.43f)); // 0-1 for sync divisions (default ~1/4 = 0.43)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("autopanPhase", "AutoPan Phase", 0.0f, 360.0f, 180.0f)); // degrees
    params.push_back(std::make_unique<juce::AudioParameterChoice>("autopanWaveType", "AutoPan Wave Type", 
        juce::StringArray {"Sine", "Triangle", "Ramp Down", "Ramp Up", "Random"}, 0)); // 0 = Sine default
    params.push_back(std::make_unique<juce::AudioParameterFloat>("autopanWaveShape", "AutoPan Wave Shape", 0.0f, 1.0f, 0.5f)); // wave shape control
    params.push_back(std::make_unique<juce::AudioParameterBool>("autopanInverted", "AutoPan Inverted", false)); // false = normal, true = inverted
    params.push_back(std::make_unique<juce::AudioParameterFloat>("autopanAmount", "AutoPan Amount", 0.0f, 1.0f, 0.5f)); // pan amount
    
    // Page and effect enable parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>("currentPage", "Current Page", 
        juce::StringArray {"SpaceDelay", "AutoPan"}, 0)); // 0 = SpaceDelay, 1 = AutoPan
    params.push_back(std::make_unique<juce::AudioParameterBool>("autopanEnabled", "AutoPan Enabled", false)); // AutoPan effect enabled
    params.push_back(std::make_unique<juce::AudioParameterBool>("autopanTimeSync", "AutoPan Time Sync", true)); // AutoPan sync mode enabled - ON by default
    
    // Master Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterInput", "Master Input", 
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.01f, 1.0f), 0.0f)); // -60 to +6 dB, default 0.0 dB, logarithmic skew
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterDryWet", "Master Dry/Wet", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterOutput", "Master Output", 
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.01f, 1.0f), 0.0f)); // -60 to +6 dB, default 0.0 dB, logarithmic skew
    
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
    autoPan.prepare(sampleRate, 30.0); // 30ms smoothing
    autoPan.setVisualState(&panVis);
    seq.prepare(sampleRate); // Initialize delay sequencer with sample rate
    autopanSeq.prepare(sampleRate); // Initialize AutoPan sequencer with sample rate
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

    // Update transport cache
    updateTransportCache(getPlayHead(), buffer.getNumSamples());
    
    // Stateless PPQ-driven sequencer logic
    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos.hasValue())
        {
            const bool isPlaying = pos->getIsPlaying();
            const bool ppqValid  = pos->getPpqPosition().hasValue();
            const double ppq     = pos->getPpqPosition().hasValue() ? *pos->getPpqPosition() : -1.0;
            const int64_t sPos   = pos->getTimeInSamples().hasValue() ? *pos->getTimeInSamples() : -1;

            // Detect play edge
            const bool playEdge = (isPlaying && !wasPlaying.load());

            // Handle hosts that set play before PPQ is valid:
            if (playEdge) {
                armPending.store(true);
                seq.resetPhase();          // visuals/phase reset
                autopanSeq.resetPhase();   // AutoPan sequencer phase reset
                
                // Auto-enable delay sequencer on DAW play (user can still disable with power button)
                if (followHost.load()) {
                    seq.enabled.store(true);  // Enable delay sequencer
                    seq.active.store(true);   // Activate delay sequencer
                }
                
                // AutoPan sequencer activates if enabled (independent of followHost)
                if (autopanSeq.enabled.load()) {
                    autopanSeq.active.store(true);  // Activate AutoPan sequencer
                    DBG("[AUTOPAN SEQ] ✓ Activated on play edge");
                } else {
                    DBG("[AUTOPAN SEQ] ✗ NOT activated (enabled=" << autopanSeq.enabled.load() << ")");
                }
                
                DBG("[SEQ] Play edge detected");
                DBG("[SEQ] Delay: enabled=" << seq.enabled.load() << " active=" << seq.active.load());
                DBG("[SEQ] AutoPan: enabled=" << autopanSeq.enabled.load() << " active=" << autopanSeq.active.load());
            }

            // If arming and PPQ now valid, lock-in
            if (armPending.load() && isPlaying && ppqValid) {
                const int step = seq.computeStepFromPPQ(ppq);
                seq.currentStep.store(step);
                seq.playingStep.store(step);
                
                // Also lock-in AutoPan sequencer if enabled
                if (autopanSeq.enabled.load()) {
                    const int autopanStep = autopanSeq.computeStepFromPPQ(ppq);
                    autopanSeq.currentStep.store(autopanStep);
                    autopanSeq.playingStep.store(autopanStep);
                    DBG("[AUTOPAN SEQ] Lock-in at PPQ=" << ppq << " -> step " << autopanStep);
                } else {
                    DBG("[AUTOPAN SEQ] Skip lock-in (not enabled)");
                }
                
                armPending.store(false);
            }

            // While playing with valid PPQ: compute step every block (stateless)
            if (isPlaying && ppqValid && seq.active.load()) {
                const int step = seq.computeStepFromPPQ(ppq);
                if (step != seq.currentStep.load()) {
                    seq.currentStep.store(step);
                    seq.playingStep.store(step);
                    DBG("[SEQ] Step changed to: " << step << " PPQ: " << ppq);
                }
            }
            
            // AutoPan sequencer stepping (shares same PPQ/transport as delay sequencer)
            static int autopanDebugCounter = 0;
            if ((autopanDebugCounter++ % 100) == 0) {  // Log every 100 blocks
                DBG("[AUTOPAN SEQ DEBUG] isPlaying=" << isPlaying << " ppqValid=" << ppqValid 
                    << " active=" << autopanSeq.active.load() << " enabled=" << autopanSeq.enabled.load()
                    << " PPQ=" << ppq);
            }
            
            if (isPlaying && ppqValid && autopanSeq.active.load()) {
                const int autopanStep = autopanSeq.computeStepFromPPQ(ppq);
                if (autopanStep != autopanSeq.currentStep.load()) {
                    autopanSeq.currentStep.store(autopanStep);
                    autopanSeq.playingStep.store(autopanStep);
                    DBG("[AUTOPAN SEQ] ★ Step changed to: " << autopanStep << " PPQ: " << ppq 
                        << " divIdx=" << autopanSeq.divisionIndex.load() 
                        << " stepsUsed=" << autopanSeq.stepsUsed.load());
                }
            } else if (autopanSeq.enabled.load() && !autopanSeq.active.load()) {
                DBG("[AUTOPAN SEQ] WARNING: Enabled but not active! isPlaying=" << isPlaying << " ppqValid=" << ppqValid);
            }

            // Stop edge: freeze
            if (!isPlaying && wasPlaying.load()) {
                // Transport stopped: don't advance; keep last lit step
                // Optionally: seq.resetPhase();
            }

            // Persist transport snapshot
            wasPlaying.store(isPlaying);
            haveValidPos.store(true);
            lastPPQ.store(ppq);
            lastSamples.store(sPos);
        }
    }
    else
    {
        // Standalone mode - no DAW transport, use internal timing
        if (seq.active.load() && seq.enabled.load()) {
            // Use standalone timing to advance sequencer
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - standaloneStartTime).count();
            
            // Calculate current step based on elapsed time and BPM
            const double bpm = transportCache.bpm.load() > 0.0 ? transportCache.bpm.load() : 120.0; // Use transport BPM or default
            const double beatsPerStep = seq.beatsPerStepFromDivision(seq.divisionIndex.load());
            const double msPerStep = (60.0 / bpm) * beatsPerStep * 1000.0; // Convert beats to milliseconds
            
            if (msPerStep > 0.0) {
                const int step = ((int)(elapsed / msPerStep)) % seq.stepsUsed.load();
                if (step != seq.currentStep.load()) {
                    seq.currentStep.store(step);
                    seq.playingStep.store(step);
                    DBG("[SEQ] Standalone step: " << step << " elapsed: " << elapsed << "ms");
                }
            }
        }
    }

    // === DELAY SEQUENCER ===
    // Apply sequencer step snapshot only if sequencer is active, otherwise use APVTS parameters
    if (seq.active.load()) {
        // Use current step's snapshot for audio processing
        int currentStep = seq.currentStep.load();
        StepSnapshot currentSnapshot = getSafeSnapshot(currentStep);
        applySnapshotTargets(currentSnapshot);
    } else {
        // Use empty snapshot to read from APVTS parameters (manual control)
        applySnapshotTargets(StepSnapshot{});
    }
    
    // Note: AutoPan sequencer stepping is now handled in the main transport section above
    // to share the same play head position and timing with the delay sequencer

    // Clear any unused output channels
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Update input meters (pre-effects)
    auto updateMeters = [&](const juce::AudioBuffer<float>& buf, MeterState& m) {
        const int N = buf.getNumSamples();
        if (N <= 0) return;
        
        const float* L = buf.getReadPointer(0);
        const float* R = buf.getNumChannels() > 1 ? buf.getReadPointer(1) : L;

        // Block RMS (cheap) + peak
        float sumL=0, sumR=0, pkL=0, pkR=0;
        for (int n=0; n<N; ++n) {
            const float a = std::abs(L[n]), b = std::abs(R[n]);
            sumL += L[n]*L[n]; sumR += R[n]*R[n];
            pkL = std::max(pkL, a); pkR = std::max(pkR, b);
        }
        const float rmsL = std::sqrt(sumL / std::max(1, N));
        const float rmsR = std::sqrt(sumR / std::max(1, N));

        // Convert to dB
        const float rmsDbL = linearToDb(rmsL);
        const float rmsDbR = linearToDb(rmsR);
        const float pkDbL  = linearToDb(pkL);
        const float pkDbR  = linearToDb(pkR);

        // Ballistics (slow rise/fast decay for RMS; sticky peak)
        auto smooth = [](float prev, float target, float rise, float fall) {
            return (target > prev) ? (prev + rise*(target - prev))
                                   : (prev + fall*(target - prev));
        };

        // Pull previous atomics, smooth, write back
        float prevRmsL = m.rmsDbL.load(), prevRmsR = m.rmsDbR.load();
        m.rmsDbL.store(smooth(prevRmsL, rmsDbL, 0.25f, 0.08f));
        m.rmsDbR.store(smooth(prevRmsR, rmsDbR, 0.25f, 0.08f));

        // Peak hold with decay ~1.5 dB/s (host-rate agnostic)
        float prevPkL = m.peakDbL.load(), prevPkR = m.peakDbR.load();
        m.peakDbL.store(std::max(pkDbL, prevPkL - 0.03f));
        m.peakDbR.store(std::max(pkDbR, prevPkR - 0.03f));

        // Clip detection removed - red bar color indicates clipping
        
        // Legacy single-value tracking for compatibility
        inputLevel.store(juce::jmax(rmsDbL, rmsDbR));
    };
    
    // Apply master input gain (pre-effects) with limiting
    auto* masterInputParam = valueTreeState.getRawParameterValue("masterInput");
    if (masterInputParam != nullptr) {
        float inputGainDb = masterInputParam->load();
        float inputGain = juce::Decibels::decibelsToGain(inputGainDb);
        buffer.applyGain(inputGain);
        
    }
    
    // Update input meters AFTER input gain is applied
    updateMeters(buffer, inputMeter);
    
    // Store dry signal for master dry/wet mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);
    
    // Process delay effect
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) {
        if (fxEnabled.load())
            spaceDelay.process(buffer, buffer.getNumSamples());
    }
    
    // Process AutoPan AFTER delay but BEFORE dry/wet mix
    auto* autopanEnabledParam = valueTreeState.getRawParameterValue("autopanEnabled");
    
    bool isAutoPanEnabled = autopanEnabledParam ? (autopanEnabledParam->load() > 0.5f) : false;
    
    if (isAutoPanEnabled) {
        // Apply AutoPan parameters (from sequencer snapshot or APVTS)
        float rate, depth, waveShape, phaseOffset;
        int waveTypeIndex;
        bool isInverted;
        
        // Check if AutoPan sequencer is active
        if (autopanSeq.active.load()) {
            // Use AutoPan sequencer snapshot
            int autopanStep = autopanSeq.currentStep.load();
            const auto& snapshot = autopanStepSnapshots[juce::jlimit(0, 15, autopanStep)];
            
            rate = snapshot.autopan.rate;  // Already in 0-1 range
            depth = snapshot.autopan.amount;
            waveShape = snapshot.autopan.waveShape;
            phaseOffset = snapshot.autopan.phase / 360.0f; // Convert degrees to 0-1
            waveTypeIndex = snapshot.autopan.waveType;
            isInverted = snapshot.autopan.inverted;
        } else {
            // Use APVTS parameters (manual control)
            auto* rateParam = valueTreeState.getRawParameterValue("autopanRate");
            auto* amountParam = valueTreeState.getRawParameterValue("autopanAmount");
            auto* waveShapeParam = valueTreeState.getRawParameterValue("autopanWaveShape");
            auto* phaseParam = valueTreeState.getRawParameterValue("autopanPhase");
            auto* waveTypeParam = valueTreeState.getRawParameterValue("autopanWaveType");
            auto* invertedParam = valueTreeState.getRawParameterValue("autopanInverted");
            
            rate = rateParam ? rateParam->load() : 0.43f;
            depth = amountParam ? amountParam->load() : 0.5f;
            waveShape = waveShapeParam ? waveShapeParam->load() : 0.5f;
            phaseOffset = phaseParam ? (phaseParam->load() / 360.0f) : 0.5f;
            waveTypeIndex = waveTypeParam ? (int)waveTypeParam->load() : 0;
            isInverted = invertedParam ? invertedParam->load() > 0.5f : false;
        }
        
        // Convert rate to Hz (applies to both sequencer and manual modes)
        auto* syncParam = valueTreeState.getRawParameterValue("autopanTimeSync");
        if (syncParam && syncParam->load() > 0.5f) {
            // Sync mode: convert 0-1 to musical division Hz
            float knobValue = juce::jlimit(0.0f, 1.0f, rate);
            int divIndex = juce::jlimit(0, numDivisions - 1, (int)(knobValue * (numDivisions - 1)));
            Div div = allDivisions[divIndex];
            const double bpm = getBpmOrDefault(120.0);
            rate = syncedHz((float)bpm, div) * 0.5f;
        } else {
            // Free mode: map 0-1 to Hz range
            rate = 0.05f + rate * (90.0f - 0.05f);
        }
        
        // Set AutoPan parameters
        const float width = 1.0f;  // Full width for classic autopan behavior
        const float mix = 1.0f;    // Full wet mix
        const WaveType wType = static_cast<WaveType>(juce::jlimit(0, 4, waveTypeIndex));

        autoPan.setTargets(rate, depth, width, mix, waveShape, phaseOffset, wType, isInverted);
        
        // Process AutoPan effect AFTER delay
        if (buffer.getNumChannels() >= 2 && buffer.getNumSamples() > 0) {
            // Check if sync mode is enabled - if so, sync to transport
            bool syncToTransport = (syncParam && syncParam->load() > 0.5f);
            bool isPlaying = syncToTransport ? wasPlaying.load() : true;
            
            if (syncToTransport) {
                // Pass transport information for sync mode
                double bpm = getBpmOrDefault(120.0);
                double ppqPosition = transportCache.ppq.load();
                autoPan.process(buffer, isPlaying, syncToTransport, bpm, ppqPosition);
            } else {
                // Free-running mode
                autoPan.process(buffer, isPlaying, syncToTransport);
            }
            
            // Publish clock data for PanManBar visualizer (once per block)
            panClock.phase01.store(autoPan.phase, std::memory_order_release);
            panClock.incPerSample.store(autoPan.phaseIncSmooth.getCurrentValue(), std::memory_order_release);
            panClock.sampleRate.store(autoPan.sampleRate, std::memory_order_release);
        }
    }
    
    // Apply master dry/wet mix (post-effects, pre-output)
    auto* masterDryWetParam = valueTreeState.getRawParameterValue("masterDryWet");
    if (masterDryWetParam != nullptr) {
        float dryWet = masterDryWetParam->load(); // 0.0 = 100% dry, 1.0 = 100% wet
        
        // Only apply dry/wet mixing if it's not 100% wet (to avoid unnecessary processing)
        if (dryWet < 1.0f) {
            float dryGain = 1.0f - dryWet;  // Dry signal gain
            float wetGain = dryWet;         // Wet signal gain
            
            // Mix dry and wet signals properly
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                // Scale the wet signal (current buffer) by wet gain
                buffer.applyGainRamp(channel, 0, buffer.getNumSamples(), wetGain, wetGain);
                
                // Add the dry signal scaled by dry gain
                buffer.addFromWithRamp(channel, 0, dryBuffer.getReadPointer(channel), 
                                     buffer.getNumSamples(), dryGain, dryGain);
            }
        }
        // If dryWet == 1.0f (100% wet), the current buffer is already the wet signal, no mixing needed
    }
    
    // Apply master output gain (post-effects) with limiting
    auto* masterOutputParam = valueTreeState.getRawParameterValue("masterOutput");
    if (masterOutputParam != nullptr) {
        float outputGainDb = masterOutputParam->load();
        float outputGain = juce::Decibels::decibelsToGain(outputGainDb);
        buffer.applyGain(outputGain);
        
    }
    
    // Call AFTER processing = output meter
    updateMeters(buffer, outputMeter);
    
    // Legacy single-value tracking for compatibility
    outputLevel.store(juce::jmax(outputMeter.rmsDbL.load(), outputMeter.rmsDbR.load()));

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
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(now - standaloneStartTime).count();
        
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
        // For standalone mode, use sequencer origin if available, otherwise use 0
        origin = seq.haveOrigin.load() ? seq.originPPQ.load() : 0.0;
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

// AutoPan snapshot methods
StepSnapshot PluginProcessor::getAutoPanSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return autopanStepSnapshots[step];
    }
    return autopanStepSnapshots[0];
}

void PluginProcessor::setAutoPanStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        autopanStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateAutoPanCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = autopanUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific AutoPan parameter in the snapshot
    switch (knobIndex) {
        case 0: // Rate
            autopanStepSnapshots[currentStep].autopan.rate = value;
            break;
        case 1: // Phase
            autopanStepSnapshots[currentStep].autopan.phase = value;
            break;
        case 2: // Wave Type
            autopanStepSnapshots[currentStep].autopan.waveType = (int)value;
            break;
        case 3: // Wave Shape
            autopanStepSnapshots[currentStep].autopan.waveShape = value;
            break;
        case 4: // Inverted
            autopanStepSnapshots[currentStep].autopan.inverted = value > 0.5f;
            break;
        case 5: // Amount
            autopanStepSnapshots[currentStep].autopan.amount = value;
            break;
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
        {
            // Always read AutoPan parameters from APVTS (no sequencer snapshots for AutoPan yet)
            auto* rateParam = valueTreeState.getRawParameterValue("autopanRate");
            auto* amountParam = valueTreeState.getRawParameterValue("autopanAmount");
            
            // Set AutoPan parameters using new energy-stable API
            const float rate = rateParam ? rateParam->load() : 1.0f;  // Hz
            const float depth = amountParam ? amountParam->load() : 0.5f;  // 0..1 - no scaling needed with mid/side
            const float width = 1.0f;  // Full width for classic autopan behavior
            const float mix = 1.0f;    // Full wet mix
            
            // Get wave parameters
            auto* waveShapeParam = valueTreeState.getRawParameterValue("autopanWaveShape");
            auto* phaseParam = valueTreeState.getRawParameterValue("autopanPhase");
            auto* waveTypeParam = valueTreeState.getRawParameterValue("autopanWaveType");
            auto* invertedParam = valueTreeState.getRawParameterValue("autopanInverted");
            
            const float waveShape = waveShapeParam ? waveShapeParam->load() : 0.5f;
            const float phaseOffset = phaseParam ? phaseParam->load() / 360.0f : 0.5f; // Convert 0-360° to 0-1
            const int waveTypeIndex = waveTypeParam ? (int)waveTypeParam->load() : 0;
            const bool isInverted = invertedParam ? invertedParam->load() > 0.5f : false;
            
            const WaveType wType = static_cast<WaveType>(juce::jlimit(0, 4, waveTypeIndex));
            
            autoPan.setTargets(rate, depth, width, mix, waveShape, phaseOffset, wType, isInverted);
            break;
        }
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
    seq.currentStep.store(0);
    seq.originPPQ.store(0.0);
    seq.haveOrigin.store(false);
    seq.samplesIntoStep = 0.0;
    
    DBG("[Processor] Sequencer state reset - playingStep: " << seq.playingStep.load() << ", currentStep: " << seq.currentStep.load() << ", originPPQ: " << seq.originPPQ.load());
}

void PluginProcessor::startStandalonePlayback() noexcept
{
    // For standalone mode, simulate DAW transport by setting up the transport state
    wasPlaying.store(false);  // Will be set to true by the sequencer logic
    haveValidPos.store(true);
    armPending.store(false);
    lastPPQ.store(0.0);
    lastSamples.store(0);
    
    // Reset standalone start time
    standaloneStartTime = std::chrono::high_resolution_clock::now();
    
    // Enable the sequencer
    seq.active.store(true);
    seq.resetPhase();
    
    DBG("[Processor] Standalone playback started - sequencer active: " << seq.active.load());
}

// Helper function for sequencer
double PluginProcessor::divisionToBeats(int divIdx)
{
    // Map division indices to beats per step
    // 0:1/4 → 0.25 beats, 1:1/8 → 0.125, 2:1/16 → 0.0625, 3:1/32 → 0.03125
    // 4:1/2 → 0.5, 5:1 → 1.0, 6:2 → 2.0, 7:4 → 4.0
    static const double beatsPerStepLUT[] = { 0.25, 0.125, 0.0625, 0.03125, 0.5, 1.0, 2.0, 4.0 };
    const int i = juce::jlimit(0, (int)std::size(beatsPerStepLUT)-1, divIdx);
    return beatsPerStepLUT[i];
}




juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}