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
    Reverb = 4
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
    
    // Get the slot where an effect currently lives
    SlotID getSlotForEffect(EffectID effect) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if (assignment[i] == effect)
                return static_cast<SlotID>(i);
        }
        // Should never happen (all effects must be assigned)
        jassertfalse;
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
        SlotID currentSlot = getSlotForEffect(effect);
        if (currentSlot != targetSlot)
        {
            swapSlots(currentSlot, targetSlot);
        }
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
        
        // Store assignment as array of ints
        juce::Array<juce::var> assignmentArray;
        for (int i = 0; i < 4; ++i)
        {
            assignmentArray.add(static_cast<int>(assignment[i]));
        }
        tree.setProperty("assignment", assignmentArray, nullptr);
        
        return tree;
    }
    
    // Restore from ValueTree
    void fromValueTree(const juce::ValueTree& tree)
    {
        if (!tree.isValid() || tree.getType() != juce::Identifier("EffectRouter"))
            return;
        
        auto assignmentVar = tree.getProperty("assignment");
        if (assignmentVar.isArray())
        {
            auto* arr = assignmentVar.getArray();
            if (arr && arr->size() == 4)
            {
                for (int i = 0; i < 4; ++i)
                {
                    int effectID = static_cast<int>(arr->getReference(i));
                    assignment[i] = static_cast<EffectID>(juce::jlimit(0, 3, effectID));
                }
            }
        }
        
        routerVersion.store(tree.getProperty("version", 0));
    }
    
    // Validate assignment (ensure no duplicates in the 4 slots)
    // Note: With 5 effects and 4 slots, one effect will always be unassigned
    bool isValid() const
    {
        bool seen[5] = { false, false, false, false, false };
        for (int i = 0; i < 4; ++i)
        {
            int effectIdx = static_cast<int>(assignment[i]);
            if (effectIdx < 0 || effectIdx > 4 || seen[effectIdx])
                return false;
            seen[effectIdx] = true;
        }
        return true;
    }
    
private:
    std::array<EffectID, 4> assignment;  // Slot index -> EffectID
    std::atomic<int> routerVersion;       // Incremented on each swap
};

