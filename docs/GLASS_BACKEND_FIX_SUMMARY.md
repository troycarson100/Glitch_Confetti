# Glass Backend Fix Summary

## Changes Made

This document summarizes the backend fixes applied to make the Glass effect audible and diagnosable.

## Problem

The Glass effect DSP was being re-prepared on every audio callback (`forcing prepare from processBlock`), indicating it never became "healthy". This prevented any audio processing from occurring.

## Root Cause

The `prepare()` and `bindParamPtrs()` methods were being called from within `processBlock()` because `isHealthy()` was always returning false. This created a cycle where:
1. DSP reports as unhealthy
2. `processBlock` tries to prepare/bind
3. DSP still reports as unhealthy (likely due to timing/initialization issues)
4. Next block repeats step 1

## Solution

### 1. Removed prepare/bind from processBlock (PluginProcessor.cpp)

**Removed:**
```cpp
if (!glassworksPageDSP.isHealthy()) {
    DBG("[Glass] forcing prepare from processBlock");
    glassworksPageDSP.prepare(...);
    glassworksPageDSP.bindParamPtrs(...);
}
```

**Why:** Preparation should only happen once during `prepareToPlay()`, not on every audio block.

### 2. Added health verification in prepareToPlay (PluginProcessor.cpp)

**Added after bind:**
```cpp
if (!glassworksPageDSP.isHealthy()) {
    DBG("[Glass:ERR] DSP not healthy after prepare/bind in prepareToPlay!");
} else {
    DBG("[Glass] DSP healthy and ready");
}
```

**Why:** This confirms whether the DSP is properly initialized at startup.

### 3. Enhanced parameter binding diagnostics (GlassworksPageDSP.cpp)

**Added logging in `bindParamPtrs()`:**
- Logs the `numSteps` value being used
- If binding fails, checks each individual parameter and logs OK/MISSING status
- Logs success message when all parameters are bound

**Why:** This helps diagnose parameter ID mismatches.

### 4. Added defensive logging in processStep (GlassworksPageDSP.cpp)

**Added separate checks for:**
- `!prepared` → logs "processStep: not prepared!"
- `!paramsBoundOk` → logs "processStep: params not bound!"
- `!healthy` → logs "processStep: not healthy!"

**Why:** This pinpoints exactly which condition is preventing audio processing.

## Files Modified

1. **source/PluginProcessor.cpp**
   - Removed prepare/bind from `processBlock` Glass case (lines ~1759-1765)
   - Added health check logging in `prepareToPlay` (lines ~534-542)

2. **source/dsp/glass/GlassworksPageDSP.cpp**
   - Enhanced `bindParamPtrs()` with detailed logging (lines ~141-167)
   - Split `processStep()` health guards with individual logging (lines ~207-229)

## Expected Logs After Fix

When Glass is selected and audio is playing, you should now see:

1. **At startup (once):**
   ```
   [Glass] prepare OK sr=48000 block=512 ch=2
   [Glass] bindParamPtrs: attempting to bind with numSteps=16
   [Glass] ParameterResolver: per-step=NO flat=YES
   [Glass] bindParamPtrs: SUCCESS - all parameters bound
   [Glass] DSP healthy and ready
   ```

2. **During playback (repeating):**
   ```
   [Glass] processor case hit
   [Glass] calling glassworksPageDSP.processStep step=0
   [Glass] heartbeat N=512 ch=2
   [Glass] RMS dry=... wetL=... wetR=... mix=...
   ```

3. **NO MORE:**
   ```
   [Glass] forcing prepare from processBlock  ← This should be GONE
   ```

## Testing Instructions

1. **Clear the log:**
   ```bash
   rm -f ~/Desktop/glass_debug.log && touch ~/Desktop/glass_debug.log
   ```

2. **Open the plugin:**
   - Launch your DAW
   - Load the Stepper AU plugin
   - Select "Glass" from the effect dropdown

3. **Start playback:**
   - Play audio through the plugin
   - Set Mix to 1.0 to hear full wet signal
   - Adjust Strike, Brightness, Pitch knobs

4. **Check the log:**
   ```bash
   tail -f ~/Desktop/glass_debug.log
   ```

5. **What to verify:**
   - ✅ See "DSP healthy and ready" at startup
   - ✅ See "heartbeat" and "RMS" logs during playback
   - ✅ Hear audio output (glass/bell tone)
   - ❌ NO "forcing prepare from processBlock" messages
   - ❌ NO "processStep: not prepared/bound/healthy" errors

## Diagnostic Flags Still Active

The following debug flags remain enabled in `GlassworksPageDSP.h`:

```cpp
static constexpr bool kDebugForceWet       = true;   // 100% wet output
static constexpr bool kDebugForceExciter   = true;   // always excite resonator
static constexpr bool kDebugInjectTestTone = false;  // 1 kHz sine (if enabled)
static constexpr bool kDebugLogRMS         = true;   // RMS logging
```

These ensure maximum audibility for testing. Once confirmed working:
- Set `kDebugForceWet = false` to restore Mix control
- Set `kDebugForceExciter = false` to restore step-triggered behavior
- Keep `kDebugLogRMS` as needed for performance monitoring

## No UI Changes

All changes are backend DSP only. No effect UI was modified.
