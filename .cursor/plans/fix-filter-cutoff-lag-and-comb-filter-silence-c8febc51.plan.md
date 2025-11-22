<!-- c8febc51-1014-4a8f-821e-696510ed8bb7 6992dc92-2b7b-4937-af36-e6c3bbf38628 -->
# Fix Comb Filter Progressive Channel Failure

## Problem Analysis

The comb filter is experiencing progressive channel failure:

1. Right channel goes silent first (returns `0.0f` on error instead of passing through)
2. Left channel fails next
3. Plugin stops processing entirely

Root causes identified:

- Right channel returns `0.0f` instead of input when buffer is invalid (`processSampleR` line 459)
- No state validation or recovery mechanism when buffers become corrupted
- Recursive exception handling that tries to process again after errors
- Write indices (`writeIndexL`, `writeIndexR`) can become corrupted without detection
- No health check or re-initialization when buffers are invalid

## Implementation Plan

### 1. Fix Right Channel Error Handling

**File**: `source/dsp/FilterProcessor.h`

- Change `processSampleR` to return `inputSample` instead of `0.0f` when buffer is invalid (line 459)
- This ensures right channel passes through input instead of going silent

### 2. Add State Validation Before Processing

**File**: `source/dsp/FilterProcessor.h`

- Add `bool isValidState()` method to `CombProc` that checks:
- Buffer sizes are valid (> 0)
- Buffers are not empty
- Write indices are within bounds
- Delay values are finite and in valid range
- Call this validation in `process()` before processing each block
- If invalid, re-prepare the filter and log a warning

### 3. Add Write Index Validation and Recovery

**File**: `source/dsp/FilterProcessor.h`

- Add validation in `processSampleL` and `processSampleR` before incrementing write indices:
- Ensure `writeIndexL < bufferSizeL` before incrementing
- Ensure `writeIndexR < bufferSizeR` before incrementing
- If invalid, clamp to valid range: `writeIndexL = juce::jlimit(0, bufferSizeL - 1, writeIndexL)`
- After incrementing, ensure indices wrap correctly

### 4. Add Recovery Mechanism

**File**: `source/dsp/FilterProcessor.h`

- Add `void recover()` method to `CombProc` that:
- Resets write indices to 0
- Clears buffer state (fills with zeros)
- Resets delay values to safe defaults
- Resets damping state variables
- Call `recover()` from `process()` if `isValidState()` returns false
- Also call `recover()` from `prepare()` to ensure clean state

### 5. Fix Exception Handling to Prevent Recursion

**File**: `source/PluginProcessor.cpp`

- In the exception handler (lines 2287-2310), add a flag to prevent recursive processing:
- Set `bool filterProcessingFailed = false;` before try block
- If exception caught, set flag and skip recursive `process()` call
- Instead, just pass through the buffer unchanged
- This prevents infinite loops if the filter is in a bad state

### 6. Add Per-Channel Error Detection

**File**: `source/dsp/FilterProcessor.h`

- Modify `process()` to catch exceptions per-sample and handle gracefully:
- Wrap `processSampleL()` and `processSampleR()` calls in try-catch
- If exception, use input sample as fallback for that channel
- Log which channel failed (if logging enabled)

### 7. Add Health Check Before Each Block

**File**: `source/dsp/FilterProcessor.cpp`

- In `FilterProcessor::process()`, before calling `cur->process()`:
- If `cur` is a `CombProc`, cast and check `isValidState()`
- If invalid, log warning and re-prepare the filter
- This catches corruption before it causes channel failure

### 8. Ensure Proper Re-initialization on Type Change

**File**: `source/dsp/FilterProcessor.cpp`

- In `FilterProcessor::process()` when switching filter types (lines 107-122):
- If switching to/from comb filter, ensure `recover()` is called on the new filter
- Reset crossfade ramp if recovery was needed
- This prevents corrupted state from carrying over

## Files to Modify

1. `source/dsp/FilterProcessor.h` - Comb filter implementation with state validation and recovery
2. `source/dsp/FilterProcessor.cpp` - Filter processor with health checks and re-initialization
3. `source/PluginProcessor.cpp` - Exception handling fix to prevent recursion

## Testing Considerations

- Test with comb filters on sequencer steps
- Test rapid switching between filter types
- Test with extreme parameter values (very high/low resonance, cutoff)
- Test with randomized sequencer steps
- Monitor for right channel failing first
- Verify audio continues processing even when errors occur