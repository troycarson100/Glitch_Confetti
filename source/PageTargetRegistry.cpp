#include "PageTargetRegistry.h"
#include "PluginProcessor.h"

PageTargetRegistry::PageTargetRegistry()
{
    buildRegistry();
}

void PageTargetRegistry::buildRegistry()
{
    // Space Delay Page
    {
        PageTargets targets;
        targets.pageId = "SpaceDelay";
        targets.knobParamIds = {
            "timeMs",      // Knob 0: Time
            "feedback",    // Knob 1: Feedback
            "wowDepth",    // Knob 2: Wow Depth
            "wowRate",     // Knob 3: Wow Rate
            "drive",       // Knob 4: Saturation/Drive
            "hiCut",       // Knob 5: High Cut
            "lowCut",      // Knob 6: Low Cut
            "mix"          // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "delayStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::SpaceDelay] = targets;
    }
    
    // AutoPan Page
    {
        PageTargets targets;
        targets.pageId = "AutoPan";
        targets.knobParamIds = {
            "panRate",      // Knob 0: Rate
            "panPhase",     // Knob 1: Phase
            "panWaveType",  // Knob 2: Wave Type
            "panWaveShape", // Knob 3: Wave Shape
            "panInvert",    // Knob 4: Invert
            "panAmount"     // Knob 5: Amount
            // Note: AutoPan only has 6 knobs
        };
        targets.sequencerStepsUsedKey = "autopanStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::AutoPan] = targets;
    }
    
    // Dirt Page
    {
        PageTargets targets;
        targets.pageId = "Dirt";
        targets.knobParamIds = {
            "dirtDrive",    // Knob 0: Drive
            "dirtColor",    // Knob 1: Color
            "dirtAsym",     // Knob 2: Asym
            "dirtTexture",  // Knob 3: Texture
            "dirtLowCut",   // Knob 4: Low Cut
            "dirtHighCut",  // Knob 5: High Cut
            "dirtTone",     // Knob 6: Tone
            "dirtMix"       // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "dirtStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::Dirt] = targets;
    }
    
    // Chorus Page
    {
        PageTargets targets;
        targets.pageId = "Chorus";
        targets.knobParamIds = {
            "chorusDelay",    // Knob 0: Delay
            "chorusRate",     // Knob 1: Rate
            "chorusDepth",    // Knob 2: Depth
            "chorusFeedback", // Knob 3: Feedback
            "chorusVoices",   // Knob 4: Voices
            "chorusWidth",    // Knob 5: Width
            "chorusTone",     // Knob 6: Tone
            "chorusMix"       // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "chorusStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::Chorus] = targets;
    }
    
    // Reverb Page
    {
        PageTargets targets;
        targets.pageId = "Reverb";
        targets.knobParamIds = {
            "verbWidth",      // Knob 0: Width
            "verbSize",       // Knob 1: Size
            "verbPredelayMs", // Knob 2: Predelay
            "verbDampHz",     // Knob 3: Damping
            "verbDiffusion",  // Knob 4: Diffusion
            "verbEarlyLevel", // Knob 5: Early
            "verbDecaySec",   // Knob 6: Decay
            "verbMix"         // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "reverbStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::Reverb] = targets;
    }
    
    // Granular Page
    {
        PageTargets targets;
        targets.pageId = "Granular";
        targets.knobParamIds = {
            "granSizeMs",     // Knob 0: Grain Size
            "granDensityHz",  // Knob 1: Density
            "granPosition",   // Knob 2: Position
            "granSprayMs",    // Knob 3: Spray
            "granPitchSemi",  // Knob 4: Pitch
            "granRandom",     // Knob 5: Random
            "granTexture",    // Knob 6: Texture
            "granMix"         // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "granularStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::Granular] = targets;
    }
    
    // Slicer Page
    {
        PageTargets targets;
        targets.pageId = "Slicer";
        targets.knobParamIds = {
            "slicerPattern",   // Knob 0: Pattern
            "slicerDivision",  // Knob 1: Division
            "slicerOffset",    // Knob 2: Offset
            "slicerShape",     // Knob 3: Shape
            "slicerReleaseMs", // Knob 4: Release
            "slicerMix"        // Knob 5: Mix
            // Note: Slicer only has 6 knobs
        };
        targets.sequencerStepsUsedKey = "slicerStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::Slicer] = targets;
    }
    
    // Dub Delay Page
    {
        PageTargets targets;
        targets.pageId = "DubDelay";
        targets.knobParamIds = {
            "dubTimeMs",      // Knob 0: Time
            "dubFeedback",    // Knob 1: Feedback
            "dubToneHz",      // Knob 2: Tone
            "dubDrive",       // Knob 3: Drive
            "dubPingPong",    // Knob 4: PingPong
            "dubWowFlutter",  // Knob 5: WowFlutter
            "dubRegenDamp",   // Knob 6: RegenDamp
            "dubMix"          // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "dubdelayStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::DubDelay] = targets;
    }
    
    // Redux Page
    {
        PageTargets targets;
        targets.pageId = "Redux";
        targets.knobParamIds = {
            "reduxBitDepth",           // Knob 0: Bit Depth
            "reduxSampleRateReduction", // Knob 1: Rate
            "reduxJitter",             // Knob 2: Jitter
            "reduxPreFilter",          // Knob 3: Pre Filter
            "reduxPostFilter",         // Knob 4: Post Filter
            "reduxDrive",              // Knob 5: Drive
            "reduxEmphasis",           // Knob 6: Emphasis
            "reduxMix"                 // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "reduxStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::Redux] = targets;
    }
    
    // PhaseBloom Page
    {
        PageTargets targets;
        targets.pageId = "PhaseBloom";
        targets.knobParamIds = {
            "phasebloomDepth",         // Knob 0: Depth
            "phasebloomRate",          // Knob 1: Rate
            "phasebloomFeedback",      // Knob 2: Feedback
            "phasebloomCenter",        // Knob 3: Center
            "phasebloomBloom",         // Knob 4: Bloom
            "phasebloomSpread",        // Knob 5: Spread
            "phasebloomResonance",     // Knob 6: Resonance
            "phasebloomMix"            // Knob 7: Mix
        };
        targets.sequencerStepsUsedKey = "phasebloomStepsUsed";
        targets.maxSteps = 16;
        registry[EffectID::PhaseBloom] = targets;
    }
}

std::array<PageTargets, 4> PageTargetRegistry::getActivePages(
    const PluginProcessor& proc,
    const juce::AudioProcessorValueTreeState& apvts) const
{
    std::array<PageTargets, 4> result;
    const auto& router = const_cast<PluginProcessor&>(proc).getEffectRouter();
    
    for (int slot = 0; slot < 4; ++slot)
    {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slot));
        result[slot] = getTargetsForEffect(effect);
    }
    
    return result;
}

PageTargets PageTargetRegistry::getTargetsForEffect(EffectID effect) const
{
    auto it = registry.find(effect);
    if (it != registry.end())
        return it->second;
    
    // Return empty targets if not found
    return PageTargets();
}

