#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "EffectRouter.h"

/**
 * Central registry of all parameters and sequencer data for each effect page.
 * Used by RandomizationManager to ensure complete coverage when randomizing.
 */
struct PageTargets {
    juce::String pageId;
    std::vector<juce::String> knobParamIds; // ALL knobs on this page
    juce::String sequencerStepsUsedKey; // e.g., "delayStepsUsed"
    int maxSteps = 16; // Maximum number of steps
};

class PageTargetRegistry {
public:
    PageTargetRegistry();
    
    // Returns the four currently active pages in routing order
    std::array<PageTargets, 4> getActivePages(
        const class PluginProcessor& proc,
        const juce::AudioProcessorValueTreeState& apvts) const;
    
    // Get targets for a specific effect
    PageTargets getTargetsForEffect(EffectID effect) const;
    
private:
    // Pre-built registry for each effect type
    std::map<EffectID, PageTargets> registry;
    
    void buildRegistry();
};

