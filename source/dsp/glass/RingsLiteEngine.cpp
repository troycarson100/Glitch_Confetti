#include "RingsLiteEngine.h"

#ifdef USE_RINGS_LITE
#include "../../../third_party/rings/port_shims.h"
#include "../../../third_party/rings/rings/dsp/resonator.h"

struct RingsLiteEngine::Impl {
  rings::Resonator reson;
  float lastF0Norm = -1.f, lastB = -1.f, lastD = -1.f;
};

void RingsLiteEngine::prepare (double sr, int /*blockSize*/, int /*numChannels*/)
{
  sampleRate = (sr > 0 ? sr : 48000.0);
  impl = std::make_unique<Impl>();
  impl->reson.Init();
  
  // Set reasonable defaults (frequency is normalized: f/sampleRate)
  impl->reson.set_frequency(440.0f / static_cast<float>(sampleRate));
  impl->reson.set_structure(0.5f);     // Modal characteristics
  impl->reson.set_brightness(0.6f);
  impl->reson.set_damping(0.5f);
  impl->reson.set_position(0.5f);      // Center strike position
  impl->reson.set_resolution(24);      // Number of modes (must be even)
  
  reset();
  
  DBG("[RingsLiteEngine] prepared: sr=" << sampleRate);
}

void RingsLiteEngine::reset()
{
  // Rings Init() already clears internal state
  if (impl) {
    impl->reson.Init();
  }
}

void RingsLiteEngine::setParams (const RingsLiteParams& p)
{
  if (!impl) return;

  const float f0Hz = juce::jlimit(20.f, 12000.f, p.f0Hz);
  const float f0Norm = f0Hz / static_cast<float>(sampleRate); // Normalized frequency
  const float br = juce::jlimit(0.f, 1.f, p.brightness);
  const float dp = juce::jlimit(0.f, 1.f, p.damping);

  // Only update when values change to avoid redundant work
  if (f0Norm != impl->lastF0Norm) { 
    impl->reson.set_frequency(f0Norm);  
    impl->lastF0Norm = f0Norm; 
  }
  if (br != impl->lastB) { 
    impl->reson.set_brightness(br); 
    impl->lastB = br; 
  }
  if (dp != impl->lastD) { 
    impl->reson.set_damping(dp);    
    impl->lastD = dp; 
  }
}

void RingsLiteEngine::process (const float* excMono, int numSamples, float* outL, float* outR)
{
  if (!impl || numSamples <= 0) {
    // Bypass: clear outputs
    std::fill(outL, outL + numSamples, 0.f);
    std::fill(outR, outR + numSamples, 0.f);
    return;
  }

  // Rings Resonator block API:
  // void Process(const float* in, float* out, float* aux, size_t size)
  // 
  // in:  mono exciter input buffer
  // out: main resonator output (becomes left channel)
  // aux: auxiliary output (becomes right channel, stereo widening)
  // size: number of samples to process
  
  impl->reson.Process(excMono, outL, outR, static_cast<size_t>(numSamples));
}

#else // !USE_RINGS_LITE

// Stub implementation when USE_RINGS_LITE is OFF
struct RingsLiteEngine::Impl {};

void RingsLiteEngine::prepare(double, int, int) {}
void RingsLiteEngine::reset() {}
void RingsLiteEngine::setParams(const RingsLiteParams&) {}
void RingsLiteEngine::process(const float*, int numSamples, float* outL, float* outR) {
  // Bypass: silence
  std::fill(outL, outL + numSamples, 0.f);
  std::fill(outR, outR + numSamples, 0.f);
}

#endif // USE_RINGS_LITE

