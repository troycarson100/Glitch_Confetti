<!-- 2b32d9db-f214-4ab6-850b-ccf70601d677 a00629b7-0c80-4918-9c13-7ce13df5be65 -->
# Fix VST3 to Work Like AU

## Problem

- VST3 doesn't appear in Ableton Live
- VST3 doesn't process audio (when it did appear)
- AU version works perfectly

## Root Cause Analysis

The VST3 and AU use the same `processBlock` code, but VST3 has format-specific initialization and validation that may be failing. The VST3 bundle exists but Ableton may be rejecting it during scan due to:

1. Validation errors during plugin initialization
2. Missing or incorrect VST3 metadata
3. Processing code differences (though they should be identical)

## Solution Plan

### 1. Clean VST3 Build

- Remove existing VST3 build artifacts
- Clean rebuild in Release mode (matching AU)
- Verify bundle structure matches AU format

### 2. Verify Processing Code Identity

- Confirm `processBlock` is identical for both formats
- Ensure `processBlockBypassed` properly passes audio through
- Remove any VST3-specific code paths that differ from AU

### 3. Fix Audio Passthrough

- Ensure `processBlockBypassed` implementation matches AU behavior
- Add safety checks to guarantee audio always passes through
- Remove debug logging that might affect performance

### 4. VST3 Bundle Validation

- Verify `Info.plist` is correct
- Check `moduleinfo.json` exists and is valid
- Ensure bundle structure matches VST3 specification

### 5. Test and Verify

- Rebuild VST3
- Test in Ableton: verify it appears in plugin list
- Test audio processing: verify audio passes through
- Compare behavior with working AU version

## Files to Modify

- `source/PluginProcessor.cpp`: Ensure `processBlock` and `processBlockBypassed` work identically for both formats
- `source/PluginProcessor.h`: Verify no format-specific code paths
- Build system: Ensure VST3 builds with same configuration as AU

## Expected Outcome

- VST3 appears in Ableton Live plugin list
- VST3 processes audio identically to AU version
- No audio dropouts or silence issues

### To-dos

- [ ] Clean VST3 build artifacts and rebuild in Release mode to match AU
- [ ] Verify processBlock code is identical for VST3 and AU (no format-specific differences)
- [ ] Ensure processBlockBypassed properly passes audio through for VST3
- [ ] Check VST3 bundle structure (Info.plist, moduleinfo.json) matches specification
- [ ] Test VST3 appears in Ableton and processes audio correctly