#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include "PluginProcessor.h"
#include "EffectRouter.h"
#include "PageTargetRegistry.h"

/**
 * Thread-safe randomization manager for the Dice button.
 * All mutations happen on the message thread via AsyncUpdater.
 * Complete coverage of all knobs and steps across all 4 active pages.
 */
class RandomizationManager : private juce::AsyncUpdater
{
public:
    RandomizationManager(PluginProcessor& proc, juce::AudioProcessorValueTreeState& apvts, class PluginEditor* editor);
    
    // Called by the dice button (message thread). Never does work inline; just queues.
    void requestRandomizeAllActivePages();
    
    // Optional: block re-entry
    bool isBusy() const noexcept { return busy.load(); }
    
private:
    void handleAsyncUpdate() override; // does the work on the message thread
    void randomizeAll();               // transactional randomization
    
    // Pipeline steps
    void collectTargets();       // gather all params + steps + sequencers for 4 random effects
    void applyParamChanges();    // randomize all knob parameters
    void applyStepChanges();     // randomize all step snapshots
    void applySequencerChanges(); // randomize sequencer settings (steps used, rate, enabled)
    void verifyAndReport();      // log coverage report
    
    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    PluginEditor* editor; // For accessing lock states
    PageTargetRegistry registry;
    
    std::atomic<bool> busy { false };
    
    // Collected targets (filled in collectTargets)
    struct ParamTarget {
        juce::RangedAudioParameter* param = nullptr;
        juce::String paramId;
        float currentNorm = 0.5f;
        bool locked = false;
    };
    std::vector<ParamTarget> paramTargets;
    
    struct StepTarget {
        EffectID effect;
        int stepIndex;
        bool locked = false;
    };
    std::vector<StepTarget> stepTargets;
    
    struct SequencerTarget {
        EffectID effect;
        juce::String pageId;
        int maxSteps;
        int maxDivisionIndex;
        bool locked = false;
    };
    std::vector<SequencerTarget> sequencerTargets;
    
    // Statistics for verification
    struct Stats {
        int paramsExpected = 0;
        int paramsRandomized = 0;
        int paramsLocked = 0;
        int stepsExpected = 0;
        int stepsRandomized = 0;
        int stepsLocked = 0;
        int sequencersExpected = 0;
        int sequencersRandomized = 0;
        int sequencersLocked = 0;
        int activeStepsIncluded = 0;
    } stats;
    
    // PRNG
    uint32_t rngState = 0x1234567u;
    float rand01();
    
    // Lock checking
    bool isParamLocked(const juce::String& paramId) const;
    bool isStepLocked(EffectID effect, int step) const;
    
    // Helper functions
    EffectID getEffectIDFromPageId(const juce::String& pageId) const;
};
