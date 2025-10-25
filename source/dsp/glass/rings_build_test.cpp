#ifdef USE_RINGS_LITE
#include "RingsLiteEngine.h"

// Build-only self-test to verify API compatibility at compile time
// This function is never called at runtime, but ensures that:
// 1. RingsLiteEngine compiles successfully
// 2. The API matches our expectations
// 3. All dependencies are correctly linked
static void __rings_build_probe__() {
  RingsLiteEngine e;
  e.prepare(48000.0, 64, 2);
  
  RingsLiteParams p;
  p.f0Hz = 440.f;
  p.brightness = 0.6f;
  p.damping = 0.5f;
  e.setParams(p);
  
  float exc[64] = {0.f};
  float L[64] = {0.f};
  float R[64] = {0.f};
  e.process(exc, 64, L, R);
}
#endif // USE_RINGS_LITE

