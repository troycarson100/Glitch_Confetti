#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "EffectRouter.h"

/**
 * Thread-safe randomization manager for the Dice button.
 * All mutations happen on the message thread via AsyncUpdater.
 * Prevents crashes from threading issues, re-entrancy, and ValueTree storms.
 */
struct RandomizationManager : private juce::AsyncUpdater
{
    RandomizationManager(PluginProcessor& proc, juce::AudioProcessorValueTreeState& apvts);
    
    // Called by the dice button (message thread). Never does work inline; just queues.
    void requestRandomizeAllActivePages();
    
    // Optional: block re-entry
    bool isBusy() const noexcept { return busy.load(); }
    
    // Callback when randomization is complete (called on message thread)
    std::function<void()> onRandomizationComplete;
    
private:
    void handleAsyncUpdate() override; // does the work on the message thread
    void randomizeAll();               // transactional randomization
    
    // helpers
    void collectTargets();  // gather unlocked parameters & step data for the 4 active pages
    void applyParamChanges();  // beginGesture, setValueNotifyingHost (normalized), endGesture (batched)
    void applyStepDataChanges(); // ValueTree transaction for sequencers (all steps)
    void notifyUI(); // minimal repaint/refresh
    
    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    
    std::atomic<bool> busy { false };
    
    // Collected work sets (filled in collectTargets)
    struct TargetParam { 
        juce::RangedAudioParameter* p = nullptr; 
        float normTarget = 0.f; 
    };
    std::vector<TargetParam> paramTargets;
    
    struct StepEdit { 
        juce::ValueTree node; 
        juce::Identifier key; 
        float newValue; 
    };
    std::vector<StepEdit> stepTargets;
    
    // PRNG
    uint32_t rngState = 0x1234567u;
    float rand01(); // fast xorshift
    
    // Lock state helpers
    bool isParamLocked(const juce::String& paramID);
    bool isStepLocked(int step, EffectID effect);
};

