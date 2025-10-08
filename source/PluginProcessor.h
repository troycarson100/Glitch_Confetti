#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "dsp/FxDelay.h"
#include "dsp/AutoPan.h"
#include "dsp/DspFlags.h"
#include "StepSnapshot.h"
#include "MeterTheme.h"

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
    void setSequencerEnabled(bool enabled) noexcept { 
        seq.enabled.store(enabled); 
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
    
    // Step snapshots storage (shared structure, independent sequencing)
    std::array<StepSnapshot, 16> stepSnapshots;
    std::array<StepSnapshot, 16> autopanStepSnapshots;
    
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
    
    // PanMan-style visualizer clock
    struct PanVisClock {
        std::atomic<double> phase01{ 0.0 };         // cycles in [0..1]
        std::atomic<double> incPerSample{ 0.0 };    // cycles per sample
        std::atomic<double> sampleRate{ 44100.0 };  // for UI interp
    };
    PanVisClock panClock;
    
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
           
           // FX routing
           FxType currentFx = FxType::Delay;
    
    // Helper functions - simplified

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};