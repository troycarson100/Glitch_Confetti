#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/DspFlags.h"
#include "dsp/PanSync.h"
#include <chrono>

//==============================================================================
// Filter Cutoff Conversion Functions
// Custom curve: 0-75% knob = 20Hz-5000Hz, 75-100% knob = 5000Hz-20000Hz
//==============================================================================
static float convertNormalizedCutoffToFrequency(float normalized)
{
    // Clamp normalized to 0-1
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    
    if (normalized <= 0.75f)
    {
        // Linear: 0-0.75 → 20Hz to 5000Hz
        return 20.0f + (5000.0f - 20.0f) * (normalized / 0.75f);
    }
    else
    {
        // Exponential: 0.75-1.0 → 5000Hz to 20000Hz
        float t = (normalized - 0.75f) / 0.25f; // 0-1 in the 75-100% range
        return 5000.0f * std::pow(4.0f, t); // 4x multiplier from 5k to 20k
    }
}

static float convertFrequencyToNormalizedCutoff(float frequency)
{
    // Clamp frequency to valid range
    frequency = juce::jlimit(20.0f, 20000.0f, frequency);
    
    if (frequency <= 5000.0f)
    {
        // Reverse linear: 20Hz to 5000Hz → 0-0.75
        return 0.75f * (frequency - 20.0f) / (5000.0f - 20.0f);
    }
    else
    {
        // Reverse exponential: 5000Hz to 20000Hz → 0.75-1.0
        float t = std::log(frequency / 5000.0f) / std::log(4.0f);
        return 0.75f + 0.25f * t;
    }
}

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
    dirtSeq.enabled.store(true); // Start enabled so it auto-activates on play
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
    
    // Initialize Slicer sequencer
    slicerSeq.enabled.store(true); // Start enabled
    slicerSeq.stepsUsed.store(16);
    slicerSeq.divisionIndex.store(3); // 1/8 default (index 3 for Slicer's 0-5 range)
    slicerSeq.playingStep.store(0);
    
    // Initialize Slicer step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        slicerStepSnapshots[i].slicer.pattern = 0.0f;
        slicerStepSnapshots[i].slicer.division = 3.0f;
        slicerStepSnapshots[i].slicer.offset = 0.5f;
        slicerStepSnapshots[i].slicer.shape = 0.5f;
        slicerStepSnapshots[i].slicer.releaseMs = 20.0f;
        slicerStepSnapshots[i].slicer.mix = 0.75f;
    }
    DBG("[Stepper] Initialized Slicer step snapshots with default values");
    
    // Initialize Dub Delay sequencer
    dubdelaySeq.enabled.store(true); // Start enabled
    dubdelaySeq.stepsUsed.store(16);
    dubdelaySeq.divisionIndex.store(5); // 1/8 default
    dubdelaySeq.playingStep.store(0);
    
    // Initialize Dub Delay step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        dubdelayStepSnapshots[i].dubdelay.timeMs = 450.0f;
        dubdelayStepSnapshots[i].dubdelay.feedback = 0.5625f; // Increased by 25%
        dubdelayStepSnapshots[i].dubdelay.toneHz = 6500.0f;
        dubdelayStepSnapshots[i].dubdelay.drive = 0.15f;
        dubdelayStepSnapshots[i].dubdelay.pingPong = true;
        dubdelayStepSnapshots[i].dubdelay.wowFlutter = 0.35f;
        dubdelayStepSnapshots[i].dubdelay.regenDamp = 0.25f;
        dubdelayStepSnapshots[i].dubdelay.mix = 0.35f;
    }
    DBG("[Stepper] Initialized Dub Delay step snapshots with default values");
    
    // Initialize Space Delay sequencer
    spacedelaySeq.enabled.store(true); // Start enabled
    spacedelaySeq.stepsUsed.store(16);
    spacedelaySeq.divisionIndex.store(5); // 1/8 default
    spacedelaySeq.playingStep.store(0);
    spacedelayUiSelectedStep.store(0); // Initialize UI selected step
    
    // Initialize Space Delay step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        spacedelayStepSnapshots[i].delay.timeMs = 250.0f;
        spacedelayStepSnapshots[i].delay.feedback = 0.2f;
        spacedelayStepSnapshots[i].delay.wowDepth = 0.0f;
        spacedelayStepSnapshots[i].delay.wowRate = 1.0f;
        spacedelayStepSnapshots[i].delay.saturation = 0.0f;
        spacedelayStepSnapshots[i].delay.highCut = 20000.0f;
        spacedelayStepSnapshots[i].delay.lowCut = 20.0f;
        spacedelayStepSnapshots[i].delay.mix = 0.5f;
    }
    DBG("[Stepper] Initialized Space Delay step snapshots with default values");
    
    // Initialize PhaseBloom sequencer
    phaseBloomSeq.enabled.store(true); // Start enabled
    phaseBloomSeq.stepsUsed.store(16);
    phaseBloomSeq.divisionIndex.store(5); // 1/8 default
    phaseBloomSeq.playingStep.store(0);
    
    // Initialize Formant sequencer
    formantSeq.enabled.store(true); // Start enabled
    formantSeq.stepsUsed.store(16);
    formantSeq.divisionIndex.store(5); // 1/8 default
    formantSeq.playingStep.store(0);
    
    // Initialize Saturate sequencer
    saturateSeq.enabled.store(true); // Start enabled
    saturateSeq.stepsUsed.store(16);
    saturateSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    saturateSeq.playingStep.store(0);
    saturateUiSelectedStep.store(0); // Initialize UI selected step
    
    // Initialize Shimmer sequencer - TODO: shimmerSeq needs to be declared
    // shimmerSeq.enabled.store(true); // Start enabled
    // shimmerSeq.stepsUsed.store(16);
    // shimmerSeq.divisionIndex.store(5); // 1/8 default (index 5 = item ID 6)
    // shimmerSeq.playingStep.store(0);
    // shimmerUiSelectedStep.store(0); // Initialize UI selected step
    
    // Initialize Shimmer step snapshots with defaults - TODO: shimmerStepSnapshots needs to be declared
    // for (int i = 0; i < 16; ++i) {
    //     shimmerStepSnapshots[i].shimmer.mode = 0.0f; // A:+12
    //     shimmerStepSnapshots[i].shimmer.size = 0.60f;
    //     shimmerStepSnapshots[i].shimmer.decay = 8.0f;
    //     shimmerStepSnapshots[i].shimmer.color = 0.55f;
    //     shimmerStepSnapshots[i].shimmer.predelay = 25.0f;
    //     shimmerStepSnapshots[i].shimmer.shimAmt = 0.35f;
    //     shimmerStepSnapshots[i].shimmer.osMode = 2.0f; // 4×
    //     shimmerStepSnapshots[i].shimmer.mix = 0.50f;
    // }
    
    // Initialize Saturate step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        saturateStepSnapshots[i].saturate.type = 0.0f; // Spiral2
        saturateStepSnapshots[i].saturate.drive = 12.0f;
        saturateStepSnapshots[i].saturate.color = 0.5f;
        saturateStepSnapshots[i].saturate.shape = 0.4f;
        saturateStepSnapshots[i].saturate.bias = 0.0f;
        saturateStepSnapshots[i].saturate.output = 0.0f;
        saturateStepSnapshots[i].saturate.oversample = 2.0f; // 4×
        saturateStepSnapshots[i].saturate.mix = 1.0f;
    }
    DBG("[Stepper] Initialized Saturate step snapshots with default values");
    
    // Initialize Redux sequencer
    reduxSeq.enabled.store(true); // Start enabled
    reduxSeq.stepsUsed.store(16);
    reduxSeq.divisionIndex.store(5); // 1/8 default
    reduxSeq.playingStep.store(0);
    
    // Initialize PhaseBloom step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        phaseBloomStepSnapshots[i].phasebloom.depth = 0.5f;
        phaseBloomStepSnapshots[i].phasebloom.rate = 0.5f;
        phaseBloomStepSnapshots[i].phasebloom.feedback = 0.3f;
        phaseBloomStepSnapshots[i].phasebloom.center = 1000.0f;
        phaseBloomStepSnapshots[i].phasebloom.bloom = 0.2f;
        phaseBloomStepSnapshots[i].phasebloom.spread = 0.8f;
        phaseBloomStepSnapshots[i].phasebloom.resonance = 0.5f;
        phaseBloomStepSnapshots[i].phasebloom.mix = 0.5f;
    }
    DBG("[Stepper] Initialized PhaseBloom step snapshots with default values");
    
    // Initialize Formant step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        formantStepSnapshots[i].formant.vowel = 0.0f; // A
        formantStepSnapshots[i].formant.resonance = 12.0f;
        formantStepSnapshots[i].formant.intensity = 6.0f;
        formantStepSnapshots[i].formant.mix = 0.8f;
    }
    DBG("[Stepper] Initialized Formant step snapshots with default values");
    
    // Initialize Form 2 step snapshots with defaults
    for (int i = 0; i < 16; ++i) {
        form2StepSnapshots[i].form2.rootNote = 0;        // C
        form2StepSnapshots[i].form2.scale = 0;          // Major
        form2StepSnapshots[i].form2.chordSize = 4;       // Tetrad
        form2StepSnapshots[i].form2.shift = 1.0f;       // Neutral
        form2StepSnapshots[i].form2.color = 0.0f;        // 0 dB neutral
        form2StepSnapshots[i].form2.motion = 0.0f;      // No motion by default
        form2StepSnapshots[i].form2.resynth = 0.5f;     // 50% vocoder
        form2StepSnapshots[i].form2.mix = 0.8f;         // 80% wet
    }
    form2Seq.enabled.store(true);
    form2Seq.active.store(false);
    DBG("[Stepper] Initialized Form 2 step snapshots with default values");
    
    // Initialize UI state
    uiSelectedStep.store(0);
    autopanUiSelectedStep.store(0);
    dirtUiSelectedStep.store(0);
    chorusUiSelectedStep.store(0);
    reverbUiSelectedStep.store(0);
    slicerUiSelectedStep.store(0);
    granularUiSelectedStep.store(0);
    phaseBloomUiSelectedStep.store(0);
    
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
        chorusStepSnapshots[i].chorus.rate = 0.8f;       // 0.8 Hz (matches parameter default)
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
    
    // Space Delay Sync Parameters
    params.push_back(std::make_unique<juce::AudioParameterBool>("delaySync", "Delay Sync", false)); // Sync disabled by default
    params.push_back(std::make_unique<juce::AudioParameterChoice>("delayTimeDiv", "Delay Time Division", 
        juce::StringArray{"2", "1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16D", "1/16T", "1/32", "1/32D", "1/32T", "1/64", "1/64D", "1/64T"}, 5)); // Default 1/4 (index 5)
    
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusRateHz",   "Ch Rate",    juce::NormalisableRange<float>(0.02f, 4.0f, 0.0f, 0.5f),    0.8f)); // Compressed range focusing on lower values
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusDepthMs",  "Ch Depth",   juce::NormalisableRange<float>(0.0f,  12.0f, 0.0f, 0.5f),    5.0f)); // modulation amplitude in ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusFeedback", "Ch Fdbk",    juce::NormalisableRange<float>(0.0f,  0.9f,  0.0f, 1.0f),     0.15f));
    params.push_back(std::make_unique<juce::AudioParameterInt  >("chorusVoices",   "Ch Voices",  2, 8, 4));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusWidth",    "Ch Width",   juce::NormalisableRange<float>(0.0f,  1.0f,  0.0f, 1.0f),    0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusShape",    "Ch Shape",   juce::NormalisableRange<float>(0.0f,  1.0f,  0.0f, 1.0f),    0.25f)); // 0=sin .. 0.5=tri .. 1=soft square
    params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusMix",      "Ch Mix",     juce::NormalisableRange<float>(0.0f,  1.0f,  0.0f, 1.0f),    0.5f));
    
    // COMPRESS+ Parameters - Master effect after all other effects
    // Top row: Compressor controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compressThreshold", "Compress Threshold", -60.0f, 0.0f, -20.0f)); // -60dB to 0dB threshold
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compressAttack", "Compress Attack", 0.1f, 100.0f, 5.0f)); // 0.1ms to 100ms attack
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compressRelease", "Compress Release", 10.0f, 1000.0f, 50.0f)); // 10ms to 1000ms release
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compressRatio", "Compress Ratio", 1.0f, 20.0f, 4.0f)); // 1:1 to 20:1 ratio
            // Bottom row: Drive, Lofi, Makeup Gain, Wet
            params.push_back(std::make_unique<juce::AudioParameterFloat>("compressDrive", "Compress Drive", 0.0f, 30.0f, 0.0f)); // 0-30dB drive
            params.push_back(std::make_unique<juce::AudioParameterFloat>("compressLofi", "Compress Lofi", 0.0f, 1.0f, 0.0f)); // 0-1 lofi intensity
            params.push_back(std::make_unique<juce::AudioParameterFloat>("compressMakeupGain", "Compress Makeup Gain", -24.0f, 24.0f, 0.0f)); // -24dB to +24dB makeup gain
            params.push_back(std::make_unique<juce::AudioParameterFloat>("compressWet", "Compress Wet", 0.0f, 1.0f, 1.0f)); // 0-1 wet/dry mix
        params.push_back(std::make_unique<juce::AudioParameterBool>("compressEnabled", "Compress Enabled", true)); // COMPRESS+ master effect enabled
    
    // Page and effect enable parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>("currentPage", "Current Page", 
        juce::StringArray {"SpaceDelay", "AutoPan", "Dirt", "Chorus", "Reverb", "Granular", "Slicer", "DubDelay", "Redux", "PhaseBloom"}, 0)); // Effect page selection
    params.push_back(std::make_unique<juce::AudioParameterBool>("delayEnabled", "Delay Enabled", true)); // Space Delay effect enabled - ON by default
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterHPQ", "HPF Resonance",
        juce::NormalisableRange<float>(0.5f, 10.0f, 0.0f, 0.5f), 0.707f)); // Default Butterworth Q
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterLPQ", "LPF Resonance",
        juce::NormalisableRange<float>(0.5f, 10.0f, 0.0f, 0.5f), 0.707f)); // Default Butterworth Q
    
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
    
        // Slicer Parameters (6 knobs + 2 toggles)
        params.push_back(std::make_unique<juce::AudioParameterInt>("slicerPattern", "Slicer Pattern", 0, 7, 0)); // 8 patterns
        params.push_back(std::make_unique<juce::AudioParameterChoice>("slicerDivision", "Slicer Division",
            juce::StringArray{"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"}, 5)); // Default 1/8 (index 5)
        params.push_back(std::make_unique<juce::AudioParameterChoice>("slicerGrid", "Slicer Grid",
            juce::StringArray{"Straight", "Dotted", "Triplet"}, 0)); // Default Straight
        params.push_back(std::make_unique<juce::AudioParameterFloat>("slicerOffset", "Slicer Offset", 
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f)); // Bipolar: 0.5=center (0%), 0=early, 1=late
        params.push_back(std::make_unique<juce::AudioParameterFloat>("slicerShape", "Slicer Shape", 
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f)); // Envelope curvature/easing (default centered)
        params.push_back(std::make_unique<juce::AudioParameterFloat>("slicerReleaseMs", "Slicer Release", 
            juce::NormalisableRange<float>(5.0f, 80.0f, 0.0f, 1.0f), 20.0f)); // Crossfade/tail duration (ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>("slicerMix", "Slicer Mix", 
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.75f)); // Wet/dry
        params.push_back(std::make_unique<juce::AudioParameterBool>("slicerSync", "Slicer Sync", true)); // Tempo sync toggle
        params.push_back(std::make_unique<juce::AudioParameterBool>("slicerEnabled", "Slicer Enabled", true)); // Start enabled
    
    // Dub Delay Parameters (8 knobs + enable)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubTimeMs", "Dub Time", 
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.0f, 0.5f), 450.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubFeedback", "Dub Feedback", 
        juce::NormalisableRange<float>(0.0f, 0.98f, 0.0f, 1.0f), 0.5625f)); // Increased by 25%
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubToneHz", "Dub Tone", 
        juce::NormalisableRange<float>(200.0f, 20000.0f, 0.0f, 0.5f), 6500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubDrive", "Dub Drive", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("dubPingPong", "Dub PingPong", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubWowFlutter", "Dub WowFlutter", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubRegenDamp", "Dub RegenDamp", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dubMix", "Dub Mix", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("dubEnabled", "Dub Delay Enabled", true)); // Enabled by default
    params.push_back(std::make_unique<juce::AudioParameterBool>("dubSync", "Dub Delay Sync", false)); // Sync disabled by default
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("dubTimeDiv", "Dub Time Division", 
        juce::StringArray{"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"}, 5)); // Default 1/8
    params.push_back(std::make_unique<juce::AudioParameterChoice>("dubTimeGrid", "Dub Time Grid", 
        juce::StringArray{"Straight", "Dotted", "Triplet"}, 0)); // Default Straight
    
    // Redux Parameters - 8 knobs (must match UI order)
    params.push_back(std::make_unique<juce::AudioParameterInt>("reduxBitDepth", "Redux Bit Depth", 1, 12, 8));
    params.push_back(std::make_unique<juce::AudioParameterInt>("reduxSampleRateReduction", "Redux Sample Rate Reduction", 1, 32, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reduxJitter", "Redux Jitter", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reduxPreFilter", "Redux Pre Filter", 
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.5f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reduxPostFilter", "Redux Post Filter", 
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.5f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reduxDrive", "Redux Drive", 0.0f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reduxEmphasis", "Redux Emphasis", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reduxMix", "Redux Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("reduxEnabled", "Redux Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("reduxStepEnabled", "Redux Step Enabled", true));
    
    // PhaseBloom Parameters - 8 sliders
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomDepth", "PhaseBloom Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomRate", "PhaseBloom Rate", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomFeedback", "PhaseBloom Feedback", -0.8f, 0.8f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomCenter", "PhaseBloom Center", 200.0f, 8000.0f, 2000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomBloom", "PhaseBloom Bloom", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomSpread", "PhaseBloom Spread", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomResonance", "PhaseBloom Resonance", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phasebloomMix", "PhaseBloom Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("phasebloomEnabled", "PhaseBloom Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("phasebloomStepEnabled", "PhaseBloom Step Enabled", true));
    
    // Formant Parameters - 8 knobs for dual-bank formant filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vowel", "Vowel", 0.0f, 4.0f, 0.0f)); // Continuous 0..4 (A=0, E=1, I=2, O=3, U=4)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance", 0.4f, 18.0f, 10.0f)); // Q 0.4-18 (sharpness)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("intensity", "Intensity", -6.0f, 18.0f, 12.0f)); // Emphasis -6..+18 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>("formantShift", "Formant Shift", 0.5f, 2.0f, 1.0f)); // Shift 0.5-2.0
    params.push_back(std::make_unique<juce::AudioParameterFloat>("formantBrightness", "Formant Brightness", -12.0f, 12.0f, 3.0f)); // Brightness -12..+12 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>("formantMotion", "Formant Motion", 0.0f, 1.0f, 0.25f)); // Motion 0-1
    params.push_back(std::make_unique<juce::AudioParameterFloat>("formantAir", "Formant Air", 0.0f, 1.0f, 0.2f)); // Air 0-1
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f)); // 0-1 dry/wet mix
    params.push_back(std::make_unique<juce::AudioParameterBool>("formantEnabled", "Formant Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("formantStepEnabled", "Formant Step Enabled", true));
    
    // Form 2 Parameters - Vocoder with scale-quantized carrier
    params.push_back(std::make_unique<juce::AudioParameterInt>("form2RootNote", "Form 2 Root Note", 0, 11, 0)); // C
    params.push_back(std::make_unique<juce::AudioParameterInt>("form2Scale", "Form 2 Scale", 0, 6, 0)); // Major
    params.push_back(std::make_unique<juce::AudioParameterInt>("form2ChordSize", "Form 2 Chord Size", 1, 8, 4));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("form2Shift", "Form 2 Shift", 0.5f, 2.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("form2Brightness", "Form 2 Brightness", -12.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("form2Motion", "Form 2 Motion", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("form2Air", "Form 2 Air", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("form2Mix", "Form 2 Mix", 0.0f, 1.0f, 0.8f)); // 80% wet
    params.push_back(std::make_unique<juce::AudioParameterBool>("form2Enabled", "Form 2 Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("form2StepEnabled", "Form 2 Step Enabled", true));
    
    // Saturate Parameters
    params.push_back(std::make_unique<juce::AudioParameterInt>("satType", "Saturate Type", 0, 7, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satDrive", "Drive", 0.0f, 36.0f, 12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satColor", "Color", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satShape", "Shape", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satBias", "Bias", -0.2f, 0.2f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satOut", "Output", -24.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("satOsMode", "Oversample", 0, 3, 2)); // 0=1×, 1=2×, 2=4×, 3=8×
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satMix", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("saturateEnabled", "Saturate Enabled", true)); // Default ON
    params.push_back(std::make_unique<juce::AudioParameterBool>("saturateStepEnabled", "Saturate Step Enabled", true));
    
    // Filter Parameters (8 knobs: fType, cutoff, res, slope, drive, spread, keytrack, filterMix)
    params.push_back(std::make_unique<juce::AudioParameterInt>("fType", "Filter Type", 0, 4, 0)); // 0=LP, 1=HP, 2=BP, 3=Comb-, 4=Comb+
    // Custom frequency range: 0-75% knob = 20Hz-5000Hz, 75-100% knob = 5000Hz-20000Hz
    // Parameter stores normalized value (0.0-1.0) for linear knob rotation
    // Conversion to frequency happens in processBlock using convertNormalizedToFrequency()
    params.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.18f)); // Normalized: ~0.18 = 1200Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("res", "Resonance", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("slope", "Slope", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f)); // 0=12dB, 1=24dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 
        juce::NormalisableRange<float>(0.0f, 36.0f, 0.1f), 6.0f)); // dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>("spread", "Spread", 
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f)); // cents
    params.push_back(std::make_unique<juce::AudioParameterFloat>("keytrack", "Key Track", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterMix", "Filter Mix", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("filterEnabled", "Filter Enabled", true)); // Default ON
    
    // Shimmer Parameters (8 knobs)
    params.push_back(std::make_unique<juce::AudioParameterChoice>("shimMode", "Mode",
        juce::StringArray{"A:+12","B:+7","C:±12","D:+12±detune","E:Quad micro"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("size", "Size",
        juce::NormalisableRange<float>(0.10f, 1.00f, 0.0f, 1.0f), 0.60f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decay", "Decay (s)",
        juce::NormalisableRange<float>(0.10f, 20.0f, 0.0f, 0.6f), 8.0f)); // skew ~log
    params.push_back(std::make_unique<juce::AudioParameterFloat>("color", "Color",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("predelay", "Pre-Delay (ms)",
        juce::NormalisableRange<float>(0.0f, 120.0f), 25.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("shimAmt", "Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osMode", "Oversample",
        juce::StringArray{"1x","2x","4x","8x"}, 3)); // index 3 = 8x (default)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("shimmerEnabled", "Shimmer Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("shimmerStepEnabled", "Shimmer Step Enabled", true));
    
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
    slicer.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels()); // Prepare Slicer engine
    filterProcessor.prepare(sampleRate, samplesPerBlock); // Prepare Filter processor
    filterSeq.prepare(sampleRate); // Initialize Filter sequencer with sample rate
    seq.prepare(sampleRate); // Initialize delay sequencer with sample rate
    autopanSeq.prepare(sampleRate); // Initialize AutoPan sequencer with sample rate
    dirtSeq.prepare(sampleRate); // Initialize Dirt sequencer with sample rate
    chorusSeq.prepare(sampleRate); // Initialize Chorus sequencer with sample rate
    reverbSeq.prepare(sampleRate); // Initialize Reverb sequencer with sample rate
    granularSeq.prepare(sampleRate); // Initialize Granular sequencer with sample rate
    slicerSeq.prepare(sampleRate); // Initialize Slicer sequencer with sample rate
    
    // Prepare Dub Delay DSP
    dubDelay.prepare(sampleRate, samplesPerBlock);
    dubdelaySeq.prepare(sampleRate); // Initialize Dub Delay sequencer with sample rate
    
    // Prepare Space Delay DSP
    spaceDelay.prepare(sampleRate, samplesPerBlock);
    spacedelaySeq.prepare(sampleRate); // Initialize Space Delay sequencer with sample rate
    
    // Prepare Redux DSP
    reduxBank.prepare(sampleRate, samplesPerBlock);
    reduxSeq.prepare(sampleRate); // Initialize Redux sequencer with sample rate
    
    // Prepare PhaseBloom DSP
    phaseBloomEngine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    phaseBloomSeq.prepare(sampleRate); // Initialize PhaseBloom sequencer with sample rate
    
    // Prepare Formant DSP
    formantProcessor.prepare(sampleRate, samplesPerBlock);
    formantSeq.prepare(sampleRate); // Initialize Formant sequencer with sample rate
    
    // Prepare Saturate DSP
    saturateProcessor.prepare(sampleRate, samplesPerBlock);
    saturateSeq.prepare(sampleRate); // Initialize Saturate sequencer with sample rate
    
    // Prepare Shimmer DSP
    juce::dsp::ProcessSpec shimmerSpec;
    shimmerSpec.sampleRate = sampleRate;
    // shimmerSpec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    // shimmerSpec.numChannels = (juce::uint32)getTotalNumOutputChannels();
    // shimmerProcessor.prepare(shimmerSpec);  // TODO: shimmerProcessor needs to be declared
    // shimmerSeq.prepare(sampleRate); // Initialize Shimmer sequencer with sample rate  // TODO: shimmerSeq needs to be declared
    
    // Prepare Form 2 DSP
    juce::dsp::ProcessSpec form2Spec;
    form2Spec.sampleRate = sampleRate;
    form2Spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    form2Spec.numChannels = static_cast<juce::uint32>(getTotalNumInputChannels());
    form2Processor.prepare(form2Spec);
    
    // Prepare COMPRESS+ DSP - Master effect
    juce::dsp::ProcessSpec compressSpec;
    compressSpec.sampleRate = sampleRate;
    compressSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    compressSpec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    compressEngine.prepare(compressSpec);
    
    // Initialize compressor with default parameter values
    compressEngine.setThreshold(-20.0f);  // Default threshold
    compressEngine.setAttack(5.0f);       // Default attack
    compressEngine.setRelease(50.0f);     // Default release
    compressEngine.setRatio(4.0f);        // Default ratio
    compressEngine.setDrive(0.0f);        // Default drive
    compressEngine.setLofi(0.0f);         // Default lofi
    compressEngine.setMakeupGain(0.0f);   // Default makeup gain
    compressEngine.setWet(1.0f);          // Default wet level
    compressEngine.setEnabled(true);      // Enable compressor
    
    // Force APVTS parameters to default values to ensure they're properly initialized
    auto* compressEnabledParam = valueTreeState.getParameter("compressEnabled");
    if (compressEnabledParam) {
        compressEnabledParam->setValueNotifyingHost(1.0f); // true
    }
    
    auto* thresholdParam = valueTreeState.getParameter("compressThreshold");
    if (thresholdParam) {
        thresholdParam->setValueNotifyingHost(thresholdParam->convertTo0to1(-20.0f));
    }
    
    auto* attackParam = valueTreeState.getParameter("compressAttack");
    if (attackParam) {
        attackParam->setValueNotifyingHost(attackParam->convertTo0to1(5.0f));
    }
    
    auto* releaseParam = valueTreeState.getParameter("compressRelease");
    if (releaseParam) {
        releaseParam->setValueNotifyingHost(releaseParam->convertTo0to1(50.0f));
    }
    
    auto* ratioParam = valueTreeState.getParameter("compressRatio");
    if (ratioParam) {
        ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));
    }
    
    auto* driveParam = valueTreeState.getParameter("compressDrive");
    if (driveParam) {
        driveParam->setValueNotifyingHost(driveParam->convertTo0to1(0.0f));
    }
    
    auto* lofiParam = valueTreeState.getParameter("compressLofi");
    if (lofiParam) {
        lofiParam->setValueNotifyingHost(0.0f); // 0-1 range, 0.0f is default
    }
    
    auto* makeupGainParam = valueTreeState.getParameter("compressMakeupGain");
    if (makeupGainParam) {
        makeupGainParam->setValueNotifyingHost(makeupGainParam->convertTo0to1(0.0f));
    }
    
    auto* wetParam = valueTreeState.getParameter("compressWet");
    if (wetParam) {
        wetParam->setValueNotifyingHost(1.0f); // 0-1 range, 1.0f is default
    }
    
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
                granularSeq.resetPhase();  // Granular sequencer phase reset
                slicerSeq.resetPhase();    // Slicer sequencer phase reset
                dubdelaySeq.resetPhase();  // Dub Delay sequencer phase reset
                spacedelaySeq.resetPhase();  // Space Delay sequencer phase reset
                phaseBloomSeq.resetPhase();  // PhaseBloom sequencer phase reset
                reduxSeq.resetPhase();      // Redux sequencer phase reset
                
                // Auto-enable delay sequencer on DAW play (only if user hasn't explicitly disabled it)
                if (followHost.load() && !userDisabledSequencer.load()) {
                    seq.enabled.store(true);  // Enable delay sequencer
                    seq.active.store(true);   // Activate delay sequencer
                    DBG("[SEQ] Auto-enabled delay sequencer (followHost=true, userDisabled=false)");
                } else if (followHost.load() && userDisabledSequencer.load()) {
                    DBG("[SEQ] Skipped auto-enable delay sequencer (user explicitly disabled)");
                } else {
                    DBG("[SEQ] Skipped auto-enable delay sequencer (followHost=false)");
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
                
                if (slicerSeq.enabled.load()) {
                    slicerSeq.active.store(true);  // Activate Slicer sequencer
                    DBG("[SLICER SEQ] ✓ Activated on play edge");
                }
                
                // Dub Delay sequencer activates if enabled (independent of followHost)
                if (dubdelaySeq.enabled.load()) {
                    dubdelaySeq.active.store(true);  // Activate Dub Delay sequencer
                    DBG("[DUBDELAY SEQ] ✓ Activated on play edge");
                }
                
                // Space Delay sequencer activates if enabled (independent of followHost)
                if (spacedelaySeq.enabled.load()) {
                    spacedelaySeq.active.store(true);  // Activate Space Delay sequencer
                    DBG("[SPACEDELAY SEQ] ✓ Activated on play edge");
                }
                
                // PhaseBloom sequencer activates if enabled (independent of followHost)
                if (phaseBloomSeq.enabled.load()) {
                    phaseBloomSeq.active.store(true);  // Activate PhaseBloom sequencer
                    DBG("[PHASEBLOOM SEQ] ✓ Activated on play edge");
                }
                
                // Formant sequencer activates if enabled (independent of followHost)
                if (formantSeq.enabled.load()) {
                    formantSeq.active.store(true);  // Activate Formant sequencer
                    DBG("[FORMANT SEQ] ✓ Activated on play edge");
                }
                
                // Saturate (Heat) sequencer activates if enabled (independent of followHost)
                if (saturateSeq.enabled.load()) {
                    saturateSeq.active.store(true);  // Activate Saturate sequencer
                    DBG("[SATURATE SEQ] ✓ Activated on play edge");
                }
                
                // Form 2 sequencer activates if enabled (independent of followHost)
                if (form2Seq.enabled.load()) {
                    form2Seq.active.store(true);  // Activate Form 2 sequencer
                    DBG("[FORM2 SEQ] ✓ Activated on play edge");
                }
                
                // Redux sequencer activates if enabled (independent of followHost)
                if (reduxSeq.enabled.load()) {
                    reduxSeq.active.store(true);  // Activate Redux sequencer
                    DBG("[REDUX SEQ] ✓ Activated on play edge");
                }
                
                DBG("[SEQ] Play edge detected");
                DBG("[SEQ] Delay: enabled=" + juce::String(seq.enabled.load() ? 1 : 0) + " active=" + juce::String(seq.active.load() ? 1 : 0));
                DBG("[SEQ] AutoPan: enabled=" + juce::String(autopanSeq.enabled.load() ? 1 : 0) + " active=" + juce::String(autopanSeq.active.load() ? 1 : 0));
                DBG("[SEQ] SpaceDelay: enabled=" + juce::String(spacedelaySeq.enabled.load() ? 1 : 0) + " active=" + juce::String(spacedelaySeq.active.load() ? 1 : 0));
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
                
                if (slicerSeq.enabled.load() && slicerSeq.active.load()) {
                    const int slicerStep = slicerSeq.computeStepFromPPQ(ppq);
                    slicerSeq.currentStep.store(slicerStep);
                    slicerSeq.playingStep.store(slicerStep);
                    DBG("[SLICER SEQ] Lock-in at PPQ=" << ppq << " -> step " << slicerStep);
                }
                
                if (dubdelaySeq.enabled.load() && dubdelaySeq.active.load()) {
                    const int dubdelayStep = dubdelaySeq.computeStepFromPPQ(ppq);
                    dubdelaySeq.currentStep.store(dubdelayStep);
                    dubdelaySeq.playingStep.store(dubdelayStep);
                    DBG("[DUBDELAY SEQ] Lock-in at PPQ=" << ppq << " -> step " << dubdelayStep);
                }
                
                if (spacedelaySeq.enabled.load() && spacedelaySeq.active.load()) {
                    const int spacedelayStep = spacedelaySeq.computeStepFromPPQ(ppq);
                    spacedelaySeq.currentStep.store(spacedelayStep);
                    spacedelaySeq.playingStep.store(spacedelayStep);
                    DBG("[SPACEDELAY SEQ] Lock-in at PPQ=" << ppq << " -> step " << spacedelayStep);
                }
                
                if (phaseBloomSeq.enabled.load() && phaseBloomSeq.active.load()) {
                    const int phaseBloomStep = phaseBloomSeq.computeStepFromPPQ(ppq);
                    phaseBloomSeq.currentStep.store(phaseBloomStep);
                    phaseBloomSeq.playingStep.store(phaseBloomStep);
                    DBG("[PHASEBLOOM SEQ] Lock-in at PPQ=" << ppq << " -> step " << phaseBloomStep);
                }
                
                if (formantSeq.enabled.load() && formantSeq.active.load()) {
                    const int formantStep = formantSeq.computeStepFromPPQ(ppq);
                    formantSeq.currentStep.store(formantStep);
                    formantSeq.playingStep.store(formantStep);
                    DBG("[FORMANT SEQ] Lock-in at PPQ=" << ppq << " -> step " << formantStep);
                }
                
                if (form2Seq.enabled.load() && form2Seq.active.load()) {
                    const int form2Step = form2Seq.computeStepFromPPQ(ppq);
                    form2Seq.currentStep.store(form2Step);
                    form2Seq.playingStep.store(form2Step);
                    DBG("[FORM2 SEQ] Lock-in at PPQ=" << ppq << " -> step " << form2Step);
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
            
            // Space Delay sequencer stepping (shares same PPQ/transport as other sequencers)
            static int spacedelayDebugCounter = 0;
            if ((spacedelayDebugCounter++ % 100) == 0) {  // Log every 100 blocks
                DBG("[SPACEDELAY SEQ DEBUG] isPlaying=" + juce::String(isPlaying ? 1 : 0) + " ppqValid=" + juce::String(ppqValid ? 1 : 0) 
                    + " active=" + juce::String(spacedelaySeq.active.load() ? 1 : 0) + " enabled=" + juce::String(spacedelaySeq.enabled.load() ? 1 : 0)
                    + " PPQ=" + juce::String(ppq));
            }
            
            if (isPlaying && ppqValid && spacedelaySeq.active.load()) {
                const int spacedelayStep = spacedelaySeq.computeStepFromPPQ(ppq);
                if (spacedelayStep != spacedelaySeq.currentStep.load()) {
                    spacedelaySeq.currentStep.store(spacedelayStep);
                    spacedelaySeq.playingStep.store(spacedelayStep);
                    DBG("[SPACEDELAY SEQ] ★ Step changed to: " << spacedelayStep << " PPQ: " << ppq 
                        << " divIdx=" << spacedelaySeq.divisionIndex.load() 
                        << " stepsUsed=" << spacedelaySeq.stepsUsed.load());
                }
            } else if (spacedelaySeq.enabled.load() && !spacedelaySeq.active.load()) {
                DBG("[SPACEDELAY SEQ] WARNING: Enabled but not active! isPlaying=" + juce::String(isPlaying ? 1 : 0) + " ppqValid=" + juce::String(ppqValid ? 1 : 0));
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
            
            // Slicer sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && slicerSeq.active.load()) {
                const int slicerStep = slicerSeq.computeStepFromPPQ(ppq);
                if (slicerStep != slicerSeq.currentStep.load()) {
                    slicerSeq.currentStep.store(slicerStep);
                    slicerSeq.playingStep.store(slicerStep);
                    DBG("[SLICER SEQ] ★ Step changed to: " << slicerStep << " PPQ: " << ppq);
                }
            }
            
            // Dub Delay sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && dubdelaySeq.active.load()) {
                const int dubdelayStep = dubdelaySeq.computeStepFromPPQ(ppq);
                if (dubdelayStep != dubdelaySeq.currentStep.load()) {
                    dubdelaySeq.currentStep.store(dubdelayStep);
                    dubdelaySeq.playingStep.store(dubdelayStep);
                    DBG("[DUBDELAY SEQ] ★ Step changed to: " << dubdelayStep << " PPQ: " << ppq);
                }
            }
            
            // PhaseBloom sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && phaseBloomSeq.active.load()) {
                const int phaseBloomStep = phaseBloomSeq.computeStepFromPPQ(ppq);
                if (phaseBloomStep != phaseBloomSeq.currentStep.load()) {
                    phaseBloomSeq.currentStep.store(phaseBloomStep);
                    phaseBloomSeq.playingStep.store(phaseBloomStep);
                    DBG("[PHASEBLOOM SEQ] ★ Step changed to: " << phaseBloomStep << " PPQ: " << ppq);
                }
            }
            
            // Formant sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && formantSeq.active.load()) {
                const int formantStep = formantSeq.computeStepFromPPQ(ppq);
                if (formantStep != formantSeq.currentStep.load()) {
                    formantSeq.currentStep.store(formantStep);
                    formantSeq.playingStep.store(formantStep);
                    DBG("[FORMANT SEQ] ★ Step changed to: " << formantStep << " PPQ: " << ppq);
                }
            }
            
            // Saturate (Heat) sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && saturateSeq.active.load()) {
                const int saturateStep = saturateSeq.computeStepFromPPQ(ppq);
                if (saturateStep != saturateSeq.currentStep.load()) {
                    saturateSeq.currentStep.store(saturateStep);
                    saturateSeq.playingStep.store(saturateStep);
                    DBG("[SATURATE SEQ] ★ Step changed to: " << saturateStep << " PPQ: " << ppq);
                }
            }
            
            // Form 2 sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && form2Seq.active.load()) {
                const int form2Step = form2Seq.computeStepFromPPQ(ppq);
                if (form2Step != form2Seq.currentStep.load()) {
                    form2Seq.currentStep.store(form2Step);
                    form2Seq.playingStep.store(form2Step);
                    DBG("[FORM2 SEQ] ★ Step changed to: " << form2Step << " PPQ: " << ppq);
                }
            }
            
            // Redux sequencer stepping (shares same PPQ/transport, independent timing)
            if (isPlaying && ppqValid && reduxSeq.active.load()) {
                const int reduxStep = reduxSeq.computeStepFromPPQ(ppq);
                if (reduxStep != reduxSeq.currentStep.load()) {
                    reduxSeq.currentStep.store(reduxStep);
                    reduxSeq.playingStep.store(reduxStep);
                    DBG("[REDUX SEQ] ★ Step changed to: " << reduxStep << " PPQ: " << ppq);
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
            
            case EffectID::Slicer:
            {
                // Check if effect is enabled
                auto* slicerEnabledParam = valueTreeState.getRawParameterValue("slicerEnabled");
                bool isSlicerEnabled = slicerEnabledParam ? (slicerEnabledParam->load() > 0.5f) : false;
                
                // Debug: Check if slicer is enabled
                static int slicerEnabledDebugCounter = 0;
                if ((slicerEnabledDebugCounter++ % 1000) == 0) {
                    DBG("[SLICER ENABLED CHECK] enabled=" << (isSlicerEnabled ? "YES" : "NO") 
                        << " param=" << (slicerEnabledParam ? juce::String(slicerEnabledParam->load(), 3) : "NULL"));
                }
                
                if (isSlicerEnabled)
                {
                    // Check if sequencer is enabled and active
                    bool seqEnabled = slicerSeq.enabled.load();
                    bool seqActive = slicerSeq.active.load();
                    int playingStep = slicerSeq.playingStep.load();
                    
                    // Get parameters from sequencer snapshot OR APVTS
                    int patternIdx;
                    float divisionValue, offset01, shape01, releaseMs, mix01;
                    
                    if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16) {
                        // Read from step snapshot
                        StepSnapshot snapshot = slicerStepSnapshots[playingStep];
                        patternIdx = static_cast<int>(snapshot.slicer.pattern);
                        divisionValue = snapshot.slicer.division; // Keep as float for smooth control
                        offset01 = snapshot.slicer.offset;
                        shape01 = snapshot.slicer.shape;
                        releaseMs = snapshot.slicer.releaseMs;
                        mix01 = snapshot.slicer.mix;
                    } else {
                        // Read from APVTS (global)
                        auto* patternParam = valueTreeState.getRawParameterValue("slicerPattern");
                        auto* offsetParam = valueTreeState.getRawParameterValue("slicerOffset");
                        auto* shapeParam = valueTreeState.getRawParameterValue("slicerShape");
                        auto* releaseParam = valueTreeState.getRawParameterValue("slicerReleaseMs");
                        auto* mixParam = valueTreeState.getRawParameterValue("slicerMix");
                        
                        patternIdx = patternParam ? static_cast<int>(patternParam->load()) : 0;
                        
                        // Get division as choice index (not raw float)
                        auto* divisionChoiceParam = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("slicerDivision"));
                        divisionValue = divisionChoiceParam ? static_cast<float>(divisionChoiceParam->getIndex()) : 5.0f; // Default to index 5 (1/8)
                        offset01 = offsetParam ? offsetParam->load() : 0.5f;
                        shape01 = shapeParam ? shapeParam->load() : 0.5f;
                        releaseMs = releaseParam ? releaseParam->load() : 20.0f;
                        mix01 = mixParam ? mixParam->load() : 0.75f;
                    }
                    
                    // Sync toggle and grid (always from APVTS)
                    auto* syncParam = valueTreeState.getRawParameterValue("slicerSync");
                    bool syncOn = syncParam ? (syncParam->load() > 0.5f) : true;
                    
                    auto* gridParam = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("slicerGrid"));
                    int gridIdx = gridParam ? gridParam->getIndex() : 0;
                    
                    // Update tempo info (use full time signature)
                    slicer.setTempoInfo(transportCache.playing.load(), transportCache.bpm.load(), 
                                       transportCache.ppq.load(), transportCache.tsNum.load(), transportCache.tsDen.load());
                    
                    // Set parameters
                    slicer.setParameters(patternIdx, divisionValue, offset01, shape01, 
                                       releaseMs, mix01, syncOn, gridIdx);
                    
                    // Debug log (throttled)
                    static int slicerDebugCounter = 0;
                    if ((slicerDebugCounter++ % 500) == 0) {  // Every 500 blocks
                        float offsetBP = (offset01 - 0.5f) * 2.0f;
                        int divIdx = std::clamp(static_cast<int>(std::round(divisionValue)), 0, 8);
                        static const char* divNames[] = {"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
                        DBG("[SLICER] pat=" << patternIdx << " divValue=" << juce::String(divisionValue, 2) 
                            << " divIdx=" << divIdx << " divName=" << divNames[divIdx] << " grid=" << gridIdx
                            << " offsetBP=" << juce::String(offsetBP, 2)
                            << " shape=" << juce::String(shape01, 2)
                            << " rel=" << juce::String(releaseMs, 1) << "ms"
                            << " mix=" << juce::String(mix01, 2)
                            << " seqActive=" << (seqActive ? "Y" : "N")
                            << " step=" << playingStep);
                    }
                    
                    // Process
                    slicer.process(buffer);
                }
                break;
            }
            
            case EffectID::DubDelay:
            {
                // Check if effect is enabled
                auto* dubEnabledParam = valueTreeState.getRawParameterValue("dubEnabled");
                bool isDubEnabled = dubEnabledParam ? (dubEnabledParam->load() > 0.5f) : false;
                
                if (isDubEnabled)
                {
                    // Check if sequencer is enabled and active
                    bool seqEnabled = dubdelaySeq.enabled.load();
                    bool seqActive = dubdelaySeq.active.load();
                    int playingStep = dubdelaySeq.playingStep.load();
                    
                    // Get parameters from sequencer snapshot OR APVTS
                    float timeMs, feedback, toneHz, drive, wowFlutter, regenDamp, mix;
                    bool pingPong;
                    
                    if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16) {
                        // Read from step snapshot
                        StepSnapshot snapshot = dubdelayStepSnapshots[playingStep];
                        timeMs = snapshot.dubdelay.timeMs;
                        feedback = snapshot.dubdelay.feedback;
                        toneHz = snapshot.dubdelay.toneHz;
                        drive = snapshot.dubdelay.drive;
                        pingPong = snapshot.dubdelay.pingPong;
                        wowFlutter = snapshot.dubdelay.wowFlutter;
                        regenDamp = snapshot.dubdelay.regenDamp;
                        mix = snapshot.dubdelay.mix;
                    } else {
                        // Read from APVTS (global)
                        auto* timeParam = valueTreeState.getRawParameterValue("dubTimeMs");
                        auto* feedbackParam = valueTreeState.getRawParameterValue("dubFeedback");
                        auto* toneParam = valueTreeState.getRawParameterValue("dubToneHz");
                        auto* driveParam = valueTreeState.getRawParameterValue("dubDrive");
                        auto* pingPongParam = valueTreeState.getRawParameterValue("dubPingPong");
                        auto* wowFlutterParam = valueTreeState.getRawParameterValue("dubWowFlutter");
                        auto* regenDampParam = valueTreeState.getRawParameterValue("dubRegenDamp");
                        auto* mixParam = valueTreeState.getRawParameterValue("dubMix");
                        
                        timeMs = timeParam ? timeParam->load() : 450.0f;
                        feedback = feedbackParam ? feedbackParam->load() : 0.45f;
                        toneHz = toneParam ? toneParam->load() : 6500.0f;
                        drive = driveParam ? driveParam->load() : 0.15f;
                        pingPong = pingPongParam ? (pingPongParam->load() > 0.5f) : true;
                        wowFlutter = wowFlutterParam ? wowFlutterParam->load() : 0.35f;
                        regenDamp = regenDampParam ? regenDampParam->load() : 0.25f;
                        mix = mixParam ? mixParam->load() : 0.35f;
                    }
                    
                    // Check if sync is enabled
                    auto* syncParam = valueTreeState.getRawParameterValue("dubSync");
                    bool syncEnabled = syncParam ? (syncParam->load() > 0.5f) : false;
                    
                    // Compute final delay time (sync or free mode)
                    float finalTimeMs = timeMs;
                    
                    if (syncEnabled) {
                        // Tempo-synced mode: compute delay time from BPM + division + grid
                        
                        // Get safe BPM (with fallback)
                        double bpmSafe = transportCache.bpm.load();
                        if (bpmSafe < 20.0 || bpmSafe > 300.0) {
                            bpmSafe = 120.0; // Fallback
                        }
                        
                        // Get division and grid indices
                        auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("dubTimeDiv"));
                        auto* gridParam = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("dubTimeGrid"));
                        
                        int divIdx = divParam ? divParam->getIndex() : 5; // Default 1/8 (index 5)
                        int gridIdx = gridParam ? gridParam->getIndex() : 0; // Default straight
                        
                        // Clamp indices
                        divIdx = juce::jlimit(0, 8, divIdx);
                        gridIdx = juce::jlimit(0, 2, gridIdx);
                        
                        // Division beats (4, 2, 1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64)
                        static const double kDivBeats[] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625};
                        double beats = kDivBeats[divIdx];
                        
                        // Grid multiplier (straight=1.0, dotted=1.5, triplet=2/3)
                        static const double kGridMult[] = {1.0, 1.5, 2.0/3.0};
                        double gridMult = kGridMult[gridIdx];
                        
                        // Compute seconds and convert to ms
                        double seconds = (beats * gridMult) * (60.0 / bpmSafe);
                        finalTimeMs = static_cast<float>(juce::jlimit(1.0, 20000.0, seconds * 1000.0));
                        
                        // Debug: Log sync rate calculation
                        static int debugCounter = 0;
                        if ((debugCounter++ % 1000) == 0) { // Every ~16 seconds at 60Hz
                            DBG("[DUBDELAY SYNC] divIdx=" << divIdx << " beats=" << beats << " gridMult=" << gridMult 
                                << " bpm=" << bpmSafe << " seconds=" << seconds << " finalTimeMs=" << finalTimeMs);
                        }
                    }
                    
                    // Set all parameters (including computed time)
                    DubDelayProcessor::Targets targets;
                    targets.timeMs = finalTimeMs;
                    targets.feedback = feedback;
                    targets.toneHz = toneHz;
                    targets.drive = drive;
                    targets.pingPong = pingPong;
                    targets.wowFlutterDepth = wowFlutter;
                    targets.regenDamp = regenDamp;
                    targets.mix = mix;
                    
                    dubDelay.setTargets(targets);
                    
                    // Process
                    dubDelay.process(buffer, buffer.getNumSamples());
                }
                break;
            }
            
            case EffectID::Redux:
            {
                // Read Redux parameters from APVTS (in UI order)
                auto* bitDepthParam = valueTreeState.getRawParameterValue("reduxBitDepth");
                auto* sampleRateReductionParam = valueTreeState.getRawParameterValue("reduxSampleRateReduction");
                auto* jitterParam = valueTreeState.getRawParameterValue("reduxJitter");
                auto* preFilterParam = valueTreeState.getRawParameterValue("reduxPreFilter");
                auto* postFilterParam = valueTreeState.getRawParameterValue("reduxPostFilter");
                auto* driveParam = valueTreeState.getRawParameterValue("reduxDrive");
                auto* emphasisParam = valueTreeState.getRawParameterValue("reduxEmphasis");
                auto* mixParam = valueTreeState.getRawParameterValue("reduxMix");
                
                if (mixParam && bitDepthParam && sampleRateReductionParam && jitterParam &&
                    preFilterParam && postFilterParam && driveParam && emphasisParam)
                {
                    // Set Redux parameters (in order: mix, bitDepth, sampleRateReduction, jitter, preFilter, postFilter, drive, emphasis)
                    // Convert UI bit depth (1-12) to internal bit depth (4-16)
                    int internalBitDepth = static_cast<int>(bitDepthParam->load()) + 3;
                    reduxBank.setParams(
                        mixParam->load(),
                        internalBitDepth,
                        static_cast<int>(sampleRateReductionParam->load()),
                        jitterParam->load(),
                        preFilterParam->load(),
                        postFilterParam->load(),
                        driveParam->load(),
                        emphasisParam->load()
                    );
                    
                    // Process Redux effect
                    juce::dsp::AudioBlock<float> audioBlock(buffer);
                    reduxBank.process(audioBlock);
                }
                break;
            }
            
            case EffectID::PhaseBloom:
            {
                // Check if effect is enabled
                auto* phaseBloomEnabledParam = valueTreeState.getRawParameterValue("phasebloomEnabled");
                bool isPhaseBloomEnabled = phaseBloomEnabledParam ? (phaseBloomEnabledParam->load() > 0.5f) : false;
                
                if (isPhaseBloomEnabled)
                {
                    // Get PhaseBloom sequencer state
                    auto& phaseBloomSeq = getPhaseBloomSeqState();
                    
                    // Check if sequencer is enabled and active
                    if (phaseBloomSeq.enabled.load() && phaseBloomSeq.active.load())
                    {
                        // Get current step snapshot
                        int currentStep = phaseBloomSeq.currentStep.load();
                        if (currentStep >= 0 && currentStep < 16)
                        {
                            StepSnapshot snapshot = getPhaseBloomSafeSnapshot(currentStep);
                            
                            // Safety check for parameter values
                            snapshot.phasebloom.depth = juce::jlimit(0.0f, 1.0f, snapshot.phasebloom.depth);
                            snapshot.phasebloom.rate = juce::jlimit(0.0f, 1.0f, snapshot.phasebloom.rate);
                            snapshot.phasebloom.feedback = juce::jlimit(-0.8f, 0.8f, snapshot.phasebloom.feedback);
                            snapshot.phasebloom.center = juce::jlimit(200.0f, 8000.0f, snapshot.phasebloom.center);
                            snapshot.phasebloom.bloom = juce::jlimit(0.0f, 1.0f, snapshot.phasebloom.bloom);
                            snapshot.phasebloom.spread = juce::jlimit(0.0f, 1.0f, snapshot.phasebloom.spread);
                            snapshot.phasebloom.resonance = juce::jlimit(0.0f, 1.0f, snapshot.phasebloom.resonance);
                            snapshot.phasebloom.mix = juce::jlimit(0.0f, 1.0f, snapshot.phasebloom.mix);
                            
                            // DEBUG: Log sequencer parameters every 1000 blocks
                            static int debugCounter = 0;
                            if ((debugCounter++ % 1000) == 0) {
                                DBG("[PHASEBLOOM SEQ] Step=" << currentStep 
                                    << " depth=" << snapshot.phasebloom.depth 
                                    << " rate=" << snapshot.phasebloom.rate 
                                    << " feedback=" << snapshot.phasebloom.feedback
                                    << " center=" << snapshot.phasebloom.center
                                    << " bloom=" << snapshot.phasebloom.bloom
                                    << " spread=" << snapshot.phasebloom.spread
                                    << " resonance=" << snapshot.phasebloom.resonance
                                    << " mix=" << snapshot.phasebloom.mix
                                    << " bpm=" << transportCache.bpm.load());
                            }
                            
                            // Apply sequencer parameters to engine
                            phaseBloomEngine.setDepth(snapshot.phasebloom.depth);
                            phaseBloomEngine.setRate(snapshot.phasebloom.rate);
                            phaseBloomEngine.setFeedback(snapshot.phasebloom.feedback);
                            phaseBloomEngine.setCenter(snapshot.phasebloom.center);
                            phaseBloomEngine.setBloom(snapshot.phasebloom.bloom);
                            phaseBloomEngine.setSpread(snapshot.phasebloom.spread);
                            phaseBloomEngine.setResonance(snapshot.phasebloom.resonance);
                            phaseBloomEngine.setMix(snapshot.phasebloom.mix);
                            phaseBloomEngine.setEnabled(true);
                            
                            // Only process if mix > 0
                            if (snapshot.phasebloom.mix > 0.0f)
                            {
                                // Process PhaseBloom effect
                                phaseBloomEngine.process(buffer, transportCache.bpm.load());
                            }
                        }
                    }
                    else
                    {
                        // Use static parameters when sequencer is disabled
                        auto* depthParam = valueTreeState.getRawParameterValue("phasebloomDepth");
                        auto* rateParam = valueTreeState.getRawParameterValue("phasebloomRate");
                        auto* feedbackParam = valueTreeState.getRawParameterValue("phasebloomFeedback");
                        auto* centerParam = valueTreeState.getRawParameterValue("phasebloomCenter");
                        auto* bloomParam = valueTreeState.getRawParameterValue("phasebloomBloom");
                        auto* spreadParam = valueTreeState.getRawParameterValue("phasebloomSpread");
                        auto* resonanceParam = valueTreeState.getRawParameterValue("phasebloomResonance");
                        auto* mixParam = valueTreeState.getRawParameterValue("phasebloomMix");
                        
                        if (depthParam && rateParam && feedbackParam && centerParam &&
                            bloomParam && spreadParam && resonanceParam && mixParam)
                        {
                            float mixValue = mixParam->load();
                            
                            // Set PhaseBloom parameters
                            phaseBloomEngine.setDepth(depthParam->load());
                            phaseBloomEngine.setRate(rateParam->load());
                            phaseBloomEngine.setFeedback(feedbackParam->load());
                            phaseBloomEngine.setCenter(centerParam->load());
                            phaseBloomEngine.setBloom(bloomParam->load());
                            phaseBloomEngine.setSpread(spreadParam->load());
                            phaseBloomEngine.setResonance(resonanceParam->load());
                            phaseBloomEngine.setMix(mixValue);
                            phaseBloomEngine.setEnabled(true);
                            
                            // Only process if mix > 0
                            if (mixValue > 0.0f)
                            {
                                // Process PhaseBloom effect
                                phaseBloomEngine.process(buffer, transportCache.bpm.load());
                            }
                        }
                    }
                }
                break;
            }
            
            case EffectID::Formant:
            {
                // TEMPORARILY DISABLED: Check if effect is enabled
                auto* formantEnabledParam = valueTreeState.getRawParameterValue("formantEnabled");
                bool isFormantEnabled = formantEnabledParam ? (formantEnabledParam->load() > 0.5f) : false;
                
                // Skip processing to prevent crash - Formant effect disabled
                if (isFormantEnabled)
                {
                    // Get Formant sequencer state
                    auto& formantSeq = getFormantSeqState();
                    
                    // Check if sequencer is enabled and active
                    if (formantSeq.enabled.load() && formantSeq.active.load())
                    {
                        // Get current step snapshot
                        int currentStep = formantSeq.currentStep.load();
                        if (currentStep >= 0 && currentStep < 16)
                        {
                        StepSnapshot snapshot = getFormantSafeSnapshot(currentStep);
                        
                        // Safety check for parameter values
                        snapshot.formant.vowel = juce::jlimit(0.0f, 4.0f, snapshot.formant.vowel);
                        snapshot.formant.resonance = juce::jlimit(0.4f, 18.0f, snapshot.formant.resonance);
                        snapshot.formant.intensity = juce::jlimit(-6.0f, 18.0f, snapshot.formant.intensity);
                        snapshot.formant.mix = juce::jlimit(0.0f, 1.0f, snapshot.formant.mix);
                        
                        // Set snapshot values into APVTS for processing
                        auto* vowelParam = valueTreeState.getRawParameterValue("vowel");
                        auto* resonanceParam = valueTreeState.getRawParameterValue("resonance");
                        auto* intensityParam = valueTreeState.getRawParameterValue("intensity");
                        auto* mixParam = valueTreeState.getRawParameterValue("mix");
                        
                        if (vowelParam) *vowelParam = snapshot.formant.vowel;
                        if (resonanceParam) *resonanceParam = snapshot.formant.resonance;
                        if (intensityParam) *intensityParam = snapshot.formant.intensity;
                        if (mixParam) *mixParam = snapshot.formant.mix;
                        
                        // Only process if mix > 0
                        if (snapshot.formant.mix > 0.0f)
                            {
                                // Process Formant effect
                                formantProcessor.process(buffer, buffer.getNumSamples(), valueTreeState);
                            }
                        }
                    }
                    else
                    {
                        // Use static parameters when sequencer is disabled
                        // Process formant effect
                        formantProcessor.process(buffer, buffer.getNumSamples(), valueTreeState);
                    }
                }
                break;
            }

            case EffectID::Saturate:
            {
                auto* saturateEnabledParam = valueTreeState.getRawParameterValue("saturateEnabled");
                bool isSaturateEnabled = saturateEnabledParam ? (saturateEnabledParam->load() > 0.5f) : false;
                
                if (isSaturateEnabled) {
                    // Get Saturate sequencer state
                    auto& seqState = saturateSeq;
                    
                    // Check if sequencer is enabled and active - only update on step change
                    static int lastSaturateStep = -1;
                    
                    // Only update parameters if sequencer is enabled, active, and step changed
                    if (seqState.enabled.load() && seqState.active.load()) {
                        int currentStep = seqState.currentStep.load();
                        bool stepChanged = (currentStep != lastSaturateStep);
                        
                        if (stepChanged && currentStep >= 0 && currentStep < 16) {
                        StepSnapshot snapshot = getSaturateSafeSnapshot(currentStep);
                        
                        // Update APVTS parameters from sequencer snapshot (only when step changes)
                        // Use parameter's convertFrom0to1 to properly convert snapshot values
                        auto* typeParam = valueTreeState.getParameter("satType");
                        auto* driveParam = valueTreeState.getParameter("satDrive");
                        auto* colorParam = valueTreeState.getParameter("satColor");
                        auto* shapeParam = valueTreeState.getParameter("satShape");
                        auto* biasParam = valueTreeState.getParameter("satBias");
                        auto* outputParam = valueTreeState.getParameter("satOut");
                        auto* mixParam = valueTreeState.getParameter("satMix");
                        
                        // Convert snapshot values to normalized 0-1 range, then let parameter convert to actual values
                        if (typeParam) {
                            // Round type to nearest integer before normalizing (fixes snapping)
                            int typeInt = juce::jlimit(0, 7, static_cast<int>(std::round(snapshot.saturate.type)));
                            float normType = static_cast<float>(typeInt) / 7.0f;
                            typeParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normType));
                        }
                        if (driveParam) {
                            float normDrive = snapshot.saturate.drive / 36.0f;
                            driveParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normDrive));
                        }
                        if (colorParam) {
                            colorParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, snapshot.saturate.color));
                        }
                        if (shapeParam) {
                            shapeParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, snapshot.saturate.shape));
                        }
                        if (biasParam) {
                            float normBias = (snapshot.saturate.bias + 0.2f) / 0.4f;
                            biasParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normBias));
                        }
                        if (outputParam) {
                            float normOut = (snapshot.saturate.output + 24.0f) / 36.0f;
                            outputParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normOut));
                        }
                        if (mixParam) {
                            mixParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, snapshot.saturate.mix));
                        }
                            
                            lastSaturateStep = currentStep;
                        }
                    } else {
                        // Sequencer disabled or inactive - reset last step tracker
                        lastSaturateStep = -1;
                    }
                    
                    // Oversample always max (3 = 8×) - check once, not every block
                    static bool osSet = false;
                    if (!osSet) {
                        auto* osParam = valueTreeState.getParameter("satOsMode");
                        if (osParam && osParam->getValue() < 1.0f) {
                            osParam->setValueNotifyingHost(1.0f); // 3/3 = max
                        }
                        osSet = true;
                    }
                    
                    // Process Saturate effect (always uses current APVTS values)
                    saturateProcessor.process(buffer, buffer.getNumSamples(), valueTreeState);
                }
                break;
            }
            
            // case EffectID::Shimmer:  // TODO: Shimmer effect not yet implemented - entire case commented out
            /*
            {
                auto* shimmerEnabledParam = valueTreeState.getRawParameterValue("shimmerEnabled");
                bool isShimmerEnabled = shimmerEnabledParam ? (shimmerEnabledParam->load() > 0.5f) : false;
                
                if (isShimmerEnabled) {
                    // Get Shimmer sequencer state
                    auto& seqState = shimmerSeq;
                    
                    // Check if sequencer is enabled and active
                    if (seqState.enabled.load() && seqState.active.load()) {
                        int currentStep = seqState.currentStep.load();
                        if (currentStep >= 0 && currentStep < 16) {
                            StepSnapshot snapshot = getShimmerSafeSnapshot(currentStep);
                            
                            // Get mix from APVTS (global, not per-step)
                            auto* mixParam = valueTreeState.getRawParameterValue("mix");
                            float mix = mixParam ? mixParam->load() : 0.5f;
                            
                            // Process with snapshot
                            // Always use 8x oversampling internally (osMode = 3)
                            shimmerProcessor.processWithSnapshot(
                                buffer, buffer.getNumSamples(),
                                snapshot.shimmer.mode,
                                snapshot.shimmer.size,
                                snapshot.shimmer.decay,
                                snapshot.shimmer.color,
                                snapshot.shimmer.predelay,
                                snapshot.shimmer.shimAmt,
                                3.0f, // Always 8x OS internally
                                mix,
                                false
                            );
                        }
                    } else {
                        // Sequencer not active - read from APVTS
                        auto* modeParam = valueTreeState.getRawParameterValue("shimMode");
                        auto* sizeParam = valueTreeState.getRawParameterValue("size");
                        auto* decayParam = valueTreeState.getRawParameterValue("decay");
                        auto* colorParam = valueTreeState.getRawParameterValue("color");
                        auto* predelayParam = valueTreeState.getRawParameterValue("predelay");
                        auto* shimAmtParam = valueTreeState.getRawParameterValue("shimAmt");
                        auto* osModeParam = valueTreeState.getRawParameterValue("osMode");
                        auto* mixParam = valueTreeState.getRawParameterValue("mix");
                        
                        auto* modeParamObj = valueTreeState.getParameter("shimMode");
                        
                        if (sizeParam && decayParam && colorParam && predelayParam && shimAmtParam && mixParam && modeParamObj) {
                            juce::dsp::AudioBlock<float> block(buffer);
                            shimmerProcessor.setParams(
                                sizeParam->load(),
                                decayParam->load(),
                                colorParam->load(),
                                predelayParam->load(),
                                shimAmtParam->load(),
                                (int)modeParamObj->convertFrom0to1(modeParamObj->getValue()),
                                3, // Always 8x OS internally (osMode index 3)
                                mixParam->load()
                            );
                            shimmerProcessor.process(block);
                        }
                    }
                }
                break;
            }
            */

            case EffectID::Form2:
            {
                // Check if effect is enabled
                auto* form2EnabledParam = valueTreeState.getRawParameterValue("form2Enabled");
                bool isForm2Enabled = form2EnabledParam ? (form2EnabledParam->load() > 0.5f) : false;
                
                if (isForm2Enabled)
                {
                    // Get Form2 sequencer state
                    auto& seqState = form2Seq;
                    
                    // Check if sequencer is enabled and active
                    if (seqState.enabled.load() && seqState.active.load())
                    {
                        // Get current step snapshot
                        int currentStep = seqState.currentStep.load();
                        if (currentStep >= 0 && currentStep < 16)
                        {
                            StepSnapshot snapshot = getForm2SafeSnapshot(currentStep);
                            
                            // Safety check for parameter values
                            snapshot.form2.rootNote = juce::jlimit(0, 11, snapshot.form2.rootNote);
                            snapshot.form2.scale = juce::jlimit(0, 6, snapshot.form2.scale);
                            snapshot.form2.chordSize = juce::jlimit(1, 8, snapshot.form2.chordSize);
                            snapshot.form2.shift = juce::jlimit(0.5f, 2.0f, snapshot.form2.shift);
                            snapshot.form2.color = juce::jlimit(-12.0f, 12.0f, snapshot.form2.color);
                            snapshot.form2.motion = juce::jlimit(0.0f, 1.0f, snapshot.form2.motion);
                            snapshot.form2.resynth = juce::jlimit(0.0f, 1.0f, snapshot.form2.resynth);
                            snapshot.form2.mix = juce::jlimit(0.0f, 1.0f, snapshot.form2.mix);
                            
                            // Set Form2 parameters from sequencer
                            // Map rootNote/scale/chordSize to vowel (Form2Processor uses vowel, not rootNote/scale/chordSize)
                            float vowelFromRoot = (snapshot.form2.rootNote % 5) / 4.0f;
                            form2Processor.setVowel(vowelFromRoot);
                            form2Processor.setShift(snapshot.form2.shift);
                            form2Processor.setBrightness(snapshot.form2.color);
                            form2Processor.setMotion(snapshot.form2.motion);
                            form2Processor.setAir(snapshot.form2.resynth);
                            form2Processor.setMix(snapshot.form2.mix);
                            
                            // Only process if mix > 0
                            if (snapshot.form2.mix > 0.0f)
                            {
                                // Set host tempo for LFO sync
                                auto bpm = getPlayHead()->getPosition()->getBpm().orFallback(120.0);
                                bool hasTempo = getPlayHead()->getPosition()->getBpm().hasValue();
                                form2Processor.setHostTempo(bpm, hasTempo);
                                
                                // Process Form2 effect
                                auto block = juce::dsp::AudioBlock<float>(buffer);
                                form2Processor.process(block);
                            }
                        }
                    }
                    else
                    {
                        // Use static parameters when sequencer is disabled
                        auto* rootNoteParam = valueTreeState.getRawParameterValue("form2RootNote");
                        auto* scaleParam = valueTreeState.getRawParameterValue("form2Scale");
                        auto* chordSizeParam = valueTreeState.getRawParameterValue("form2ChordSize");
                        auto* shiftParam = valueTreeState.getRawParameterValue("form2Shift");
                        auto* brightnessParam = valueTreeState.getRawParameterValue("form2Brightness");
                        auto* motionParam = valueTreeState.getRawParameterValue("form2Motion");
                        auto* airParam = valueTreeState.getRawParameterValue("form2Air");
                        auto* mixParam = valueTreeState.getRawParameterValue("form2Mix");
                        
                        if (rootNoteParam && scaleParam && chordSizeParam && shiftParam &&
                            brightnessParam && motionParam && airParam && mixParam)
                        {
                            // Set Form2 parameters
                            // Map rootNote/scale/chordSize to vowel (Form2Processor uses vowel, not rootNote/scale/chordSize)
                            float vowelFromRoot = (static_cast<int>(rootNoteParam->load()) % 5) / 4.0f;
                            form2Processor.setVowel(vowelFromRoot);
                            form2Processor.setShift(shiftParam->load());
                            form2Processor.setBrightness(brightnessParam->load());
                            form2Processor.setMotion(motionParam->load());
                            form2Processor.setAir(airParam->load());
                            form2Processor.setMix(mixParam->load());
                            
                            // Set host tempo for LFO sync
                            auto bpm = getPlayHead()->getPosition()->getBpm().orFallback(120.0);
                            bool hasTempo = getPlayHead()->getPosition()->getBpm().hasValue();
                            form2Processor.setHostTempo(bpm, hasTempo);
                            
                            // Process Form2 effect
                            auto block = juce::dsp::AudioBlock<float>(buffer);
                            form2Processor.process(block);
                        }
                    }
                }
                break;
            }
            
            case EffectID::Filter:
            {
                // Process filter (enabled check is handled by UI power button, but we process if in routing)
                // Note: filterEnabled parameter may not exist, so we process regardless
                {
                    // Get Filter sequencer state
                    auto& seqState = filterSeq;
                    
                    // Read filter parameters (from sequencer snapshot if active, else APVTS)
                    FilterTargets targets;
                    
                    if (seqState.enabled.load() && seqState.active.load())
                    {
                        // Get current step snapshot
                        int currentStep = seqState.currentStep.load();
                        if (currentStep >= 0 && currentStep < 16)
                        {
                            StepSnapshot snapshot = getFilterSafeSnapshot(currentStep);
                            
                            targets.type = static_cast<int>(snapshot.filter.type);
                            targets.cutoff = snapshot.filter.cutoff;
                            targets.res = snapshot.filter.resonance;
                            targets.slope = (snapshot.filter.slope > 0.5f) ? 1 : 0;
                            targets.drive = snapshot.filter.drive;
                            targets.spread = snapshot.filter.spread;
                            targets.keytrack = snapshot.filter.keytrack;
                            // Mix is always from APVTS (global, not per-step)
                            auto* mixParam = valueTreeState.getRawParameterValue("filterMix");
                            targets.mix = mixParam ? mixParam->load() : 1.0f;
                        }
                        else
                        {
                            // Fallback to APVTS if step invalid
                            auto* typeParam = valueTreeState.getRawParameterValue("fType");
                            auto* cutoffParam = valueTreeState.getRawParameterValue("cutoff");
                            auto* resParam = valueTreeState.getRawParameterValue("res");
                            auto* slopeParam = valueTreeState.getRawParameterValue("slope");
                            auto* driveParam = valueTreeState.getRawParameterValue("drive");
                            auto* spreadParam = valueTreeState.getRawParameterValue("spread");
                            auto* keytrackParam = valueTreeState.getRawParameterValue("keytrack");
                            auto* mixParam = valueTreeState.getRawParameterValue("filterMix");
                            
                            if (typeParam && cutoffParam && resParam && slopeParam && 
                                driveParam && spreadParam && keytrackParam && mixParam)
                            {
                            targets.type = static_cast<int>(typeParam->load());
                            // Convert normalized cutoff (0-1) to frequency using custom curve
                            float normalized = juce::jlimit(0.0f, 1.0f, cutoffParam->load());
                            targets.cutoff = convertNormalizedCutoffToFrequency(normalized);
                            targets.res = juce::jlimit(0.0f, 1.0f, resParam->load());
                                targets.slope = (slopeParam->load() > 0.5f) ? 1 : 0;
                                targets.drive = juce::jlimit(0.0f, 36.0f, driveParam->load());
                                targets.spread = juce::jlimit(-50.0f, 50.0f, spreadParam->load());
                                targets.keytrack = juce::jlimit(0.0f, 1.0f, keytrackParam->load());
                                targets.mix = juce::jlimit(0.0f, 1.0f, mixParam->load());
                            }
                            else
                            {
                                // Use defaults if parameters missing
                                targets.type = 0;
                                targets.cutoff = 1200.0f;
                                targets.res = 0.35f;
                                targets.slope = 1;
                                targets.drive = 6.0f;
                                targets.spread = 0.0f;
                                targets.keytrack = 0.0f;
                                targets.mix = 1.0f;
                            }
                        }
                    }
                    else
                    {
                        // Sequencer not active - read from APVTS
                        auto* typeParam = valueTreeState.getRawParameterValue("fType");
                        auto* cutoffParam = valueTreeState.getRawParameterValue("cutoff");
                        auto* resParam = valueTreeState.getRawParameterValue("res");
                        auto* slopeParam = valueTreeState.getRawParameterValue("slope");
                        auto* driveParam = valueTreeState.getRawParameterValue("drive");
                        auto* spreadParam = valueTreeState.getRawParameterValue("spread");
                        auto* keytrackParam = valueTreeState.getRawParameterValue("keytrack");
                        auto* mixParam = valueTreeState.getRawParameterValue("filterMix");
                        
                        if (typeParam && cutoffParam && resParam && slopeParam && 
                            driveParam && spreadParam && keytrackParam && mixParam)
                        {
                            targets.type = static_cast<int>(typeParam->load());
                            // Convert normalized cutoff (0-1) to frequency using custom curve
                            float normalized = juce::jlimit(0.0f, 1.0f, cutoffParam->load());
                            targets.cutoff = convertNormalizedCutoffToFrequency(normalized);
                            targets.res = juce::jlimit(0.0f, 1.0f, resParam->load());
                            targets.slope = (slopeParam->load() > 0.5f) ? 1 : 0;
                            targets.drive = juce::jlimit(0.0f, 36.0f, driveParam->load());
                            targets.spread = juce::jlimit(-50.0f, 50.0f, spreadParam->load());
                            targets.keytrack = juce::jlimit(0.0f, 1.0f, keytrackParam->load());
                            targets.mix = juce::jlimit(0.0f, 1.0f, mixParam->load());
                        }
                        else
                        {
                            // Use defaults if parameters missing
                            targets.type = 0;
                            targets.cutoff = 1200.0f;
                            targets.res = 0.35f;
                            targets.slope = 1;
                            targets.drive = 6.0f;
                            targets.spread = 0.0f;
                            targets.keytrack = 0.0f;
                            targets.mix = 1.0f;
                        }
                    }
                    
                    // Get current MIDI note for key tracking (if available)
                    if (targets.keytrack > 0.001f && midiMessages.getNumEvents() > 0)
                    {
                        for (const auto metadata : midiMessages)
                        {
                            auto message = metadata.getMessage();
                            if (message.isNoteOn())
                            {
                                filterProcessor.setCurrentMIDINote(message.getNoteNumber());
                                break;
                            }
                        }
                    }
                    
                    // Set filter targets
                    filterProcessor.setTargets(targets);
                    
                    // Process filter
                    filterProcessor.process(buffer, buffer.getNumSamples());
                }
                break;
            }
        }
    }
    
    // Process COMPRESS+ Master Effect (after all other effects)
    processCompressEffect(buffer);
    
    // Safety check: detect and fix NaN/infinity values that could cause audio to go silent
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float& sampleValue = channelData[sample];
            
            // Check for NaN or infinity values
            if (!std::isfinite(sampleValue))
            {
                DBG("[SAFETY] Detected invalid audio value: " << sampleValue << " at channel " << channel << " sample " << sample);
                sampleValue = 0.0f; // Reset to silence
            }
            
            // Clamp to reasonable range to prevent extreme values
            sampleValue = juce::jlimit(-1.0f, 1.0f, sampleValue);
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
    auto* hpQParam = valueTreeState.getRawParameterValue("masterHPQ");
    auto* lpQParam = valueTreeState.getRawParameterValue("masterLPQ");
    
    if (hpParam && lpParam && hpQParam && lpQParam)
    {
        const float hpTarget = juce::jlimit(20.0f, 20000.0f, hpParam->load());
        const float lpTarget = juce::jlimit(20.0f, 20000.0f, lpParam->load());
        const float hpQ = hpQParam->load();
        const float lpQ = lpQParam->load();
        
        hpCutoffSmooth.setTargetValue(hpTarget);
        lpCutoffSmooth.setTargetValue(lpTarget);
        
        const int numSamplesLocal = buffer.getNumSamples();
        const int numChannelsLocal = buffer.getNumChannels();
        
        for (int n = 0; n < numSamplesLocal; ++n)
        {
            const float hpHz = hpCutoffSmooth.getNextValue();
            const float lpHz = lpCutoffSmooth.getNextValue();
            
            // Update both channels' cutoffs and resonance (per-sample for ultra-smooth sweeps)
            masterHPF[0].setCutoffFrequency(hpHz);
            masterLPF[0].setCutoffFrequency(lpHz);
            masterHPF[0].setResonance(hpQ);
            masterLPF[0].setResonance(lpQ);
            
            if (numChannelsLocal > 1)
            {
                masterHPF[1].setCutoffFrequency(hpHz);
                masterLPF[1].setCutoffFrequency(lpHz);
                masterHPF[1].setResonance(hpQ);
                masterLPF[1].setResonance(lpQ);
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
    
    // Debug: show saved assignment
    auto routingOrder = effectRouter.getRoutingOrder();
    DBG("[State] Saving router: Slot1=" + juce::String(static_cast<int>(routingOrder[0])) + 
        " Slot2=" + juce::String(static_cast<int>(routingOrder[1])) + 
        " Slot3=" + juce::String(static_cast<int>(routingOrder[2])) + 
        " Slot4=" + juce::String(static_cast<int>(routingOrder[3])));
    
    // Save sequencer settings for all effects
    auto seqSettings = juce::ValueTree("SequencerSettings");
    
    // SpaceDelay sequencer
    auto delaySeqTree = juce::ValueTree("DelaySequencer");
    delaySeqTree.setProperty("enabled", seq.enabled.load(), nullptr);
    delaySeqTree.setProperty("stepsUsed", seq.stepsUsed.load(), nullptr);
    delaySeqTree.setProperty("divisionIndex", seq.divisionIndex.load(), nullptr);
    delaySeqTree.setProperty("stdMode", seq.stdMode.load(), nullptr);
    seqSettings.addChild(delaySeqTree, -1, nullptr);
    
    // AutoPan sequencer
    auto autopanSeqTree = juce::ValueTree("AutoPanSequencer");
    autopanSeqTree.setProperty("enabled", autopanSeq.enabled.load(), nullptr);
    autopanSeqTree.setProperty("stepsUsed", autopanSeq.stepsUsed.load(), nullptr);
    autopanSeqTree.setProperty("divisionIndex", autopanSeq.divisionIndex.load(), nullptr);
    autopanSeqTree.setProperty("stdMode", autopanSeq.stdMode.load(), nullptr);
    seqSettings.addChild(autopanSeqTree, -1, nullptr);
    
    // Dirt sequencer
    auto dirtSeqTree = juce::ValueTree("DirtSequencer");
    dirtSeqTree.setProperty("enabled", dirtSeq.enabled.load(), nullptr);
    dirtSeqTree.setProperty("stepsUsed", dirtSeq.stepsUsed.load(), nullptr);
    dirtSeqTree.setProperty("divisionIndex", dirtSeq.divisionIndex.load(), nullptr);
    dirtSeqTree.setProperty("stdMode", dirtSeq.stdMode.load(), nullptr);
    seqSettings.addChild(dirtSeqTree, -1, nullptr);
    
    // Chorus sequencer
    auto chorusSeqTree = juce::ValueTree("ChorusSequencer");
    chorusSeqTree.setProperty("enabled", chorusSeq.enabled.load(), nullptr);
    chorusSeqTree.setProperty("stepsUsed", chorusSeq.stepsUsed.load(), nullptr);
    chorusSeqTree.setProperty("divisionIndex", chorusSeq.divisionIndex.load(), nullptr);
    chorusSeqTree.setProperty("stdMode", chorusSeq.stdMode.load(), nullptr);
    seqSettings.addChild(chorusSeqTree, -1, nullptr);
    
    // Reverb sequencer
    auto reverbSeqTree = juce::ValueTree("ReverbSequencer");
    reverbSeqTree.setProperty("enabled", reverbSeq.enabled.load(), nullptr);
    reverbSeqTree.setProperty("stepsUsed", reverbSeq.stepsUsed.load(), nullptr);
    reverbSeqTree.setProperty("divisionIndex", reverbSeq.divisionIndex.load(), nullptr);
    reverbSeqTree.setProperty("stdMode", reverbSeq.stdMode.load(), nullptr);
    seqSettings.addChild(reverbSeqTree, -1, nullptr);
    
    // Granular sequencer
    auto granularSeqTree = juce::ValueTree("GranularSequencer");
    granularSeqTree.setProperty("enabled", granularSeq.enabled.load(), nullptr);
    granularSeqTree.setProperty("stepsUsed", granularSeq.stepsUsed.load(), nullptr);
    granularSeqTree.setProperty("divisionIndex", granularSeq.divisionIndex.load(), nullptr);
    granularSeqTree.setProperty("stdMode", granularSeq.stdMode.load(), nullptr);
    seqSettings.addChild(granularSeqTree, -1, nullptr);
    
    // Slicer sequencer
    auto slicerSeqTree = juce::ValueTree("SlicerSequencer");
    slicerSeqTree.setProperty("enabled", slicerSeq.enabled.load(), nullptr);
    slicerSeqTree.setProperty("stepsUsed", slicerSeq.stepsUsed.load(), nullptr);
    slicerSeqTree.setProperty("divisionIndex", slicerSeq.divisionIndex.load(), nullptr);
    slicerSeqTree.setProperty("stdMode", slicerSeq.stdMode.load(), nullptr);
    seqSettings.addChild(slicerSeqTree, -1, nullptr);
    
    // Formant sequencer
    auto formantSeqTree = juce::ValueTree("FormantSequencer");
    formantSeqTree.setProperty("enabled", formantSeq.enabled.load(), nullptr);
    formantSeqTree.setProperty("stepsUsed", formantSeq.stepsUsed.load(), nullptr);
    formantSeqTree.setProperty("divisionIndex", formantSeq.divisionIndex.load(), nullptr);
    formantSeqTree.setProperty("stdMode", formantSeq.stdMode.load(), nullptr);
    seqSettings.addChild(formantSeqTree, -1, nullptr);
    
    // Saturate (Heat) sequencer
    auto saturateSeqTree = juce::ValueTree("SaturateSequencer");
    saturateSeqTree.setProperty("enabled", saturateSeq.enabled.load(), nullptr);
    saturateSeqTree.setProperty("stepsUsed", saturateSeq.stepsUsed.load(), nullptr);
    saturateSeqTree.setProperty("divisionIndex", saturateSeq.divisionIndex.load(), nullptr);
    saturateSeqTree.setProperty("stdMode", saturateSeq.stdMode.load(), nullptr);
    seqSettings.addChild(saturateSeqTree, -1, nullptr);
    
    state.addChild(seqSettings, -1, nullptr);
    
    // Save step snapshots for all effects
    auto stepsnapshots = juce::ValueTree("StepSnapshots");
    
    // SpaceDelay snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("DelayStep" + juce::String(i));
        stepTree.setProperty("timeMs", stepSnapshots[i].delay.timeMs, nullptr);
        stepTree.setProperty("feedback", stepSnapshots[i].delay.feedback, nullptr);
        stepTree.setProperty("wowDepth", stepSnapshots[i].delay.wowDepth, nullptr);
        stepTree.setProperty("wowRate", stepSnapshots[i].delay.wowRate, nullptr);
        stepTree.setProperty("saturation", stepSnapshots[i].delay.saturation, nullptr);
        stepTree.setProperty("highCut", stepSnapshots[i].delay.highCut, nullptr);
        stepTree.setProperty("lowCut", stepSnapshots[i].delay.lowCut, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // AutoPan snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("AutoPanStep" + juce::String(i));
        stepTree.setProperty("rate", autopanStepSnapshots[i].autopan.rate, nullptr);
        stepTree.setProperty("phase", autopanStepSnapshots[i].autopan.phase, nullptr);
        stepTree.setProperty("waveType", autopanStepSnapshots[i].autopan.waveType, nullptr);
        stepTree.setProperty("waveShape", autopanStepSnapshots[i].autopan.waveShape, nullptr);
        stepTree.setProperty("inverted", autopanStepSnapshots[i].autopan.inverted, nullptr);
        stepTree.setProperty("amount", autopanStepSnapshots[i].autopan.amount, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Dirt snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("DirtStep" + juce::String(i));
        stepTree.setProperty("drive", dirtStepSnapshots[i].dirt.drive, nullptr);
        stepTree.setProperty("color", dirtStepSnapshots[i].dirt.color, nullptr);
        stepTree.setProperty("asym", dirtStepSnapshots[i].dirt.asym, nullptr);
        stepTree.setProperty("texture", dirtStepSnapshots[i].dirt.texture, nullptr);
        stepTree.setProperty("lowCut", dirtStepSnapshots[i].dirt.lowCut, nullptr);
        stepTree.setProperty("highCut", dirtStepSnapshots[i].dirt.highCut, nullptr);
        stepTree.setProperty("tone", dirtStepSnapshots[i].dirt.tone, nullptr);
        stepTree.setProperty("mix", dirtStepSnapshots[i].dirt.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Chorus snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("ChorusStep" + juce::String(i));
        stepTree.setProperty("rate", chorusStepSnapshots[i].chorus.rate, nullptr);
        stepTree.setProperty("depth", chorusStepSnapshots[i].chorus.depth, nullptr);
        stepTree.setProperty("voices", chorusStepSnapshots[i].chorus.voices, nullptr);
        stepTree.setProperty("delayTime", chorusStepSnapshots[i].chorus.delayTime, nullptr);
        stepTree.setProperty("feedback", chorusStepSnapshots[i].chorus.feedback, nullptr);
        stepTree.setProperty("width", chorusStepSnapshots[i].chorus.width, nullptr);
        stepTree.setProperty("tone", chorusStepSnapshots[i].chorus.tone, nullptr);
        stepTree.setProperty("mix", chorusStepSnapshots[i].chorus.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Reverb snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("ReverbStep" + juce::String(i));
        stepTree.setProperty("type", reverbStepSnapshots[i].reverb.type, nullptr);
        stepTree.setProperty("size", reverbStepSnapshots[i].reverb.size, nullptr);
        stepTree.setProperty("predelayMs", reverbStepSnapshots[i].reverb.predelayMs, nullptr);
        stepTree.setProperty("dampHz", reverbStepSnapshots[i].reverb.dampHz, nullptr);
        stepTree.setProperty("diffusion", reverbStepSnapshots[i].reverb.diffusion, nullptr);
        stepTree.setProperty("early", reverbStepSnapshots[i].reverb.early, nullptr);
        stepTree.setProperty("decaySec", reverbStepSnapshots[i].reverb.decaySec, nullptr);
        stepTree.setProperty("mix", reverbStepSnapshots[i].reverb.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Granular snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("GranularStep" + juce::String(i));
        stepTree.setProperty("sizeMs", granularStepSnapshots[i].granular.sizeMs, nullptr);
        stepTree.setProperty("densityHz", granularStepSnapshots[i].granular.densityHz, nullptr);
        stepTree.setProperty("position", granularStepSnapshots[i].granular.position, nullptr);
        stepTree.setProperty("sprayMs", granularStepSnapshots[i].granular.sprayMs, nullptr);
        stepTree.setProperty("pitchSemi", granularStepSnapshots[i].granular.pitchSemi, nullptr);
        stepTree.setProperty("random", granularStepSnapshots[i].granular.random, nullptr);
        stepTree.setProperty("texture", granularStepSnapshots[i].granular.texture, nullptr);
        stepTree.setProperty("mix", granularStepSnapshots[i].granular.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Slicer snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("SlicerStep" + juce::String(i));
        stepTree.setProperty("pattern", slicerStepSnapshots[i].slicer.pattern, nullptr);
        stepTree.setProperty("division", slicerStepSnapshots[i].slicer.division, nullptr);
        stepTree.setProperty("offset", slicerStepSnapshots[i].slicer.offset, nullptr);
        stepTree.setProperty("shape", slicerStepSnapshots[i].slicer.shape, nullptr);
        stepTree.setProperty("releaseMs", slicerStepSnapshots[i].slicer.releaseMs, nullptr);
        stepTree.setProperty("mix", slicerStepSnapshots[i].slicer.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Formant snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("FormantStep" + juce::String(i));
        stepTree.setProperty("vowel", formantStepSnapshots[i].formant.vowel, nullptr);
        stepTree.setProperty("resonance", formantStepSnapshots[i].formant.resonance, nullptr);
        stepTree.setProperty("intensity", formantStepSnapshots[i].formant.intensity, nullptr);
        stepTree.setProperty("mix", formantStepSnapshots[i].formant.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    // Saturate (Heat) snapshots
    for (int i = 0; i < 16; ++i)
    {
        auto stepTree = juce::ValueTree("SaturateStep" + juce::String(i));
        stepTree.setProperty("type", saturateStepSnapshots[i].saturate.type, nullptr);
        stepTree.setProperty("drive", saturateStepSnapshots[i].saturate.drive, nullptr);
        stepTree.setProperty("color", saturateStepSnapshots[i].saturate.color, nullptr);
        stepTree.setProperty("shape", saturateStepSnapshots[i].saturate.shape, nullptr);
        stepTree.setProperty("bias", saturateStepSnapshots[i].saturate.bias, nullptr);
        stepTree.setProperty("output", saturateStepSnapshots[i].saturate.output, nullptr);
        stepTree.setProperty("oversample", saturateStepSnapshots[i].saturate.oversample, nullptr);
        stepTree.setProperty("mix", saturateStepSnapshots[i].saturate.mix, nullptr);
        stepsnapshots.addChild(stepTree, -1, nullptr);
    }
    
    state.addChild(stepsnapshots, -1, nullptr);
    
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
            DBG("[State] Restoring EffectRouter from preset");
            effectRouter.fromValueTree(routerState);
            
            // Debug: show restored assignment
            auto routingOrder = effectRouter.getRoutingOrder();
            DBG("[State] Restored router: Slot1=" + juce::String(static_cast<int>(routingOrder[0])) + 
                " Slot2=" + juce::String(static_cast<int>(routingOrder[1])) + 
                " Slot3=" + juce::String(static_cast<int>(routingOrder[2])) + 
                " Slot4=" + juce::String(static_cast<int>(routingOrder[3])));
            
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
        else
        {
            DBG("[State] No EffectRouter found in preset tree!");
        }
        
        // Restore sequencer settings
        auto seqSettings = tree.getChildWithName("SequencerSettings");
        if (seqSettings.isValid())
        {
            // SpaceDelay sequencer
            auto delaySeqTree = seqSettings.getChildWithName("DelaySequencer");
            if (delaySeqTree.isValid())
            {
                seq.enabled.store(delaySeqTree.getProperty("enabled", true));
                seq.stepsUsed.store(delaySeqTree.getProperty("stepsUsed", 16));
                seq.divisionIndex.store(delaySeqTree.getProperty("divisionIndex", 3));
                seq.stdMode.store(delaySeqTree.getProperty("stdMode", 0));
            }
            
            // AutoPan sequencer
            auto autopanSeqTree = seqSettings.getChildWithName("AutoPanSequencer");
            if (autopanSeqTree.isValid())
            {
                autopanSeq.enabled.store(autopanSeqTree.getProperty("enabled", true));
                autopanSeq.stepsUsed.store(autopanSeqTree.getProperty("stepsUsed", 16));
                autopanSeq.divisionIndex.store(autopanSeqTree.getProperty("divisionIndex", 5));
                autopanSeq.stdMode.store(autopanSeqTree.getProperty("stdMode", 0));
            }
            
            // Dirt sequencer
            auto dirtSeqTree = seqSettings.getChildWithName("DirtSequencer");
            if (dirtSeqTree.isValid())
            {
                dirtSeq.enabled.store(dirtSeqTree.getProperty("enabled", true));
                dirtSeq.stepsUsed.store(dirtSeqTree.getProperty("stepsUsed", 16));
                dirtSeq.divisionIndex.store(dirtSeqTree.getProperty("divisionIndex", 5));
                dirtSeq.stdMode.store(dirtSeqTree.getProperty("stdMode", 0));
            }
            
            // Chorus sequencer
            auto chorusSeqTree = seqSettings.getChildWithName("ChorusSequencer");
            if (chorusSeqTree.isValid())
            {
                chorusSeq.enabled.store(chorusSeqTree.getProperty("enabled", true));
                chorusSeq.stepsUsed.store(chorusSeqTree.getProperty("stepsUsed", 16));
                chorusSeq.divisionIndex.store(chorusSeqTree.getProperty("divisionIndex", 5));
                chorusSeq.stdMode.store(chorusSeqTree.getProperty("stdMode", 0));
            }
            
            // Reverb sequencer
            auto reverbSeqTree = seqSettings.getChildWithName("ReverbSequencer");
            if (reverbSeqTree.isValid())
            {
                reverbSeq.enabled.store(reverbSeqTree.getProperty("enabled", true));
                reverbSeq.stepsUsed.store(reverbSeqTree.getProperty("stepsUsed", 16));
                reverbSeq.divisionIndex.store(reverbSeqTree.getProperty("divisionIndex", 5));
                reverbSeq.stdMode.store(reverbSeqTree.getProperty("stdMode", 0));
            }
            
            // Granular sequencer
            auto granularSeqTree = seqSettings.getChildWithName("GranularSequencer");
            if (granularSeqTree.isValid())
            {
                granularSeq.enabled.store(granularSeqTree.getProperty("enabled", true));
                granularSeq.stepsUsed.store(granularSeqTree.getProperty("stepsUsed", 16));
                granularSeq.divisionIndex.store(granularSeqTree.getProperty("divisionIndex", 5));
                granularSeq.stdMode.store(granularSeqTree.getProperty("stdMode", 0));
            }
            
            // Slicer sequencer
            auto slicerSeqTree = seqSettings.getChildWithName("SlicerSequencer");
            if (slicerSeqTree.isValid())
            {
                slicerSeq.enabled.store(slicerSeqTree.getProperty("enabled", true));
                slicerSeq.stepsUsed.store(slicerSeqTree.getProperty("stepsUsed", 16));
                slicerSeq.divisionIndex.store(slicerSeqTree.getProperty("divisionIndex", 3));
                slicerSeq.stdMode.store(slicerSeqTree.getProperty("stdMode", 0));
            }
            
            // Formant sequencer
            auto formantSeqTree = seqSettings.getChildWithName("FormantSequencer");
            if (formantSeqTree.isValid())
            {
                formantSeq.enabled.store(formantSeqTree.getProperty("enabled", true));
                formantSeq.stepsUsed.store(formantSeqTree.getProperty("stepsUsed", 16));
                formantSeq.divisionIndex.store(formantSeqTree.getProperty("divisionIndex", 5));
                formantSeq.stdMode.store(formantSeqTree.getProperty("stdMode", 0));
            }
            
            // Saturate (Heat) sequencer
            auto saturateSeqTree = seqSettings.getChildWithName("SaturateSequencer");
            if (saturateSeqTree.isValid())
            {
                saturateSeq.enabled.store(saturateSeqTree.getProperty("enabled", true));
                saturateSeq.stepsUsed.store(saturateSeqTree.getProperty("stepsUsed", 16));
                saturateSeq.divisionIndex.store(saturateSeqTree.getProperty("divisionIndex", 5));
                saturateSeq.stdMode.store(saturateSeqTree.getProperty("stdMode", 0));
            }
        }
        
        // Restore step snapshots
        auto stepsnapshots = tree.getChildWithName("StepSnapshots");
        if (stepsnapshots.isValid())
        {
            // SpaceDelay snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("DelayStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    stepSnapshots[i].delay.timeMs = stepTree.getProperty("timeMs", 250.0f);
                    stepSnapshots[i].delay.feedback = stepTree.getProperty("feedback", 0.2f);
                    stepSnapshots[i].delay.wowDepth = stepTree.getProperty("wowDepth", 0.0f);
                    stepSnapshots[i].delay.wowRate = stepTree.getProperty("wowRate", 1.0f);
                    stepSnapshots[i].delay.saturation = stepTree.getProperty("saturation", 0.0f);
                    stepSnapshots[i].delay.highCut = stepTree.getProperty("highCut", 20000.0f);
                    stepSnapshots[i].delay.lowCut = stepTree.getProperty("lowCut", 20.0f);
                }
            }
            
            // AutoPan snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("AutoPanStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    autopanStepSnapshots[i].autopan.rate = stepTree.getProperty("rate", 0.43f);
                    autopanStepSnapshots[i].autopan.phase = stepTree.getProperty("phase", 180.0f);
                    autopanStepSnapshots[i].autopan.waveType = stepTree.getProperty("waveType", 0);
                    autopanStepSnapshots[i].autopan.waveShape = stepTree.getProperty("waveShape", 0.5f);
                    autopanStepSnapshots[i].autopan.inverted = stepTree.getProperty("inverted", false);
                    autopanStepSnapshots[i].autopan.amount = stepTree.getProperty("amount", 1.0f);
                }
            }
            
            // Dirt snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("DirtStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    dirtStepSnapshots[i].dirt.drive = stepTree.getProperty("drive", 12.0f);
                    dirtStepSnapshots[i].dirt.color = stepTree.getProperty("color", 0.0f);
                    dirtStepSnapshots[i].dirt.asym = stepTree.getProperty("asym", 0.0f);
                    dirtStepSnapshots[i].dirt.texture = stepTree.getProperty("texture", 0.35f);
                    dirtStepSnapshots[i].dirt.lowCut = stepTree.getProperty("lowCut", 60.0f);
                    dirtStepSnapshots[i].dirt.highCut = stepTree.getProperty("highCut", 12000.0f);
                    dirtStepSnapshots[i].dirt.tone = stepTree.getProperty("tone", 0.0f);
                    dirtStepSnapshots[i].dirt.mix = stepTree.getProperty("mix", 1.0f);
                }
            }
            
            // Chorus snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("ChorusStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    chorusStepSnapshots[i].chorus.rate = stepTree.getProperty("rate", 0.8f);
                    chorusStepSnapshots[i].chorus.depth = stepTree.getProperty("depth", 40.0f);
                    chorusStepSnapshots[i].chorus.voices = stepTree.getProperty("voices", 2.0f);
                    chorusStepSnapshots[i].chorus.delayTime = stepTree.getProperty("delayTime", 20.0f);
                    chorusStepSnapshots[i].chorus.feedback = stepTree.getProperty("feedback", 20.0f);
                    chorusStepSnapshots[i].chorus.width = stepTree.getProperty("width", 100.0f);
                    chorusStepSnapshots[i].chorus.tone = stepTree.getProperty("tone", 0.0f);
                    chorusStepSnapshots[i].chorus.mix = stepTree.getProperty("mix", 50.0f);
                }
            }
            
            // Reverb snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("ReverbStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    reverbStepSnapshots[i].reverb.type = stepTree.getProperty("type", 0.0f);
                    reverbStepSnapshots[i].reverb.size = stepTree.getProperty("size", 0.7f);
                    reverbStepSnapshots[i].reverb.predelayMs = stepTree.getProperty("predelayMs", 20.0f);
                    reverbStepSnapshots[i].reverb.dampHz = stepTree.getProperty("dampHz", 8000.0f);
                    reverbStepSnapshots[i].reverb.diffusion = stepTree.getProperty("diffusion", 0.7f);
                    reverbStepSnapshots[i].reverb.early = stepTree.getProperty("early", 0.35f);
                    reverbStepSnapshots[i].reverb.decaySec = stepTree.getProperty("decaySec", 4.0f);
                    reverbStepSnapshots[i].reverb.mix = stepTree.getProperty("mix", 0.25f);
                }
            }
            
            // Granular snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("GranularStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    granularStepSnapshots[i].granular.sizeMs = stepTree.getProperty("sizeMs", 50.0f);
                    granularStepSnapshots[i].granular.densityHz = stepTree.getProperty("densityHz", 20.0f);
                    granularStepSnapshots[i].granular.position = stepTree.getProperty("position", 0.0f);
                    granularStepSnapshots[i].granular.sprayMs = stepTree.getProperty("sprayMs", 10.0f);
                    granularStepSnapshots[i].granular.pitchSemi = stepTree.getProperty("pitchSemi", 0.0f);
                    granularStepSnapshots[i].granular.random = stepTree.getProperty("random", 0.0f);
                    granularStepSnapshots[i].granular.texture = stepTree.getProperty("texture", 0.5f);
                    granularStepSnapshots[i].granular.mix = stepTree.getProperty("mix", 0.5f);
                }
            }
            
            // Slicer snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("SlicerStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    slicerStepSnapshots[i].slicer.pattern = stepTree.getProperty("pattern", 0.0f);
                    slicerStepSnapshots[i].slicer.division = stepTree.getProperty("division", 3.0f);
                    slicerStepSnapshots[i].slicer.offset = stepTree.getProperty("offset", 0.5f);
                    slicerStepSnapshots[i].slicer.shape = stepTree.getProperty("shape", 0.5f);
                    slicerStepSnapshots[i].slicer.releaseMs = stepTree.getProperty("releaseMs", 20.0f);
                    slicerStepSnapshots[i].slicer.mix = stepTree.getProperty("mix", 0.75f);
                }
            }
            
            // Formant snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("FormantStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    formantStepSnapshots[i].formant.vowel = stepTree.getProperty("vowel", 0.0f);
                    formantStepSnapshots[i].formant.resonance = stepTree.getProperty("resonance", 12.0f);
                    formantStepSnapshots[i].formant.intensity = stepTree.getProperty("intensity", 6.0f);
                    formantStepSnapshots[i].formant.mix = stepTree.getProperty("mix", 0.8f);
                }
            }
            
            // Saturate (Heat) snapshots
            for (int i = 0; i < 16; ++i)
            {
                auto stepTree = stepsnapshots.getChildWithName("SaturateStep" + juce::String(i));
                if (stepTree.isValid())
                {
                    saturateStepSnapshots[i].saturate.type = stepTree.getProperty("type", 0.0f);
                    saturateStepSnapshots[i].saturate.drive = stepTree.getProperty("drive", 12.0f);
                    saturateStepSnapshots[i].saturate.color = stepTree.getProperty("color", 0.5f);
                    saturateStepSnapshots[i].saturate.shape = stepTree.getProperty("shape", 0.4f);
                    saturateStepSnapshots[i].saturate.bias = stepTree.getProperty("bias", 0.0f);
                    saturateStepSnapshots[i].saturate.output = stepTree.getProperty("output", 0.0f);
                    saturateStepSnapshots[i].saturate.oversample = stepTree.getProperty("oversample", 3.0f);
                    saturateStepSnapshots[i].saturate.mix = stepTree.getProperty("mix", 1.0f);
                }
            }
        }
        
        DBG("[Preset] Complete plugin state restored including all sequencer patterns");
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

// Redux snapshot methods
StepSnapshot PluginProcessor::getReduxSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return reduxStepSnapshots[step];
    }
    return reduxStepSnapshots[0];
}

void PluginProcessor::setReduxStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        reduxStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateReduxCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = reduxUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific Redux parameter in the snapshot
    // Note: knob order matches UI: Bit Depth, Rate, Jitter, Pre Filter, Post Filter, Drive, Emphasis, Mix
    switch (knobIndex) {
        case 0: // Bit Depth (UI 1-12 -> internal 4-16)
            reduxStepSnapshots[currentStep].redux.bitDepth = (int)value + 3;
            break;
        case 1: // Sample Rate Reduction (Rate)
            reduxStepSnapshots[currentStep].redux.sampleRateReduction = (int)value;
            break;
        case 2: // Jitter
            reduxStepSnapshots[currentStep].redux.jitter = value;
            break;
        case 3: // Pre Filter
            reduxStepSnapshots[currentStep].redux.preFilter = value;
            break;
        case 4: // Post Filter
            reduxStepSnapshots[currentStep].redux.postFilter = value;
            break;
        case 5: // Drive
            reduxStepSnapshots[currentStep].redux.drive = value;
            break;
        case 6: // Emphasis
            reduxStepSnapshots[currentStep].redux.emphasis = value;
            break;
        case 7: // Mix
            reduxStepSnapshots[currentStep].redux.mix = value;
            break;
    }
}

// PhaseBloom snapshot methods
StepSnapshot PluginProcessor::getPhaseBloomSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return phaseBloomStepSnapshots[step];
    }
    return phaseBloomStepSnapshots[0];
}

void PluginProcessor::setPhaseBloomStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        phaseBloomStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updatePhaseBloomCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = phaseBloomUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific PhaseBloom parameter in the snapshot
    // Note: knob order matches UI: Depth, Rate, Feedback, Center, Bloom, Spread, Resonance, Mix
    switch (knobIndex) {
        case 0: // Depth
            phaseBloomStepSnapshots[currentStep].phasebloom.depth = value;
            break;
        case 1: // Rate
            phaseBloomStepSnapshots[currentStep].phasebloom.rate = value;
            break;
        case 2: // Feedback
            phaseBloomStepSnapshots[currentStep].phasebloom.feedback = value;
            break;
        case 3: // Center
            phaseBloomStepSnapshots[currentStep].phasebloom.center = value;
            break;
        case 4: // Bloom
            phaseBloomStepSnapshots[currentStep].phasebloom.bloom = value;
            break;
        case 5: // Spread
            phaseBloomStepSnapshots[currentStep].phasebloom.spread = value;
            break;
        case 6: // Resonance
            phaseBloomStepSnapshots[currentStep].phasebloom.resonance = value;
            break;
        case 7: // Mix
            phaseBloomStepSnapshots[currentStep].phasebloom.mix = value;
            break;
    }
}

void PluginProcessor::setPhaseBloomStepsUsed(int stepsUsed)
{
    phaseBloomSeq.stepsUsed.store(juce::jlimit(1, 16, stepsUsed));
}

void PluginProcessor::setPhaseBloomDivisionIndex(int divisionIndex)
{
    phaseBloomSeq.divisionIndex.store(juce::jlimit(0, 8, divisionIndex));
}

// Formant snapshot methods
StepSnapshot PluginProcessor::getFormantSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return formantStepSnapshots[step];
    }
    return formantStepSnapshots[0];
}

void PluginProcessor::setFormantStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        formantStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateFormantCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = formantUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Update the specific Formant parameter in the snapshot
    // Note: knob order matches UI: Vowel, Resonance, Intensity, Mix
    switch (knobIndex) {
        case 0: // Vowel
            formantStepSnapshots[currentStep].formant.vowel = value;
            break;
        case 1: // Resonance
            formantStepSnapshots[currentStep].formant.resonance = value;
            break;
        case 2: // Intensity
            formantStepSnapshots[currentStep].formant.intensity = value;
            break;
        case 3: // Mix
            formantStepSnapshots[currentStep].formant.mix = value;
            break;
    }
}

void PluginProcessor::setFormantStepsUsed(int stepsUsed)
{
    formantSeq.stepsUsed.store(juce::jlimit(1, 16, stepsUsed));
}

void PluginProcessor::setFormantDivisionIndex(int divisionIndex)
{
    formantSeq.divisionIndex.store(juce::jlimit(0, 8, divisionIndex));
}

void PluginProcessor::setFormantStdMode(int stdMode)
{
    // Store STD mode for Formant sequencer timing calculations
    formantSeq.stdMode = stdMode;
    DBG("[PROCESSOR] Formant STD mode set to: " << stdMode);
}

// Form 2 snapshot methods
StepSnapshot PluginProcessor::getForm2SafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return form2StepSnapshots[step];
    }
    return form2StepSnapshots[0];
}

void PluginProcessor::setForm2StepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        form2StepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateForm2CurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = form2UiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Convert 0.0-1.0 value from knob to actual parameter range
    // Update the specific Form 2 parameter in the snapshot
    switch (knobIndex) {
        case 0: // Root Note: 0.0-1.0 → 0-11
            form2StepSnapshots[currentStep].form2.rootNote = static_cast<int>(value * 12);
            break;
        case 1: // Scale: 0.0-1.0 → 0-6
            form2StepSnapshots[currentStep].form2.scale = static_cast<int>(value * 7);
            break;
        case 2: // Chord Size: 0.0-1.0 → 1-8
            form2StepSnapshots[currentStep].form2.chordSize = 1 + static_cast<int>(value * 7);
            break;
        case 3: // Shift: 0.5-2.0
            form2StepSnapshots[currentStep].form2.shift = value;
            break;
        case 4: // Color: -12 to +12 dB
            form2StepSnapshots[currentStep].form2.color = value;
            break;
        case 5: // Motion: 0-1
            form2StepSnapshots[currentStep].form2.motion = value;
            break;
        case 6: // Resynth: 0-1
            form2StepSnapshots[currentStep].form2.resynth = value;
            break;
        case 7: // Mix: 0-1
            form2StepSnapshots[currentStep].form2.mix = value;
            break;
    }
}

void PluginProcessor::setForm2StepsUsed(int stepsUsed)
{
    form2Seq.stepsUsed.store(juce::jlimit(1, 16, stepsUsed));
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

// Slicer snapshot accessors
StepSnapshot PluginProcessor::getSlicerSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return slicerStepSnapshots[step];
    }
    return slicerStepSnapshots[0];
}

void PluginProcessor::setSlicerStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        slicerStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateSlicerCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = slicerUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Mix (knob 5) is global, not saved to snapshots
    if (knobIndex == 5) return;
    
    // Update the specific Slicer parameter in the snapshot
    switch (knobIndex) {
        case 0: // Pattern
            slicerStepSnapshots[currentStep].slicer.pattern = value;
            break;
        case 1: // Division
            slicerStepSnapshots[currentStep].slicer.division = value;
            break;
        case 2: // Offset
            slicerStepSnapshots[currentStep].slicer.offset = value;
            break;
        case 3: // Shape
            slicerStepSnapshots[currentStep].slicer.shape = value;
            break;
        case 4: // Release (ms)
            slicerStepSnapshots[currentStep].slicer.releaseMs = value;
            break;
    }
}

// Dub Delay snapshot accessors
StepSnapshot PluginProcessor::getDubDelaySafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return dubdelayStepSnapshots[step];
    }
    return dubdelayStepSnapshots[0];
}

void PluginProcessor::setDubDelayStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        dubdelayStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateDubDelayCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = dubdelayUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Mix (knob 7) is global, not saved to snapshots
    if (knobIndex == 7) return;
    
    // Update the specific Dub Delay parameter in the snapshot
    switch (knobIndex) {
        case 0: // Time (ms)
            dubdelayStepSnapshots[currentStep].dubdelay.timeMs = value;
            break;
        case 1: // Feedback
            dubdelayStepSnapshots[currentStep].dubdelay.feedback = value;
            break;
        case 2: // Tone (Hz)
            dubdelayStepSnapshots[currentStep].dubdelay.toneHz = value;
            break;
        case 3: // Drive
            dubdelayStepSnapshots[currentStep].dubdelay.drive = value;
            break;
        case 4: // PingPong (bool stored as float 0 or 1)
            dubdelayStepSnapshots[currentStep].dubdelay.pingPong = (value > 0.5f);
            break;
        case 5: // WowFlutter
            dubdelayStepSnapshots[currentStep].dubdelay.wowFlutter = value;
            break;
        case 6: // RegenDamp
            dubdelayStepSnapshots[currentStep].dubdelay.regenDamp = value;
            break;
    }
}

// Saturate snapshot accessors
StepSnapshot PluginProcessor::getSaturateSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return saturateStepSnapshots[step];
    }
    return saturateStepSnapshots[0];
}

void PluginProcessor::setSaturateStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        saturateStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateSaturateCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = saturateUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Mix (knob 6) is global, not saved to snapshots
    if (knobIndex == 6) return;
    
    // Update the specific Saturate parameter in the snapshot
    switch (knobIndex) {
        case 0: // Type - round to nearest integer to prevent snapping
            saturateStepSnapshots[currentStep].saturate.type = static_cast<float>(juce::jlimit(0, 7, static_cast<int>(std::round(value))));
            break;
        case 1: // Drive
            saturateStepSnapshots[currentStep].saturate.drive = value;
            break;
        case 2: // Color
            saturateStepSnapshots[currentStep].saturate.color = value;
            break;
        case 3: // Shape
            saturateStepSnapshots[currentStep].saturate.shape = value;
            break;
        case 4: // Bias
            saturateStepSnapshots[currentStep].saturate.bias = value;
            break;
        case 5: // Output
            saturateStepSnapshots[currentStep].saturate.output = value;
            break;
        // Oversample is always max (3 = 8×), handled separately
        // Mix (case 6) is global, not saved per step
    }
}

// Filter snapshot accessors
StepSnapshot PluginProcessor::getFilterSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return filterStepSnapshots[step];
    }
    return filterStepSnapshots[0];
}

void PluginProcessor::setFilterStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        filterStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateFilterCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = filterUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Mix knob (filterKnobs[5]) is global, not saved to snapshots
    if (knobIndex == 5) return;
    
    // Update the specific Filter parameter in the snapshot
    // knobIndex: -1 = Type, -2 = Slope, 0-4 = filterKnobs[0-4] (Cutoff, Res, Drive, Spread, Key Track)
    if (knobIndex == -1) {
        // Type - round to nearest integer to prevent snapping
        filterStepSnapshots[currentStep].filter.type = static_cast<float>(juce::jlimit(0, 4, static_cast<int>(std::round(value))));
    } else if (knobIndex == -2) {
        // Slope
        filterStepSnapshots[currentStep].filter.slope = value;
    } else {
        switch (knobIndex) {
            case 0: // Cutoff - convert normalized value (0-1) to frequency for snapshot
                filterStepSnapshots[currentStep].filter.cutoff = convertNormalizedCutoffToFrequency(value);
                break;
            case 1: // Resonance
                filterStepSnapshots[currentStep].filter.resonance = value;
                break;
            case 2: // Drive
                filterStepSnapshots[currentStep].filter.drive = value;
                break;
            case 3: // Spread
                filterStepSnapshots[currentStep].filter.spread = value;
                break;
            case 4: // Key Track
                filterStepSnapshots[currentStep].filter.keytrack = value;
                break;
            // Mix (case 5) is global, not saved per step
        }
    }
}

// Shimmer snapshot accessors - TODO: Shimmer not yet implemented
/*
StepSnapshot PluginProcessor::getShimmerSafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return shimmerStepSnapshots[step];
    }
    return shimmerStepSnapshots[0];
}

void PluginProcessor::setShimmerStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        shimmerStepSnapshots[step] = snapshot;
    }
}
*/

// TODO: Shimmer not yet implemented
/*
void PluginProcessor::updateShimmerCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = shimmerUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Mix (knob 7) is global, not saved to snapshots
    if (knobIndex == 7) return;
    
    switch (knobIndex) {
        case 0: shimmerStepSnapshots[currentStep].shimmer.mode = (int)value; break;
        case 1: shimmerStepSnapshots[currentStep].shimmer.size = value; break;
        case 2: shimmerStepSnapshots[currentStep].shimmer.decay = value; break;
        case 3: shimmerStepSnapshots[currentStep].shimmer.color = value; break;
        case 4: shimmerStepSnapshots[currentStep].shimmer.predelay = value; break;
        case 5: shimmerStepSnapshots[currentStep].shimmer.shimAmt = value; break;
        case 6: shimmerStepSnapshots[currentStep].shimmer.osMode = value; break;
        // Mix (case 7) is global, not saved per step
    }
}
*/

// Space Delay snapshot accessors
StepSnapshot PluginProcessor::getSpaceDelaySafeSnapshot(int step) const
{
    if (step >= 0 && step < 16) {
        return spacedelayStepSnapshots[step];
    }
    return spacedelayStepSnapshots[0];
}

void PluginProcessor::setSpaceDelayStepSnapshot(int step, const StepSnapshot& snapshot) noexcept
{
    if (step >= 0 && step < 16) {
        spacedelayStepSnapshots[step] = snapshot;
    }
}

void PluginProcessor::updateSpaceDelayCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = spacedelayUiSelectedStep.load();
    if (currentStep < 0 || currentStep >= 16) return;
    
    switch (knobIndex) {
        case 0: spacedelayStepSnapshots[currentStep].delay.timeMs = value; break;
        case 1: spacedelayStepSnapshots[currentStep].delay.feedback = value; break;
        case 2: spacedelayStepSnapshots[currentStep].delay.wowDepth = value; break;
        case 3: spacedelayStepSnapshots[currentStep].delay.wowRate = value; break;
        case 4: spacedelayStepSnapshots[currentStep].delay.saturation = value; break;
        case 5: spacedelayStepSnapshots[currentStep].delay.highCut = value; break;
        case 6: spacedelayStepSnapshots[currentStep].delay.lowCut = value; break;
        case 7: spacedelayStepSnapshots[currentStep].delay.mix = value; break;
        default: break;
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
        // Check if effect is enabled
        auto* delayEnabledParam = valueTreeState.getRawParameterValue("delayEnabled");
        bool isDelayEnabled = delayEnabledParam ? (delayEnabledParam->load() > 0.5f) : false;
        
        if (!isDelayEnabled) return;
        
        // Check if sequencer is enabled and active
        bool seqEnabled = spacedelaySeq.enabled.load();
        bool seqActive = spacedelaySeq.active.load();
        int playingStep = spacedelaySeq.playingStep.load();
        
        // Get parameters from sequencer snapshot OR APVTS
        float timeMs, feedback, wowDepth, wowRate, drive, hiCut, lowCut, mix;
        
        if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16) {
            // Read from step snapshot
            StepSnapshot snapshot = spacedelayStepSnapshots[playingStep];
            timeMs = snapshot.delay.timeMs;
            feedback = snapshot.delay.feedback;
            wowDepth = snapshot.delay.wowDepth;
            wowRate = snapshot.delay.wowRate;
            drive = snapshot.delay.saturation; // Note: saturation field is used for drive
            hiCut = snapshot.delay.highCut;
            lowCut = snapshot.delay.lowCut;
            mix = snapshot.delay.mix;
            
            DBG("[SPACE DELAY SEQ] Using sequencer step " << playingStep << " - timeMs: " << timeMs << ", feedback: " << feedback);
        } else {
            // Read from APVTS parameters (manual control)
            auto* timeMsParam = valueTreeState.getRawParameterValue("timeMs");
            auto* feedbackParam = valueTreeState.getRawParameterValue("feedback");
            auto* wowDepthParam = valueTreeState.getRawParameterValue("wowDepth");
            auto* wowRateParam = valueTreeState.getRawParameterValue("wowRate");
            auto* driveParam = valueTreeState.getRawParameterValue("drive");
            auto* hiCutParam = valueTreeState.getRawParameterValue("hiCut");
            auto* lowCutParam = valueTreeState.getRawParameterValue("lowCut");
            auto* mixParam = valueTreeState.getRawParameterValue("mix");
            
            timeMs = timeMsParam ? timeMsParam->load() : 250.0f;
            feedback = feedbackParam ? feedbackParam->load() : 0.2f;
            wowDepth = wowDepthParam ? wowDepthParam->load() : 0.0f;
            wowRate = wowRateParam ? wowRateParam->load() : 1.0f;
            drive = driveParam ? driveParam->load() : 0.0f;
            hiCut = hiCutParam ? hiCutParam->load() : 20000.0f;
            lowCut = lowCutParam ? lowCutParam->load() : 20.0f;
            mix = mixParam ? mixParam->load() : 0.5f;
            
            DBG("[SPACE DELAY MANUAL] Using APVTS parameters - timeMs: " << timeMs << ", feedback: " << feedback);
        }
        
        // Check if sync is enabled and compute final delay time
        auto* syncParam = valueTreeState.getRawParameterValue("delaySync");
        bool syncEnabled = syncParam ? (syncParam->load() > 0.5f) : false;
        
        float finalTimeMs = timeMs;
        
        if (syncEnabled) {
            // Tempo-synced mode: compute delay time from BPM + division
            double bpmSafe = transportCache.bpm.load();
            if (bpmSafe < 20.0 || bpmSafe > 300.0) {
                bpmSafe = 120.0; // Fallback
            }
            
            // Get division index from parameter
            auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("delayTimeDiv"));
            int divIdx = divParam ? divParam->getIndex() : 5; // Default 1/4 (index 5)
            
            // Define delay divisions: 2, 1, 1/2, 1/2D, 1/2T, 1/4, 1/4D, 1/4T, 1/8, 1/8D, 1/8T, 1/16, 1/16D, 1/16T, 1/32, 1/32D, 1/32T, 1/64, 1/64D, 1/64T
            static const float delayMultipliers[] = {
                8.0f,    // 2 (2 whole notes = 8 quarter notes)
                4.0f,    // 1 (1 whole note = 4 quarter notes)
                2.0f,    // 1/2 (half note = 2 quarter notes)
                3.0f,    // 1/2D (dotted half = 3 quarter notes)
                1.333f,  // 1/2T (triplet half = 4/3 quarter notes)
                1.0f,    // 1/4 (quarter note = 1 quarter note)
                1.5f,    // 1/4D (dotted quarter = 1.5 quarter notes)
                0.667f,  // 1/4T (triplet quarter = 2/3 quarter notes)
                0.5f,    // 1/8 (eighth note = 0.5 quarter notes)
                0.75f,   // 1/8D (dotted eighth = 0.75 quarter notes)
                0.333f,  // 1/8T (triplet eighth = 1/3 quarter notes)
                0.25f,   // 1/16 (sixteenth note = 0.25 quarter notes)
                0.375f,  // 1/16D (dotted sixteenth = 0.375 quarter notes)
                0.167f,  // 1/16T (triplet sixteenth = 1/6 quarter notes)
                0.125f,  // 1/32 (thirty-second note = 0.125 quarter notes)
                0.188f,  // 1/32D (dotted thirty-second = 0.188 quarter notes)
                0.083f,  // 1/32T (triplet thirty-second = 1/12 quarter notes)
                0.0625f, // 1/64 (sixty-fourth note = 0.0625 quarter notes)
                0.094f,  // 1/64D (dotted sixty-fourth = 0.094 quarter notes)
                0.042f   // 1/64T (triplet sixty-fourth = 1/24 quarter notes)
            };
            
            divIdx = juce::jlimit(0, (int)std::size(delayMultipliers) - 1, divIdx);
            float multiplier = delayMultipliers[divIdx];
            
            // Compute delay time: 60,000 ms per minute / BPM * multiplier
            double quarterNoteMs = 60000.0 / bpmSafe;
            finalTimeMs = static_cast<float>(multiplier * quarterNoteMs);
            
            DBG("[SPACE DELAY SYNC] BPM: " << bpmSafe << ", DivIdx: " << divIdx << ", Multiplier: " << multiplier << ", FinalTimeMs: " << finalTimeMs);
        }
        
        // Set Space Delay parameters
        FxDelay::Targets t;
        t.timeMs = finalTimeMs;
        t.feedback = juce::jlimit(0.0f, 0.85f, feedback);
        t.wowDepth = wowDepth;
        t.wowRate = wowRate;
        t.drive = drive;
        t.hiCutHz = hiCut;
        t.lowCutHz = lowCut;
        t.mix = mix;
        
        spaceDelay.setTargets(t);
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

void PluginProcessor::processCompressEffect(juce::AudioBuffer<float>& buffer)
{
    // Always process compressor - ignore APVTS enabled parameter for now
    // This ensures compressor works immediately in AU without parameter dependency
    bool isCompressEnabled = true; // Force enabled
    
    // Debug logging for AU troubleshooting
    static int debugCounter = 0;
    if (debugCounter++ % 1000 == 0) { // Print every 1000 calls to avoid spam
        DBG("[CompressEngine] FORCED ENABLED - processing compressor");
    }
    
    DBG("[CompressEngine] Processing COMPRESS+ effect - enabled: " << (isCompressEnabled ? "YES" : "NO"));
    
    // Read all COMPRESS+ parameters with fallback defaults for AU compatibility
    auto* thresholdParam = valueTreeState.getRawParameterValue("compressThreshold");
    auto* attackParam = valueTreeState.getRawParameterValue("compressAttack");
    auto* releaseParam = valueTreeState.getRawParameterValue("compressRelease");
    auto* ratioParam = valueTreeState.getRawParameterValue("compressRatio");
    auto* driveParam = valueTreeState.getRawParameterValue("compressDrive");
    auto* lofiParam = valueTreeState.getRawParameterValue("compressLofi");
    auto* makeupGainParam = valueTreeState.getRawParameterValue("compressMakeupGain");
    auto* wetParam = valueTreeState.getRawParameterValue("compressWet");

    // Use parameter values if available, otherwise use defaults (for AU compatibility)
    float threshold = thresholdParam ? thresholdParam->load() : -20.0f;
    float attack = attackParam ? attackParam->load() : 5.0f;
    float release = releaseParam ? releaseParam->load() : 50.0f;
    float ratio = ratioParam ? ratioParam->load() : 4.0f;
    float drive = driveParam ? driveParam->load() : 0.0f;
    float lofi = lofiParam ? lofiParam->load() : 0.0f;
    float makeupGain = makeupGainParam ? makeupGainParam->load() : 0.0f;
    float wet = wetParam ? wetParam->load() : 1.0f;

    // Check if compressor is enabled
    auto* compressEnabledParam = valueTreeState.getParameter("compressEnabled");
    bool isCompressorEnabled = compressEnabledParam ? (compressEnabledParam->getValue() > 0.5f) : true;
    
    if (isCompressorEnabled) {
        // Set COMPRESS+ parameters
        compressEngine.setThreshold(threshold);
        compressEngine.setAttack(attack);
        compressEngine.setRelease(release);
        compressEngine.setRatio(ratio);
        compressEngine.setDrive(drive);
        compressEngine.setLofi(lofi);
        compressEngine.setMakeupGain(makeupGain);
        compressEngine.setWet(wet);
        compressEngine.setEnabled(true);
        
        // Process COMPRESS+ effect
        compressEngine.process(buffer);
    } else {
        // Compressor is disabled - bypass it
        compressEngine.setEnabled(false);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}