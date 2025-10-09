# Effect Page Implementation Template

## Critical Fixes Discovered During Chorus Implementation

When implementing a new effect page, follow this template to avoid the issues we encountered with the Chorus page.

### 1. Lock Button Setup (CRITICAL)

**❌ WRONG - Causes white circles:**
```cpp
chorusLockButtons[i]->setBounds(x + knobSize - 8, y - 8, 16, 16); // Wrong size and position
// Missing image setup
```

**✅ CORRECT - Matches Dirt page:**
```cpp
const int diceSize = 10; // NOT 16x16!
const int diceSpacing = 5;
juce::Font labelFont(12.0f, juce::Font::bold);
int textWidth = labelFont.getStringWidth(chorusKnobNames[i]);
int lockX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
int lockY = y - 10;
chorusLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);

// CRITICAL: Set images to prevent white circles
if (assets.unlockedIcon && assets.lockedIcon) {
    auto imgUnlocked = assets.unlockedIcon->createCopy();
    auto imgLocked = assets.lockedIcon->createCopy();
    chorusLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
}
```

### 2. All Steps Toggle Setup (CRITICAL)

**❌ WRONG - Wrong position and missing images:**
```cpp
chorusAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth() - 70, 
                                effectArea.getY() + effectArea.getHeight() - 35, 24, 24);
// Missing image setup
```

**✅ CORRECT - Matches Dirt page:**
```cpp
const int buttonSize = 29;
chorusAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                effectArea.getY() - 1, buttonSize, buttonSize);

// CRITICAL: Set proper images
if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr) {
    static_cast<AllStepsToggleButton*>(chorusAllStepsToggle.get())->setImages(
        assets.stepTopInactive->createCopy(),
        assets.stepTopActive->createCopy()
    );
}
```

### 3. All Steps Label Setup (CRITICAL)

**❌ WRONG - Wrong position and justification:**
```cpp
chorusAllStepsLabel->setJustificationType(juce::Justification::centred);
chorusAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 - 40, 
                               effectArea.getY() - 1 + buttonSize + 2, 80, 20);
```

**✅ CORRECT - To the right of toggle button:**
```cpp
chorusAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
chorusAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
chorusAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                               effectArea.getY() + 1, 80, 24);
```

### 4. Step Amount TextEditor Setup (CRITICAL)

**❌ WRONG - Using Label:**
```cpp
chorusStepAmountLabel = std::make_unique<juce::Label>(); // Can't edit!
```

**✅ CORRECT - Using TextEditor with full configuration:**
```cpp
chorusStepAmountLabel = std::make_unique<juce::TextEditor>();
chorusStepAmountLabel->setText("16");
chorusStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
chorusStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
chorusStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
chorusStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
chorusStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
chorusStepAmountLabel->setJustification(juce::Justification::centred);
chorusStepAmountLabel->setBorder(juce::BorderSize<int>(2));
chorusStepAmountLabel->setIndents(0, 0);
chorusStepAmountLabel->setInputRestrictions(2, "0123456789");
chorusStepAmountLabel->setWantsKeyboardFocus(true);
chorusStepAmountLabel->setMouseClickGrabsKeyboardFocus(true);
chorusStepAmountLabel->setCaretVisible(true);
chorusStepAmountLabel->setPopupMenuEnabled(false);
chorusStepAmountLabel->setScrollbarsShown(false);
chorusStepAmountLabel->setMultiLine(false);
chorusStepAmountLabel->setReturnKeyStartsNewLine(false);
chorusStepAmountLabel->setInterceptsMouseClicks(true, false);
chorusStepAmountLabel->setAlwaysOnTop(true);
chorusStepAmountLabel->onReturnKey = [this]() {
    int value = juce::jlimit(1, 16, chorusStepAmountLabel->getText().getIntValue());
    processorRef.setChorusStepsUsed(value);
    chorusStepAmountLabel->setText(juce::String(value), false);
    updateChorusSequencerUI();
    chorusStepAmountLabel->giveAwayKeyboardFocus();
};
chorusStepAmountLabel->onFocusLost = [this]() {
    int value = juce::jlimit(1, 16, chorusStepAmountLabel->getText().getIntValue());
    processorRef.setChorusStepsUsed(value);
    chorusStepAmountLabel->setText(juce::String(value), false);
    updateChorusSequencerUI();
};
```

### 5. Power Button Initialization (CRITICAL)

**❌ WRONG - Forcing to true:**
```cpp
// In showPage()
chorusFxAreaEnabled = true; // Forces parameter regardless of actual state
```

**✅ CORRECT - Reading from parameter:**
```cpp
// In constructor
auto* chorusEnabledParam = processorRef.getAPVTS().getRawParameterValue("chorusEnabled");
if (chorusEnabledParam) {
    chorusFxAreaEnabled = chorusEnabledParam->load() > 0.5f;
    if (chorusFxPowerButton) {
        chorusFxPowerButton->setToggleState(chorusFxAreaEnabled, juce::dontSendNotification);
    }
    updateChorusFxAreaVisibility();
}
```

### 6. Timer Update Protection (CRITICAL)

**❌ WRONG - Overwrites user input:**
```cpp
chorusStepAmountLabel->setText(juce::String(stepsUsed), false); // Always overwrites!
```

**✅ CORRECT - Respects user editing:**
```cpp
// Update step amount display (don't overwrite if user is editing)
if (chorusStepAmountLabel != nullptr && !chorusStepAmountLabel->hasKeyboardFocus(true)) {
    chorusStepAmountLabel->setText(juce::String(stepsUsed), false);
}
```

### 7. KeyPressed Override (CRITICAL)

**❌ WRONG - Missing TextEditor support:**
```cpp
// No keyPressed override - TextEditors can't receive input
```

**✅ CORRECT - Allow TextEditor input:**
```cpp
bool PluginEditor::keyPressed(const juce::KeyPress& key) override {
    // Allow keyboard input to step amount TextEditors
    if (delayStepAmountLabel && delayStepAmountLabel->hasKeyboardFocus(true)) return false;
    if (autopanStepAmountLabel && autopanStepAmountLabel->hasKeyboardFocus(true)) return false;
    if (dirtStepAmountLabel && dirtStepAmountLabel->hasKeyboardFocus(true)) return false;
    if (chorusStepAmountLabel && chorusStepAmountLabel->hasKeyboardFocus(true)) return false;
    return true;
}
```

## Complete Template for New Effect Pages

### 1. DSP Backend
- Create `source/dsp/Fx[Name].h` with `prepare()`, `setTargets()`, `process()` methods
- Use `juce::SmoothedValue` for all parameters (30ms smoothing)
- Add to `PluginProcessor.h` includes and as member variable

### 2. Parameters
- Add 8 `AudioParameterFloat` params with IDs: `[name]Param1`, `[name]Param2`, etc.
- Add `AudioParameterBool` for `"[name]Enabled"` (default `true`)
- Update `"currentPage"` Choice parameter to include new page

### 3. StepSnapshot
- Add struct with all 8 parameters and defaults in `StepSnapshot.h`
- Name it lowercase matching the effect name

### 4. PluginProcessor Integration
- Add `SeqState [name]Seq`, `atomic<int> [name]UiSelectedStep`
- Add `array<StepSnapshot,16> [name]StepSnapshots`
- Initialize sequencer in constructor (enabled=false, stepsUsed=16, divisionIndex=5)
- Initialize snapshots with musical defaults
- Add accessor methods for sequencer and snapshots
- Call `[name]Seq.prepare(sampleRate)` in `prepareToPlay`
- Add `[name].prepare()` call
- In `processBlock`: wrap entire effect processing in `if ([name]Enabled)` check
- Check `[name]Seq.enabled.load() && [name]Seq.active.load()` before using snapshots
- Add sequencer activation, lock-in, and stepping logic

### 5. UI Components (PluginEditor.h)
- Add `FxPageID::[Name]` to enum
- Declare all arrays and components following the pattern
- Declare helper methods

### 6. Assets
- Add `[Name]_Background_Tab[N].svg` and `[Name]_Icon.svg` to `assets/ui/`
- Add to `UiAssets` struct and `loadAll()` in `Assets.cpp`

### 7. UI Implementation (PluginEditor.cpp)
- Follow the CRITICAL FIXES above for all components
- Use exact positioning formulas from working pages
- Ensure proper image setup for all buttons
- Initialize power button state from parameters
- Protect TextEditor updates from timer overwrites

## Key Lessons Learned

1. **Lock buttons MUST be 10x10 size** and positioned relative to label text width
2. **All Steps toggle MUST use proper images** and center positioning
3. **All Steps label MUST be positioned to the right** of toggle button
4. **Step amount MUST use TextEditor** (not Label) with full keyboard configuration
5. **Power buttons MUST be initialized from parameters**, not forced to true
6. **Lock button images MUST be set during setup** to prevent white circles
7. **UI state MUST be synchronized with DSP parameters** on startup
8. **Timer updates MUST respect user editing** of TextEditors
9. **KeyPressed MUST be overridden** to allow TextEditor input

Following this template will ensure new effect pages work correctly from the start without requiring multiple rounds of fixes.
