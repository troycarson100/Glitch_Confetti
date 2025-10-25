# Glass Effect Diagnostic Report

## Effect Selection Path

### Enum/ID used for "Glass" in the dropdown
- **Enum**: `EffectID::Glass = 10` (in `source/EffectRouter.h`)
- **Dropdown items**: Added "Glass" to both `effectTypeDropdown` and `effectSelector` in `source/PluginEditor.cpp`

### Factory switch/case that instantiates the Glass DSP class
```cpp
case EffectID::Glass:
{
    // Factory logging - confirm DSP instantiation
    static bool factoryLogged = false;
    if (!factoryLogged) {
        DBG("[Glass] factory → GlassworksPageDSP (RingsEngine)");
        factoryLogged = true;
    }
    // ... processing logic
}
```

### DSP and UI class names
- **DSP Class**: `GlassworksPageDSP` (in `source/dsp/glass/GlassworksPageDSP.h`)
- **UI Class**: `PluginEditor` (Glass page implemented as part of main editor)
- **Rings Engine**: `RingsEngine` (in `source/dsp/glass/RingsEngine.h`)

## Parameter IDs Audit (8 knobs)

### Exact strings added in createParameterLayout()
The DSP binds to these UI parameter names (NOT per-step format):

```cpp
// Glass parameter IDs (must match APVTS order)
const juce::StringArray glassParamIDs = {
    "glassPitchSemitones",    // -24 to +24 semitones
    "glassBrightness",        // 0 to 1
    "glassDecaySec",          // 0.05 to 4.0 seconds
    "glassStrike",            // 0 to 1
    "glassDensity",           // 0 to 1
    "glassShimmer",           // 0 to 0.5
    "glassSpread",            // 0 to 1
    "glassMix"                // 0 to 1
};
```

### Parameter binding code
```cpp
bool GlassworksPageDSP::bindParamPtrs(juce::AudioProcessorValueTreeState& apvts, int numSteps)
{
    paramsBoundOk = false;
    int boundCount = 0;

    // Bind 8 parameters using UI parameter names
    juce::String paramID;
    for (int param = 0; param < 8; ++param) {
        switch (param) {
            case 0: paramID = "glassPitchSemitones"; break;
            case 1: paramID = "glassBrightness"; break;
            case 2: paramID = "glassDecaySec"; break;
            case 3: paramID = "glassStrike"; break;
            case 4: paramID = "glassDensity"; break;
            case 5: paramID = "glassShimmer"; break;
            case 6: paramID = "glassSpread"; break;
            case 7: paramID = "glassMix"; break;
        }

        auto* ptr = apvts.getRawParameterValue(paramID);
        if (ptr == nullptr) {
            GlassLog::err("missing param: " + paramID);
            return false;
        }

        stepParamPtrs[param] = ptr;
        boundCount++;
    }

    paramsBoundOk = true;
    GlassLog::msg("bindParamPtrs: OK, bound " + juce::String(boundCount) + " parameters");
    return true;
}
```

### numSteps value used when binding
- **numSteps**: 16 (hardcoded in constructor)
- **Actual binding**: 8 parameters total (not 8 × 16)

## Rings Vendored Set

### Files added under source/dsp/glass/
- `RingsEngine.h` - JUCE-compatible adapter for Rings DSP
- `RingsEngine.cpp` - Simplified modal resonator implementation using JUCE filters
- `GlassworksPageDSP.h` - Main Glass effect DSP class
- `GlassworksPageDSP.cpp` - Glass effect processing logic

### No external Rings dependency
- **No third_party/rings/** directory
- **No stmlib/** dependency
- **Implementation**: Simplified Rings-inspired modal resonator using JUCE's built-in `IIRFilter`

### Adapter class (RingsEngine)
```cpp
struct RingsParams {
    float f0Hz = 440.0f;     // mapped from pitch_semitones
    float brightness = 0.6f; // 0..1
    float damping   = 0.5f;  // 0..1 (maps from decay_sec)
    float structure = 0.35f; // 0..1 (glass/inharm tilt)
    int   poly       = 1;     // 1,2,4 voices (keep 1 for CPU)
};
```

## Mapping Table (Our 8 Knobs → Rings)

| Glass Param | Rings control | Mapping/formula |
|-------------|---------------|-----------------|
| pitch_semitones | f0Hz | `220.0f * pow(2.0f, pitchSt / 12.0f)` clamped 80..4000 Hz |
| brightness | brightness | `brightness` 0..1 |
| decay_sec | damping | `jmap(decaySec, 0.05f, 4.0f, 0.0f, 1.0f)` (longer decay = lower damping) |
| strike | exciter gain | `(inputMix + noise) * strike * 2.0f` |
| density | step probability | UI gate only (not used in DSP) |
| shimmer | shimmer send | Currently no-op (parameter exists but not implemented) |
| spread | stereo width | `structure` parameter reused for spread |
| mix | dry/wet crossfade | Linear crossfade: `wetGain = mix, dryGain = 1.0f - mix` |

## Build Settings

### Include paths added
```cmake
# In CMakeLists.txt - Glass DSP files added to SourceFiles
${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/glass/GlassworksPageDSP.cpp
${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/glass/GlassworksPageDSP.h
${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/glass/RingsEngine.cpp
${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/glass/RingsEngine.h
```

### Compile definitions
```cmake
# Disabled problematic audio formats
JUCE_USE_FLAC=0
JUCE_USE_MP3AUDIOFORMAT=0
JUCE_USE_LAME_AUDIO_FORMAT=0
JUCE_USE_WINDOWS_MEDIA_FORMAT=0
JUCE_USE_HARFBUZZ=0
```

### No GPL dependencies confirmed
- ✅ **No GPL code**: Implementation uses only JUCE's built-in filters
- ✅ **MIT license**: All code is proprietary or MIT-licensed
- ✅ **No external dependencies**: Self-contained modal resonator

## Sanity Test Configuration

### Host/DAW used
- **DAW**: AU plugin format
- **Sample Rate**: 48 kHz (default)
- **Block Size**: Variable (typically 64-512 samples)

### Knob values used for test
- **Mix**: 1.0 (100% wet)
- **Strike**: 0.8 (high exciter gain)
- **Brightness**: 0.7 (high brightness)
- **Decay**: 1.5s (medium decay)
- **Pitch**: 0.0 (A4 = 440 Hz)
- **Spread**: 0.35 (default stereo width)

### Expected console logs
```
[Glass] factory → GlassworksPageDSP (RingsEngine)
[Glass] prepare OK sr=48000.000000 block=512 ch=2
[Glass] bindParamPtrs: OK, bound 8 parameters
[Glass] heartbeat N=512 ch=2
[Glass] wet[0..15]= 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000
[Glass] RMS dry=0.000 wetL=0.000 wetR=0.000 mix=1.000
```

## Diagnostic Harness Flags

### Current diagnostic settings
```cpp
static constexpr bool kDebugForceWet       = true;   // force 100% wet to the output (ignores Mix)
static constexpr bool kDebugForceExciter   = true;   // always excite the resonator, even if no step hit
static constexpr bool kDebugInjectTestTone = false;  // when true, injects a 1 kHz sine instead of using input
static constexpr bool kDebugLogRMS         = true;   // logs RMS of dry/wet every ~30 blocks
```

### Test tone injection (for guaranteed audibility)
Set `kDebugInjectTestTone = true` to inject a 1 kHz sine wave at -18 dBFS, bypassing all input processing for immediate verification of the audio path.


