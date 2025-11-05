# Saturate Effect Page Implementation Plan

## Overview
Create a new "Saturate" effect page that reuses the Dub Delay page UI scaffold exactly. The page will feature 8 Airwindows-style saturation models with dynamic knob labels that change based on the selected Type.

## Key Requirements
- **UI**: Exact duplicate of Dub Delay layout (8 knobs, step sequencer, power buttons, snapshots, All Steps button)
- **DSP**: 8 saturation models with strategy pattern
- **Dynamic Labels**: Knobs 2-5 change labels/ranges based on `satType` selection
- **Oversampling**: JUCE oversampling (1×/2×/4×/8×) integrated
- **CPU Target**: <1%/stereo @48k with 4× OS on Apple M-class

## Implementation Steps

### 1. DSP Layer

#### 1.1 Create SaturateProcessor Header (`source/dsp/SaturateProcessor.h`)
- Define interface `ISat` with `process(float x)` method
- Create 8 concrete classes:
  - `SatSpiral2` - sweet analog-ish drive
  - `SatDensity2` - thick transformer/tape-ish
  - `SatDrive` - classic diode-ish
  - `SatPurestDrive` - ultra-clean saturation
  - `SatMojo` - warm mid push
  - `SatConsole` - console channel sim
  - `SatCoils` - transformer color
  - `SatTubey` - tube-ish even harmonics
- Main `SaturateProcessor` class:
  - Holds `std::unique_ptr<ISat> current`
  - JUCE oversampling wrapper (1×/2×/4×/8×)
  - Input HPF (20-30 Hz)
  - PreGain, Post EQ, Output gain, Mix
  - Smooth parameter changes (10-30 ms)

#### 1.2 Implement SaturateProcessor (`source/dsp/SaturateProcessor.cpp`)
- Strategy pattern for model switching
- Each model implements single-sample `process(float x)`
- Oversampling wrapper: upsample → process → downsample
- Model-specific EQ stages
- Crossfade on model switch (5-10 ms) to avoid clicks
- Use `ScopedNoDenormals` for stability

#### 1.3 Model Implementations (Clean-Room)
Each model implements the curve sketches provided:
- **Spiral2**: tanh/sin blend with density/asym/bias controls
- **Density2**: Two-stage soft clip with low-freq management
- **Drive**: Soft→hard morph with asymmetry
- **PurestDrive**: Minimal math, high shelf as AirGain
- **Mojo**: Soft curve + gentle tilt EQ
- **Console**: Nonlinear + frequency pre/de-emphasis
- **Coils**: Saturating tanh + 3-5 kHz bump
- **Tubey**: Even-harmonic bias + compressor-like sag

### 2. APVTS Parameters (`source/PluginProcessor.cpp`)

Add 8 parameters to `createParameterLayout()`:
```cpp
params.push_back(std::make_unique<juce::AudioParameterInt>("satType", "Saturate Type", 0, 7, 0));
params.push_back(std::make_unique<juce::AudioParameterFloat>("satDrive", "Drive", 0.0f, 36.0f, 12.0f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("satColor", "Color", 0.0f, 1.0f, 0.5f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("satShape", "Shape", 0.0f, 1.0f, 0.4f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("satBias", "Bias", -0.2f, 0.2f, 0.0f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("satOut", "Output", -24.0f, 12.0f, 0.0f));
params.push_back(std::make_unique<juce::AudioParameterInt>("satOsMode", "Oversample", 0, 3, 2)); // 0=1×, 1=2×, 2=4×, 3=8×
params.push_back(std::make_unique<juce::AudioParameterFloat>("satMix", "Mix", 0.0f, 1.0f, 1.0f));
params.push_back(std::make_unique<juce::AudioParameterBool>("saturateEnabled", "Saturate Enabled", false));
params.push_back(std::make_unique<juce::AudioParameterBool>("saturateStepEnabled", "Saturate Step Enabled", true));
```

### 3. Effect Router (`source/EffectRouter.h`)
Add `Saturate = 12` to `EffectID` enum (after Form2 = 11)

### 4. UI Layer

#### 4.1 Page Registration (`source/PageTargetRegistry.cpp`)
Add Saturate page to registry with 8 parameter IDs:
```cpp
{
    PageTargets targets;
    targets.pageId = "Saturate";
    targets.knobParamIds = {
        "satType",      // Knob 0: Type
        "satDrive",     // Knob 1: Drive
        "satColor",     // Knob 2: Color (dynamic label)
        "satShape",     // Knob 3: Shape (dynamic label)
        "satBias",      // Knob 4: Bias (dynamic label)
        "satOut",       // Knob 5: Output
        "satOsMode",    // Knob 6: Oversample
        "satMix"        // Knob 7: Mix
    };
    targets.sequencerStepsUsedKey = "saturateStepsUsed";
    targets.maxSteps = 16;
    registry[EffectID::Saturate] = targets;
}
```

#### 4.2 UI Setup (`source/PluginEditor.cpp`)
Duplicate `setupDubDelayKnobs()` → `setupSaturateKnobs()`:
- Same knob positions (8 knobs in 2 rows of 4)
- Parameter attachments to satType, satDrive, satColor, satShape, satBias, satOut, satOsMode, satMix
- Callback to update dynamic labels when `satType` changes

#### 4.3 Dynamic Label Helper (`source/PluginEditor.cpp`)
Create `updateSaturateKnobLabels(int type)` function:
```cpp
void PluginEditor::updateSaturateKnobLabels(int type)
{
    struct ModelInfo {
        juce::String label2, label3, label4, label5;
        float min2, max2, min3, max3, min4, max4, min4, max4;
    };
    
    static constexpr ModelInfo models[8] = {
        {"Density", "Asym", "Bias", "Output", 0, 1, 0, 1, -0.2, 0.2}, // Spiral2
        {"Thickness", "Focus", "LoCut", "Output", 0, 1, 0, 1, 20, 200}, // Density2
        // ... etc for all 8 models
    };
    
    auto& info = models[type];
    if (saturateKnobLabels[2]) saturateKnobLabels[2]->setText(info.label2, juce::dontSendNotification);
    if (saturateKnobLabels[3]) saturateKnobLabels[3]->setText(info.label3, juce::dontSendNotification);
    if (saturateKnobLabels[4]) saturateKnobLabels[4]->setText(info.label4, juce::dontSendNotification);
    // Update knob ranges...
}
```

#### 4.4 UI Components (`source/PluginEditor.h`)
Add Saturate UI components (mirror Dub Delay):
- `saturateKnobs[8]`, `saturateAttachments[8]`
- `saturateKnobLabels[8]`, `saturateValueLabels[8]`
- `saturateIndicatorBars[8]`, `saturateDiceButtons[8]`
- `saturateFxPowerButton`, `saturateStepPowerButton`
- `saturateGroup` vector for visibility management
- Step sequencer arrays (`saturateStepButtons[16]`, etc.)

#### 4.5 Router Dropdown (`source/PluginEditor.cpp`)
Add "Saturate" to effect selector dropdown (ID 13)

### 5. Assets
Add to `source/ui/Assets.h` and load:
- `Saturate_Icon.svg`
- `Saturate_Background_Tab1.svg` through `Saturate_Background_Tab4.svg`

### 6. Processor Integration (`source/PluginProcessor.cpp`)
- Add `SaturateProcessor saturateProcessor` member
- Call `saturateProcessor.prepare()` in `prepareToPlay()`
- Process in `processBlock()` when Saturate is active
- Step sequencer snapshot handling (mirror Dub Delay pattern)

### 7. Step Sequencer
- Reuse Dub Delay sequencer widget exactly
- Target Drive knob by default
- Snapshot system for per-step saturation settings

## File Changes Summary

**New Files:**
- `source/dsp/SaturateProcessor.h`
- `source/dsp/SaturateProcessor.cpp`

**Modified Files:**
- `source/EffectRouter.h` - Add Saturate to enum
- `source/PageTargetRegistry.cpp` - Register Saturate page
- `source/PluginProcessor.h` - Add SaturateProcessor member
- `source/PluginProcessor.cpp` - Add parameters, prepare, process
- `source/PluginEditor.h` - Add Saturate UI components
- `source/PluginEditor.cpp` - Setup Saturate UI, dynamic labels
- `source/ui/Assets.h` - Add Saturate icon/backgrounds
- `StepSnapshot.h` - Add Saturate snapshot struct

## Testing Checklist
- [ ] All 8 models switch smoothly without clicks
- [ ] Dynamic labels update correctly for each model
- [ ] Oversampling removes aliasing at high drive
- [ ] CPU usage <1% @48k with 4× OS
- [ ] Step sequencer controls Drive correctly
- [ ] Mix=0 is dry, Mix=1 is fully saturated
- [ ] Power button greys out all controls
- [ ] All Steps toggle updates all steps
- [ ] Each model has distinct, musical character

## Notes
- Keep parameter IDs fixed (satDrive, satColor, etc.)
- Only labels and value→DSP mapping change per model
- Use clean-room implementations (no copy/paste)
- Normalize outputs so unity-ish at low Drive
- Add auto-trim per model if needed to prevent level explosions



