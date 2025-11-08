#include "PhaseBloomEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMinCenterParam = 200.0f;
constexpr float kMaxCenterParam = 8000.0f;
constexpr float kCenterMinHz = 100.0f;
constexpr float kCenterMaxHz = 6000.0f;
constexpr float kToneMinHz = 10000.0f;
constexpr float kToneMaxHz = 22000.0f;
constexpr float kMinFeedback = -0.9f;
constexpr float kMaxFeedback = 0.9f;
constexpr double smoothingTimeSeconds = 0.035;
constexpr float stageSpread = 1.35f;

inline float softClip(float x, float amount)
{
    return x / (1.0f + amount * std::abs(x));
}

inline float clampFrequency(float freq, double sampleRate)
{
    const float nyquist = static_cast<float>(sampleRate) * 0.45f;
    return juce::jlimit(10.0f, nyquist, freq);
}

inline float wrapPhase(float phase)
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    if (phase >= twoPi)
        phase -= twoPi;
    else if (phase < 0.0f)
        phase += twoPi;
    return phase;
}
} // namespace

//==============================================================================
void PhaseBloomEngine::AllpassLadder::prepare(int numStages, float spread)
{
    numStages = juce::jlimit(1, 8, numStages);
    stages.assign(static_cast<size_t>(numStages), {});

    if (numStages == 0)
        return;

    const float centerIndex = (static_cast<float>(numStages) - 1.0f) * 0.5f;
    for (int i = 0; i < numStages; ++i)
    {
        stages[static_cast<size_t>(i)].multiplier = std::pow(spread, static_cast<float>(i) - centerIndex);
        stages[static_cast<size_t>(i)].z = 0.0f;
    }
}

void PhaseBloomEngine::AllpassLadder::reset()
{
    for (auto& stage : stages)
        stage.z = 0.0f;
}

void PhaseBloomEngine::AllpassLadder::copyFrom(const AllpassLadder& other)
{
    const size_t minStages = std::min(stages.size(), other.stages.size());
    for (size_t i = 0; i < minStages; ++i)
        stages[i].z = other.stages[i].z;
}

float PhaseBloomEngine::AllpassLadder::process(float input,
                                               float baseFrequencyHz,
                                               float sampleRate)
{
    float x = input;
    if (stages.empty())
        return x;

    for (auto& stage : stages)
    {
        const float freq = clampFrequency(baseFrequencyHz * stage.multiplier, sampleRate);
        float g = std::tan(juce::MathConstants<float>::pi * freq / sampleRate);
        float denom = 1.0f + g;
        if (std::abs(denom) < 1.0e-6f)
            denom = denom >= 0.0f ? 1.0e-6f : -1.0e-6f;

        const float a = (1.0f - g) / denom;
        const float y = (-a * x) + stage.z;
        stage.z = x + a * y;
        x = y;
    }

    return x;
}

//==============================================================================
PhaseBloomEngine::PhaseBloomEngine()
{
    characters[0] = CharacterProfile { 6, 6, 0.75f, 0.85f, 0.7f, 0.0f, 1.0f, 0.8f }; // Type A
    characters[1] = CharacterProfile { 8, 8, 0.65f, 1.0f, 1.0f, 1.0f, 1.2f, 0.8f }; // Type B
    characters[2] = CharacterProfile { 4, 4, 0.85f, 1.1f, 0.5f, 0.0f, 1.0f, 0.9f }; // Type C

    currentStages = targetStages = characters[currentCharacter].preferredStages;

    depthTarget = 0.5f;
    rateTargetHz = mapRateToHz(0.5f);
    feedbackTarget = 0.0f;
    centerTargetHz = mapCenterToHz(2000.0f);
    stereoTarget = 0.5f;
    toneTargetHz = mapToneToHz(0.5f);
    mixTarget = 0.5f;
    bloomParameterCached = (static_cast<float>(targetStages) - 2.0f) / 6.0f;

    updateSmoothers();
}

void PhaseBloomEngine::prepare(double newSampleRate, int samplesPerBlock, int inNumChannels)
{
    sampleRate = juce::jmax(10.0, newSampleRate);
    maxBlockSize = juce::jmax(1, samplesPerBlock);
    numChannels = juce::jmax(1, inNumChannels);

    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
    filterSpec.numChannels = 1;

    updateSmoothers();

    dryBuffer.setSize(numChannels, maxBlockSize, false, false, true);

    ensureChannelCount(numChannels);

    for (auto& channel : channels)
    {
        channel.current.prepare(currentStages, stageSpread);
        channel.pending.prepare(currentStages, stageSpread);
        channel.current.reset();
        channel.pending.reset();
        channel.feedbackStateCurrent = 0.0f;
        channel.feedbackStatePending = 0.0f;
        channel.lfoPhase = 0.0f;

        channel.tone.prepare(filterSpec);
        channel.tone.reset();
        channel.tone.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        channel.tone.setResonance(0.707f);

        channel.pendingTone.prepare(filterSpec);
        channel.pendingTone.reset();
        channel.pendingTone.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        channel.pendingTone.setResonance(0.707f);

        channel.crossfadeActive = false;
        channel.crossfadeSamplesRemaining = 0;
        channel.crossfadeSamplesTotal = 0;
    }

    prepared = true;
}

void PhaseBloomEngine::reset()
{
    if (!prepared)
        return;

    for (auto& channel : channels)
    {
        channel.current.reset();
        channel.pending.reset();
        channel.tone.reset();
        channel.pendingTone.reset();
        channel.feedbackStateCurrent = 0.0f;
        channel.feedbackStatePending = 0.0f;
        channel.crossfadeActive = false;
        channel.crossfadeSamplesRemaining = 0;
        channel.crossfadeSamplesTotal = 0;
        channel.lfoPhase = 0.0f;
    }

    depthSmoothed.setCurrentAndTargetValue(depthTarget);
    rateSmoothed.setCurrentAndTargetValue(rateTargetHz);
    feedbackSmoothed.setCurrentAndTargetValue(feedbackTarget);
    centerSmoothed.setCurrentAndTargetValue(centerTargetHz);
    stereoSmoothed.setCurrentAndTargetValue(stereoTarget);
    toneSmoothed.setCurrentAndTargetValue(toneTargetHz);
    mixSmoothed.setCurrentAndTargetValue(mixTarget);
}

void PhaseBloomEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    if (!prepared || !enabled)
        return;

    const int channelsToProcess = juce::jmin(numChannels, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (channelsToProcess <= 0 || numSamples <= 0)
        return;

    jassert(numSamples <= dryBuffer.getNumSamples());

    for (int ch = 0; ch < channelsToProcess; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    const CharacterProfile& activeProfile = characters[currentCharacter];
    const CharacterProfile& pendingProfile = characters[targetCharacter];

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float rateHz = rateSmoothed.getNextValue();
        const float depthBase = depthSmoothed.getNextValue();
        const float feedbackBase = feedbackSmoothed.getNextValue();
        const float centerBaseHz = centerSmoothed.getNextValue();
        const float stereoParam = stereoSmoothed.getNextValue();
        const float toneBaseHz = toneSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float dryGain = 1.0f - mix;
        const float wetGain = mix;
        const float rateIncrement = juce::MathConstants<float>::twoPi * rateHz / static_cast<float>(sampleRate);

        const float stereoCurrent = juce::jmax(stereoParam, activeProfile.stereoFloor);
        const float stereoPending = juce::jmax(stereoParam, pendingProfile.stereoFloor);
        const auto centrePairCurrent = computeStereoCentres(centerBaseHz, stereoCurrent, activeProfile);
        const auto centrePairPending = computeStereoCentres(centerBaseHz, stereoPending, pendingProfile);
        const float phaseOffsetCurrent = computeStereoPhaseOffset(stereoCurrent, activeProfile);
        const float phaseOffsetPending = computeStereoPhaseOffset(stereoPending, pendingProfile);

        for (int ch = 0; ch < channelsToProcess; ++ch)
        {
            auto& state = channels[ch];
            const float dry = dryBuffer.getSample(ch, sample);
            const float basePhase = state.lfoPhase;

            const float lfoCurrent = std::sinf(basePhase + (ch == 0 ? 0.0f : phaseOffsetCurrent));
            const float lfoPending = std::sinf(basePhase + (ch == 0 ? 0.0f : phaseOffsetPending));

            const float centreCurrent = (ch == 0 ? centrePairCurrent.first : centrePairCurrent.second);
            const float centrePending = (ch == 0 ? centrePairPending.first : centrePairPending.second);

            const float depthCurrent = juce::jlimit(0.0f, 1.0f, depthBase * activeProfile.depthScale);
            const float depthPending = juce::jlimit(0.0f, 1.0f, depthBase * pendingProfile.depthScale);

            float feedbackCurrent = juce::jlimit(kMinFeedback, kMaxFeedback, feedbackBase);
            float feedbackPending = feedbackCurrent;

            if (centreCurrent > 0.0f)
            {
                const float compensation = std::pow(juce::jmax(0.001f, centreCurrent * 0.001f), 0.2f);
                feedbackCurrent /= compensation;
            }
            if (centrePending > 0.0f)
            {
                const float compensation = std::pow(juce::jmax(0.001f, centrePending * 0.001f), 0.2f);
                feedbackPending /= compensation;
            }

            feedbackCurrent = juce::jlimit(-activeProfile.feedbackCap, activeProfile.feedbackCap, feedbackCurrent);
            feedbackPending = juce::jlimit(-pendingProfile.feedbackCap, pendingProfile.feedbackCap, feedbackPending);

            const float toneHzCurrent = juce::jlimit(kToneMinHz,
                                                     kToneMaxHz,
                                                     toneBaseHz * activeProfile.toneMultiplier);
            const float toneHzPending = juce::jlimit(kToneMinHz,
                                                     kToneMaxHz,
                                                     toneBaseHz * pendingProfile.toneMultiplier);

            updateFilter(state.tone, toneHzCurrent);

            const float modFreqCurrent = clampFrequency(centreCurrent * std::pow(2.0f, lfoCurrent * depthCurrent * sweepOctaves),
                                                        sampleRate);
            float ladderInputCurrent = dry + feedbackCurrent * softClip(state.feedbackStateCurrent,
                                                                        activeProfile.softClipAmount);
            float wetCurrent = state.current.process(ladderInputCurrent, modFreqCurrent, static_cast<float>(sampleRate));
            state.feedbackStateCurrent = wetCurrent;
            wetCurrent = state.tone.processSample(0, wetCurrent);

            float wetOutput = wetCurrent;

            if (state.crossfadeActive)
            {
                updateFilter(state.pendingTone, toneHzPending);

                const float modFreqPending = clampFrequency(centrePending * std::pow(2.0f, lfoPending * depthPending * sweepOctaves),
                                                            sampleRate);
                float ladderInputPending = dry + feedbackPending * softClip(state.feedbackStatePending,
                                                                            pendingProfile.softClipAmount);
                float wetPending = state.pending.process(ladderInputPending, modFreqPending, static_cast<float>(sampleRate));
                state.feedbackStatePending = wetPending;
                wetPending = state.pendingTone.processSample(0, wetPending);

                const float progress = juce::jlimit(0.0f, 1.0f,
                                                    1.0f - static_cast<float>(state.crossfadeSamplesRemaining)
                                                                / static_cast<float>(state.crossfadeSamplesTotal));
                const float fadeIn = progress;
                const float fadeOut = 1.0f - progress;
                wetOutput = wetCurrent * fadeOut + wetPending * fadeIn;

                if (--state.crossfadeSamplesRemaining <= 0)
                {
                    state.crossfadeActive = false;
                    state.current = state.pending;
                    state.feedbackStateCurrent = state.feedbackStatePending;
                    state.tone = state.pendingTone;
                }
            }
            else
            {
                state.feedbackStatePending = state.feedbackStateCurrent;
            }

            const float output = dryGain * dry + wetGain * wetOutput;
            buffer.setSample(ch, sample, output);

            state.lfoPhase = wrapPhase(basePhase + rateIncrement);
        }

        if (!anyChannelCrossfading())
        {
            currentStages = targetStages;
            currentCharacter = targetCharacter;
        }
    }
}

//==============================================================================
void PhaseBloomEngine::setDepth(float value)
{
    depthTarget = juce::jlimit(0.0f, 1.0f, value);
    depthSmoothed.setTargetValue(depthTarget);
}

void PhaseBloomEngine::setRate(float value)
{
    rateTargetHz = mapRateToHz(value);
    rateSmoothed.setTargetValue(rateTargetHz);
}

void PhaseBloomEngine::setFeedback(float value)
{
    feedbackTarget = mapFeedback(value);
    feedbackSmoothed.setTargetValue(feedbackTarget);
}

void PhaseBloomEngine::setCenter(float value)
{
    centerTargetHz = mapCenterToHz(value);
    centerSmoothed.setTargetValue(centerTargetHz);
}

void PhaseBloomEngine::setBloom(float value)
{
    bloomParameterCached = value;
    int newStages = mapStages(value);
    newStages = juce::jlimit(2, characters[targetCharacter].stageLimit, newStages);

    if (!prepared)
    {
        currentStages = targetStages = newStages;
        return;
    }

    if (newStages != targetStages || (!anyChannelCrossfading() && newStages != currentStages))
    {
        targetStages = newStages;
        beginStageCrossfade(targetStages);
    }
}

void PhaseBloomEngine::setSpread(float value)
{
    stereoTarget = mapStereoAmount(value);
    stereoSmoothed.setTargetValue(stereoTarget);
}

void PhaseBloomEngine::setResonance(float value)
{
    toneTargetHz = mapToneToHz(value);
    toneSmoothed.setTargetValue(toneTargetHz);
}

void PhaseBloomEngine::setMix(float value)
{
    mixTarget = mapMix(value);
    mixSmoothed.setTargetValue(mixTarget);
}

void PhaseBloomEngine::setType(int typeIndex)
{
    const int clamped = juce::jlimit(0, static_cast<int>(characters.size()) - 1, typeIndex);

    if (!prepared)
    {
        currentCharacter = targetCharacter = clamped;
        currentStages = targetStages = characters[clamped].preferredStages;
        bloomParameterCached = (static_cast<float>(targetStages) - 2.0f) / 6.0f;
        return;
    }

    if (clamped != targetCharacter)
    {
        targetCharacter = clamped;
        beginCharacterCrossfade(targetCharacter);
    }
    else if (!anyChannelCrossfading() && targetCharacter != currentCharacter)
    {
        beginCharacterCrossfade(targetCharacter);
    }

    const int desiredStages = characters[targetCharacter].preferredStages;
    if (desiredStages != targetStages || (!anyChannelCrossfading() && desiredStages != currentStages))
    {
        targetStages = desiredStages;
        bloomParameterCached = (static_cast<float>(desiredStages) - 2.0f) / 6.0f;
        beginStageCrossfade(targetStages);
    }
}

void PhaseBloomEngine::setEnabled(bool shouldBeEnabled)
{
    enabled = shouldBeEnabled;
}

juce::String PhaseBloomEngine::getRateLabel(float normalisedValue)
{
    const float minHz = 0.01f;
    const float maxHz = 6.0f;
    const float norm = juce::jlimit(0.0f, 1.0f, normalisedValue);
    const float hz = std::exp(std::log(minHz) + norm * (std::log(maxHz) - std::log(minHz)));
    return hz >= 1.0f ? juce::String(hz, 2) + " Hz"
                      : juce::String(hz, 3) + " Hz";
}

//==============================================================================
void PhaseBloomEngine::ensureChannelCount(int requiredChannels)
{
    if (static_cast<int>(channels.size()) >= requiredChannels)
        return;

    const int previousSize = static_cast<int>(channels.size());
    channels.resize(requiredChannels);

    for (int ch = previousSize; ch < requiredChannels; ++ch)
    {
        channels[ch].current.prepare(currentStages, stageSpread);
        channels[ch].pending.prepare(currentStages, stageSpread);
        channels[ch].current.reset();
        channels[ch].pending.reset();
        channels[ch].feedbackStateCurrent = 0.0f;
        channels[ch].feedbackStatePending = 0.0f;
        channels[ch].lfoPhase = 0.0f;

        channels[ch].tone.prepare(filterSpec);
        channels[ch].tone.reset();
        channels[ch].tone.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        channels[ch].tone.setResonance(0.707f);

        channels[ch].pendingTone.prepare(filterSpec);
        channels[ch].pendingTone.reset();
        channels[ch].pendingTone.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        channels[ch].pendingTone.setResonance(0.707f);

        channels[ch].crossfadeActive = false;
        channels[ch].crossfadeSamplesRemaining = 0;
        channels[ch].crossfadeSamplesTotal = 0;
    }
}

void PhaseBloomEngine::updateSmoothers()
{
    depthSmoothed.reset(sampleRate, smoothingTimeSeconds);
    rateSmoothed.reset(sampleRate, smoothingTimeSeconds);
    feedbackSmoothed.reset(sampleRate, smoothingTimeSeconds);
    centerSmoothed.reset(sampleRate, smoothingTimeSeconds);
    stereoSmoothed.reset(sampleRate, smoothingTimeSeconds);
    toneSmoothed.reset(sampleRate, smoothingTimeSeconds);
    mixSmoothed.reset(sampleRate, smoothingTimeSeconds);

    depthSmoothed.setCurrentAndTargetValue(depthTarget);
    rateSmoothed.setCurrentAndTargetValue(rateTargetHz);
    feedbackSmoothed.setCurrentAndTargetValue(feedbackTarget);
    centerSmoothed.setCurrentAndTargetValue(centerTargetHz);
    stereoSmoothed.setCurrentAndTargetValue(stereoTarget);
    toneSmoothed.setCurrentAndTargetValue(toneTargetHz);
    mixSmoothed.setCurrentAndTargetValue(mixTarget);
}

void PhaseBloomEngine::updateFilter(juce::dsp::StateVariableTPTFilter<float>& filter, float cutoffHz)
{
    filter.setCutoffFrequency(cutoffHz);
    filter.setResonance(0.707f);
}

void PhaseBloomEngine::beginStageCrossfade(int newStageCount)
{
    if (!prepared)
        return;

    newStageCount = juce::jlimit(2, 8, newStageCount);

    const int crossfadeSamples = juce::jmax(1, static_cast<int>(std::round(sampleRate * crossfadeSeconds)));

    for (auto& channel : channels)
    {
        channel.pending.prepare(newStageCount, stageSpread);
        channel.pending.copyFrom(channel.current);
        channel.feedbackStatePending = channel.feedbackStateCurrent;
        channel.pendingTone = channel.tone;
        channel.crossfadeActive = true;
        channel.crossfadeSamplesTotal = crossfadeSamples;
        channel.crossfadeSamplesRemaining = crossfadeSamples;
    }
}

void PhaseBloomEngine::beginCharacterCrossfade(int /*newCharacter*/)
{
    if (!prepared)
        return;

    const int crossfadeSamples = juce::jmax(1, static_cast<int>(std::round(sampleRate * crossfadeSeconds)));

    for (auto& channel : channels)
    {
        channel.pending.prepare(channel.current.getStageCount(), stageSpread);
        channel.pending.copyFrom(channel.current);
        channel.feedbackStatePending = channel.feedbackStateCurrent;
        channel.pendingTone = channel.tone;
        channel.crossfadeActive = true;
        channel.crossfadeSamplesTotal = crossfadeSamples;
        channel.crossfadeSamplesRemaining = crossfadeSamples;
    }
}

bool PhaseBloomEngine::anyChannelCrossfading() const
{
    for (const auto& channel : channels)
        if (channel.crossfadeActive)
            return true;
    return false;
}

//==============================================================================
float PhaseBloomEngine::mapRateToHz(float parameterValue) const
{
    const float norm = juce::jlimit(0.0f, 1.0f, parameterValue);
    const float minHz = 0.01f;
    const float maxHz = 6.0f;
    return std::exp(std::log(minHz) + norm * (std::log(maxHz) - std::log(minHz)));
}

float PhaseBloomEngine::mapFeedback(float parameterValue) const
{
    return juce::jlimit(kMinFeedback, kMaxFeedback, parameterValue);
}

float PhaseBloomEngine::mapCenterToHz(float parameterValue) const
{
    const float clamped = juce::jlimit(kMinCenterParam, kMaxCenterParam, parameterValue);
    const float logMinParam = std::log(kMinCenterParam);
    const float logMaxParam = std::log(kMaxCenterParam);
    const float norm = (std::log(clamped) - logMinParam) / (logMaxParam - logMinParam);
    return std::exp(std::log(kCenterMinHz) + norm * (std::log(kCenterMaxHz) - std::log(kCenterMinHz)));
}

int PhaseBloomEngine::mapStages(float parameterValue) const
{
    const float norm = juce::jlimit(0.0f, 1.0f, parameterValue);
    const int stages = static_cast<int>(std::round(norm * 6.0f)) + 2;
    return juce::jlimit(2, 8, stages);
}

float PhaseBloomEngine::mapStereoAmount(float parameterValue) const
{
    return juce::jlimit(0.0f, 1.0f, parameterValue);
}

float PhaseBloomEngine::computeStereoPhaseOffset(float stereoAmount,
                                                 const CharacterProfile& profile) const
{
    const float amount = juce::jlimit(0.0f, 1.0f, stereoAmount);
    return juce::degreesToRadians(amount * 120.0f);
}

std::pair<float, float> PhaseBloomEngine::computeStereoCentres(float baseHz,
                                                               float stereoAmount,
                                                               const CharacterProfile& profile) const
{
    const float spread = stereoCentreOffsetPercent * profile.centreSpreadScale * juce::jlimit(0.0f, 1.0f, stereoAmount);
    const float left = juce::jlimit(kCenterMinHz, kCenterMaxHz, baseHz * (1.0f - spread));
    const float right = juce::jlimit(kCenterMinHz, kCenterMaxHz, baseHz * (1.0f + spread));
    return { left, right };
}

float PhaseBloomEngine::mapToneToHz(float parameterValue) const
{
    const float norm = juce::jlimit(0.0f, 1.0f, parameterValue);
    return juce::jmap(norm, kToneMinHz, kToneMaxHz);
}

float PhaseBloomEngine::mapMix(float parameterValue) const
{
    return juce::jlimit(0.0f, 1.0f, parameterValue);
}