#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/DspFlags.h"
#include "dsp/PanSync.h"
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
    
    // Initialize Dirt sequencer state (independent)
    dirtSeq.enabled.store(false);
    dirtSeq.stepsUsed.store(16);
    dirtSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    dirtSeq.playingStep.store(0);
    
    // Initialize Chorus sequencer state (independent)
    chorusSeq.enabled.store(false);
    chorusSeq.stepsUsed.store(16);
    chorusSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    chorusSeq.playingStep.store(0);
    
    // Initialize Reverb sequencer state (starts enabled like delay sequencer)
    reverbSeq.enabled.store(true); // Start enabled so it auto-activates on play
    reverbSeq.stepsUsed.store(16);
    reverbSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    reverbSeq.playingStep.store(0);
    
    // Initialize Granular sequencer state (starts enabled)
    granularSeq.enabled.store(true); // Start enabled so it auto-activates on play
    granularSeq.stepsUsed.store(16);
    granularSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    granularSeq.playingStep.store(0);
    
    // Initialize UI state
    uiSelectedStep.store(0);
    autopanUiSelectedStep.store(0);
    dirtUiSelectedStep.store(0);
    chorusUiSelectedStep.store(0);
    reverbUiSelectedStep.store(0);
    granularUiSelectedStep.store(0);
    
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
    
    // Initialize Dirt step snapshots with default values
    for (int i = 0; i < 16; ++i) {
        dirtStepSnapshots[i].dirt.drive = 12.0f;      // 12 dB default
        dirtStepSnapshots[i].dirt.color = 0.0f;       // Neutral
        dirtStepSnapshots[i].dirt.asym = 0.0f;        // No asymmetry
        dirtStepSnapshots[i].dirt.texture = 0.35f;    // Warm/medium
        dirtStepSnapshots[i].dirt.lowCut = 60.0f;     // 60 Hz
        dirtStepSnapshots[i].dirt.highCut = 12000.0f; // 12 kHz
        dirtStepSnapshots[i].dirt.tone = 0.0f;        // Neutral
        dirtStepSnapshots[i].dirt.mix = 1.0f;         // 100% wet
    }
    DBG("[Stepper] Initialized Dirt step snapshots with default values");
    
    // Initialize Chorus step snapshots with default values
    for (int i = 0; i < 16; ++i) {
        chorusStepSnapshots[i].chorus.rate = 1.5f;       // 1.5 Hz
        chorusStepSnapshots[i].chorus.depth = 40.0f;     // 40%
        chorusStepSnapshots[i].chorus.voices = 2.0f;     // 2 voices
        chorusStepSnapshots[i].chorus.delayTime = 20.0f; // 20 ms
        chorusStepSnapshots[i].chorus.feedback = 20.0f;  // 20%
        chorusStepSnapshots[i].chorus.width = 100.0f;    // 100%
        chorusStepSnapshots[i].chorus.tone = 0.0f;       // Neutral
        chorusStepSnapshots[i].chorus.mix = 50.0f;       // 50%
    }
    DBG("[Stepper] Initialized Chorus step snapshots with default values");
    
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
    
    // Dirt Parameters - 8 knobs
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtDrive", "Dirt Drive", 0.0f, 36.0f, 12.0f)); // dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtColor", "Dirt Color", -1.0f, 1.0f, 0.0f)); // pre-emphasis tilt
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtAsym", "Dirt Asym", -1.0f, 1.0f, 0.0f)); // bias/asymmetry
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtTexture", "Dirt Texture", 0.0f, 1.0f, 0.35f)); // curve hardness
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtLowCut", "Dirt Low-Cut", 20.0f, 300.0f, 60.0f)); // Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtHighCut", "Dirt High-Cut", 3000.0f, 22000.0f, 12000.0f)); // Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtTone", "Dirt Tone", -1.0f, 1.0f, 0.0f)); // post tilt
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dirtMix", "Dirt Mix", 0.0f, 1.0f, 1.0f)); // dry/wet
    
    // Chorus Parameters (best-in-class 8-knob DSP)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusDelayMs",  "Ch Delay",   juce::NormalisableRange<float>(5.0f, 50.0f, 0.01f, 0.4f), 18.0f)); // base delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusRateHz",   "Ch Rate",    juce::NormalisableRange<float>(0.02f, 8.0f, 0.0f, 0.3f),    0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusDepthMs",  "Ch Depth",   juce::NormalisableRange<float>(0.0f,  12.0f, 0.0f, 0.5f),    5.0f)); // modulation amplitude in ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusFeedback", "Ch Fdbk",    juce::NormalisableRange<float>(0.0f,  0.9f,  0.0f, 1.0f),     0.15f));
    params.push_back(std::make_unique<juce::AudioParameterInt  >("chorusVoices",   "Ch Voices",  2, 8, 4));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusWidth",    "Ch Width",   juce::NormalisableRange<float>(0.0f,  1.0f,  0.0f, 1.0f),    0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusShape",    "Ch Shape",   juce::NormalisableRange<float>(0.0f,  1.0f,  0.0f, 1.0f),    0.25f)); // 0=sin .. 0.5=tri .. 1=soft square
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusMix",      "Ch Mix",     juce::NormalisableRange<float>(0.0f,  1.0f,  0.0f, 1.0f),    0.5f));
    
    // Page and effect enable parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>("currentPage", "Current Page", 
        juce::StringArray {"SpaceDelay", "AutoPan", "Dirt", "Chorus"}, 0)); // 0 = SpaceDelay, 1 = AutoPan, 2 = Dirt, 3 = Chorus
    params.push_back(std::make_unique<juce::AudioParameterBool>("autopanEnabled", "AutoPan Enabled", true)); // AutoPan effect enabled - ON by default
    params.push_back(std::make_unique<juce::AudioParameterBool>("autopanTimeSync", "AutoPan Time Sync", true)); // AutoPan sync mode enabled - ON by default
    params.push_back(std::make_unique<juce::AudioParameterBool>("dirtEnabled", "Dirt Enabled", true)); // Dirt effect enabled - ON by default
    params.push_back(std::make_unique<juce::AudioParameterBool>("chorusEnabled", "Chorus Enabled", true)); // Chorus effect enabled - ON by default
    
    // Master Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterInput", "Master Input", 
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.01f, 1.0f), 0.0f)); // -60 to +6 dB, default 0.0 dB, logarithmic skew
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterDryWet", "Master Dry/Wet", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterOutput", "Master Output", 
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.01f, 1.0f), 0.0f)); // -60 to +6 dB, default 0.0 dB, logarithmic skew
    
    // Master HP/LP Filters (log-scaled for frequency)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterHPHz", "HPF",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.5f), 20.0f)); // Start at 20 Hz (bypass)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterLPHz", "LPF",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.5f), 20000.0f)); // Start at 20 kHz (bypass)
    
    // Reverb Parameters (8 knobs: Width, Size, Predelay, Damping, Diffusion, Early, Decay, Mix)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbWidth", "Width", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f)); // Stereo width
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbSize", "Size", juce::NormalisableRange<float>(0.1f, 1.5f, 0.0f, 0.7f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbPredelayMs", "PreDelay", juce::NormalisableRange<float>(0.0f, 200.0f, 0.01f, 0.5f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbDampHz", "Damping", juce::NormalisableRange<float>(1000.0f, 20000.0f, 0.0f, 0.5f), 8000.0f)); // log-ish
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbDiffusion", "Diffusion", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbEarlyLevel", "Early", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.35f)); // room only
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbDecaySec", "Decay", juce::NormalisableRange<float>(0.2f, 20.0f, 0.0f, 0.5f), 4.0f)); // RT60 in seconds
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "verbMix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("verbEnabled", "Reverb Enabled", true)); // Reverb effect enabled
    
    // Granular parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granSizeMs", "Grain Size", juce::NormalisableRange<float>(5.0f, 200.0f, 0.0f, 0.6f), 40.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granDensityHz", "Density", juce::NormalisableRange<float>(1.0f, 40.0f, 0.0f, 0.5f), 10.0f)); // Reduced max to 40Hz, default to 10Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granPosition", "Position", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f)); // 0=oldest, 1=latest
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granSprayMs", "Spray", juce::NormalisableRange<float>(0.0f, 200.0f, 0.0f, 0.5f), 35.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granPitchSemi", "Pitch", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granRandom", "Random", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granTexture", "Texture", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "granMix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f)); // Start at 50% mix
    params.push_back(std::make_unique<juce::AudioParameterBool>("granEnabled", "Granular Enabled", true)); // Start enabled
    params.push_back(std::make_unique<juce::AudioParameterBool>("granDensitySync", "Granular Density Sync", false)); // Density sync mode
    
    // Rhythm Gate Parameters (8 knobs + 1 toggle)
    params.push_back(std::make_unique<juce::AudioParameterInt>("gatePattern", "Gate Pattern", 0, 7, 0)); // 8 patterns
    params.push_back(std::make_unique<juce::AudioParameterInt>("gateDivision", "Gate Division", 0, 5, 3)); // 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, 5=1/32
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateOffset", "Gate Offset", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.0f)); // Pattern phase shift
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateShape", "Gate Shape", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.35f)); // 0=hard/exp, 1=smooth/sine
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gatePitchSemi", "Gate Pitch", 
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f, 1.0f), 0.0f)); // Varispeed pitch
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateReverse", "Gate Reverse", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.0f)); // Reverse probability/mix
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateGlitch", "Gate Glitch", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.0f)); // Micro-stutter amount
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateMix", "Gate Mix", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.75f)); // Wet/dry
    params.push_back(std::make_unique<juce::AudioParameterBool>("gateSync", "Gate Sync", true)); // Tempo sync toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>("gateEnabled", "Gate Enabled", false)); // Start disabled to prevent crashes
    
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
    dirt.prepare(sampleRate, samplesPerBlock); // Prepare Dirt saturation
    chorus.prepare(sampleRate, samplesPerBlock); // Prepare Chorus effect
    hall.prepare(sampleRate, samplesPerBlock, 300); // Prepare JUCE Hall (300ms max predelay)
    granular.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels()); // Prepare Granular engine
    rhythmGate.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels()); // Prepare Rhythm Gate engine
    seq.prepare(sampleRate); // Initialize delay sequencer with sample rate
    autopanSeq.prepare(sampleRate); // Initialize AutoPan sequencer with sample rate
    dirtSeq.prepare(sampleRate); // Initialize Dirt sequencer with sample rate
    chorusSeq.prepare(sampleRate); // Initialize Chorus sequencer with sample rate
    reverbSeq.prepare(sampleRate); // Initialize Reverb sequencer with sample rate
    granularSeq.prepare(sampleRate); // Initialize Granular sequencer with sample rate
    
    // Prepare output visualizer buffer (store ~1 second of downsampled audio)
    const int bufferSize = (int)(sampleRate / downsampleRate); // ~1 second at downsample rate
    outputVisualizerBuffer.prepare(bufferSize);
    downsampleCounter = 0;
    
    // Prepare spectrum analyzer
    spectrumAnalyzer.prepare(sampleRate);
    
    // Prepare master HP/LP filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 2;
    
    for (int ch = 0; ch < 2; ++ch)
    {
        masterHPF[ch].reset();
        masterLPF[ch].reset();
        masterHPF[ch].prepare(spec);
        masterLPF[ch].prepare(spec);
        
        masterHPF[ch].setType(juce::dsp::StateVariableTPTFilterType::highpass);
        masterLPF[ch].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        
        // Butterworth-like 12 dB/oct: Q = 1/sqrt(2) ≈ 0.707
        masterHPF[ch].setResonance(1.0f / std::sqrt(2.0f));
        masterLPF[ch].setResonance(1.0f / std::sqrt(2.0f));
    }
    
    // Smooth cutoffs (glide ~50 ms for smooth sweeps)
    const double glideSec = 0.05;
    hpCutoffSmooth.reset(sampleRate, glideSec);
    lpCutoffSmooth.reset(sampleRate, glideSec);
    
    // Initialize from current params
    auto* hpParam = valueTreeState.getRawParameterValue("masterHPHz");
    auto* lpParam = valueTreeState.getRawParameterValue("masterLPHz");
    if (hpParam) hpCutoffSmooth.setCurrentAndTargetValue(hpParam->load());
    if (lpParam) lpCutoffSmooth.setCurrentAndTargetValue(lpParam->load());
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
                dirtSeq.resetPhase();      // Dirt sequencer phase reset
                chorusSeq.resetPhase();    // Chorus sequencer phase reset
                reverbSeq.resetPhase();    // Reverb sequencer phase reset
                
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
                    DBG("[AUTOPAN SEQ] ✗ NOT activated (enabled=" + juce::String(autopanSeq.enabled.load() ? 1 : 0) + ")");
                }
                
                // Dirt sequencer activates if enabled (independent of followHost)
                if (dirtSeq.enabled.load()) {
                    dirtSeq.active.store(true);  // Activate Dirt sequencer
                    DBG("[DIRT SEQ] ✓ Activated on play edge");
                }
                
                // Chorus sequencer activates if enabled (independent of followHost)
                if (chorusSeq.enabled.load()) {
                    chorusSeq.active.store(true);  // Activate Chorus sequencer
                    DBG("[CHORUS SEQ] ✓ Activated on play edge");
                }
                
                // Reverb sequencer activates if enabled (independent of followHost)
                if (reverbSeq.enabled.load()) {
                    reverbSeq.active.store(true);  // Activate Reverb sequencer
                    DBG("[REVERB SEQ] ✓ Activated on play edge");
                }
                
                // Granular sequencer activates if enabled (independent of followHost)
                if (granularSeq.enabled.load()) {
                    granularSeq.active.store(true);  // Activate Granular sequencer
                    DBG("[GRANULAR SEQ] ✓ Activated on play edge");
                }
                
                DBG("[SEQ] Play edge detected");
                DBG("[SEQ] Delay: enabled=" + juce::String(seq.enabled.load() ? 1 : 0) + " active=" + juce::String(seq.active.load() ? 1 : 0));
                DBG("[SEQ] AutoPan: enabled=" + juce::String(autopanSeq.enabled.load() ? 1 : 0) + " active=" + juce::String(autopanSeq.active.load() ? 1 : 0));
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
                
                // Also lock-in Dirt sequencer if enabled
                if (dirtSeq.enabled.load()) {
                    const int dirtStep = dirtSeq.computeStepFromPPQ(ppq);
                    dirtSeq.currentStep.store(dirtStep);
                    dirtSeq.playingStep.store(dirtStep);
                    DBG("[DIRT SEQ] Lock-in at PPQ=" << ppq << " -> step " << dirtStep);
                }
                
                // Also lock-in Chorus sequencer if enabled
                if (chorusSeq.enabled.load()) {
                    const int chorusStep = chorusSeq.computeStepFromPPQ(ppq);
                    chorusSeq.currentStep.store(chorusStep);
                    chorusSeq.playingStep.store(chorusStep);
                    DBG("[CHORUS SEQ] Lock-in at PPQ=" << ppq << " -> step " << chorusStep);
                }
                
                // Also lock-in Reverb sequencer if enabled AND active
                if (reverbSeq.enabled.load() && reverbSeq.active.load()) {
                    const int reverbStep = reverbSeq.computeStepFromPPQ(ppq);
                    reverbSeq.currentStep.store(reverbStep);
                    reverbSeq.playingStep.store(reverbStep);
                    DBG("[REVERB SEQ] Lock-in at PPQ=" << ppq << " -> step " << reverbStep);
                }
                
                // Also lock-in Granular sequencer if enabled AND active
                if (granularSeq.enabled.load() && granularSeq.active.load()) {
                    const int granularStep = granularSeq.computeStepFromPPQ(ppq);
                    granularSeq.currentStep.store(granularStep);
                    granularSeq.playingStep.store(granularStep);
                    DBG("[GRANULAR SEQ] Lock-in at PPQ=" << ppq << " -> step " << granularStep);
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
                DBG("[AUTOPAN SEQ DEBUG] isPlaying=" + juce::String(isPlaying ? 1 : 0) + " ppqValid=" + juce::String(ppqValid ? 1 : 0) 
                    + " active=" + juce::String(autopanSeq.active.load() ? 1 : 0) + " enabled=" + juce::String(autopanSeq.enabled.load() ? 1 : 0)
                    + " PPQ=" + juce::String(ppq));
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
                DBG("[AUTOPAN SEQ] WARNING: Enabled but not active! isPlaying=" + juce::String(isPlaying ? 1 : 0) + " ppqValid=" + juce::String(ppqValid ? 1 : 0));
            }
            
            // Dirt sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && dirtSeq.active.load()) {
                const int dirtStep = dirtSeq.computeStepFromPPQ(ppq);
                if (dirtStep != dirtSeq.currentStep.load()) {
                    dirtSeq.currentStep.store(dirtStep);
                    dirtSeq.playingStep.store(dirtStep);
                    DBG("[DIRT SEQ] ★ Step changed to: " << dirtStep << " PPQ: " << ppq);
                }
            }
            
            // Chorus sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && chorusSeq.active.load()) {
                const int chorusStep = chorusSeq.computeStepFromPPQ(ppq);
                if (chorusStep != chorusSeq.currentStep.load()) {
                    chorusSeq.currentStep.store(chorusStep);
                    chorusSeq.playingStep.store(chorusStep);
                    DBG("[CHORUS SEQ] ★ Step changed to: " << chorusStep << " PPQ: " << ppq);
                }
            }
            
            // Reverb sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && reverbSeq.active.load()) {
                const int reverbStep = reverbSeq.computeStepFromPPQ(ppq);
                if (reverbStep != reverbSeq.currentStep.load()) {
                    reverbSeq.currentStep.store(reverbStep);
                    reverbSeq.playingStep.store(reverbStep);
                    DBG("[REVERB SEQ] ★ Step changed to: " << reverbStep << " PPQ: " << ppq);
                }
            }
            
            // Granular sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && granularSeq.active.load()) {
                const int granularStep = granularSeq.computeStepFromPPQ(ppq);
                if (granularStep != granularSeq.currentStep.load()) {
                    granularSeq.currentStep.store(granularStep);
                    granularSeq.playingStep.store(granularStep);
                    DBG("[GRANULAR SEQ] ★ Step changed to: " << granularStep << " PPQ: " << ppq);
                }
            }
            
            // Publish AutoPan Sequencer Visual Clock (independent from Delay sequencer)
            autopanSeqClock.ppqAtBlockStart.store(ppq, std::memory_order_release);
            autopanSeqClock.isPlaying.store(isPlaying, std::memory_order_release);
            
            // Calculate PPQ per sample from BPM
            const double bpm = pos->getBpm().hasValue() ? *pos->getBpm() : 120.0;
            const double ppqPerSample = (bpm / 60.0) / dspSampleRate;
            autopanSeqClock.ppqPerSample.store(ppqPerSample, std::memory_order_release);
            
            // Time signature
            if (pos->getTimeSignature().hasValue()) {
                auto ts = *pos->getTimeSignature();
                autopanSeqClock.timeSigNumerator.store(ts.numerator, std::memory_order_release);
                autopanSeqClock.timeSigDenominator.store(ts.denominator, std::memory_order_release);
            }
            
            // Loop points
            if (pos->getLoopPoints().hasValue()) {
                auto loop = *pos->getLoopPoints();
                autopanSeqClock.loopStartPPQ.store(loop.ppqStart, std::memory_order_release);
                autopanSeqClock.loopEndPPQ.store(loop.ppqEnd, std::memory_order_release);
            } else {
                autopanSeqClock.loopStartPPQ.store(-1.0, std::memory_order_release);
                autopanSeqClock.loopEndPPQ.store(-1.0, std::memory_order_release);
            }
            
            // Bar start PPQ (calculate from current PPQ and time signature)
            if (pos->getPpqPositionOfLastBarStart().hasValue()) {
                autopanSeqClock.barStartPPQ.store(*pos->getPpqPositionOfLastBarStart(), std::memory_order_release);
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
    
    // === DYNAMIC EFFECT ROUTING ===
    // Process effects in page order (Slot1 → Slot2 → Slot3 → Slot4)
    // Routing order = page order (left to right)
    auto routingOrder = effectRouter.getRoutingOrder();
    
    for (int i = 0; i < 4; ++i)
    {
        EffectID effect = routingOrder[i];
        
        switch (effect)
        {
            case EffectID::SpaceDelay:
                processDelayEffect(buffer);
                break;
                
            case EffectID::AutoPan:
                processAutoPanEffect(buffer);
                break;
                
            case EffectID::Dirt:
                processDirtEffect(buffer);
                break;
                
            case EffectID::Chorus:
                processChorusEffect(buffer);
                break;
                
            case EffectID::Reverb:
            {
                // Process Reverb effect with safety guards
                auto* verbEnabledParam = valueTreeState.getRawParameterValue("verbEnabled");
                bool isVerbEnabled = verbEnabledParam ? (verbEnabledParam->load() > 0.5f) : false;
                
                if (!isVerbEnabled) {
                    break; // Early out if disabled
                }
                
                if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
                    DBG("[REVERB] WARNING: Invalid buffer dimensions");
                    break;
                }
                
                if (true) // Scoped block for local variables
                {
                    // Read reverb parameters (from sequencer snapshot or APVTS)
                    float width, size, predelay, dampHz, diffusion, early, decaySec, mix;
                    
                    // Check if Reverb sequencer is enabled AND active
                    if (reverbSeq.enabled.load() && reverbSeq.active.load()) {
                        // Use Reverb sequencer snapshot
                        int reverbStep = reverbSeq.currentStep.load();
                        const auto& snapshot = reverbStepSnapshots[juce::jlimit(0, 15, reverbStep)];
                        
                        width     = snapshot.reverb.type; // Repurpose type field as width
                        size      = snapshot.reverb.size;
                        predelay  = snapshot.reverb.predelayMs;
                        dampHz    = snapshot.reverb.dampHz;
                        diffusion = snapshot.reverb.diffusion;
                        early     = snapshot.reverb.early;
                        decaySec  = snapshot.reverb.decaySec;
                        mix       = snapshot.reverb.mix;
                    } else {
                        // Use APVTS parameters (manual control)
                        width     = valueTreeState.getRawParameterValue("verbWidth")->load();
                        size      = valueTreeState.getRawParameterValue("verbSize")->load();
                        predelay  = valueTreeState.getRawParameterValue("verbPredelayMs")->load();
                        dampHz    = valueTreeState.getRawParameterValue("verbDampHz")->load();
                        diffusion = valueTreeState.getRawParameterValue("verbDiffusion")->load();
                        early     = valueTreeState.getRawParameterValue("verbEarlyLevel")->load();
                        decaySec  = valueTreeState.getRawParameterValue("verbDecaySec")->load();
                        mix       = valueTreeState.getRawParameterValue("verbMix")->load();
                    }
                    
                    // Update reverb targets (smoothed internally)
                    hall.pushParams(size, predelay, dampHz, diffusion, early, decaySec, width, mix);
                    
                    // Process in-place
                    hall.process(buffer);
                }
                break;
            }
            
            case EffectID::Granular:
            {
                // Process Granular effect
                auto* granEnabledParam = valueTreeState.getRawParameterValue("granEnabled");
                bool isGranEnabled = granEnabledParam ? (granEnabledParam->load() > 0.5f) : false;
                
                if (!isGranEnabled || buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
                    break;
                }
                
                // Read granular parameters (from sequencer snapshot if active, else APVTS)
                // NOTE: Mix is ALWAYS read from APVTS (global control, not per-step)
                float sizeMs, densityHz, position, sprayMs, pitchSemi, randomAmt, texture;
                
                // Mix is always global (not sequenced)
                auto* mixParam = valueTreeState.getRawParameterValue("granMix");
                float mix = mixParam ? mixParam->load() : 0.5f;
                
                // Check if Granular sequencer is enabled AND active
                bool useSequencer = granularSeq.enabled.load() && granularSeq.active.load();
                
                if (useSequencer)
                {
                    // Read from sequencer snapshot (except mix)
                    int currentStep = granularSeq.currentStep.load();
                    if (currentStep >= 0 && currentStep < 16)
                    {
                        auto snapshot = granularStepSnapshots[currentStep];
                        sizeMs     = snapshot.granular.sizeMs;
                        
                        // Convert density: if sync enabled, interpret as division index
                        auto* densitySyncParam = valueTreeState.getRawParameterValue("granDensitySync");
                        bool densitySyncEnabled = densitySyncParam && (densitySyncParam->load() > 0.5f);
                        
                        if (densitySyncEnabled) {
                            // Density value is division index (0-7)
                            int divIdx = juce::jlimit(0, 7, (int)std::round(snapshot.granular.densityHz));
                            double bpm = getBpmOrDefault(120.0);
                            std::vector<float> divHz = {
                                (float)(bpm/30.0), (float)(bpm/60.0), (float)(bpm/120.0), (float)(bpm/240.0),
                                (float)(bpm/480.0), (float)(bpm/960.0), (float)(bpm/1920.0), (float)(bpm/3840.0)
                            };
                            densityHz = divHz[divIdx];
                        } else {
                            densityHz = snapshot.granular.densityHz;
                        }
                        
                        position   = snapshot.granular.position;
                        sprayMs    = snapshot.granular.sprayMs;
                        pitchSemi  = snapshot.granular.pitchSemi;
                        randomAmt  = snapshot.granular.random;
                        texture    = snapshot.granular.texture;
                        // mix already set from APVTS above
                    }
                    else
                    {
                        // Fallback to APVTS if step invalid
                        auto* sizeParam     = valueTreeState.getRawParameterValue("granSizeMs");
                        auto* densityParam  = valueTreeState.getRawParameterValue("granDensityHz");
                        auto* positionParam = valueTreeState.getRawParameterValue("granPosition");
                        auto* sprayParam    = valueTreeState.getRawParameterValue("granSprayMs");
                        auto* pitchParam    = valueTreeState.getRawParameterValue("granPitchSemi");
                        auto* randomParam   = valueTreeState.getRawParameterValue("granRandom");
                        auto* textureParam  = valueTreeState.getRawParameterValue("granTexture");
                        
                        if (!sizeParam || !densityParam || !positionParam || !sprayParam || 
                            !pitchParam || !randomParam || !textureParam) {
                            break;
                        }
                        
                        sizeMs     = sizeParam->load();
                        densityHz  = densityParam->load();
                        position   = positionParam->load();
                        sprayMs    = sprayParam->load();
                        pitchSemi  = pitchParam->load();
                        randomAmt  = randomParam->load();
                        texture    = textureParam->load();
                        // mix already set from APVTS above
                    }
                }
                else
                {
                    // Read directly from APVTS when sequencer not active (except mix which is always APVTS)
                    auto* sizeParam     = valueTreeState.getRawParameterValue("granSizeMs");
                    auto* densityParam  = valueTreeState.getRawParameterValue("granDensityHz");
                    auto* positionParam = valueTreeState.getRawParameterValue("granPosition");
                    auto* sprayParam    = valueTreeState.getRawParameterValue("granSprayMs");
                    auto* pitchParam    = valueTreeState.getRawParameterValue("granPitchSemi");
                    auto* randomParam   = valueTreeState.getRawParameterValue("granRandom");
                    auto* textureParam  = valueTreeState.getRawParameterValue("granTexture");
                    
                    if (!sizeParam || !densityParam || !positionParam || !sprayParam || 
                        !pitchParam || !randomParam || !textureParam) {
                        DBG("[GRANULAR] ERROR: Missing parameter!");
                        break;
                    }
                    
                    sizeMs     = sizeParam->load();
                    
                    // Convert density if sync enabled
                    auto* densitySyncParam = valueTreeState.getRawParameterValue("granDensitySync");
                    bool densitySyncEnabled = densitySyncParam && (densitySyncParam->load() > 0.5f);
                    
                    if (densitySyncEnabled) {
                        int divIdx = juce::jlimit(0, 7, (int)std::round(densityParam->load()));
                        double bpm = getBpmOrDefault(120.0);
                        std::vector<float> divHz = {
                            (float)(bpm/30.0), (float)(bpm/60.0), (float)(bpm/120.0), (float)(bpm/240.0),
                            (float)(bpm/480.0), (float)(bpm/960.0), (float)(bpm/1920.0), (float)(bpm/3840.0)
                        };
                        densityHz = divHz[divIdx];
                    } else {
                        densityHz = densityParam->load();
                    }
                    
                    position   = positionParam->load();
                    sprayMs    = sprayParam->load();
                    pitchSemi  = pitchParam->load();
                    randomAmt  = randomParam->load();
                    texture    = textureParam->load();
                    // mix already set from APVTS above
                }
                
                // Set parameters and process
                granular.setParameters(sizeMs, densityHz, position, sprayMs, pitchSemi, randomAmt, texture, mix);
                granular.process(buffer);
                break;
            }
            
            case EffectID::RhythmGate:
            {
                // Check if effect is enabled
                auto* gateEnabledParam = valueTreeState.getRawParameterValue("gateEnabled");
                bool isGateEnabled = gateEnabledParam ? (gateEnabledParam->load() > 0.5f) : false;
                
                if (isGateEnabled)
                {
                    // Get parameters from APVTS
                    auto* patternParam = valueTreeState.getRawParameterValue("gatePattern");
                    auto* divisionParam = valueTreeState.getRawParameterValue("gateDivision");
                    auto* offsetParam = valueTreeState.getRawParameterValue("gateOffset");
                    auto* shapeParam = valueTreeState.getRawParameterValue("gateShape");
                    auto* pitchParam = valueTreeState.getRawParameterValue("gatePitchSemi");
                    auto* reverseParam = valueTreeState.getRawParameterValue("gateReverse");
                    auto* glitchParam = valueTreeState.getRawParameterValue("gateGlitch");
                    auto* mixParam = valueTreeState.getRawParameterValue("gateMix");
                    auto* syncParam = valueTreeState.getRawParameterValue("gateSync");
                    
                    int patternIdx = patternParam ? static_cast<int>(patternParam->load()) : 0;
                    int divisionIdx = divisionParam ? static_cast<int>(divisionParam->load()) : 3;
                    float offset01 = offsetParam ? offsetParam->load() : 0.0f;
                    float shape01 = shapeParam ? shapeParam->load() : 0.35f;
                    float pitchSemi = pitchParam ? pitchParam->load() : 0.0f;
                    float reverse01 = reverseParam ? reverseParam->load() : 0.0f;
                    float glitch01 = glitchParam ? glitchParam->load() : 0.0f;
                    float mix01 = mixParam ? mixParam->load() : 0.75f;
                    bool syncOn = syncParam ? (syncParam->load() > 0.5f) : true;
                    
                    // Update tempo info
                    rhythmGate.setTempoInfo(transportCache.playing.load(), transportCache.bpm.load(), 
                                           transportCache.ppq.load(), transportCache.tsNum.load());
                    
                    // Set parameters
                    rhythmGate.setParameters(patternIdx, divisionIdx, offset01, shape01, 
                                           pitchSemi, reverse01, glitch01, mix01, syncOn);
                    
                    // Process
                    rhythmGate.process(buffer);
                }
                break;
            }
        }
    }
    
    // Skip old hardcoded effect processing (now done via router above)
    #if 0
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
        
        // Check if AutoPan sequencer is enabled AND active
        if (autopanSeq.enabled.load() && autopanSeq.active.load()) {
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
    
    // === DIRT SATURATION (Post-Panner) ===
    // Processing order: Delay → Panner → Dirt
    auto* dirtEnabledParam = valueTreeState.getRawParameterValue("dirtEnabled");
    bool isDirtEnabled = dirtEnabledParam ? (dirtEnabledParam->load() > 0.5f) : false;
    
    if (isDirtEnabled) {
        float drive, color, asym, texture, lowCut, highCut, tone, mix;
        
        // Check if Dirt sequencer is enabled AND active
        if (dirtSeq.enabled.load() && dirtSeq.active.load()) {
            // Use Dirt sequencer snapshot
            int dirtStep = dirtSeq.currentStep.load();
            const auto& snapshot = dirtStepSnapshots[juce::jlimit(0, 15, dirtStep)];
            
            drive = snapshot.dirt.drive;
            color = snapshot.dirt.color;
            asym = snapshot.dirt.asym;
            texture = snapshot.dirt.texture;
            lowCut = snapshot.dirt.lowCut;
            highCut = snapshot.dirt.highCut;
            tone = snapshot.dirt.tone;
            mix = snapshot.dirt.mix;
        } else {
            // Use APVTS parameters (manual control)
            drive = valueTreeState.getRawParameterValue("dirtDrive")->load();
            color = valueTreeState.getRawParameterValue("dirtColor")->load();
            asym = valueTreeState.getRawParameterValue("dirtAsym")->load();
            texture = valueTreeState.getRawParameterValue("dirtTexture")->load();
            lowCut = valueTreeState.getRawParameterValue("dirtLowCut")->load();
            highCut = valueTreeState.getRawParameterValue("dirtHighCut")->load();
            tone = valueTreeState.getRawParameterValue("dirtTone")->load();
            mix = valueTreeState.getRawParameterValue("dirtMix")->load();
        }
        
        // Set targets and process
        dirt.setTargets(drive, color, asym, texture, lowCut, highCut, tone, mix);
        dirt.process(buffer);
    }
    
    // === CHORUS (Post-Dirt) ===
    // Processing order: Delay → Panner → Dirt → Chorus
    auto* chorusEnabledParam = valueTreeState.getRawParameterValue("chorusEnabled");
    bool isChorusEnabled = chorusEnabledParam ? (chorusEnabledParam->load() > 0.5f) : false;
    
    if (isChorusEnabled) {
        int voices;
        float baseDelayMs, rateHz, depthMs, width, feedback, shape, mix;
        
        // Check if Chorus sequencer is enabled AND active
        if (chorusSeq.enabled.load() && chorusSeq.active.load()) {
            // Use Chorus sequencer snapshot
            int chorusStep = chorusSeq.currentStep.load();
            const auto& snapshot = chorusStepSnapshots[juce::jlimit(0, 15, chorusStep)];
            
            baseDelayMs = snapshot.chorus.delayTime;  // delayMs
            rateHz = snapshot.chorus.rate;            // rateHz
            depthMs = snapshot.chorus.depth;          // depthMs
            voices = (int)snapshot.chorus.voices;     // voices (int)
            width = snapshot.chorus.width;            // width (0-1)
            feedback = snapshot.chorus.feedback;      // feedback (0-1)
            shape = snapshot.chorus.tone;             // shape (0-1)
            mix = snapshot.chorus.mix;                // mix (0-1)
        } else {
            // Use APVTS parameters (manual control)
            baseDelayMs = valueTreeState.getRawParameterValue("chorusDelayMs")->load();
            rateHz = valueTreeState.getRawParameterValue("chorusRateHz")->load();
            depthMs = valueTreeState.getRawParameterValue("chorusDepthMs")->load();
            voices = (int)valueTreeState.getRawParameterValue("chorusVoices")->load();
            width = valueTreeState.getRawParameterValue("chorusWidth")->load();
            feedback = valueTreeState.getRawParameterValue("chorusFeedback")->load();
            shape = valueTreeState.getRawParameterValue("chorusShape")->load();
            mix = valueTreeState.getRawParameterValue("chorusMix")->load();
        }
        
        // Set smoothed targets and process
        chorus.setParams(voices, baseDelayMs, rateHz, depthMs, width, feedback, shape, mix);
        chorus.process(buffer);
    }
    #endif // End of old hardcoded effect processing (replaced by router above)
    
    // Apply master HP/LP filters to WET signal only (before dry/wet mix)
    auto* hpParam = valueTreeState.getRawParameterValue("masterHPHz");
    auto* lpParam = valueTreeState.getRawParameterValue("masterLPHz");
    
    if (hpParam && lpParam)
    {
        const float hpTarget = juce::jlimit(20.0f, 20000.0f, hpParam->load());
        const float lpTarget = juce::jlimit(20.0f, 20000.0f, lpParam->load());
        
        hpCutoffSmooth.setTargetValue(hpTarget);
        lpCutoffSmooth.setTargetValue(lpTarget);
        
        const int numSamplesLocal = buffer.getNumSamples();
        const int numChannelsLocal = buffer.getNumChannels();
        
        for (int n = 0; n < numSamplesLocal; ++n)
        {
            const float hpHz = hpCutoffSmooth.getNextValue();
            const float lpHz = lpCutoffSmooth.getNextValue();
            
            // Update both channels' cutoffs (per-sample for ultra-smooth sweeps)
            masterHPF[0].setCutoffFrequency(hpHz);
            masterLPF[0].setCutoffFrequency(lpHz);
            
            if (numChannelsLocal > 1)
            {
                masterHPF[1].setCutoffFrequency(hpHz);
                masterLPF[1].setCutoffFrequency(lpHz);
            }
            
            // Process each channel (wet signal only)
            for (int ch = 0; ch < numChannelsLocal; ++ch)
            {
                float x = buffer.getWritePointer(ch)[n];
                
                // Apply HPF then LPF to wet signal
                if (hpHz > 20.0f)
                    x = masterHPF[ch].processSample(0, x);
                if (lpHz < 20000.0f)
                    x = masterLPF[ch].processSample(0, x);
                
                buffer.getWritePointer(ch)[n] = x;
            }
        }
    }
    
    // Apply master dry/wet mix (post-effects & filters, pre-output)
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
    
    // Apply master output gain (post dry/wet mix)
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
    
    // Feed output visualizer (post-FX, downsampled)
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels > 0) {
        for (int i = 0; i < numSamples; ++i) {
            if (++downsampleCounter >= downsampleRate) {
                downsampleCounter = 0;
                
                // Calculate mono amplitude (average L+R if stereo)
                float monoSample = buffer.getSample(0, i);
                if (numChannels > 1) {
                    monoSample = (monoSample + buffer.getSample(1, i)) * 0.5f;
                }
                
                // Push to ring buffer for visualization
                outputVisualizerBuffer.push(monoSample);
            }
        }
    }
    
    // Feed spectrum analyzer (post-FX output)
    // Process entire buffer for FFT analysis (read-only, doesn't modify output)
    spectrumAnalyzer.processBlock(buffer.getArrayOfReadPointers(), numChannels, numSamples);

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
    // Create a ValueTree to hold all state
    juce::ValueTree state("PluginState");
    
    // Save APVTS parameters
    auto apvtsState = valueTreeState.copyState();
    state.addChild(apvtsState, -1, nullptr);
    
    // Save EffectRouter assignment
    auto routerState = effectRouter.toValueTree();
    state.addChild(routerState, -1, nullptr);
    
    // TODO: Save per-effect instance state (sequencer patterns, etc.)
    // This will be added as we refactor effect instances
    
    // Serialize to MemoryBlock
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Parse ValueTree from MemoryBlock
    auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    
    if (tree.isValid() && tree.hasType("PluginState"))
    {
        // Restore APVTS parameters
        auto apvtsState = tree.getChildWithName(valueTreeState.state.getType());
        if (apvtsState.isValid())
        {
            valueTreeState.replaceState(apvtsState);
        }
        
        // Restore EffectRouter assignment
        auto routerState = tree.getChildWithName("EffectRouter");
        if (routerState.isValid())
        {
            effectRouter.fromValueTree(routerState);
            
            // Validate router assignment
            if (!effectRouter.isValid())
            {
                // Restore default if invalid
                DBG("[State] Invalid router assignment detected, restoring defaults");
                // Reinitialize with default constructor (can't assign due to atomic members)
                effectRouter.~EffectRouter();
                new (&effectRouter) EffectRouter();
            }
        }
        
        // TODO: Restore per-effect instance state
        // This will be added as we refactor effect instances
    }
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

// Dirt snapshot methods
StepSnapshot PluginProcessor::getDirtSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return dirtStepSnapshots[step];
    }
    return dirtStepSnapshots[0];
}

void PluginProcessor::setDirtStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        dirtStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateDirtCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = dirtUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific Dirt parameter in the snapshot
    switch (knobIndex) {
        case 0: // Drive
            dirtStepSnapshots[currentStep].dirt.drive = value;
            break;
        case 1: // Color
            dirtStepSnapshots[currentStep].dirt.color = value;
            break;
        case 2: // Asym
            dirtStepSnapshots[currentStep].dirt.asym = value;
            break;
        case 3: // Texture
            dirtStepSnapshots[currentStep].dirt.texture = value;
            break;
        case 4: // Low-Cut
            dirtStepSnapshots[currentStep].dirt.lowCut = value;
            break;
        case 5: // High-Cut
            dirtStepSnapshots[currentStep].dirt.highCut = value;
            break;
        case 6: // Tone
            dirtStepSnapshots[currentStep].dirt.tone = value;
            break;
        case 7: // Mix
            dirtStepSnapshots[currentStep].dirt.mix = value;
            break;
    }
}

StepSnapshot PluginProcessor::getChorusSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return chorusStepSnapshots[step];
    }
    return chorusStepSnapshots[0];
}

void PluginProcessor::setChorusStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        chorusStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateChorusCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = chorusUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific Chorus parameter in the snapshot
    switch (knobIndex) {
        case 0: // Rate
            chorusStepSnapshots[currentStep].chorus.rate = value;
            break;
        case 1: // Depth
            chorusStepSnapshots[currentStep].chorus.depth = value;
            break;
        case 2: // Voices
            chorusStepSnapshots[currentStep].chorus.voices = value;
            break;
        case 3: // Delay Time
            chorusStepSnapshots[currentStep].chorus.delayTime = value;
            break;
        case 4: // Feedback
            chorusStepSnapshots[currentStep].chorus.feedback = value;
            break;
        case 5: // Width
            chorusStepSnapshots[currentStep].chorus.width = value;
            break;
        case 6: // Tone
            chorusStepSnapshots[currentStep].chorus.tone = value;
            break;
        case 7: // Mix
            chorusStepSnapshots[currentStep].chorus.mix = value;
            break;
    }
}

// ===============================================================================
// REVERB SEQUENCER SNAPSHOT METHODS
// ===============================================================================

StepSnapshot PluginProcessor::getReverbSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return reverbStepSnapshots[step];
    }
    return reverbStepSnapshots[0];
}

void PluginProcessor::setReverbStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        reverbStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateReverbCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = reverbUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific Reverb parameter in the snapshot
    switch (knobIndex) {
        case 0: // Type
            reverbStepSnapshots[currentStep].reverb.type = value;
            break;
        case 1: // Size
            reverbStepSnapshots[currentStep].reverb.size = value;
            break;
        case 2: // Predelay
            reverbStepSnapshots[currentStep].reverb.predelayMs = value;
            break;
        case 3: // Damping
            reverbStepSnapshots[currentStep].reverb.dampHz = value;
            break;
        case 4: // Diffusion
            reverbStepSnapshots[currentStep].reverb.diffusion = value;
            break;
        case 5: // Early
            reverbStepSnapshots[currentStep].reverb.early = value;
            break;
        case 6: // Decay
            reverbStepSnapshots[currentStep].reverb.decaySec = value;
            break;
        case 7: // Mix
            reverbStepSnapshots[currentStep].reverb.mix = value;
            break;
    }
}

StepSnapshot PluginProcessor::getGranularSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return granularStepSnapshots[step];
    }
    return granularStepSnapshots[0];
}

void PluginProcessor::setGranularStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        granularStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateGranularCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = granularUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Mix (knob 7) is global, not saved to snapshots
    if (knobIndex == 7) return;
    
    // Update the specific Granular parameter in the snapshot
    switch (knobIndex) {
        case 0: // Size
            granularStepSnapshots[currentStep].granular.sizeMs = value;
            break;
        case 1: // Density
            granularStepSnapshots[currentStep].granular.densityHz = value;
            break;
        case 2: // Position
            granularStepSnapshots[currentStep].granular.position = value;
            break;
        case 3: // Spray
            granularStepSnapshots[currentStep].granular.sprayMs = value;
            break;
        case 4: // Pitch
            granularStepSnapshots[currentStep].granular.pitchSemi = value;
            break;
        case 5: // Random
            granularStepSnapshots[currentStep].granular.random = value;
            break;
        case 6: // Texture
            granularStepSnapshots[currentStep].granular.texture = value;
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
    
    DBG("[Processor] Standalone playback started - sequencer active: " + juce::String(seq.active.load() ? 1 : 0));
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




// ===============================================================================
// DYNAMIC EFFECT PROCESSING (For Router-based Routing)
// ===============================================================================

void PluginProcessor::processDelayEffect(juce::AudioBuffer<float>& buffer)
{
    // Process delay effect (original SpaceDelay processing)
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) {
        if (fxEnabled.load())
            spaceDelay.process(buffer, buffer.getNumSamples());
    }
}

void PluginProcessor::processAutoPanEffect(juce::AudioBuffer<float>& buffer)
{
    // Process AutoPan effect
    auto* autopanEnabledParam = valueTreeState.getRawParameterValue("autopanEnabled");
    bool isAutoPanEnabled = autopanEnabledParam ? (autopanEnabledParam->load() > 0.5f) : false;
    
    if (!isAutoPanEnabled) return;
    
    // Apply AutoPan parameters (from sequencer snapshot or APVTS)
    float rate, depth, waveShape, phaseOffset;
    int waveTypeIndex;
    bool isInverted;
    
    // Check if AutoPan sequencer is enabled AND active
    if (autopanSeq.enabled.load() && autopanSeq.active.load()) {
        // Use AutoPan sequencer snapshot
        int autopanStep = autopanSeq.currentStep.load();
        const auto& snapshot = autopanStepSnapshots[juce::jlimit(0, 15, autopanStep)];
        
        rate = snapshot.autopan.rate;
        depth = snapshot.autopan.amount;
        waveShape = snapshot.autopan.waveShape;
        phaseOffset = snapshot.autopan.phase;
        waveTypeIndex = snapshot.autopan.waveType;
        isInverted = snapshot.autopan.inverted;
    } else {
        // Use APVTS parameters (manual control)
        rate = valueTreeState.getRawParameterValue("autopanRate")->load();
        depth = valueTreeState.getRawParameterValue("autopanAmount")->load();
        waveShape = valueTreeState.getRawParameterValue("autopanWaveShape")->load();
        phaseOffset = valueTreeState.getRawParameterValue("autopanPhase")->load();
        waveTypeIndex = (int)valueTreeState.getRawParameterValue("autopanWaveType")->load();
        isInverted = valueTreeState.getRawParameterValue("autopanInverted")->load() > 0.5f;
    }
    
    // Check if sync mode is enabled
    auto* syncParam = valueTreeState.getRawParameterValue("autopanTimeSync");
    if (syncParam && syncParam->load() > 0.5f) {
        // Sync mode: convert 0-1 to musical division Hz
        float knobValue = juce::jlimit(0.0f, 1.0f, rate);
        const int numDivisions = 16;
        static const Div allDivisions[] = {
            Div::Bars4, Div::Bars2, Div::Bar, Div::DottedHalf, Div::Half, Div::DottedQuarter,
            Div::Quarter, Div::TripletQuarter, Div::Eighth, Div::DottedEighth, Div::TripletEighth,
            Div::Sixteenth, Div::DottedSixteenth, Div::TripletSixteenth, Div::ThirtySecond, Div::SixtyFourth
        };
        int divIndex = juce::jlimit(0, numDivisions - 1, (int)(knobValue * (numDivisions - 1)));
        Div div = allDivisions[divIndex];
        const double bpm = getBpmOrDefault(120.0);
        rate = syncedHz((float)bpm, div) * 0.5f;
    } else {
        // Free mode: map 0-1 to Hz range
        rate = 0.05f + rate * (90.0f - 0.05f);
    }
    
    // Set AutoPan parameters
    const float width = 1.0f;
    const float mix = 1.0f;
    const WaveType wType = static_cast<WaveType>(juce::jlimit(0, 4, waveTypeIndex));

    autoPan.setTargets(rate, depth, width, mix, waveShape, phaseOffset, wType, isInverted);
    
    // Process AutoPan effect
    if (buffer.getNumChannels() >= 2 && buffer.getNumSamples() > 0) {
        bool syncToTransport = (syncParam && syncParam->load() > 0.5f);
        bool isPlaying = syncToTransport ? wasPlaying.load() : true;
        
        if (syncToTransport) {
            double bpm = getBpmOrDefault(120.0);
            double ppqPosition = transportCache.ppq.load();
            autoPan.process(buffer, isPlaying, syncToTransport, bpm, ppqPosition);
        } else {
            autoPan.process(buffer, isPlaying, syncToTransport);
        }
        
        // Publish clock data for PanManBar visualizer
        panClock.phase01.store(autoPan.phase, std::memory_order_release);
        panClock.incPerSample.store(autoPan.phaseIncSmooth.getCurrentValue(), std::memory_order_release);
        panClock.sampleRate.store(autoPan.sampleRate, std::memory_order_release);
    }
}

void PluginProcessor::processDirtEffect(juce::AudioBuffer<float>& buffer)
{
    // Process Dirt saturation effect
    auto* dirtEnabledParam = valueTreeState.getRawParameterValue("dirtEnabled");
    bool isDirtEnabled = dirtEnabledParam ? (dirtEnabledParam->load() > 0.5f) : false;
    
    if (!isDirtEnabled) return;
    
    float drive, color, asym, texture, lowCut, highCut, tone, mix;
    
    // Check if Dirt sequencer is enabled AND active
    if (dirtSeq.enabled.load() && dirtSeq.active.load()) {
        int dirtStep = dirtSeq.currentStep.load();
        const auto& snapshot = dirtStepSnapshots[juce::jlimit(0, 15, dirtStep)];
        
        drive = snapshot.dirt.drive;
        color = snapshot.dirt.color;
        asym = snapshot.dirt.asym;
        texture = snapshot.dirt.texture;
        lowCut = snapshot.dirt.lowCut;
        highCut = snapshot.dirt.highCut;
        tone = snapshot.dirt.tone;
        mix = snapshot.dirt.mix;
    } else {
        drive = valueTreeState.getRawParameterValue("dirtDrive")->load();
        color = valueTreeState.getRawParameterValue("dirtColor")->load();
        asym = valueTreeState.getRawParameterValue("dirtAsym")->load();
        texture = valueTreeState.getRawParameterValue("dirtTexture")->load();
        lowCut = valueTreeState.getRawParameterValue("dirtLowCut")->load();
        highCut = valueTreeState.getRawParameterValue("dirtHighCut")->load();
        tone = valueTreeState.getRawParameterValue("dirtTone")->load();
        mix = valueTreeState.getRawParameterValue("dirtMix")->load();
    }
    
    dirt.setTargets(drive, color, asym, texture, lowCut, highCut, tone, mix);
    dirt.process(buffer);
}

void PluginProcessor::processChorusEffect(juce::AudioBuffer<float>& buffer)
{
    // Process Chorus effect
    auto* chorusEnabledParam = valueTreeState.getRawParameterValue("chorusEnabled");
    bool isChorusEnabled = chorusEnabledParam ? (chorusEnabledParam->load() > 0.5f) : false;
    
    if (!isChorusEnabled) return;
    
    int voices;
    float baseDelayMs, rateHz, depthMs, width, feedback, shape, mix;
    
    // Check if Chorus sequencer is enabled AND active
    if (chorusSeq.enabled.load() && chorusSeq.active.load()) {
        int chorusStep = chorusSeq.currentStep.load();
        const auto& snapshot = chorusStepSnapshots[juce::jlimit(0, 15, chorusStep)];
        
        baseDelayMs = snapshot.chorus.delayTime;
        rateHz = snapshot.chorus.rate;
        depthMs = snapshot.chorus.depth;
        voices = (int)snapshot.chorus.voices;
        width = snapshot.chorus.width;
        feedback = snapshot.chorus.feedback;
        shape = snapshot.chorus.tone;
        mix = snapshot.chorus.mix;
    } else {
        baseDelayMs = valueTreeState.getRawParameterValue("chorusDelayMs")->load();
        rateHz = valueTreeState.getRawParameterValue("chorusRateHz")->load();
        depthMs = valueTreeState.getRawParameterValue("chorusDepthMs")->load();
        voices = (int)valueTreeState.getRawParameterValue("chorusVoices")->load();
        width = valueTreeState.getRawParameterValue("chorusWidth")->load();
        feedback = valueTreeState.getRawParameterValue("chorusFeedback")->load();
        shape = valueTreeState.getRawParameterValue("chorusShape")->load();
        mix = valueTreeState.getRawParameterValue("chorusMix")->load();
    }
    
    chorus.setParams(voices, baseDelayMs, rateHz, depthMs, width, feedback, shape, mix);
    chorus.process(buffer);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}