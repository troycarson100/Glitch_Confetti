#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <array>
#include <vector>
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
    int maxDivisionIndex = 7; // Maximum division index (0-7 for most, 0-10 for Granular)
};

class PageTargetRegistry {
public:
    PageTargetRegistry();
    
    // Returns the four currently active pages in routing order
    std::array<PageTargets, 4> getActivePages(
        const class PluginProcessor& proc,
        const juce::AudioProcessorValueTreeState& apvts) const;
    
    // Returns 4 random effects (excluding master/compressor)
    std::array<PageTargets, 4> getRandomEffects() const;
    
    // Get targets for a specific effect
    PageTargets getTargetsForEffect(EffectID effect) const;
    
private:
    // Pre-built registry for each effect type
    std::map<EffectID, PageTargets> registry;
    
    void buildRegistry();
};

