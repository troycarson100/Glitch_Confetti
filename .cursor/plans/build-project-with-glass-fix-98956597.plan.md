<!-- 98956597-cd06-4832-926a-20825391598f 4b0373a0-2ee3-4665-bda7-3f85bb1944cb -->
# Fix Glass Effect Build with STK Modal Backend

## Problem

Builds keep timing out during lengthy CMake configuration. Glass effect needs working, audible DSP backend.

## Solution

Use incremental build approach with the JUCE-only `StkModalBarEngine` already created (zero external dependencies).

## Changes

### 1. Incremental Build Script

Create `quick_glass_build.sh`:

```bash
#!/bin/bash
cd build 2>/dev/null || { mkdir build && cd build; cmake ..; }
make -j2 Stepper_Standalone 2>&1 | tail -100
```

- Uses existing build dir (no slow CMake reconfigure)
- Limits parallelism to avoid timeouts
- Shows last 100 lines for errors

### 2. Verify Source Files Complete

Check `source/dsp/glass/`:

- `GlassBuildFlags.h` ✓ (already created)
- `StkModalBarEngine.h` ✓ (already created)  
- `StkModalBarEngine.cpp` ✓ (already created)
- `GlassworksPageDSP.{h,cpp}` - verify `#if USE_GLASS_STK` guards

### 3. Fix Any Compilation Errors

If build fails, check:

- Missing `#include "GlassBuildFlags.h"` in `.cpp` files
- Undefined `engine` member (needs `#if USE_GLASS_STK` guard)
- Missing StepSnapshot fields (e.g., `snapshot.glass.spread`)

### 4. Test Harness Verification

With `GLASS_DEBUG_HARNESS=1`:

- `kInjectTestTone=true` → 1kHz sine input
- `kForceWet=true` → 100% wet output
- `kLogRMS=true` → console logs every 30 blocks

### 5. Build & Test Flow

```bash
cd build
make -j2 Stepper_Standalone
# If successful:
./Stepper_artefacts/Debug/Standalone/Stepper.app/Contents/MacOS/Stepper
```

Expected logs:

```
[Glass] Using STK ModalBar backend
[Glass] heartbeat N=512 ch=2
[Glass] RMS dry=0.xx wet=0.xx
```

### 6. After Success

Turn off debug flags one by one in `GlassBuildFlags.h`:

```cpp
#define GLASS_DEBUG_HARNESS 0  // disable test tone/force wet
```

## Files Modified

- `quick_glass_build.sh` (new)
- `source/dsp/glass/GlassworksPageDSP.cpp` (verify guards)
- `source/dsp/glass/GlassBuildFlags.h` (toggle harness after testing)

## No UI Changes

All UI, assets, layout, and other effects remain untouched.