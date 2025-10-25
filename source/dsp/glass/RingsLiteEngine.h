#pragma once
#include <JuceHeader.h>

struct RingsLiteParams {
  float f0Hz       = 440.0f; // clamp 20..12000
  float brightness = 0.6f;   // 0..1
  float damping    = 0.5f;   // 0..1 (higher = more damped = shorter decay)
};

class RingsLiteEngine {
public:
  void prepare (double sr, int blockSize, int numChannels);
  void reset();
  void setParams (const RingsLiteParams& p);
  
  // Mono exciter → stereo wet out (L/R valid even if host is mono)
  void process (const float* excMono, int numSamples, float* outL, float* outR);

private:
  double sampleRate = 48000.0;
  
  struct Impl;
  std::unique_ptr<Impl> impl;
};

