#pragma once

#include "port_shims.h"

// Upstream sometimes relies on rings/dsp/dsp.h for constants.
// Provide minimal fallbacks if missing.
namespace rings_shim {
  #ifndef RINGS_KSAMPLE_RATE_DEFINED
    // Default 48k; we feed normalized freq anyway.
    static constexpr float kSampleRate = 48000.0f;
  #endif
  #ifndef RINGS_KBLOCK_SIZE_DEFINED
    static constexpr size_t kBlockSize = 24; // Rings default block size
  #endif
}



