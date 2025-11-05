<!-- 009bc860-58ca-4e2f-980f-38afef20b598 26555eb2-67ea-48d1-94f5-ec9ce190579b -->
# Fix Saturate Knob Interaction and Make Types Distinct

## Problem 1: Knobs Don't Work During Sequencer Playback

**Root Cause:** When the sequencer is enabled, parameter updates from snapshots may conflict with user knob changes. The sequencer reads from snapshots but user changes only update snapshots when `updateSaturateParameterFromKnob()` is called, which might not be happening reliably during playback.

**Solution:** Ensure `updateSaturateParameterFromKnob()` always updates the selected (clicked) step snapshot, and make the sequencer respect user knob changes immediately.

### Changes Required

**File: `source/PluginEditor.cpp`**

In `setupSaturateKnobs()`, around line 11520-11598, the `onValueChange` callback already calls `updateSaturateParameterFromKnob(i)`, but verify it's not being blocked:

```cpp
// Line ~11520-11598 - onValueChange callback
// Ensure this always runs, even during sequencer playback:
updateSaturateParameterFromKnob(i);  // Line 11575 - already present, verify it works
```

Add a check to ensure knob changes update APVTS immediately, overriding sequencer values:

```cpp
// After updateSaturateParameterFromKnob(i) call:
// Update APVTS immediately so user changes take effect, even during sequencer
if (i == 0) { // Type knob
    auto* typeParam = processorRef.getAPVTS().getParameter("satType");
    if (typeParam) {
        float normType = value / 7.0f;
        typeParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normType));
    }
} else if (i == 1) { // Drive
    auto* driveParam = processorRef.getAPVTS().getParameter("satDrive");
    if (driveParam) {
        float normDrive = value / 36.0f;
        driveParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normDrive));
    }
}
// ... etc for all knobs
```

**Actually, simpler approach:** The knob attachments should already handle this. The issue might be that sequencer updates are happening every audio block. Verify `updateSaturateParameterFromKnob()` updates the **selected step** snapshot (not the playing step), and ensure sequencer respects this.

**File: `source/PluginProcessor.cpp`**

Verify `updateSaturateCurrentStepSnapshot()` updates the **selected** step, not the playing step. Check line ~3734-3765:

```cpp
void PluginProcessor::updateSaturateCurrentStepSnapshot(int knobIndex, float value)
{
    int currentStep = saturateUiSelectedStep.load();  // ← MUST use SELECTED step, not playing step!
    // ... rest of implementation
}
```

## Problem 2: All Types Sound Identical

**Root Cause:** Even with different curves, all models are too similar because:

1. They all receive the same gain scaling
2. Parameter differences aren't dramatic enough
3. Default drive at 12dB isn't high enough to reveal differences
4. Some models (Density2, Drive, PurestDrive, Tubey) are applying lo-cut filtering which masks character
5. The saturation curves aren't different enough in their basic character

**Solution:** Make each model dramatically unique with:

- Different saturation curves (soft clip vs hard clip vs linear with compression)
- Different harmonic content (even vs odd vs mixed)
- Different drive response (some models need more gain to saturate)
- Different frequency shaping built into the saturation
- Remove or make lo-cut less aggressive so saturation character shows through

### Changes Required

**File: `source/dsp/SaturateProcessor.cpp`**

For each model, rewrite the `process()` function to be dramatically more distinct:

**1. SatSpiral2 (Type 0)** - Gentle, even-harmonic rich

```cpp
float process(float x) override {
    float s = gain * (x + bias * 2.0f);  // Bias has stronger effect
    // Very gentle saturation - almost linear up to high levels
    float y = std::tanh(s * 0.5f);  // Soft, musical clip
    
    // Strong even harmonics emphasis
    float even = std::tanh(s * 0.6f);
    float odd = s / (1.0f + std::abs(s));
    y = juce::jmap(density, y, even * 1.3f + odd * 0.3f);  // Favor even harmonics
    
    // Light asymmetry only
    if (asym > 0.01f) {
        y = (1.0f - asym * 0.5f) * y + asym * 0.5f * ((s >= 0 ? y * 1.2f : -y * 0.8f));
    }
    return y;
}
```

**2. SatDensity2 (Type 1)** - Aggressive hard clipping

```cpp
float process(float x) override {
    float s = gain * 2.0f * x;  // 2x more drive sensitivity
    // HARD clipping - very aggressive
    float stage1 = std::tanh(s * (0.2f + 1.5f * thickness));
    // Second stage is hard limiter
    float stage2 = std::fmin(1.0f, std::fmax(-1.0f, stage1 * (2.0f - 1.5f * focus)));
    float y = stage1 * (1.0f - focus * 0.5f) + stage2 * focus * 0.5f;
    
    // REMOVE lo-cut filtering - it masks the aggressive character
    
    return y;
}
```

**3. SatDrive (Type 2)** - Diode-style asymmetric distortion

```cpp
float process(float x) override {
    float s = gain * 1.5f * x;  // More sensitive
    // Diode rectification curve - very asymmetric
    float exponent = juce::jmap(hardness, 0.5f, 4.0f);
    float y = s / std::pow(1.0f + std::abs(s), exponent);
    
    // EXTREME asymmetry - diode character
    if (asym > 0.01f) {
        float rectification = 1.0f + asym * 2.0f;
        y = (1.0f - asym) * y + asym * ((x >= 0 ? y * rectification : y * 0.2f));
    }
    
    // REMOVE lo-cut - keep low end for diode character
    
    return y;
}
```

**4. SatPurestDrive (Type 3)** - Almost linear, transparent

```cpp
float process(float x) override {
    float s = gain * 0.8f * x;  // Less gain = stays linear longer
    // Almost perfectly linear until very high levels
    float threshold = 0.5f + 0.5f * (1.0f - saturation);  // High threshold
    float y;
    if (std::abs(s) < threshold) {
        y = s;  // Completely linear
    } else {
        float excess = std::abs(s) - threshold;
        float compressed = threshold + excess / (1.0f + excess * 2.0f);  // Very gentle
        y = (s >= 0) ? compressed : -compressed;
    }
    
    // Strong air boost (high shelf) for clarity
    float air = y * airGain;
    y = y * (1.0f - std::abs(airGain - 1.0f) * 0.6f) + air * std::abs(airGain - 1.0f) * 0.6f;
    
    // REMOVE lo-cut - keep it transparent
    
    return y;
}
```

**5. SatMojo (Type 4)** - Tube warmth with mid push

```cpp
float process(float x) override {
    float s = gain * x;
    // Tube-style saturation with STRONG mid emphasis
    float tubeCurve = std::tanh(s * (0.1f + 1.2f * warmth));
    // Heavy mid-frequency resonance
    float midBoost = 1.0f + warmth * 1.2f * std::abs(s) * std::tanh(std::abs(s) * 3.0f);
    float y = tubeCurve * midBoost;
    
    // VERY aggressive presence
    y = y * (1.0f + presence * 1.0f);  // Up to 2x boost
    
    return y;
}
```

**6. SatConsole (Type 5)** - Console channel strip character

```cpp
float process(float x) override {
    float s = gain * trim * x;
    // Console-style: aggressive compression with EQ character
    float compAmount = 0.1f + 1.5f * focus;
    float y = s / (1.0f + std::abs(s) * compAmount);
    
    // Strong frequency emphasis - console pre/de-emphasis
    float eqBoost = 1.0f + (focus - 0.5f) * 1.2f;
    // Mid scoop characteristic of console EQs
    y = y * eqBoost * (1.0f - std::abs(focus - 0.5f) * 0.4f);
    
    return y;
}
```

**7. SatCoils (Type 6)** - Transformer with magnetic hysteresis

```cpp
float process(float x) override {
    float s = gain * x;
    // Transformer saturation - magnetic core behavior
    float ironFactor = 0.2f + 1.5f * iron;
    float coreSat = std::tanh(s * ironFactor);
    // Hysteresis: different behavior on rising vs falling
    static float prevInput = 0.0f;
    float hysteresis = (s > prevInput) ? 1.15f : 0.85f;  // Rising = more sensitive
    float y = coreSat * hysteresis;
    prevInput = s;
    
    // EXTREME asymmetry for magnetic bias
    y = (1.0f - asym) * y + asym * std::abs(y) * (1.5f - 0.7f * (x >= 0 ? 1.0f : -1.0f));
    
    // Strong HF resonance (transformer ringing)
    y = y * (1.0f + (hiBump - 1.0f) * 0.6f);
    
    return y;
}
```

**8. SatTubey (Type 7)** - Classic tube with sag

```cpp
float process(float x) override {
    float s = gain * x;
    // Classic tube: strong even harmonics
    float even = std::tanh(s * 1.0f);  // Even harmonics (symmetric)
    float odd = s / (1.0f + std::abs(s) * 0.7f);  // Odd harmonics
    float y = juce::jmap(evenBlend, odd * 0.8f, even * 1.3f);  // Favor even
    
    // STRONG sag - power supply droop
    if (sag > 0.01f) {
        float envTarget = std::abs(y);
        envelope = envelope * envCoeff + envTarget * (1.0f - envCoeff);
        float sagAmount = 1.0f - sag * envelope * 0.7f;  // More sag
        y *= sagAmount;
    }
    
    // REMOVE lo-cut - keep tube warmth
    
    return y;
}
```

**Key Changes:**

- Remove lo-cut filtering from models that have it (Density2, Drive, PurestDrive, Tubey) OR make it much gentler (200Hz+ only, not aggressive)
- Increase drive sensitivity in some models (Density2, Drive get more gain)
- Make harmonic content dramatically different (Spiral2 = even, Drive = odd, Mojo = mid, etc.)
- Make saturation curves more different (soft vs hard vs linear vs tube)
- Remove crossfade between models or make it much faster (5ms instead of 20ms) so character differences are immediate

## Implementation Order

1. Fix knob interaction (ensure selected step updates immediately)
2. Remove/reduce lo-cut filtering that masks character
3. Rewrite all 8 saturation models with dramatically different curves
4. Test each type at high drive (30dB+) to verify differences
5. Adjust drive sensitivity per model if needed

## Testing Checklist

- [ ] Turn sequencer ON, drag knobs → should update selected step immediately
- [ ] Play audio, turn knobs → should hear changes in real-time
- [ ] Switch between types at high drive (30dB) → each should sound dramatically different
- [ ] Spiral2: gentle, even-harmonic rich
- [ ] Density2: aggressive, hard-clipped
- [ ] Drive: asymmetric, diode-like
- [ ] PurestDrive: transparent, almost linear
- [ ] Mojo: warm, mid-emphasized
- [ ] Console: compressed, EQ-colored
- [ ] Coils: transformer-like with hysteresis
- [ ] Tubey: classic tube with sag

### To-dos

- [ ] Replace granular PitchBlock with SpectralPitchShifter (FFT phase vocoder)
- [ ] Replace FDN comb algorithm with Dattorro plate reverb
- [ ] Add LFO modulation to reverb delay lines
- [ ] Remove pitched shimmer from wet output (feedback only)
- [ ] Reduce feedback injection to 0.1-0.6 range for stability
- [ ] Test for smooth bloom, no grain artifacts, musical tail