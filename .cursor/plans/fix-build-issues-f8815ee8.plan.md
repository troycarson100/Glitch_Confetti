<!-- f8815ee8-e613-4d71-b763-a13c63933429 1388a70b-043f-40b8-80c0-c252e5c03533 -->
# Fix Space Delay Sync Mode Knob Issue

## Problem Analysis
The Space Delay time knob in sync mode (0-19 range for 20 divisions) keeps snapping back to 1/64 when released. Root causes:

1. **Timer Callback Interference**: The `timerCallback()` function (lines 793-842) runs every 16ms and reads from `getParameters().getUnchecked(0)` which is the `timeMs` parameter, not `delayTimeDiv`. Even though line 807 has a guard, there may be edge cases or timing issues.

2. **Outdated Label Logic**: Lines 821-834 use old division display logic that assumes 8 divisions and a 0-1 knob range, incompatible with our new 0-19 range.

3. **Parameter Confusion**: The knob index 0 is tied to `timeMs` parameter in `getParameters()` array, but in sync mode we want it to control `delayTimeDiv` parameter instead.

## Solution

### 1. Update Timer Callback (lines 793-842 in `PluginEditor.cpp`)
- **Fix the guard condition** on line 807 to be more robust
- **Skip ALL timer updates** for knob 0 when in sync mode (not just setValue, but also label updates)
- **Call `updateSpaceDelayTimeLabel()`** instead of the old division logic (lines 821-834)

Replace lines 795-840 with:
```cpp
for (int i = 0; i < 8; ++i)
{
    if (knobs[i] != nullptr && i < processorRef.getParameters().size())
    {
        // Special handling for Time knob in sync mode
        if (i == 0 && timeSyncEnabled) {
            // In sync mode, skip timer-based updates entirely
            // The knob is controlled by delayTimeDiv parameter, not timeMs
            // Label is updated by updateSpaceDelayTimeLabel()
            continue;
        }
        
        auto* param = processorRef.getParameters().getUnchecked(i);
        float paramValue = param->getValue();
        
        // Only update knob value if it's not currently being dragged
        if (!knobs[i]->isMouseButtonDown())
        {
            knobs[i]->setValue(paramValue, juce::dontSendNotification);
        }
        
        // Update value label
        if (valueLabels[i] != nullptr)
        {
            // Format value based on parameter type
            juce::String valueText;
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
            {
                float actualValue = floatParam->convertFrom0to1(paramValue);
                if (i == 0) {
                    // Time knob in non-sync mode
                    valueText = juce::String(actualValue, 0) + "ms";
                } else if (i == 5 || i == 6) {
                    // Hi-Cut, Low-Cut - show in Hz
                    valueText = juce::String((int) std::round(actualValue)) + "Hz";
                } else {
                    // Other knobs show percentage
                    valueText = juce::String((int) std::round(actualValue * 100));
                }
            }
            valueLabels[i]->setText(valueText, juce::dontSendNotification);
        }
        
        // Update indicator bar
        if (indicatorBars[i] != nullptr)
        {
            indicatorBars[i]->setValue(paramValue);
        }
    }
}
```

### 2. Verify Guard Condition in onValueChange (line 2103)
The condition `if (i != 0 || !timeSyncEnabled)` should prevent calling `updateParameterFromKnob(0)` when in sync mode. This is correct but add a debug log to confirm:

```cpp
// Always update parameter from knob for non-sync knobs or after sync handling
if (i != 0 || !timeSyncEnabled) {
    updateParameterFromKnob(i);
} else {
    DBG("[SYNC] Skipping updateParameterFromKnob for knob 0 in sync mode");
}
```

### 3. Add Defensive Check in updateParameterFromKnob (line 4054)
Add a safety check to prevent updating `timeMs` parameter when in sync mode:

```cpp
void PluginEditor::updateParameterFromKnob(int knobIndex)
{
    // Safety: never update timeMs parameter (index 0) when in sync mode
    if (knobIndex == 0 && timeSyncEnabled) {
        DBG("[SYNC] Blocked updateParameterFromKnob for knob 0 in sync mode");
        return;
    }
    
    if (knobIndex >= 0 && knobIndex < 8 && knobs[knobIndex] != nullptr && knobIndex < processorRef.getParameters().size())
    {
        // ... rest of existing code
    }
}
```

## Files to Modify
- `source/PluginEditor.cpp`:
  - Timer callback function (~lines 793-842)
  - onValueChange lambda (~line 2103)
  - updateParameterFromKnob function (~line 4054)


### To-dos

- [ ] Create source/ui/pages/ directory for effect page files
- [ ] Create EffectPageBase.h/.cpp with common effect page interface and shared functionality
- [ ] Extract Space Delay page as first template - move all Space Delay code to SpaceDelayPage.h/.cpp
- [ ] Test that Space Delay works correctly after extraction
- [ ] Extract AutoPan page following Space Delay pattern
- [ ] Extract Dirt page following Space Delay pattern
- [ ] Extract Chorus page following Space Delay pattern
- [ ] Extract Reverb page following Space Delay pattern
- [ ] Extract Granular page following Space Delay pattern
- [ ] Extract Slicer page following Space Delay pattern
- [ ] Extract Dub Delay page following Space Delay pattern
- [ ] Extract Redux page following Space Delay pattern
- [ ] Extract PhaseBloom page following Space Delay pattern
- [ ] Extract COMPRESS+ page following Space Delay pattern
- [ ] Create MasterSection.h/.cpp and move all master area code
- [ ] Create SequencerSection, PowerButtonManager, and VisibilityManager utility classes
- [ ] Update CMakeLists.txt to include all new source files
- [ ] Test all effects and functionality work correctly after refactoring
- [ ] Verify that the refactored code builds successfully for Standalone and AU