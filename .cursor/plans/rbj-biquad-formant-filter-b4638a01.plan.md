<!-- b4638a01-463a-4f82-b207-6aea25f906e1 fa29bc11-3661-4ab9-a61e-15011ae491af -->
# Fix Saturate Dropdown Visibility

## Root Cause
The `EffectRouter` class has hardcoded limits that only support effect IDs 0-9, but we now have effects up to ID 12 (Saturate). This causes validation failures and serialization issues that prevent Saturate from being properly registered in the router system.

## Changes Required

### 1. Fix `EffectRouter.h` - Update validation and serialization limits

**File**: `source/EffectRouter.h`

**Change 1** - Fix `isValid()` function (line ~178):
```cpp
// OLD (only checks 0-9):
bool seen[10] = { false, false, false, false, false, false, false, false, false, false };
for (int i = 0; i < 4; ++i)
{
    int effectIdx = static_cast<int>(assignment[i]);
    if (effectIdx < 0 || effectIdx > 9 || seen[effectIdx])
        return false;
    seen[effectIdx] = true;
}

// NEW (supports 0-12):
bool seen[13] = {}; // 13 effects: IDs 0-12
for (int i = 0; i < 4; ++i)
{
    int effectIdx = static_cast<int>(assignment[i]);
    if (effectIdx < 0 || effectIdx > 12 || seen[effectIdx])
        return false;
    seen[effectIdx] = true;
}
```

**Change 2** - Fix `fromValueTree()` jlimit calls (lines ~148-151):
```cpp
// OLD (clamps to 0-8):
assignment[0] = static_cast<EffectID>(juce::jlimit(0, 8, static_cast<int>(tree.getProperty("slot0", 0))));
assignment[1] = static_cast<EffectID>(juce::jlimit(0, 8, static_cast<int>(tree.getProperty("slot1", 1))));
assignment[2] = static_cast<EffectID>(juce::jlimit(0, 8, static_cast<int>(tree.getProperty("slot2", 2))));
assignment[3] = static_cast<EffectID>(juce::jlimit(0, 8, static_cast<int>(tree.getProperty("slot3", 3))));

// NEW (allows 0-12):
assignment[0] = static_cast<EffectID>(juce::jlimit(0, 12, static_cast<int>(tree.getProperty("slot0", 0))));
assignment[1] = static_cast<EffectID>(juce::jlimit(0, 12, static_cast<int>(tree.getProperty("slot1", 1))));
assignment[2] = static_cast<EffectID>(juce::jlimit(0, 12, static_cast<int>(tree.getProperty("slot2", 2))));
assignment[3] = static_cast<EffectID>(juce::jlimit(0, 12, static_cast<int>(tree.getProperty("slot3", 3))));
```

**Change 3** - Fix second jlimit block for backwards compatibility (lines ~165):
```cpp
// OLD:
assignment[i] = static_cast<EffectID>(juce::jlimit(0, 8, effectID));

// NEW:
assignment[i] = static_cast<EffectID>(juce::jlimit(0, 12, effectID));
```

### 2. Fix debug code in `PluginEditor.cpp`

**File**: `source/PluginEditor.cpp` (line ~4523)

Fix the getItemText call to use 0-based index instead of 1-based ID:
```cpp
// OLD:
DBG("[UI] Last item (ID 12): " << selector->getItemText(12));

// NEW:
DBG("[UI] Last item (index 11): " << selector->getItemText(11));
```

## Expected Result
After these changes, Saturate will:
- Pass router validation checks
- Serialize/deserialize correctly
- Appear in the effect selector dropdown as the 12th item
- Be selectable and functional like all other effects

## Testing
1. Rebuild the plugin
2. Open standalone app
3. Click any effect selector dropdown
4. Verify "Saturate" appears at the bottom of the list (12th item)
5. Select Saturate and verify it loads correctly


### To-dos

- [ ] Update FormantProcessor.h to use IIR biquad filters instead of StateVariableTPT
- [ ] Add formant bandwidth constants (F1: 110Hz, F2: 90Hz, F3: 150Hz, F4: 200Hz)
- [ ] Remove bankA/bankB and LFO variables, replace with single filter bank
- [ ] Rewrite prepare() to initialize IIR biquad filters
- [ ] Add helper method to calculate RBJ bandpass coefficients from freq/bandwidth
- [ ] Rewrite process() with single-bank coefficient interpolation approach
- [ ] Implement vowel morphing via coefficient interpolation (not gain crossfade)
- [ ] Apply proper per-formant scaling and emphasis/brightness gains
- [ ] Build and test AU with sawtooth, noise, and drums