#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>

/**
 * EffectRouter - Manages dynamic page-to-effect assignments with swapping
 * 
 * Enables any effect to appear on any page slot, with routing order = page order.
 * Swapping preserves all effect state (DSP, UI, sequencer, parameters).
 */

enum class EffectID {
    SpaceDelay = 0,
    AutoPan = 1,
    Dirt = 2,
    Chorus = 3,
    Reverb = 4,
    Granular = 5,
    Slicer = 6
};

enum class SlotID {
    Slot1 = 0,  // Page 1 (leftmost)
    Slot2 = 1,  // Page 2
    Slot3 = 2,  // Page 3
    Slot4 = 3   // Page 4 (rightmost)
};

class EffectRouter
{
public:
    EffectRouter()
    {
        // Default assignment: first 4 effects in slots (Reverb initially unassigned)
        assignment[0] = EffectID::SpaceDelay;
        assignment[1] = EffectID::AutoPan;
        assignment[2] = EffectID::Dirt;
        assignment[3] = EffectID::Chorus;
        
        routerVersion.store(0);
    }
    
    // Get the effect currently assigned to a slot
    EffectID getEffectInSlot(SlotID slot) const
    {
        return assignment[static_cast<int>(slot)];
    }
    
    // Get the slot where an effect currently lives (returns -1 if not assigned)
    int getSlotIndexForEffect(EffectID effect) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if (assignment[i] == effect)
                return i;
        }
        // Effect not currently assigned to any slot
        return -1;
    }
    
    // Get the slot where an effect currently lives (legacy wrapper)
    SlotID getSlotForEffect(EffectID effect) const
    {
        int idx = getSlotIndexForEffect(effect);
        if (idx >= 0)
            return static_cast<SlotID>(idx);
        // Fallback (should only happen during initial assignment)
        return SlotID::Slot1;
    }
    
    // Swap two slots' assignments
    void swapSlots(SlotID slotA, SlotID slotB)
    {
        if (slotA == slotB) return;
        
        const int a = static_cast<int>(slotA);
        const int b = static_cast<int>(slotB);
        
        std::swap(assignment[a], assignment[b]);
        
        // Increment version so audio thread knows to rebuild routing
        routerVersion.fetch_add(1);
    }
    
    // Assign an effect to a slot (triggers swap if target effect is already assigned elsewhere)
    void assignEffectToSlot(EffectID effect, SlotID targetSlot)
    {
        int currentSlotIdx = getSlotIndexForEffect(effect);
        
        // If effect is not currently assigned, just place it in the target slot
        // (this replaces whatever was there)
        if (currentSlotIdx < 0)
        {
            assignment[static_cast<int>(targetSlot)] = effect;
            routerVersion.fetch_add(1);
        }
        // If effect is already in a different slot, swap them
        else if (currentSlotIdx != static_cast<int>(targetSlot))
        {
            swapSlots(static_cast<SlotID>(currentSlotIdx), targetSlot);
        }
        // If effect is already in target slot, no-op
    }
    
    // Get the routing order (effects in slot order for DSP chain)
    std::array<EffectID, 4> getRoutingOrder() const
    {
        return assignment;
    }
    
    // Get router version (for audio thread to detect changes)
    int getRouterVersion() const
    {
        return routerVersion.load();
    }
    
    // Serialize to ValueTree
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree tree("EffectRouter");
        tree.setProperty("version", routerVersion.load(), nullptr);
        
        // Store assignment as individual properties (XML-friendly)
        tree.setProperty("slot0", static_cast<int>(assignment[0]), nullptr);
        tree.setProperty("slot1", static_cast<int>(assignment[1]), nullptr);
        tree.setProperty("slot2", static_cast<int>(assignment[2]), nullptr);
        tree.setProperty("slot3", static_cast<int>(assignment[3]), nullptr);
        
        return tree;
    }
    
    // Restore from ValueTree
    void fromValueTree(const juce::ValueTree& tree)
    {
        if (!tree.isValid() || tree.getType() != juce::Identifier("EffectRouter"))
            return;
        
        // Restore assignment from individual properties (XML-friendly)
        if (tree.hasProperty("slot0"))
        {
            assignment[0] = static_cast<EffectID>(juce::jlimit(0, 6, static_cast<int>(tree.getProperty("slot0", 0))));
            assignment[1] = static_cast<EffectID>(juce::jlimit(0, 6, static_cast<int>(tree.getProperty("slot1", 1))));
            assignment[2] = static_cast<EffectID>(juce::jlimit(0, 6, static_cast<int>(tree.getProperty("slot2", 2))));
            assignment[3] = static_cast<EffectID>(juce::jlimit(0, 6, static_cast<int>(tree.getProperty("slot3", 3))));
        }
        else
        {
            // Fallback: try old Array format for backwards compatibility
            auto assignmentVar = tree.getProperty("assignment");
            if (assignmentVar.isArray())
            {
                auto* arr = assignmentVar.getArray();
                if (arr && arr->size() == 4)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        int effectID = static_cast<int>(arr->getReference(i));
                        assignment[i] = static_cast<EffectID>(juce::jlimit(0, 6, effectID));
                    }
                }
            }
        }
        
        routerVersion.store(tree.getProperty("version", 0));
    }
    
    // Validate assignment (ensure no duplicates in the 4 slots)
    // Note: With 7 effects and 4 slots, three effects will always be unassigned
    bool isValid() const
    {
        bool seen[7] = { false, false, false, false, false, false, false };
        for (int i = 0; i < 4; ++i)
        {
            int effectIdx = static_cast<int>(assignment[i]);
            if (effectIdx < 0 || effectIdx > 6 || seen[effectIdx])
                return false;
            seen[effectIdx] = true;
        }
        return true;
    }
    
private:
    std::array<EffectID, 4> assignment;  // Slot index -> EffectID
    std::atomic<int> routerVersion;       // Incremented on each swap
};

