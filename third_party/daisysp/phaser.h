#pragma once

/**
 * Minimal DaisySP Phaser port (MIT).
 *
 * This is a lightweight adaptation of the Electro-Smith DaisySP phaser core
 * with a small public API that mirrors the original interface closely enough
 * for reuse inside Stepper.  The implementation lives in phaser.cpp.
 *
 * The namespace and class name intentionally match DaisySP so future updates
 * can wholesale drop-in the upstream version if desired.
 */

#include <array>

namespace daisysp
{
class Phaser
{
public:
    Phaser();

    void Init(float sampleRate);
    void Reset();

    void SetLfoFreq(float freqHz);
    void SetDepth(float depth);                // 0..1
    void SetFeedback(float amount);            // -0.99..0.99
    void SetCenterFreq(float freqHz);          // > 0
    void SetStereoOffset(float radians);       // phase offset for LFO
    void SetSoftClipGain(float gain);          // 0..1 scaling for tanh feedback
    void SetPoles(int poles);                  // 1..8

    void SetPhase(float phase);                // set LFO phase
    float GetPhase() const;

    float Process(float inputSample);

private:
    static constexpr int kMaxPoles = 8;

    float sampleRate_ = 48000.0f;
    float lfoFreq_ = 0.5f;
    float depth_ = 0.5f;
    float feedback_ = 0.0f;
    float centerFreq_ = 1000.0f;
    float stereoOffset_ = 0.0f;
    float lfoPhase_ = 0.0f;
    float softClipGain_ = 0.8f;
    int poles_ = 4;

    float feedbackState_ = 0.0f;
    std::array<float, kMaxPoles> stageBuffers_{};
    std::array<float, kMaxPoles> stageMultipliers_{};

    void updateStageMultipliers();
    float computeSweepFrequency(float lfoValue) const;
};
} // namespace daisysp


