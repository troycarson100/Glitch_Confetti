<!-- f8815ee8-e613-4d71-b763-a13c63933429 c1cf0380-a62e-479e-aa90-0a01eda224ae -->
# Fix Form 2 UI Implementation

## Critical Issues Identified

The Form 2 UI has several structural problems that prevent it from working like other effect pages:

1. **Wrong sequencer area bounds** - Using `(25, 410, 413, 156)` instead of `(25, 374, 413, 140)`
2. **Missing snapshot update in knob callbacks** - Knobs don't call `updateForm2CurrentStepSnapshot()`
3. **Missing "All Steps" logic in knob callbacks** - No code to update all 16 steps when toggle is active
4. **Wrong power button positioning** - Effects and step power buttons are incorrectly placed
5. **Missing step button save/load** - Step buttons don't save current step before switching
6. **Syntax error in form2Group** - Line 11744 missing semicolon after `form2Knobs[i])`
7. **Missing sequencer UI elements** - Step dice, rate dropdown, STD toggle callbacks not implemented
8. **Wrong "All Steps" toggle position** - Should be in effect area, not step area

## Files to Modify

### `source/PluginEditor.cpp`

**1. Fix `setupForm2Knobs()` - Add snapshot update callbacks (lines 11526-11550)**

Current knob callback only updates UI labels. Need to add:
- Call to `processorRef.updateForm2CurrentStepSnapshot(i, value)`
- "All Steps" logic that updates all 16 step snapshots when toggle is active
- Match PhaseBloom pattern exactly (lines 13061-13154)

**2. Fix `setupForm2EffectsArea()` - Power button positioning (lines 11563-11613)**

Current: `form2FxPowerButton->setBounds(effectArea.getX() + 10, effectArea.getY() + 38, 50, 50);`

Should match PhaseBloom (line 13186):
```cpp
const int buttonSize = 46;
form2FxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                             effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
```

Also need to add dice button setup with proper positioning and assets.

**3. Fix `setupForm2SequencerArea()` - Complete rewrite (lines 11615-11701)**

Current issues:
- Wrong bounds: `auto stepArea = juce::Rectangle<int>(25, 410, 413, 156);`
- Should be: `auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);`
- Missing proper step button grid (should be 2x8 with 40px buttons, 8px spacing)
- Missing step amount TextEditor with callbacks
- Missing rate dropdown with 8 items and proper styling
- Missing STD toggle with cycling behavior
- Missing step dice button with randomization logic
- Wrong step power button positioning

Match PhaseBloom pattern exactly (lines 13221-13350):
- Step title: "STEP" (not "FORM 2 MOD")
- Proper step button grid with onClick handlers
- TextEditor for step amount (not Label)
- Rate dropdown with transparent styling
- STD toggle with "-" initial text
- Step dice button with proper callback
- Step power button at top-right corner

**4. Fix `setupForm2AllStepsToggle()` - Move to effect area (lines 11703-11766)**

Current position: `stepArea.getX() + 380, stepArea.getY() + 10`

Should match PhaseBloom (line 9939):
```cpp
auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
form2AllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                               effectArea.getY() - 1, buttonSize, buttonSize);
```

Also fix label position and font (should be 14.4f bold, not 10.0f plain).

**5. Fix `form2Group` population (line 11744)**

Current: `if (form2Knobs[i]) form2Group.push_back(form2Knobs[i].get());`

Missing semicolon - should be:
```cpp
if (form2Knobs[i]) form2Group.push_back(form2Knobs[i].get());
```

**6. Add `onForm2StepButtonClicked()` handler**

Create new method matching PhaseBloom pattern (lines 13693-13712):
- Save current step snapshot before switching (if not "All Steps" mode)
- Update `form2UiSelectedStep`
- Call `processorRef.setForm2SelectedStep()`
- Load new step snapshot into all 8 knobs
- Call `updateForm2SequencerUI()`

**7. Implement `updateForm2SequencerUI()`**

Match PhaseBloom pattern (lines 13669-13691):
- Update step button selected/playing/enabled states
- Update step amount display
- Call repaint()

**8. Add Form 2 cases to unified methods**

Add `case FxPageID::Form2:` to:
- `randomizeEffectStepSnapshot()` - randomize all 8 parameters
- `loadSelectedStepIntoKnobs()` - load 8 parameters from snapshot
- `saveCurrentStepSnapshot()` - save 8 parameters to snapshot
- `updateSnapshotValue()` - update single parameter in snapshot
- `getParameterIdForKnob()` - return correct parameter ID

### `source/PluginProcessor.cpp`

**9. Verify Form 2 snapshot methods exist**

Ensure these are implemented:
- `getForm2SafeSnapshot(int step)`
- `setForm2StepSnapshot(int step, StepSnapshot snapshot)`
- `updateForm2CurrentStepSnapshot(int knobIndex, float value)`
- `setForm2StepsUsed(int steps)`
- `setForm2DivisionIndex(int index)`
- `setForm2StdMode(int mode)`
- `setForm2SelectedStep(int step)`
- `setForm2SequencerEnabled(bool enabled)`
- `getForm2CurrentStep()`
- `getForm2SeqState()` (non-const)

## Implementation Order

1. Fix sequencer area bounds in `setupForm2SequencerArea()`
2. Rewrite sequencer area to match PhaseBloom exactly
3. Fix power button positions in effects area
4. Move "All Steps" toggle to effect area
5. Add snapshot update logic to knob callbacks
6. Add "All Steps" logic to knob callbacks
7. Implement `onForm2StepButtonClicked()` handler
8. Implement `updateForm2SequencerUI()`
9. Fix `form2Group` syntax error
10. Add Form 2 cases to unified methods
11. Verify processor methods exist
12. Build and test standalone
13. Update AU plugin

## Key Patterns to Follow

**Sequencer Area Bounds**: Always `juce::Rectangle<int>(25, 374, 413, 140)`

**Step Button Grid**: 40px buttons, 8px spacing, 2 rows of 8, starting at `sequencerArea.getX() + 20, sequencerArea.getY() + 35`

**Power Button Position**: Effects power at top-right, step power at top-right of sequencer

**"All Steps" Toggle**: In effect area, centered horizontally with +30px offset

**Knob Callbacks**: Must call `updateCurrentStepSnapshot()` and handle "All Steps" mode

**Step Button Click**: Save current step (if not "All Steps"), load new step, update UI


### To-dos

- [ ] Reduce formant knobs from 8 to 4 (Vowel, Resonance, Intensity, Mix) in setupFormantKnobs()
- [ ] Change DSP from bandpass to peaking filters and remove LFO vibrato
- [ ] Update APVTS parameters and step snapshot structure to match 4-knob design
- [ ] Remove drag interaction from visualization, keep as read-only formant position display
- [ ] Test each vowel (A/E/I/O/U) to verify clear vowel character without phaser artifacts