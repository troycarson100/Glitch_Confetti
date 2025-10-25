# Glass Effect - Rings DSP Fix Summary

## Problems Found & Fixed

### Issue 1: Rings Wasn't Being Triggered
**Problem:** Rings DSP requires `performanceState->strum = true` to trigger the resonator. We were never setting this flag, so Rings was just passing audio through without resonating.

**Fix:** 
- Set `internal_exciter = true` in RingsEngine so Rings generates its own noise bursts
- Detect exciter level and set `strum = true` when level > 0.05
- Reset `strum = false` after processing so it only triggers once per burst

### Issue 2: Weak Exciter Signal
**Problem:** The exciter was too quiet and filtered too aggressively (5kHz high-pass removed most energy).

**Fix:**
- Lowered high-pass filter cutoff from 5kHz to 1kHz
- Increased exciter gain from 0.8× to 3.0×
- Added noise to exciter signal for more punch
- Boosted Rings output gain by 4× for better audibility

### Issue 3: Parameters Not Being Logged
**Problem:** No visibility into whether knobs were actually changing values.

**Fix:**
- Added parameter value logging every 60 blocks
- Logs show: pitch, frequency, brightness, decay, damping, mix

## Key Changes Made

### `source/dsp/glass/RingsEngine.cpp`

**Critical settings:**
```cpp
performanceState->internal_exciter = true;  // Rings generates noise bursts
performanceState->strum = shouldStrum;      // Trigger based on exciter level
```

**Strum detection:**
```cpp
// Detect strong enough signal to trigger
bool shouldStrum = (maxLevel > 0.05f);
performanceState->strum = shouldStrum;

// Process through Rings...

// Reset strum after processing
performanceState->strum = false;
```

**Output boost:**
```cpp
outL[i] = outBuffer[i] * 4.0f;  // 4x gain
outR[i] = outBuffer[i] * 4.0f;
```

### `source/dsp/glass/GlassworksPageDSP.cpp`

**Stronger exciter:**
```cpp
// Was: 0.8× strike, 5kHz HPF
// Now: 3.0× strike + noise, 1kHz HPF
float noise = (rand() / RAND_MAX) * 0.2f - 0.1f;
float s = std::tanh(3.0f * strike * (src + noise));
// HPF at 1000Hz instead of 5000Hz
```

**Parameter logging:**
```cpp
if (hb % 60 == 0) {
    GlassLog::msg("Params: pitch=" + juce::String(pitchSt, 1) + 
                  " freq=" + juce::String(f0, 1) + "Hz" +
                  " bright=" + juce::String(bright, 2) +
                  " decay=" + juce::String(decay, 2) + "s" +
                  " damp=" + juce::String(damping, 2) +
                  " mix=" + juce::String(mixVal, 2));
}
```

## Testing Instructions

1. **Restart your DAW** to reload the updated AU plugin
2. **Open the plugin** and select "Glass" effect
3. **Monitor the log:**
   ```bash
   tail -f ~/Desktop/glass_debug.log
   ```
4. **Play audio** through the plugin

### Expected Log Output

You should see:
```
[Glass] heartbeat N=512 ch=2
[Glass] Params: pitch=0.0 freq=440.0Hz bright=0.60 decay=1.20s damp=0.50 mix=0.30
[Glass] RMS dry=0.012345 wetL=0.045678 wetR=0.045678 mix=0.30
```

### What to Listen For

With the fixes, you should now hear:
- ✅ **Bell/glass/metal resonance** (not just pops!)
- ✅ **Pitch knob changes resonant frequency** (watch "freq=" in log)
- ✅ **Brightness knob changes timbre** (darker ↔ brighter)
- ✅ **Decay knob changes ring time** (shorter ↔ longer)
- ✅ **Strike knob affects excitation intensity**

### Testing Each Knob

1. **Pitch Knob**
   - Turn knob and watch log for "freq=" value changing
   - Should hear pitch of resonance shift up/down
   
2. **Brightness Knob**
   - Set to 0.3 = dark, mellow tone
   - Set to 0.8 = bright, sparkly tone
   - Watch "bright=" in log
   
3. **Decay Knob**
   - Set to 0.1s = short, percussive
   - Set to 3.0s = long, sustaining rings
   - Watch "decay=" and "damp=" in log
   
4. **Mix Knob**
   - Currently bypassed by `kDebugForceWet = true`
   - Watch "mix=" in log to confirm it's reading

## If Knobs Still Don't Respond

If the log shows the same values even when you turn knobs, the issue is parameter binding. Check:

1. **Are parameters created in PluginProcessor.cpp?**
   ```cpp
   params.push_back(std::make_unique<juce::AudioParameterFloat>(
       "glassPitchSemitones", "Glass Pitch", -24.0f, 24.0f, 0.0f));
   ```

2. **Are UI knobs attached to correct IDs?**
   In PluginEditor.cpp, knob attachments should use matching IDs.

3. **Is ParameterResolver finding them?**
   Check log for:
   ```
   [Glass] bindParamPtrs: SUCCESS - all parameters bound
   ```

## Build Status

✅ **AU Plugin built successfully**  
✅ **No errors, only pre-existing warnings**  
✅ **Plugin installed to ~/Library/Audio/Plug-Ins/Components/**

---

**Date:** October 22, 2024  
**Fix:** Rings triggering + exciter strength + parameter visibility  



