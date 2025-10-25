#pragma once
#include <JuceHeader.h>

// STK-inspired modal bar resonator (JUCE-only implementation)
// Glass/bell/bar physical model using bank of resonant filters
// Zero external dependencies, guaranteed to build

struct StkModalParams {
  float f0Hz       = 440.f; // clamp 20..12000
  float brightness = 0.6f;  // 0..1  (strike hardness / mode tilt)
  float damping    = 0.5f;  // 0..1  (higher = shorter decay)
  float spread     = 0.35f; // 0..1  (stereo width)
};

class StkModalBarEngine {
public:
  void prepare(double sr, int blockSize, int numChannels);
  void reset();
  void setParams(const StkModalParams& p);
  
  // Mono exciter → stereo wet out
  void process(const float* excMono, int numSamples, float* outL, float* outR);

private:
  double sampleRate = 48000.0;
  
  // Modal synthesis: bank of resonant band-pass filters
  static constexpr int kNumModes = 8;
  juce::IIRFilter modesL[kNumModes];
  juce::IIRFilter modesR[kNumModes];
  
  // Current parameters
  float currentF0 = 440.0f;
  float currentBrightness = 0.6f;
  float currentDamping = 0.5f;
  float currentSpread = 0.35f;
  
  // Update filters when parameters change
  void updateModes();
};



