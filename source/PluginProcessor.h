#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "dsp/FxDelay.h"
#include "dsp/AutoPan.h"
#include "dsp/FxDirt.h"
#include "dsp/FxChorus.h"
#include "dsp/SimpleHallReverb.h"
#include "dsp/granular/GranularEngine.h"
#include "dsp/rhythm_gate/RhythmGateEngine.h"
#include "dsp/DubDelayProcessor.h"
#include "dsp/CompressEngine.h"
#include "dsp/PhaseBloomEngine.h"
#include "dsp/FormantProcessor.h"
#include "dsp/SaturateProcessor.h"
#include "dsp/Form2Processor.h"
#include "dsp/FxFilter.h"
#include "Effects/Redux/ReduxBank.h"
#include "dsp/DspFlags.h"
#include "StepSnapshot.h"
#include "MeterTheme.h"
#include "ui/AudioRingBuffer.h"
#include "EffectRouter.h"
#include "dsp/SpectrumAnalyzer.h"

// Real-time safe level tracking for meters
struct MeterState {
    std::atomic<float> rmsDbL { -100.0f }, rmsDbR { -100.0f };
    std::atomic<float> peakDbL{ -100.0f }, peakDbR{ -100.0f };
    std::atomic<bool>  clippedL{ false }, clippedR{ false };
};

// Transport cache for reliable DAW sync
struct TransportCache
{
    std::atomic<bool> valid { false };
    std::atomic<bool> playing { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> ppq { 0.0 };              // absolute ppq
    std::atomic<double> barStartPpq { 0.0 };      // ppq of last bar start (0 if unknown)
    std::atomic<int> tsNum { 4 };
    std::atomic<int> tsDen { 4 };
};

// Sequencer state for DSP control
struct SeqState {
    std::atomic<bool> enabled { true };     // UI toggle "Sequencer ON"
    std::atomic<bool> active { true };      // internal sequencer state (independent of UI)
    std::atomic<int>  stepsUsed { 16 };     // 1..16 from the Steps chip
    std::atomic<int>  divisionIndex { 5 };  // 0:4,1:2,2:1,3:1/2,4:1/4,5:1/8,6:1/16,7:1/32
    std::atomic<int>  playingStep { -1 };   // computed in processBlock
    std::atomic<int>  currentStep { 0 };    // current step index (0-based)
    std::atomic<int>  stdMode { 0 };        // 0: straight, 1: triplet, 2: dotted
    std::atomic<double> originPPQ { 0.0 };  // origin for free-run stepping
    std::atomic<bool>   haveOrigin { false };
    
    // Sequencer timing state
    double samplesIntoStep = 0.0;
    double samplesPerStep = 0.0;
    double sr = 44100.0;
    
    // Stateless PPQ→step mapping
    // Return beats-per-step from the rate label string
    static inline double beatsPerStepFromLabel(const juce::String& label) {
        if (label == "1/32") return 0.125; // 8 steps per beat (4x faster than current)
        if (label == "1/16") return 0.25;  // 4 steps per beat (2x faster than current)
        if (label == "1/8") return 0.5;    // 2 steps per beat
        if (label == "1/4") return 1.0;    // 1 step per beat
        if (label == "1/2") return 2.0;    // 1 step per 2 beats
        if (label == "1")   return 4.0;    // 1 step per 4 beats
        if (label == "2")   return 8.0;    // 1 step per 8 beats
        if (label == "4")   return 16.0;   // 1 step per 16 beats
        return 1.0; // sane default = quarter-note
    }
    
    static inline double beatsPerStepFromDivision(int divIdx) {
        // Map your UI indices to musical divisions (matches dropdown order)
        // 4, 2, 1, 1/2, 1/4, 1/8, 1/16, 1/32
        static const juce::String labels[] = { "4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        const int i = juce::jlimit(0, (int)std::size(labels)-1, divIdx);
        const double result = beatsPerStepFromLabel(labels[i]);
        DBG("[SEQ] Division " << divIdx << " -> \"" << labels[i] << "\" -> " << result << " beats per step");
        return result;
    }
    
    int computeStepFromPPQ(double ppq) const noexcept {
        const int N = juce::jmax(1, stepsUsed.load());
        const double bps = beatsPerStepFromDivision(divisionIndex.load());
        if (bps <= 0.0 || N <= 0 || !std::isfinite(ppq))
            return currentStep.load(); // fallback: keep current

        // Which step index are we on in the bar-agnostic sense:
        // step = floor(ppq / beatsPerStep) % N
        const double stepsExact = ppq / bps;
        const int k = (int) std::floor(stepsExact);
        return ((k % N) + N) % N; // Manual modulo for negative numbers
    }

    // Sequencer API methods
    void resetPhase() noexcept { 
        currentStep.store(0); 
        playingStep.store(0);  // Also reset playing step for UI consistency
    }
    void setActive(bool on) { active.store(on); } // keep existing, but don't tie to UI only
    void prepare(double sampleRate) { sr = sampleRate; }
};

// StepSnapshot is now defined in PluginEditor.h to avoid circular dependencies

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // Public accessors for UI
    juce::AudioProcessorValueTreeState& getAPVTS() { return valueTreeState; }
    bool isTransportPlaying() const noexcept { return wasPlaying.load(); }
    void getTransportSnapshot(TransportCache& dest) const noexcept;
    const SeqState& getSeqState() const { return seq; }
    
    // Effect router access
    EffectRouter& getEffectRouter() { return effectRouter; }
    
    // Sequencer state access for editor (Delay)
    int getPlayingStep() const noexcept { return seq.playingStep.load(); }
    int getCurrentSeqStepAudioThread() const noexcept { return seq.currentStep.load(); } // Read from audio thread
    
    // AutoPan sequencer state access for editor
    const SeqState& getAutoPanSeqState() const { return autopanSeq; }
    int getAutoPanPlayingStep() const noexcept { return autopanSeq.playingStep.load(); }
    int getAutoPanCurrentStep() const noexcept { return autopanSeq.currentStep.load(); }
    void setAutoPanSelectedStep(int step) noexcept { autopanUiSelectedStep.store(step); }
    void setAutoPanSequencerEnabled(bool enabled) noexcept {
        autopanSeq.enabled.store(enabled);
        if (enabled) {
            autopanSeq.active.store(true);
        }
    }
    void setAutoPanStepsUsed(int steps) noexcept { autopanSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setAutoPanDivisionIndex(int idx) noexcept { autopanSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    // Dirt sequencer accessors (independent sequencer #3)
    const SeqState& getDirtSeqState() const { return dirtSeq; }
    int getDirtPlayingStep() const noexcept { return dirtSeq.playingStep.load(); }
    int getDirtCurrentStep() const noexcept { return dirtSeq.currentStep.load(); }
    void setDirtSelectedStep(int step) noexcept { dirtUiSelectedStep.store(step); }
    void setReduxSelectedStep(int step) noexcept { reduxUiSelectedStep.store(step); }
    void setReduxSequencerEnabled(bool enabled) noexcept {
        reduxSeq.enabled.store(enabled);
        if (enabled) {
            reduxSeq.active.store(true);
        }
    }
    void setReduxStepsUsed(int steps) noexcept { reduxSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setReduxDivisionIndex(int idx) noexcept { reduxSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    const SeqState& getReduxSeqState() const { return reduxSeq; }
    int getReduxPlayingStep() const noexcept { return reduxSeq.playingStep.load(); }
    int getReduxCurrentStep() const noexcept { return reduxSeq.currentStep.load(); }
    
    // PhaseBloom sequencer accessors
    void setPhaseBloomSelectedStep(int step) noexcept { phaseBloomUiSelectedStep.store(step); }
    void setPhaseBloomSequencerEnabled(bool enabled) noexcept {
        phaseBloomSeq.enabled.store(enabled);
        if (enabled) {
            phaseBloomSeq.active.store(true);
        }
    }
    const SeqState& getPhaseBloomSeqState() const { return phaseBloomSeq; }
    int getPhaseBloomPlayingStep() const noexcept { return phaseBloomSeq.playingStep.load(); }
    int getPhaseBloomCurrentStep() const noexcept { return phaseBloomSeq.currentStep.load(); }
    
    // Formant sequencer accessors
    void setFormantSelectedStep(int step) noexcept { formantUiSelectedStep.store(step); }
    int getFormantUiSelectedStep() const noexcept { return formantUiSelectedStep.load(); }
    void setFormantSequencerEnabled(bool enabled) noexcept {
        formantSeq.enabled.store(enabled);
        if (enabled) {
            formantSeq.active.store(true);
        }
    }
    SeqState& getFormantSeqState() { return formantSeq; }
    const SeqState& getFormantSeqState() const { return formantSeq; }
    int getFormantPlayingStep() const noexcept { return formantSeq.playingStep.load(); }
    int getFormantCurrentStep() const noexcept { return formantSeq.currentStep.load(); }
    
    void setDirtSequencerEnabled(bool enabled) noexcept {
        dirtSeq.enabled.store(enabled);
        if (enabled) {
            dirtSeq.active.store(true);
        }
    }
    void setDirtStepsUsed(int steps) noexcept { dirtSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setDirtDivisionIndex(int idx) noexcept { dirtSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    // Chorus sequencer accessors (independent from Delay, AutoPan, and Dirt)
    const SeqState& getChorusSeqState() const { return chorusSeq; }
    int getChorusPlayingStep() const noexcept { return chorusSeq.playingStep.load(); }
    int getChorusCurrentStep() const noexcept { return chorusSeq.currentStep.load(); }
    void setChorusSelectedStep(int step) noexcept { chorusUiSelectedStep.store(step); }
    void setChorusSequencerEnabled(bool enabled) noexcept {
        chorusSeq.enabled.store(enabled);
        if (enabled) {
            chorusSeq.active.store(true);
        }
    }
    void setChorusStepsUsed(int steps) noexcept { chorusSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setChorusDivisionIndex(int idx) noexcept { chorusSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    bool getSeqActive() const noexcept { return seq.active.load(); }
    int getSelectedStep() const noexcept { return uiSelectedStep.load(); }
    bool isSequencerEnabled() const noexcept { return seq.enabled.load(); }
    double getBpmOrDefault(double fallback = 120.0) const noexcept { auto b = transportCache.bpm.load(); return b > 0.0 ? b : fallback; }
    
    // Step snapshot access (Delay)
    StepSnapshot getSafeSnapshot(int step) const;
    void setStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void setSelectedStep(int step) noexcept { uiSelectedStep.store(step); }
    
    // AutoPan snapshot access
    StepSnapshot getAutoPanSafeSnapshot(int step) const;
    void setAutoPanStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateAutoPanCurrentStepSnapshot(int knobIndex, float value);
    
    // Step snapshot access (Dirt)
    StepSnapshot getDirtSafeSnapshot(int step) const;
    void setDirtStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateDirtCurrentStepSnapshot(int knobIndex, float value);
    
    // Step snapshot access (Redux)
    StepSnapshot getReduxSafeSnapshot(int step) const;
    void setReduxStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateReduxCurrentStepSnapshot(int knobIndex, float value);
    
    // Step snapshot access (PhaseBloom)
    StepSnapshot getPhaseBloomSafeSnapshot(int step) const;
    void setPhaseBloomStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updatePhaseBloomCurrentStepSnapshot(int knobIndex, float value);
    void setPhaseBloomStepsUsed(int stepsUsed);
    void setPhaseBloomDivisionIndex(int divisionIndex);
    
    // Step snapshot access (Formant)
    StepSnapshot getFormantSafeSnapshot(int step) const;
    void setFormantStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateFormantCurrentStepSnapshot(int knobIndex, float value);
    void setFormantStepsUsed(int stepsUsed);
    
    // Step snapshot access (Form 2)
    StepSnapshot getForm2SafeSnapshot(int step) const;
    void setForm2StepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateForm2CurrentStepSnapshot(int knobIndex, float value);
    void setForm2StepsUsed(int stepsUsed);
    void setForm2DivisionIndex(int divisionIndex) noexcept {
        form2Seq.divisionIndex.store(juce::jlimit(0, 8, divisionIndex));
    }
    void setForm2SequencerEnabled(bool enabled) noexcept {
        form2Seq.enabled.store(enabled);
        if (enabled) {
            form2Seq.active.store(true);
        }
    }
    const SeqState& getForm2SeqState() const { return form2Seq; }
    int getForm2PlayingStep() const noexcept { return form2Seq.playingStep.load(); }
    int getForm2CurrentStep() const noexcept { return form2Seq.currentStep.load(); }
    void setFormantDivisionIndex(int divisionIndex);
    void setFormantStdMode(int stdMode);
    
    // Chorus snapshot access
    StepSnapshot getChorusSafeSnapshot(int step) const;
    void setChorusStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateChorusCurrentStepSnapshot(int knobIndex, float value);
    
    // Reverb sequencer accessors
    const SeqState& getReverbSeqState() const { return reverbSeq; }
    int getReverbPlayingStep() const noexcept { return reverbSeq.playingStep.load(); }
    int getReverbCurrentStep() const noexcept { return reverbSeq.currentStep.load(); }
    void setReverbSelectedStep(int step) noexcept { reverbUiSelectedStep.store(step); }
    void setReverbSequencerEnabled(bool enabled) noexcept {
        reverbSeq.enabled.store(enabled);
        reverbSeq.active.store(enabled); // Active follows enabled state
    }
    void setReverbStepsUsed(int steps) noexcept { reverbSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setReverbDivisionIndex(int index) noexcept { reverbSeq.divisionIndex.store(juce::jlimit(0, 7, index)); }
    void setReverbStdMode(int mode) noexcept { reverbSeq.stdMode.store(juce::jlimit(0, 2, mode)); }
    StepSnapshot getReverbSafeSnapshot(int step) const;
    void setReverbStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateReverbCurrentStepSnapshot(int knobIndex, float value);
    
    // Granular sequencer accessors
    const SeqState& getGranularSeqState() const { return granularSeq; }
    int getGranularPlayingStep() const noexcept { return granularSeq.playingStep.load(); }
    int getGranularCurrentStep() const noexcept { return granularSeq.currentStep.load(); }
    void setGranularSelectedStep(int step) noexcept { granularUiSelectedStep.store(step); }
    void setGranularSequencerEnabled(bool enabled) noexcept {
        granularSeq.enabled.store(enabled);
        granularSeq.active.store(enabled);
    }
    void setGranularStepsUsed(int steps) noexcept { granularSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setGranularDivisionIndex(int idx) noexcept { granularSeq.divisionIndex.store(juce::jlimit(0, 10, idx)); }
    
    // Slicer sequencer accessors
    const SeqState& getSlicerSeqState() const { return slicerSeq; }
    int getSlicerPlayingStep() const noexcept { return slicerSeq.playingStep.load(); }
    int getSlicerCurrentStep() const noexcept { return slicerSeq.currentStep.load(); }
    void setSlicerSelectedStep(int step) noexcept { slicerUiSelectedStep.store(step); }
    void setSlicerSequencerEnabled(bool enabled) noexcept {
        slicerSeq.enabled.store(enabled);
        slicerSeq.active.store(enabled);
    }
    void setSlicerStepsUsed(int steps) noexcept { slicerSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setSlicerDivisionIndex(int idx) noexcept { slicerSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    // Dub Delay sequencer accessors
    const SeqState& getDubDelaySeqState() const { return dubdelaySeq; }
    int getDubDelayPlayingStep() const noexcept { return dubdelaySeq.playingStep.load(); }
    int getDubDelayCurrentStep() const noexcept { return dubdelaySeq.currentStep.load(); }
    int getDubDelayUiSelectedStep() const noexcept { return dubdelayUiSelectedStep.load(); }
    void setDubDelaySelectedStep(int step) noexcept { dubdelayUiSelectedStep.store(step); }
    void setDubDelaySequencerEnabled(bool enabled) noexcept {
        dubdelaySeq.enabled.store(enabled);
        // Only set active to false when disabling, don't auto-enable when enabling
        if (!enabled) {
            dubdelaySeq.active.store(false);
        }
    }
    void setDubDelayStepsUsed(int steps) noexcept { dubdelaySeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setDubDelayDivisionIndex(int idx) noexcept { dubdelaySeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    // Space Delay sequencer accessors
    const SeqState& getSpaceDelaySeqState() const { return spacedelaySeq; }
    int getSpaceDelayPlayingStep() const noexcept { return spacedelaySeq.playingStep.load(); }
    int getSpaceDelayCurrentStep() const noexcept { return spacedelaySeq.currentStep.load(); }
    int getSpaceDelayUiSelectedStep() const noexcept { return spacedelayUiSelectedStep.load(); }
    void setSpaceDelaySelectedStep(int step) noexcept { spacedelayUiSelectedStep.store(step); }
    void setSpaceDelaySequencerEnabled(bool enabled) noexcept { 
        spacedelaySeq.enabled.store(enabled);
        // Only set active to false when disabling, don't auto-enable when enabling
        if (!enabled) {
            spacedelaySeq.active.store(false);
        }
    }
    void setSpaceDelayStepsUsed(int steps) noexcept { spacedelaySeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setSpaceDelayDivisionIndex(int idx) noexcept { spacedelaySeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    StepSnapshot getGranularSafeSnapshot(int step) const;
    void setGranularStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateGranularCurrentStepSnapshot(int knobIndex, float value);
    
    // Slicer snapshot accessors
    StepSnapshot getSlicerSafeSnapshot(int step) const;
    void setSlicerStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateSlicerCurrentStepSnapshot(int knobIndex, float value);
    
    // Dub Delay snapshot accessors
    StepSnapshot getDubDelaySafeSnapshot(int step) const;
    void setDubDelayStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateDubDelayCurrentStepSnapshot(int knobIndex, float value);
    
    StepSnapshot getSpaceDelaySafeSnapshot(int step) const;
    void setSpaceDelayStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateSpaceDelayCurrentStepSnapshot(int knobIndex, float value);
    
    void setSequencerEnabled(bool enabled) noexcept { 
        seq.enabled.store(enabled); 
        // Track if user explicitly disabled sequencer
        userDisabledSequencer.store(!enabled);
        // Only set active if enabled, otherwise leave active state for transport watcher
        if (enabled) {
            seq.active.store(true);
        }
    }
    void setSequencerActive(bool active) noexcept { 
        seq.active.store(active); 
    }
    void setFxEnabled(bool enabled) noexcept { fxEnabled.store(enabled); }
    void setStepsUsed(int steps) noexcept { seq.stepsUsed.store(steps); }
    void setDivisionIndex(int index) noexcept { seq.divisionIndex.store(index); }
    void setStdMode(int mode) noexcept { seq.stdMode.store(juce::jlimit(0, 2, mode)); }
    void resetSequencerState() noexcept;
    void startStandalonePlayback() noexcept;
    void randomizeAllStepSnapshots() noexcept;
    
    // Level tracking for meters
    float getInputLevel() const noexcept { return inputLevel.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }
    
    // Modern dual-bar meter access
    const MeterState& getInputMeter() const noexcept { return inputMeter; }
    const MeterState& getOutputMeter() const noexcept { return outputMeter; }

private:
    // Parameters
    juce::AudioProcessorValueTreeState valueTreeState;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Transport cache
    mutable TransportCache transportCache;
    void updateTransportCache (juce::AudioPlayHead* playHead, int numSamples) noexcept;
    
    // Transport snapshot (audio-thread owned, atomics for UI reads if needed)
    std::atomic<bool> wasPlaying{false};
    std::atomic<bool> haveValidPos{false};
    std::atomic<bool> armPending{false};     // true when play just started but PPQ not valid yet
    std::atomic<double> lastPPQ{-1.0};
    std::atomic<int64_t> lastSamples{-1};
    std::atomic<bool> followHost{true};     // follow host transport
    std::atomic<bool> userDisabledSequencer{false}; // track if user explicitly disabled sequencer

    
    // Helper function for sequencer (legacy - now handled by SeqState::beatsPerStepFromDivision)
    static double divisionToBeats(int divIdx);
    
    // Sequencer state (Delay page)
    SeqState seq;
    std::atomic<int> uiSelectedStep { 0 };  // Editor's selected step for editing only
    bool prevHostPlaying = false;
    std::chrono::high_resolution_clock::time_point standaloneStartTime;
    
    // AutoPan sequencer state (independent from delay)
    SeqState autopanSeq;
    std::atomic<int> autopanUiSelectedStep { 0 };  // AutoPan editor's selected step
    
    // Dirt sequencer state (independent from delay and autopan)
    SeqState dirtSeq;
    std::atomic<int> dirtUiSelectedStep { 0 };  // Dirt editor's selected step
    
    // Redux sequencer state (independent from other effects)
    SeqState reduxSeq;
    std::atomic<int> reduxUiSelectedStep { 0 };  // Redux editor's selected step
    
    // PhaseBloom sequencer state (independent from other effects)
    SeqState phaseBloomSeq;
    std::atomic<int> phaseBloomUiSelectedStep { 0 };  // PhaseBloom editor's selected step
    
    // Formant sequencer state (independent from other effects)
    SeqState formantSeq;
    std::atomic<int> formantUiSelectedStep { 0 };  // Formant editor's selected step
    
    // Form 2 sequencer state (independent from other effects)
    SeqState form2Seq;
    std::atomic<int> form2UiSelectedStep { 0 };  // Form 2 editor's selected step
    
    // Chorus Sequencer State (independent from Delay, AutoPan, and Dirt)
    SeqState chorusSeq;
    std::atomic<int> chorusUiSelectedStep { 0 };  // Chorus editor's selected step
    
    // Reverb Sequencer State (independent from all other sequencers)
    SeqState reverbSeq;
    std::atomic<int> reverbUiSelectedStep { 0 };  // Reverb editor's selected step
    
    // Granular Sequencer State (independent from all other sequencers)
    SeqState granularSeq;
    std::atomic<int> granularUiSelectedStep { 0 };  // Granular editor's selected step
    
    // Slicer Sequencer State (independent from all other sequencers)
    SeqState slicerSeq;
    std::atomic<int> slicerUiSelectedStep { 0 };  // Slicer editor's selected step
    
    // Dub Delay Sequencer State (independent from all other sequencers)
    SeqState dubdelaySeq;
    std::atomic<int> dubdelayUiSelectedStep { 0 };  // Dub Delay editor's selected step
    
    // Saturate Sequencer State (independent from all other sequencers)
    SeqState saturateSeq;
    std::atomic<int> saturateUiSelectedStep { 0 };  // Saturate editor's selected step
    
    // Filter Sequencer State (independent from all other sequencers)
    SeqState filterSeq;
    std::atomic<int> filterUiSelectedStep { 0 };  // Filter editor's selected step
    
    // Space Delay Sequencer State (independent from all other sequencers)
    SeqState spacedelaySeq;
    std::atomic<int> spacedelayUiSelectedStep { 0 };  // Space Delay editor's selected step
    
    // Step snapshots storage (shared structure, independent sequencing)
    std::array<StepSnapshot, 16> stepSnapshots;
    std::array<StepSnapshot, 16> autopanStepSnapshots;
    std::array<StepSnapshot, 16> dirtStepSnapshots;
    std::array<StepSnapshot, 16> reduxStepSnapshots;
    std::array<StepSnapshot, 16> phaseBloomStepSnapshots;
    std::array<StepSnapshot, 16> formantStepSnapshots;
    std::array<StepSnapshot, 16> form2StepSnapshots;
    std::array<StepSnapshot, 16> chorusStepSnapshots;
    std::array<StepSnapshot, 16> reverbStepSnapshots;
    std::array<StepSnapshot, 16> granularStepSnapshots;
    std::array<StepSnapshot, 16> slicerStepSnapshots;
    std::array<StepSnapshot, 16> dubdelayStepSnapshots;
    std::array<StepSnapshot, 16> saturateStepSnapshots;
    std::array<StepSnapshot, 16> filterStepSnapshots;
    std::array<StepSnapshot, 16> spacedelayStepSnapshots;
    
    // Level tracking for meters
    std::atomic<float> inputLevel { -60.0f };
    std::atomic<float> outputLevel { -60.0f };
    
    // Modern dual-bar meter state
    MeterState inputMeter, outputMeter;
    
    // Helper functions for meter processing
    static inline float linearToDb(float x) {
        return x > 1e-9f ? 20.0f * std::log10(x) : -100.0f;
    }
    
    // Sequencer methods
    void updatePlayingStepFromTransport();
    static inline double stepPeriodBeatsFromDivision(int idx);
    void applySnapshotTargets(const StepSnapshot& s);
    std::atomic<bool> fxEnabled { true };
    
public:
    // Public method for UI to update snapshots
    void updateCurrentStepSnapshot(int knobIndex, float value);
    
           // RE-201 Space Delay DSP - High Quality Implementation
           FxDelay spaceDelay;
           
    // AutoPan DSP Implementation
    AutoPan autoPan;
    PanVisualState panVis;
    
    // Dirt DSP Implementation
    FxDirt dirt;
    
    // Chorus DSP Implementation
    ChorusEngine chorus;
    
    // Reverb DSP Implementation
    SimpleHallReverb hall;
    GranularEngine granular;
    
    // Slicer DSP Implementation
    RhythmGateEngine slicer;
    
    // Dub Delay DSP Implementation
    DubDelayProcessor dubDelay;
    
    // COMPRESS+ DSP Implementation - Master effect
    CompressEngine compressEngine;
    
    // Getter for CompressEngine (for UI access)
    CompressEngine& getCompressEngine() { return compressEngine; }
    
    // PhaseBloom DSP Implementation
    PhaseBloomEngine phaseBloomEngine;
    
    // Formant DSP Implementation
    FormantProcessor formantProcessor;
    
    FormantProcessor& getFormantProcessor() { return formantProcessor; }
    
    // Saturate DSP Implementation
    SaturateProcessor saturateProcessor;
    
    SaturateProcessor& getSaturateProcessor() { return saturateProcessor; }
    
    // Saturate sequencer accessors
    SeqState& getSaturateSeqState() { return saturateSeq; }
    const SeqState& getSaturateSeqState() const { return saturateSeq; }
    int getSaturatePlayingStep() const noexcept { return saturateSeq.playingStep.load(); }
    int getSaturateCurrentStep() const noexcept { return saturateSeq.currentStep.load(); }
    int getSaturateUiSelectedStep() const noexcept { return saturateUiSelectedStep.load(); }
    void setSaturateSelectedStep(int step) noexcept { saturateUiSelectedStep.store(step); }
    void setSaturateSequencerEnabled(bool enabled) noexcept { saturateSeq.enabled.store(enabled); }
    void setSaturateStepsUsed(int steps) noexcept { saturateSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setSaturateDivisionIndex(int idx) noexcept { saturateSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    // Saturate snapshot accessors
    StepSnapshot getSaturateSafeSnapshot(int step) const;
    void setSaturateStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateSaturateCurrentStepSnapshot(int knobIndex, float value);
    
    // Filter sequencer state access
    const SeqState& getFilterSeqState() const { return filterSeq; }
    int getFilterPlayingStep() const noexcept { return filterSeq.playingStep.load(); }
    int getFilterCurrentStep() const noexcept { return filterSeq.currentStep.load(); }
    void setFilterSelectedStep(int step) noexcept { filterUiSelectedStep.store(step); }
    void setFilterSequencerEnabled(bool enabled) noexcept { filterSeq.enabled.store(enabled); }
    void setFilterStepAmount(int steps) noexcept { filterSeq.stepsUsed.store(juce::jlimit(1, 16, steps)); }
    void setFilterDivisionIndex(int idx) noexcept { filterSeq.divisionIndex.store(juce::jlimit(0, 7, idx)); }
    
    // Filter snapshot accessors
    StepSnapshot getFilterSafeSnapshot(int step) const;
    void setFilterStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void updateFilterCurrentStepSnapshot(int knobIndex, float value);
    
    // Form 2 DSP Implementation
    Form2Processor form2Processor;
    
    Form2Processor& getForm2Processor() { return form2Processor; }
    
    // Filter processor
    FxFilter filterProcessor;
    
    // Redux DSP Implementation
    ReduxBank reduxBank;
    
    // PanMan-style visualizer clock
    struct PanVisClock {
        std::atomic<double> phase01{ 0.0 };         // cycles in [0..1]
        std::atomic<double> incPerSample{ 0.0 };    // cycles per sample
        std::atomic<double> sampleRate{ 44100.0 };  // for UI interp
    };
    PanVisClock panClock;
    
    // AutoPan Sequencer Visual Clock (independent from Delay sequencer)
    struct AutoPanSeqVisClock {
        std::atomic<double> ppqAtBlockStart{ 0.0 };
        std::atomic<double> ppqPerSample{ 0.0 };
        std::atomic<bool>   isPlaying{ false };
        std::atomic<double> barStartPPQ{ 0.0 };
        std::atomic<int>    timeSigNumerator{ 4 };
        std::atomic<int>    timeSigDenominator{ 4 };
        std::atomic<double> loopStartPPQ{ -1.0 };  // -1 = no loop
        std::atomic<double> loopEndPPQ{ -1.0 };
        std::atomic<uint64_t> sampleCounter{ 0 };
    };
    AutoPanSeqVisClock autopanSeqClock;
    
    // Output Visualizer - ring buffer for waveform display
    AudioRingBuffer outputVisualizerBuffer;
    int downsampleCounter = 0;
    const int downsampleRate = 128; // Push to visualizer every 128 samples
    
    // Spectrum Analyzer - FFT-based spectrum view with trails
    SpectrumAnalyzer spectrumAnalyzer;
    
    // Master HP/LP Filters (12dB/oct Butterworth)
    juce::dsp::StateVariableTPTFilter<float> masterHPF[2], masterLPF[2]; // Stereo
    juce::SmoothedValue<float> hpCutoffSmooth, lpCutoffSmooth; // Smooth cutoff changes
    
    // Get current pan position for visualizer
    float getCurrentPanPosition() const { 
        // Check if sync mode is enabled
        auto* syncParam = valueTreeState.getRawParameterValue("autopanTimeSync");
        bool syncToTransport = syncParam && syncParam->load() > 0.5f;
        bool isPlaying = wasPlaying.load();
        
        if (syncToTransport) {
            double bpm = getBpmOrDefault(120.0);
            double ppqPosition = transportCache.ppq.load();
            return autoPan.getCurrentPanPosition(syncToTransport, isPlaying, bpm, ppqPosition);
        } else {
            return autoPan.getCurrentPanPosition();
        }
    }
           
           double dspSampleRate = 44100.0;
           
           // Effect Router - manages dynamic page-effect assignments
           EffectRouter effectRouter;
           int lastRouterVersion = 0;  // Cached version to detect routing changes
           
           // FX routing
           FxType currentFx = FxType::Delay;
    
    // Helper functions - simplified
    
    // Effect processing helpers (for dynamic routing)
    void processDelayEffect(juce::AudioBuffer<float>& buffer);
    void processAutoPanEffect(juce::AudioBuffer<float>& buffer);
    void processDirtEffect(juce::AudioBuffer<float>& buffer);
    void processChorusEffect(juce::AudioBuffer<float>& buffer);
    void processCompressEffect(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};