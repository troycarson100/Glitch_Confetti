#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <array>
#include <cmath>

// PitchBlock v3 — dual-head granular shifter (reverted from PVOC)
struct PitchBlock
{
  void prepare (double sampleRate, int osFactorIn, int maxChannels = 2,
                double grainLenSec = 0.055, double xfadeSec = 0.018);
  void setOSFactor (int f);
  void setGrain (double lenS, double xfS);
  void processBuffer (juce::AudioBuffer<float>& inOut, float targetRatio, float amount);

private:
  struct Head { double phase=0.0, step=0.0; int base=0; };
  
  std::vector<juce::AudioBuffer<float>> ring;
  int ringMask=0, writePos=0, channels=2;
  Head headA, headB;
  bool primed=false;

  double fs=48000.0;
  int osFactor=4;
  double grainSec=0.055, xfadeSec=0.018;
  int grainSmp=0, xfadeSmp=0;

  juce::SmoothedValue<double> ratioSm;
  juce::dsp::IIR::Filter<float> lp, hp;

  inline int wrap (int x) const noexcept { return x & ringMask; }
  inline float sin2 (double x) const noexcept { const auto s = std::sin (x); return (float)(s*s); }
  inline float cos2 (double x) const noexcept { const auto c = std::cos (x); return (float)(c*c); }
  inline void winPair (float phase01, float& wa, float& wb) const noexcept;
  
  static inline float hermite4 (float xm1, float x0, float x1, float x2, float frac);
  float readHermite (const juce::AudioBuffer<float>& rb, int base, double phase) const;
  void advanceHead (Head& h);
  void pushToRing (const juce::AudioBuffer<float>& in);
  void updateFilters();
};

struct ReverbTank
{
  void prepare(const juce::dsp::ProcessSpec& spec);
  void setPredelayMs(float ms);
  void setSize(float s);
  void setDecaySeconds(float s);
  void setColor(float c);
  void process(juce::dsp::AudioBlock<float>& in, juce::AudioBuffer<float>& wetOut);
  void injectFeedback(const juce::AudioBuffer<float>& regen, float guard);

private:
  double fs = 48000.0;
  int maxBlock = 0;
  float size = 0.6f, decay = 8.0f, color = 0.55f, predelayMs = 25.0f;
  juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> predelay { 48000 };
  
  // Feedback buffer for shimmer injection (avoids read/write corruption)
  juce::AudioBuffer<float> feedbackBuffer;
  float feedbackGain = 0.0f;
  
  struct Comb { 
    juce::dsp::DelayLine<float> delay;
    float damping = 0.5f;
    float readPos = 0.0f;
    void prepare(const juce::dsp::ProcessSpec& spec) {
      delay.prepare(spec);
      delay.setMaximumDelayInSamples(static_cast<int>(spec.sampleRate * 0.1));
      delay.setDelay(static_cast<int>(spec.sampleRate * 0.05));
    }
  } comb[8];
  
  struct AP { 
    juce::dsp::DelayLine<float> delay;
    float coeff = 0.5f;
    void prepare(const juce::dsp::ProcessSpec& spec) {
      delay.prepare(spec);
      delay.setMaximumDelayInSamples(static_cast<int>(spec.sampleRate * 0.02));
      delay.setDelay(static_cast<int>(spec.sampleRate * 0.01));
    }
  } ap[4];
};

class ShimmerProcessor
{
public:
    ShimmerProcessor();
    ~ShimmerProcessor() = default;
    
    void prepare(const juce::dsp::ProcessSpec& spec);
    void setParams(float size, float decay, float color, float predelayMs, float shimAmt, int modeIndex, int osIndexIn, float mix, float pitchTuneIn = 1.0f);
    void process(juce::dsp::AudioBlock<float>& block);
    void processWithSnapshot(juce::AudioBuffer<float>& buffer, int numSamples, float mode, float size, float decay, float color, float predelay, float shimAmt, float osMode, float mix, bool stepChanged = false);

private:
    enum class ShimmerMode { A, B, C, D, E } mode { ShimmerMode::A };
    double fs = 48000.0;
    int blockSize = 0;
    int osIndex = 2; // 0=1x,1=2x,2=4x,3=8x
    int osFactor = 4;
    
    juce::SmoothedValue<float> sizeSm, decaySm, colorSm, predelaySm, shimSm, mixSm;
    
    juce::AudioBuffer<float> wet, regen, regenStore;
    
    ReverbTank tank;
    
    // Shimmer EQ filters (HP 600-800Hz, LP 10-12kHz, air shelf)
    juce::dsp::IIR::Filter<float> shimmerHP, shimmerLP;
    juce::dsp::IIR::Filter<float> shimmerAir;
    
    // Chorus/modulation for shimmer (reduces metallic artifacts)
    juce::dsp::Chorus<float> shimmerChorus;
    
    PitchBlock pitA, pitB, pitC;
    
    // Mode crossfade
    ShimmerMode prevMode = ShimmerMode::A;
    int modeCrossfadeRemaining = 0;
    static constexpr int kModeCrossfadeSamples = 2048;  // ~40ms at 48kHz
    
    // Debug toggles (internal only)
    bool dbgSoloWet = true;  // when true: force Mix=1.0
    bool dbgBypassLoopFilters = true;  // bypass HP/LP in feedback path
    bool firstRun = true;  // force audition defaults on first run
    
    int currentOSIndexToFactor(int idx) const;
    void setOSFactor(int f);
    
    float ratioA=2.0f, ratioB=1.0f, ratioC=1.0f, detCents=0.0f;
    float pitchTune = 1.0f;  // Pitch tuning multiplier (0.5x = -12 semitones, 1.0x = no shift, 2.0x = +12 semitones)
    void setModeRatios();
    static float cent(float c);
    
    float feedbackGuardGain(float shim, float decaySec) const;
    float softLimit(float x) const;
    void softLimitInPlace(juce::AudioBuffer<float>& buf);
    void processShimmerEQ(juce::AudioBuffer<float>& buf);
    void processPitchModes(juce::AudioBuffer<float>& buf, float amount);
    static void sumInPlace(juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b);
};
