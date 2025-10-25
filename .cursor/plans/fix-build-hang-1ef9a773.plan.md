<!-- 1ef9a773-647d-49af-acfc-f909dae21987 e02e039c-3d21-4917-ae5a-547296f559b7 -->
# Fix Glass Effect Audio Output

## Root Causes Identified

1. **Parameter ID Mismatch**: UI knobs use `"glassPitchSemitones"` etc., but DSP expects `"glass/pitch_semitones/step_0"` format
2. **Weak Exciter Signal**: The fallback exciter (when not triggered by step edge) uses input audio but it's too quiet
3. **High-Pass Filter Too Aggressive**: 5kHz HP filter on exciter removes too much energy
4. **Modal Resonator Gain Too Low**: The Rings implementation needs more output gain to be audible
5. **Mix Crossfade Issue**: Constant-power crossfade may be reducing wet signal too much

## Implementation Plan

### 1. Fix Parameter Binding (GlassworksPageDSP.cpp)

**In `bindParamPtrs()` method (lines 88-127):**
- Change from looking for per-step parameters to using the UI parameter names
- Update parameter ID construction to match what the UI actually uses:
  ```cpp
  case 0: paramID = "glassPitchSemitones"; break;
  case 1: paramID = "glassBrightness"; break;
  case 2: paramID = "glassDecaySec"; break;
  case 3: paramID = "glassStrike"; break;
  case 4: paramID = "glassDensity"; break;
  case 5: paramID = "glassShimmer"; break;
  case 6: paramID = "glassSpread"; break;
  case 7: paramID = "glassMix"; break;
  ```
- Only bind 8 parameters total (not 8 × 16)
- Update `stepParamPtrs` array size from 128 to 8
- Modify `readStep()` to just read from the 8 parameter pointers (ignore step index)

### 2. Improve Exciter Signal Generation (GlassworksPageDSP.cpp)

**In `processStep()` method (lines 196-210):**
- Increase fallback exciter gain from 0.2f to 2.0f for audibility
- Add white noise component to exciter for richer transients
- Generate continuous exciter signal (not just on step edges) for testing
- Replace current logic with:
  ```cpp
  // Always generate exciter from input (for immediate testing)
  for (int n = 0; n < numSamples; ++n) {
      float inputMix = inL[n] + (inR ? inR[n] : 0.0f);
      float noise = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * 0.1f;
      exciter[n] = (inputMix + noise) * smStrike.getNextValue() * 2.0f;
  }
  ```

### 3. Reduce High-Pass Filter Cutoff (RingsEngine.cpp)

**In `Impl::updateFilters()` method (line 58):**
- Lower exciter HP filter from 5000Hz to 800Hz
- Change: `exciterHP.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 800.0f, 0.7f));`
- This preserves more of the exciter energy while still removing DC

### 4. Increase Modal Resonator Output Gain (RingsEngine.cpp)

**In `process()` method (lines 120-147):**
- Increase output gain significantly for audibility
- After line 138 (soft clipping), multiply sum by 10.0f:
  ```cpp
  sum = std::tanh(sum * 0.7f) / 0.7f;
  sum *= 10.0f;  // Boost output for audibility
  ```
- Increase stereo spread gain from 0.7f to 1.2f (lines 142-143)

### 5. Fix Mix Crossfade Calculation (GlassworksPageDSP.cpp)

**In `processStep()` method (lines 243-257):**
- Current constant-power crossfade may be too conservative
- Simplify to linear crossfade for more predictable behavior:
  ```cpp
  if (mixValue > 0.01f) {
      for (int n = 0; n < numSamples; ++n) {
          float dryL = inL[n];
          float dryR = inR ? inR[n] : dryL;
          
          // Simple linear crossfade (more predictable)
          float wetGain = mixValue;
          float dryGain = 1.0f - mixValue;
          
          outL[n] = dryGain * dryL + wetGain * outL[n];
          if (outR) {
              outR[n] = dryGain * dryR + wetGain * outR[n];
          }
      }
  }
  ```

### 6. Update Header File (GlassworksPageDSP.h)

**Line 79 (stepParamPtrs array):**
- Change array size from `std::array<std::atomic<float>*, 8 * 16>` to `std::array<std::atomic<float>*, 8>`

### 7. Increase Modal Q Values (RingsEngine.cpp)

**In `Impl::updateFilters()` method (line 41):**
- Increase Q range for longer ring times
- Change from `juce::jmap(params.damping, 0.0f, 1.0f, 20.0f, 2.0f)` to `juce::jmap(params.damping, 0.0f, 1.0f, 80.0f, 10.0f)`
- This creates more pronounced resonances

## Files to Modify

1. `/Users/troycarson/Documents/JUCE Projects/Stepper/source/dsp/glass/GlassworksPageDSP.h` - Update array size
2. `/Users/troycarson/Documents/JUCE Projects/Stepper/source/dsp/glass/GlassworksPageDSP.cpp` - Fix parameter binding, improve exciter, fix mix
3. `/Users/troycarson/Documents/JUCE Projects/Stepper/source/dsp/glass/RingsEngine.cpp` - Reduce HP filter, increase gain, increase Q values

## Testing Steps

After implementation:
1. Build and install AU plugin
2. Load Glass effect on tab 4
3. Turn Mix knob to 50% or higher
4. Turn Strike knob to 0.8 or higher
5. Play audio through plugin
6. Should hear immediate glass/bell resonance effect
7. Adjust Pitch knob to hear frequency changes
8. Adjust Brightness to hear timbre changes
9. Adjust Decay to hear ring duration changes

## No UI Changes

All fixes are in the DSP layer only - no UI modifications required as requested.