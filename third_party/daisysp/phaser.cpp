#include "phaser.h"

#include <algorithm>
#include <cmath>

namespace daisysp
{
namespace
{
constexpr float kTwoPi = 6.283185307179586476925286766559f;
constexpr float kPi = 3.1415926535897932384626433832795f;
constexpr float kMinFreq = 10.0f;

inline float safeTanh(float x)
{
    return std::tanh(x);
}

inline float clampFrequency(float freq, float sampleRate)
{
    const float nyquist = sampleRate * 0.45f;
    return std::clamp(freq, kMinFreq, nyquist);
}
} // namespace

Phaser::Phaser()
{
    updateStageMultipliers();
}

void Phaser::Init(float sampleRate)
{
    sampleRate_ = std::max(sampleRate, 1.0f);
    Reset();
}

void Phaser::Reset()
{
    feedbackState_ = 0.0f;
    stageBuffers_.fill(0.0f);
    lfoPhase_ = 0.0f;
}

void Phaser::SetLfoFreq(float freqHz)
{
    lfoFreq_ = std::max(0.0f, freqHz);
}

void Phaser::SetDepth(float depth)
{
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Phaser::SetFeedback(float amount)
{
    feedback_ = std::clamp(amount, -0.99f, 0.99f);
}

void Phaser::SetCenterFreq(float freqHz)
{
    centerFreq_ = std::max(freqHz, kMinFreq);
}

void Phaser::SetStereoOffset(float radians)
{
    stereoOffset_ = radians;
}

void Phaser::SetSoftClipGain(float gain)
{
    softClipGain_ = std::clamp(gain, 0.0f, 4.0f);
}

void Phaser::SetPoles(int poles)
{
    poles_ = std::clamp(poles, 1, kMaxPoles);
    updateStageMultipliers();
    Reset();
}

void Phaser::SetPhase(float phase)
{
    lfoPhase_ = phase;
    while (lfoPhase_ < 0.0f)
        lfoPhase_ += kTwoPi;
    while (lfoPhase_ >= kTwoPi)
        lfoPhase_ -= kTwoPi;
}

float Phaser::GetPhase() const
{
    return lfoPhase_;
}

float Phaser::Process(float inputSample)
{
    const float phase = lfoPhase_ + stereoOffset_;
    const float lfo = std::sinf(phase);
    const float sweepFreq = computeSweepFrequency(lfo);

    float x = inputSample;
    if (feedback_ != 0.0f)
    {
        const float clipped = softClipGain_ > 0.0f ? safeTanh(softClipGain_ * feedbackState_) : feedbackState_;
        x += feedback_ * clipped;
    }

    float y = x;
    for (int stage = 0; stage < poles_; ++stage)
    {
        const float stageFreq = clampFrequency(sweepFreq * stageMultipliers_[stage], sampleRate_);
        const float g = std::tan(kPi * stageFreq / sampleRate_);
        const float a = (g - 1.0f) / (g + 1.0f);

        const float buf = stageBuffers_[stage];
        const float out = -a * y + buf;
        stageBuffers_[stage] = y + a * out;
        y = out;
    }

    feedbackState_ = y;

    lfoPhase_ += kTwoPi * (lfoFreq_ / sampleRate_);
    if (lfoPhase_ >= kTwoPi)
        lfoPhase_ -= kTwoPi;
    if (lfoPhase_ < 0.0f)
        lfoPhase_ += kTwoPi;

    return y;
}

void Phaser::updateStageMultipliers()
{
    const float spread = 1.35f;
    float centerIndex = (poles_ - 1) * 0.5f;
    for (int i = 0; i < kMaxPoles; ++i)
    {
        if (i < poles_)
        {
            float offset = static_cast<float>(i) - centerIndex;
            stageMultipliers_[i] = std::pow(spread, offset);
        }
        else
        {
            stageMultipliers_[i] = 1.0f;
        }
    }
}

float Phaser::computeSweepFrequency(float lfoValue) const
{
    const float depthAmount = std::clamp(depth_, 0.0f, 1.0f);
    const float base = centerFreq_;
    const float sweepRatio = 0.9f * depthAmount;
    const float minFreq = std::max(kMinFreq, base * (1.0f - sweepRatio));
    const float maxFreq = std::max(minFreq + 5.0f, base * (1.0f + 1.35f * depthAmount));

    const float norm = (lfoValue + 1.0f) * 0.5f;
    return minFreq + (maxFreq - minFreq) * norm;
}

} // namespace daisysp


