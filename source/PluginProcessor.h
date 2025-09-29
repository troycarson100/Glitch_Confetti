#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "dsp/FxDelay.h"
#include "dsp/DspFlags.h"
#include "StepSnapshot.h"

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
    std::atomic<int>  stepsUsed { 16 };     // 1..16 from the Steps chip
    std::atomic<int>  divisionIndex { 4 };  // 0:1/1,1:1/2,2:1/4,3:1/8,4:1/16,5:1/32
    std::atomic<int>  playingStep { -1 };   // computed in processBlock
    std::atomic<int>  stdMode { 0 };        // 0: straight, 1: triplet, 2: dotted
    std::atomic<double> originPPQ { 0.0 };  // origin for free-run stepping
    std::atomic<bool>   haveOrigin { false };
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
    void getTransportSnapshot(TransportCache& dest) const noexcept;
    const SeqState& getSeqState() const { return seq; }
    
    // Sequencer state access for editor
    int getPlayingStep() const noexcept { return seq.playingStep.load(); }
    int getSelectedStep() const noexcept { return uiSelectedStep.load(); }
    bool isSequencerEnabled() const noexcept { return seq.enabled.load(); }
    
    // Step snapshot access
    StepSnapshot getSafeSnapshot(int step) const;
    void setStepSnapshot(int step, const StepSnapshot& snapshot) noexcept;
    void setSelectedStep(int step) noexcept { uiSelectedStep.store(step); }
    void setSequencerEnabled(bool enabled) noexcept { seq.enabled.store(enabled); }
    void setStepsUsed(int steps) noexcept { seq.stepsUsed.store(steps); }
    void setDivisionIndex(int index) noexcept { seq.divisionIndex.store(index); }
    void setStdMode(int mode) noexcept { seq.stdMode.store(juce::jlimit(0, 2, mode)); }
    void randomizeAllStepSnapshots() noexcept;

private:
    // Parameters
    juce::AudioProcessorValueTreeState valueTreeState;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Transport cache
    mutable TransportCache transportCache;
    void updateTransportCache (juce::AudioPlayHead* playHead, int numSamples) noexcept;
    
    // Sequencer state
    SeqState seq;
    std::atomic<int> uiSelectedStep { 0 };  // Editor's selected step for editing only
    bool prevHostPlaying = false;
    
    // Step snapshots storage
    std::array<StepSnapshot, 16> stepSnapshots;
    
    // Sequencer methods
    void updatePlayingStepFromTransport();
    static inline double stepPeriodBeatsFromDivision(int idx);
    void applySnapshotTargets(const StepSnapshot& s);
    
public:
    // Public method for UI to update snapshots
    void updateCurrentStepSnapshot(int knobIndex, float value);
    
           // RE-201 Space Delay DSP - High Quality Implementation
           FxDelay spaceDelay;
           double dspSampleRate = 44100.0;
           
           // FX routing
           FxType currentFx = FxType::Delay;
    
    // Helper functions - simplified

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};