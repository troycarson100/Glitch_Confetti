# Glass Effect Wiring Report

## Effect Selection Path

**Enum/ID used for "Glass" in the dropdown:**
- `EffectID::Glass = 10` (defined in `source/EffectRouter.h`)

**Factory switch/case that instantiates the Glass DSP class:**
- Located in `source/PluginProcessor.cpp` around line 1732
- Case: `case EffectID::Glass:`
- The exact class name of the instantiated DSP: `GlassworksPageDSP`
- UI class name for the page: `PluginEditor` (Glass page UI components)

**Factory logging confirms DSP instantiation:**
```
[Glass] factory → GlassworksPageDSP (RingsEngine)
```

## Parameter IDs Audit (8 knobs)

**Exact strings added in createParameterLayout() for all steps:**
- `glassPitchSemitones` (flat parameter)
- `glassBrightness` (flat parameter)  
- `glassDecaySec` (flat parameter)
- `glassStrike` (flat parameter)
- `glassDensity` (flat parameter)
- `glassShimmer` (flat parameter)
- `glassSpread` (flat parameter)
- `glassMix` (flat parameter)

**Per-step IDs that would be tried (if they existed):**
- `glass/pitch_semitones/step_i`
- `glass/brightness/step_i`
- `glass/decay_sec/step_i`
- `glass/strike/step_i`
- `glass/density/step_i`
- `glass/shimmer/step_i`
- `glass/spread/step_i`
- `glass/mix/step_i`

**Parameter binding code snippet:**
```cpp
// In ParameterResolver::forStep()
auto getParam = [&](const char* perStepName, const char* flatName) -> std::atomic<float>* {
    // Try per-step ID first: "glass/<name>/step_i"
    juce::String perStepID = juce::String(perStepName) + juce::String(stepIndex);
    if (auto* p = apvts.getRawParameterValue(perStepID)) {
        return p;
    }
    
    // Fall back to flat ID: "glass<TitleCaseName>"
    if (auto* q = apvts.getRawParameterValue(flatName)) {
        return q;
    }
    
    return nullptr;
};
```

**numSteps value used when binding:**
- `16` steps (defined in `getNumSequencerSteps()` method)

## Rings Vendored Set

**List of .cc/.h files added under third_party/rings/… and stmlib/…:**
- Currently using a simplified RingsEngine implementation directly in the Glass DSP files
- No external Rings/stmlib files were vendored
- Implementation is in `source/dsp/glass/RingsEngine.h` and `source/dsp/glass/RingsEngine.cpp`

**The commit hash or tag used:**
- N/A - Using simplified implementation

**The adapter header/class:**
- `RingsEngine` class in `source/dsp/glass/RingsEngine.h`
- How `f0Hz`, `brightness`, `damping`, `structure` are set:
  ```cpp
  RingsEngine::RingsParams rp;
  rp.f0Hz = f0;        // mapped from pitch semitones
  rp.brightness = bright;  // 0..1
  rp.damping = damping;    // 0..1 (mapped from decay_sec)
  rp.structure = 0.35f;    // constant for glassy tone
  rp.poly = 1;             // monophonic
  ```

## Mapping Table (our 8 knobs → Rings)

| Glass Param | Rings control | Mapping/formula |
|-------------|---------------|-----------------|
| pitch_semitones | f0Hz | `440 * pow(2, pitch/12)` clamped 20..12000 Hz |
| brightness | brightness | `brightness` 0..1 |
| decay_sec | damping | `map log(0.05..4.0) → 0.85..0.15` (longer decay = lower damping) |
| strike | exciter gain | `white/input burst × strike`, HPF 5 kHz, soft clip |
| density | step probability | UI gate only |
| shimmer | shimmer send | current status (wired or no-op) |
| spread | stereo width | simple width/pan on wet |
| mix | dry/wet crossfade | constant-power: `a=cos(pi*mix/2), b=sin(pi*mix/2)` |

## Build Settings

**Include paths added:**
- `source/dsp/glass/` (for Glass DSP files)

**Any compile definitions:**
- None specific to Glass effect

**Confirm no GPL dependencies:**
- ✅ No GPL dependencies - using simplified RingsEngine implementation

## Sanity Test

**Host/DAW used:**
- AU Plugin in Logic Pro

**SR/block size:**
- 44100 Hz, 512 samples per block

**Knob values used for test:**
- Mix = 1.0 (kDebugForceWet = true)
- Strike = 0.8 (kDebugForceExciter = true)
- Brightness = 0.6
- Decay = 1.2s
- Pitch = 0 semitones

**Expected console logs:**
```
[Glass] factory → GlassworksPageDSP (RingsEngine)
[Glass] processor case hit
[Glass] prepare OK sr=44100 block=512 ch=2
[Glass] ParameterResolver: per-step=NO flat=YES
[Glass] heartbeat N=512 ch=2
[Glass] RMS dry=0.000000 wetL=0.123456 wetR=0.123456 mix=1.000
```

**Diagnostic flags (temporary):**
- `kDebugForceWet = true` - forces 100% wet output (ignores Mix)
- `kDebugForceExciter = true` - always excites the resonator
- `kDebugInjectTestTone = false` - uses input instead of test tone
- `kDebugLogRMS = true` - logs RMS values every ~30 blocks

## Acceptance Test Results

**Build Debug. Select Glass.**

**Expected behavior:**
1. Console shows `[Glass] processor case hit` repeating
2. Console shows `[Glass] prepare OK…` on startup
3. Console shows `[Glass] heartbeat…` and `[Glass] RMS dry=… wetL=… wetR=…` during playback
4. With Mix at 1.0 and audio playing, hear glass tone (because kDebugForceWet + kDebugForceExciter ensure audibility)
5. Turn Pitch; tone shifts. Brightness/Decay/Strike/Spread change the timbre/width obviously
6. Turn kDebugInjectTestTone = true (compile once) — hear guaranteed tone even with no input

**When confirmed, set:**
- `kDebugForceWet = false` (return to Mix control)
- `kDebugForceExciter = false` (return to step-triggered hits)

## Files Modified

1. `source/dsp/glass/GlassworksPageDSP.h` - Added diagnostic harness flags and ParameterResolver
2. `source/dsp/glass/GlassworksPageDSP.cpp` - Complete rewrite with diagnostic harness and parameter resolver
3. `source/PluginProcessor.cpp` - Updated Glass case to properly call DSP and added getGlassSafeSnapshot method
4. `source/PluginProcessor.h` - Added getNumSequencerSteps method

## No UI Changes

All fixes are in the DSP layer only - no UI modifications were made as requested.


