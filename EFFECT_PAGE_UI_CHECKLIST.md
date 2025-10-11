# Effect Page UI/UX Consistency Checklist

This checklist documents all the critical patterns learned from implementing the Slicer page. Follow these EXACTLY when adding new effect pages to ensure perfect consistency.

---

## 📐 Critical UI Patterns

### 1. Effect Title Label
```cpp
effectTitle = std::make_unique<juce::Label>();
effectTitle->setText("EFFECT", juce::dontSendNotification);  // ⚠️ ALWAYS "EFFECT", never the effect name!
effectTitle->setFont(juce::Font(27.648f, juce::Font::bold));
effectTitle->setColour(juce::Label::textColourId, juce::Colours::white);
effectTitle->setJustificationType(juce::Justification::centredLeft);
effectTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);  // ⚠️ X+10, Y+5 (NOT X+12, Y+10!)
```

**Common mistakes:**
- ❌ setText("SLICER") → ✅ setText("EFFECT")
- ❌ setBounds(X+12, Y+10, 250, 30) → ✅ setBounds(X+10, Y+5, 100, 30)

---

### 2. Step Dice Button
```cpp
// ⚠️ Declaration in header:
std::unique_ptr<CustomDiceButton> stepDiceButton;  // NOT DrawableButton!

// Setup in constructor:
stepDiceButton = std::make_unique<CustomDiceButton>();  // ⚠️ CustomDiceButton!
stepDiceButton->setVisible(false);
int stepDiceSize = static_cast<int>(35 * 0.7);  // ⚠️ 30% smaller = ~24px (NOT full 35px!)
stepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, stepDiceSize, stepDiceSize);

if (assets.diceLarge != nullptr) {
    stepDiceButton->setDiceImage(assets.diceLarge->createCopy());  // ⚠️ setDiceImage(), NOT setImages()!
}
```

**Common mistakes:**
- ❌ `DrawableButton` → ✅ `CustomDiceButton`
- ❌ `setImages()` → ✅ `setDiceImage()`
- ❌ Size = 35 → ✅ Size = 35 * 0.7 (~24px)

---

### 3. Array Sizes (Critical for Crash Prevention!)

If you have **N knobs**, ALL arrays must be size **N**:

```cpp
// In PluginEditor.h:
std::array<std::unique_ptr<CustomKnob>, N> knobs;
std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, N> attachments;
std::array<std::unique_ptr<juce::Label>, N> knobLabels;
std::array<std::unique_ptr<juce::Label>, N> valueLabels;
std::array<std::unique_ptr<IndicatorBar>, N> indicatorBars;

// ALL loops must use N:
for (int i = 0; i < N; ++i) { ... }  // ⚠️ NOT hardcoded 8!

// ALL bounds checks must use N:
if (knobIndex < 0 || knobIndex >= N || !knobs[knobIndex]) return;  // ⚠️ NOT >= 8!
```

**Example**: "6 knobs" means:
- Arrays: `std::array<..., 6>`
- Loops: `for (int i = 0; i < 6; ++i)`
- Bounds: `knobIndex >= 6`

**Crash locations to check**:
- `setupEffectKnobs()` loop
- `updateEffectFxAreaVisibility()` loop  
- `showPage()` - two loops for initial value updates
- `updateEffectParameterFromKnob()` bounds check
- `randomizeIndividualEffectKnob()` bounds check

---

### 4. Parameter ID Arrays

```cpp
std::vector<juce::String> paramIds = {
    "effectParam1", "effectParam2", "effectParam3", ...
    // ⚠️ MUST match EXACT APVTS parameter names!
};
```

**Common mistake:**
```cpp
// ❌ Using old/wrong parameter name:
{"slicerPattern", "slicerDivision", "slicerOffset", "slicerShape", "slicerSwing", "slicerMix"}

// ✅ Using correct parameter name after change:
{"slicerPattern", "slicerDivision", "slicerOffset", "slicerShape", "slicerReleaseMs", "slicerMix"}
```

**Where this array appears:**
- `setupEffectKnobs()` for attachments
- `updateEffectParameterFromKnob()` for APVTS updates

---

## 🔄 Sequencer Integration

### Required in PluginProcessor.cpp

#### A. Play Edge Activation (around line 448-483)
```cpp
// Granular sequencer activates if enabled
if (granularSeq.enabled.load()) {
    granularSeq.active.store(true);
    DBG("[GRANULAR SEQ] ✓ Activated on play edge");
}

// ⚠️ ADD YOUR EFFECT HERE:
if (effectSeq.enabled.load()) {
    effectSeq.active.store(true);
    DBG("[EFFECT SEQ] ✓ Activated on play edge");
}
```

#### B. Play Edge Reset (around line 436-442)
```cpp
seq.resetPhase();
autopanSeq.resetPhase();
dirtSeq.resetPhase();
chorusSeq.resetPhase();
reverbSeq.resetPhase();
granularSeq.resetPhase();
effectSeq.resetPhase();  // ⚠️ ADD YOUR EFFECT HERE
```

#### C. PPQ Lock-In (around line 530-543)
```cpp
if (granularSeq.enabled.load() && granularSeq.active.load()) {
    const int granularStep = granularSeq.computeStepFromPPQ(ppq);
    granularSeq.currentStep.store(granularStep);
    granularSeq.playingStep.store(granularStep);
    DBG("[GRANULAR SEQ] Lock-in at PPQ=" << ppq << " -> step " << granularStep);
}

// ⚠️ ADD YOUR EFFECT HERE:
if (effectSeq.enabled.load() && effectSeq.active.load()) {
    const int effectStep = effectSeq.computeStepFromPPQ(ppq);
    effectSeq.currentStep.store(effectStep);
    effectSeq.playingStep.store(effectStep);
    DBG("[EFFECT SEQ] Lock-in at PPQ=" << ppq << " -> step " << effectStep);
}
```

#### D. Transport Stepping (around line 620-627)
```cpp
// Slicer sequencer stepping (shares same PPQ/transport, independent timing)
if (isPlaying && ppqValid && slicerSeq.active.load()) {
    const int slicerStep = slicerSeq.computeStepFromPPQ(ppq);
    if (slicerStep != slicerSeq.currentStep.load()) {
        slicerSeq.currentStep.store(slicerStep);
        slicerSeq.playingStep.store(slicerStep);
        DBG("[SLICER SEQ] ★ Step changed to: " << slicerStep << " PPQ: " << ppq);
    }
}

// ⚠️ ADD YOUR EFFECT SIMILARLY
```

---

### Required in PluginEditor.cpp

#### Step Power Button onClick:
```cpp
effectStepPowerButton->onClick = [this]() {
    effectStepAreaEnabled = effectStepPowerButton->getToggleState();
    processorRef.setEffectSequencerEnabled(effectStepAreaEnabled);  // ⚠️ MUST call this!
    updateEffectStepAreaVisibility();
    DBG("[UI] Effect step power: " << (effectStepAreaEnabled ? "ON" : "OFF"));
};
```

**Common mistake:** Not calling `setEffectSequencerEnabled()` → sequencer UI and DSP get out of sync

---

### Required in RandomizationManager.cpp

#### A. applyParamChanges() - Reload Knobs
```cpp
case EffectID::Slicer:
{
    int step = editor->slicerUiSelectedStep;
    if (step >= 0 && step < 16) {
        auto s = processor.getSlicerSafeSnapshot(step);
        // ⚠️ Load ALL knobs (use correct count!)
        if (editor->slicerKnobs[0]) editor->slicerKnobs[0]->setValue(s.slicer.pattern, juce::sendNotification);
        if (editor->slicerKnobs[1]) editor->slicerKnobs[1]->setValue(s.slicer.division, juce::sendNotification);
        // ... etc for all N knobs
        DBG("[RAND]   Slicer step " + juce::String(step) + " reloaded");
    }
    break;
}
```

#### B. applyStepChanges() - Randomize Snapshots
```cpp
case EffectID::Slicer:
{
    auto snapshot = processor.getSlicerSafeSnapshot(target.stepIndex);
    snapshot.slicer.pattern = std::floor(rand01() * 8.0f);
    snapshot.slicer.division = std::floor(rand01() * 6.0f);
    // ... randomize all parameters
    processor.setSlicerStepSnapshot(target.stepIndex, snapshot);
    break;
}
```

**Common mistake:** Forgetting to add the effect → master dice doesn't randomize it

---

## 📊 Standard Dimensions

### Effect Area
```cpp
auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);  // ⚠️ Always the same
```

### Sequencer Area
```cpp
auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);  // ⚠️ Always the same
```

### Knob Grid
```cpp
const int knobSize = 80;
const int knobSpacing = 20;
const int startX = effectArea.getX() + 15;
const int startY = effectArea.getY() + effectArea.getHeight() - 210;

// Position (4x2 grid):
int x = startX + (i % 4) * (knobSize + knobSpacing);
int y = startY + (i / 4) * (knobSize + 20);

// Y-offset for visual alignment:
if (i < 4) y -= 23;  // Top row
else y -= 1;         // Bottom row
```

---

## ✅ Pre-Launch Verification Checklist

Before considering a new effect page complete, verify:

### UI/Layout
- [ ] Effect title says "EFFECT" (not effect name)
- [ ] Effect title at (X+10, Y+5, 100x30)
- [ ] Step dice is CustomDiceButton (not DrawableButton)
- [ ] Step dice size is 35*0.7 (~24px)
- [ ] Step dice position is (X+75, Y+5)

### Arrays & Bounds
- [ ] All arrays match actual knob count N
- [ ] No hardcoded loop limits (i<8 when N=6)
- [ ] All bounds checks use >= N (not >= 8)
- [ ] showPage() has TWO loops - both use correct N
- [ ] updateFxAreaVisibility() uses correct N

### Parameters
- [ ] Parameter ID array matches APVTS names
- [ ] Default values match in all 4 places:
  - APVTS createParameterLayout()
  - StepSnapshot.h struct defaults
  - UI knob setValue() in setup
  - Fallback values in ternary operators

### Sequencer Integration
- [ ] Added to play edge activation block
- [ ] Added to resetPhase() call on play edge
- [ ] Added to PPQ lock-in block
- [ ] Added to transport stepping section
- [ ] Step power onClick calls setEffectSequencerEnabled()
- [ ] Added to RandomizationManager::applyParamChanges()
- [ ] Added to RandomizationManager::applyStepChanges()

### Testing
- [ ] Plugin loads without crashing
- [ ] Tab click doesn't crash
- [ ] All knobs work (no silent knobs)
- [ ] Sequencer starts at step 0 when play is pressed
- [ ] Sequencer stops when power button clicked
- [ ] Master dice randomizes this effect
- [ ] Step dice randomizes all 16 steps
- [ ] Per-step snapshots save and recall correctly

---

## 🚨 Most Common Mistakes (From Slicer Implementation)

1. **Step Dice Button Type**: Used `DrawableButton` instead of `CustomDiceButton` → wrong rendering
2. **Step Dice Size**: Used 35px instead of `35 * 0.7` → too large
3. **Effect Title Text**: Used effect name instead of "EFFECT" → inconsistent
4. **Effect Title Position**: Used (X+12, Y+10) instead of (X+10, Y+5) → misaligned
5. **Array Size Mismatch**: Declared arrays[8] but only created 6 knobs → crashes accessing [6] and [7]
6. **Loop Count Mismatch**: Used `i < 8` in showPage() loops when only 6 knobs exist → crash
7. **Bounds Check Wrong**: Used `>= 8` when should be `>= 6` → crash
8. **Parameter ID Wrong**: Used "slicerSwing" when param renamed to "slicerReleaseMs" → knob didn't work
9. **Missing Sequencer Sync**: Didn't call `setEffectSequencerEnabled()` → sequencer didn't turn off
10. **Missing Reset**: Didn't add to resetPhase() → sequencer started at random step
11. **Missing Randomization**: Didn't add to RandomizationManager → master dice didn't work
12. **Default Value Mismatch**: APVTS said 0.5, StepSnapshot said 0.35 → inconsistent behavior

---

## 📝 Quick Copy-Paste Templates

### Effect Title (setupEffectEffectsArea)
```cpp
effectTitle = std::make_unique<juce::Label>();
effectTitle->setText("EFFECT", juce::dontSendNotification);
effectTitle->setFont(juce::Font(27.648f, juce::Font::bold));
effectTitle->setColour(juce::Label::textColourId, juce::Colours::white);
effectTitle->setJustificationType(juce::Justification::centredLeft);
addAndMakeVisible(effectTitle.get());
effectTitle->setVisible(false);
effectTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
```

### Step Dice Button (setupEffectSequencerArea)
```cpp
effectStepDiceButton = std::make_unique<CustomDiceButton>();
addAndMakeVisible(effectStepDiceButton.get());
effectStepDiceButton->setVisible(false);
int effectStepDiceSize = static_cast<int>(35 * 0.7);
effectStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, effectStepDiceSize, effectStepDiceSize);

if (assets.diceLarge != nullptr) {
    effectStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
}
```

### Step Power Button onClick
```cpp
effectStepPowerButton->onClick = [this]() {
    effectStepAreaEnabled = effectStepPowerButton->getToggleState();
    processorRef.setEffectSequencerEnabled(effectStepAreaEnabled);  // ⚠️ CRITICAL!
    updateEffectStepAreaVisibility();
    DBG("[UI] Effect step power: " << (effectStepAreaEnabled ? "ON" : "OFF"));
};
```

---

## 🎯 Reference: AutoPan Page (Gold Standard)

AutoPan is implemented correctly. When in doubt, copy patterns from AutoPan:
- Effect title: Line ~3806-3813
- Step dice: Line ~4102-4112
- Step power: Search for "autopanStepPowerButton->onClick"
- Array sizes: All use 6 (AutoPan has 6 knobs)
- Sequencer integration: All present in PluginProcessor.cpp

---

## 💾 Always Match These Values

| Element | Value | Location |
|---------|-------|----------|
| Effect Area | `(25, 54, 413, 296)` | Always |
| Sequencer Area | `(25, 374, 413, 140)` | Always |
| Effect Title Font | `27.648f, bold` | Always |
| Effect Title Text | `"EFFECT"` | Always |
| Effect Title Bounds | `(X+10, Y+5, 100, 30)` | Always |
| Step Dice Type | `CustomDiceButton` | Always |
| Step Dice Size | `35 * 0.7` (~24px) | Always |
| Step Dice Position | `(X+75, Y+5, size, size)` | Always |
| Step Dice Method | `setDiceImage()` | Always |
| Knob Size | `80` | Always |
| Knob Spacing | `20` | Always |
| Knob Start X | `effectArea.getX() + 15` | Always |
| Knob Start Y | `effectArea.getY() + height - 210` | Always |

---

## 🔍 Where to Search for Issues

If a new effect page has problems, check these locations:

### Crashes:
1. Array declarations in header (match N!)
2. setupEffectKnobs() loop (use N!)
3. showPage() - TWO loops (both use N!)
4. updateEffectFxAreaVisibility() loop (use N!)
5. updateEffectParameterFromKnob() bounds check (use >= N!)
6. randomizeIndividualEffectKnob() bounds check (use >= N!)

### UI Misalignment:
1. Effect title text/position in setupEffectEffectsArea()
2. Step dice type/size/position in setupEffectSequencerArea()

### Sequencer Not Working:
1. Play edge activation (PluginProcessor.cpp ~line 475)
2. resetPhase() call (~line 442)
3. PPQ lock-in (~line 540)
4. Transport stepping (~line 620)
5. Step power onClick in PluginEditor.cpp

### Master Dice Not Working:
1. RandomizationManager::applyParamChanges() - missing case
2. RandomizationManager::applyStepChanges() - missing case

### Parameters Not Working:
1. Parameter ID array has wrong names
2. Default values inconsistent across 4 locations
3. Snapshot reading in processBlock() not checking seqActive

---

Use this checklist for every new effect page to ensure perfect consistency! ✅

