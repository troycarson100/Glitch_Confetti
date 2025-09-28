#pragma once
// Stage in UI gradually:
#define UI_STAGE_DELAY_ONLY        1  // 1 = only Delay page + Header + Sequencer
#define UI_ENABLE_TIMER            1  // 0 until views & bars are constructed
#define UI_USE_EMBEDDED_SVGS       1  // 1 = BinaryData, 0 = loadFromAssets()
#define UI_STRICT_NULL_GUARDS      1  // extra runtime checks + fallbacks
