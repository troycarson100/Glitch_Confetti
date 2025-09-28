#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "StepScheduler.h"
#include "CircularBuffer.h"
#include "dsp/StepSync.h"
#include <random>

// Forward declaration for preset loader
class PresetLoader;

// Sequencer step modes
enum class StepMode : int { Straight = 0, Reverse = 1, Glitch = 2 };
static inline const juce::StringArray kStepModeNames { "Straight", "Reverse", "Glitch" };

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

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

    // APVTS for GlitchConfetti parameters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    // Peak meter access for UI
    float getInputPeakL() const { return inputPeakL.load(); }
    float getInputPeakR() const { return inputPeakR.load(); }
    float getOutputPeakL() const { return outputPeakL.load(); }
    float getOutputPeakR() const { return outputPeakR.load(); }
    
    // Preset management
    void loadPreset(int presetIndex);
    void saveCurrentAsUserPreset(const juce::String& presetName);
    juce::StringArray getPresetNames() const;
    int getNumPresets() const;
    
    // Sequencer access for UI
    int getCurrentStep() const { return currentStep.load(); }
    void requestSequencerReset() { stepSync.requestReset(); }

private:
    // Parameter layout for GlitchConfetti
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // APVTS instance
    juce::AudioProcessorValueTreeState apvts;
    
    // Step scheduler for timing
    StepScheduler stepScheduler;
    
    // Host-locked step sync
    StepSync stepSync;
    
    // Circular buffer for stutter effects
    CircularBuffer circularBuffer;
    CircularBuffer::StutterInfo stutterInfo;
    CircularBuffer::ReverseInfo reverseInfo;
    CircularBuffer::FlickInfo flickInfo;
    
    // Random number generation for effect triggering
    std::mt19937 stutterRng;
    std::uniform_real_distribution<float> stutterChance{0.0f, 1.0f};
    std::uniform_real_distribution<float> reverseChance{0.0f, 1.0f};
    std::uniform_real_distribution<float> flickChance{0.0f, 1.0f};
    
    // Peak meters (atomics for UI thread access)
    std::atomic<float> inputPeakL{0.0f};
    std::atomic<float> inputPeakR{0.0f};
    std::atomic<float> outputPeakL{0.0f};
    std::atomic<float> outputPeakR{0.0f};
    
    // Preset loader
    std::unique_ptr<PresetLoader> presetLoader;
    
    // === Sequencer state ===
    std::atomic<int> currentStep { 0 };              // 0..15
    std::atomic<int> currentMode { 0 };              // cached mode of current step
    std::array<std::atomic<int>, 16> stepModes;      // 0..2 per step (Straight/Reverse/Glitch)

    // transport/scheduling
    double lastSampleRate = 44100.0;
    double lastBpm = 120.0;
    int    activeSteps = 16;                         // 1..16
    int    stepIndex   = 0;                          // 0..(activeSteps-1)
    int    samplesToNext = 0;                        // countdown

    // cached params (updated each block)
    int    divisionIndex = 2;                        // 0:1/4,1:1/8,2:1/16,3:1/32,4:Free
    float  freeRateHz    = 8.0f;
    bool   wasFollowingHost = false;                 // edge detector for Follow Host toggle

    // helper converts division index to beats
    static inline double divisionToBeats (int divIdx)
    {
        switch (divIdx) {
            case 0: return 1.0;    // 1/4
            case 1: return 0.5;    // 1/8
            case 2: return 0.25;   // 1/16
            case 3: return 0.125;  // 1/32
            default: return 0.25;  // fallback
        }
    }

    // helpers
    int computeSamplesPerStep (double sr, double bpm) const;
    void reseedStepTimer (double sr, double bpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
