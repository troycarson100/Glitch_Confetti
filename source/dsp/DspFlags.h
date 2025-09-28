#pragma once

// Emergency triage flag to isolate Delay-only processing
#define GC_SAFE_DELAY_ONLY 1

// FX types for routing
enum class FxType {
    Delay,
    Crunch,
    Pitch,
    AutoPan
};
