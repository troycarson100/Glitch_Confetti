#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_set>
#include "MacroRouter.h"

struct ModSlot
{
    juce::String paramID;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> effNorm; // normalized 0..1
    std::atomic<float> effNormAtomic { 0.0f }; // UI overlay reads this
    
    ModSlot() : effNorm(0.0f) {}
    ModSlot(const juce::String& id) : paramID(id), effNorm(0.0f) {}
    ModSlot(const ModSlot& other) : paramID(other.paramID), effNorm(other.effNorm.getCurrentValue()), effNormAtomic(other.effNormAtomic.load()) {}
};

class ModEngine
{
public:
    ModEngine(juce::AudioProcessorValueTreeState& apvtsRef, MacroRouter& routerRef)
        : apvts(apvtsRef), router(routerRef) {}

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& kv : slots)
            kv.second.effNorm.reset(sr, smoothingSeconds);
    }

    void setSmoothingMs(float ms)
    {
        smoothingSeconds = juce::jmax(0.0f, ms) / 1000.0f;
        for (auto& kv : slots)
            kv.second.effNorm.reset(sr, smoothingSeconds);
    }

    // Called once per processBlock
    void computeBlock (float macro1, float macro2, int numSamples)
    {
        auto routes = router.snapshot();
        std::unordered_set<juce::String> touched;

        // Set targets for routed params
        for (const auto& r : routes)
        {
            auto* p = apvts.getParameter(r.paramID);
            if (p == nullptr) continue;

            const float b = p->getValue(); // baseline normalized
            const float m = (r.macroIndex == 0 ? macro1 : macro2);
            float eTarget;

            if (r.mode == MacroMode::Unipolar)
            {
                const float delta = (r.direction == MacroDirection::Forward)
                    ? (r.depth * m)
                    : (-r.depth * m);
                eTarget = juce::jlimit(0.f, 1.f, b + delta);
            }
            else // Bipolar
            {
                eTarget = juce::jlimit(0.f, 1.f, b + r.depth * (2.0f * m - 1.0f));
            }

            auto& slot = getOrCreate(r.paramID);
            slot.effNorm.setTargetValue(eTarget);
            touched.insert(r.paramID);
        }

        // Unrouted known slots follow baseline to avoid drift
        for (auto& kv : slots)
        {
            if (touched.find(kv.first) == touched.end())
                if (auto* p = apvts.getParameter(kv.first))
                    kv.second.effNorm.setTargetValue(p->getValue());
        }

        // Advance smoothing & publish to UI
        for (int n = 0; n < numSamples; ++n)
            for (auto& kv : slots)
                kv.second.effNormAtomic.store(kv.second.effNorm.getNextValue(), std::memory_order_relaxed);
    }

    float getEffectiveNormUI (const juce::String& pid) const
    {
        if (auto it = slots.find(pid); it != slots.end())
            return it->second.effNormAtomic.load(std::memory_order_relaxed);
        if (auto* p = apvts.getParameter(pid)) return p->getValue();
        return 0.f;
    }

private:
    ModSlot& getOrCreate (const juce::String& pid)
    {
        auto it = slots.find(pid);
        if (it == slots.end())
        {
            ModSlot s(pid);
            s.effNorm.reset(sr, smoothingSeconds);
            it = slots.emplace(std::piecewise_construct, 
                              std::forward_as_tuple(pid), 
                              std::forward_as_tuple(std::move(s))).first;
        }
        return it->second;
    }

    juce::AudioProcessorValueTreeState& apvts;
    MacroRouter& router;
    std::unordered_map<juce::String, ModSlot> slots;
    double sr = 44100.0;
    double smoothingSeconds = 0.020; // 20 ms
};