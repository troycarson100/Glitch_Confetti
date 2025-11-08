#pragma once

#include <array>
#include <utility>
#include <vector>

#include <juce_dsp/juce_dsp.h>

/**
 * PhaseBloomEngine
 *
 * High-quality stereo phaser inspired by CHOWPhaser.  Implements a ladder of
 * first-order allpass sections with smoothed parameter control, stereo width,
 * soft-clipped feedback, tone control, and clickless topology/mode transitions.
 * UI/layout for PhaseBloom remains untouched – this class performs all DSP.
 */
class PhaseBloomEngine
{
public:
    PhaseBloomEngine();

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    // Parameter setters (raw values as delivered by existing UI/APVTS)
    void setDepth(float value);
    void setRate(float value);
    void setFeedback(float value);
    void setCenter(float value);
    void setBloom(float value);      // mapped to stage count
    void setSpread(float value);     // stereo amount
    void setResonance(float value);  // tone control
    void setMix(float value);
    void setType(int typeIndex);
    void setEnabled(bool shouldBeEnabled);

    static juce::String getRateLabel(float normalisedValue);

private:
    struct CharacterProfile
    {
        int preferredStages = 6;
        int stageLimit = 6;
        float feedbackCap = 0.8f;
        float depthScale = 1.0f;
        float toneMultiplier = 1.0f;
        float stereoFloor = 0.0f;
        float centreSpreadScale = 1.0f;
        float softClipAmount = 0.8f;
    };

    struct FirstOrderAllpass
    {
        float z = 0.0f;
        float multiplier = 1.0f;
    };

    struct AllpassLadder
    {
        void prepare(int numStages, float spread);
        void reset();
        void copyFrom(const AllpassLadder& other);
        float process(float input, float baseFrequencyHz, float sampleRate);
        int getStageCount() const { return static_cast<int>(stages.size()); }

    private:
        std::vector<FirstOrderAllpass> stages;
    };

    struct ChannelState
    {
        AllpassLadder current;
        AllpassLadder pending;
        juce::dsp::StateVariableTPTFilter<float> tone;
        juce::dsp::StateVariableTPTFilter<float> pendingTone;
        bool crossfadeActive = false;
        int crossfadeSamplesRemaining = 0;
        int crossfadeSamplesTotal = 0;
        float feedbackStateCurrent = 0.0f;
        float feedbackStatePending = 0.0f;
        float lfoPhase = 0.0f;
    };

    void ensureChannelCount(int requiredChannels);
    void updateSmoothers();
    void updateFilter(juce::dsp::StateVariableTPTFilter<float>& filter, float cutoffHz);

    void beginStageCrossfade(int newStageCount);
    void beginCharacterCrossfade(int newCharacter);
    bool anyChannelCrossfading() const;

    float mapRateToHz(float parameterValue) const;
    float mapFeedback(float parameterValue) const;
    float mapCenterToHz(float parameterValue) const;
    int mapStages(float parameterValue) const;
    float mapStereoAmount(float parameterValue) const;
    float computeStereoPhaseOffset(float stereoAmount, const CharacterProfile& profile) const;
    std::pair<float, float> computeStereoCentres(float baseHz,
                                                 float stereoAmount,
                                                 const CharacterProfile& profile) const;
    float mapToneToHz(float parameterValue) const;
    float mapMix(float parameterValue) const;

    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    int numChannels = 2;
    bool prepared = false;
    bool enabled = false;

    juce::LinearSmoothedValue<float> depthSmoothed;
    juce::LinearSmoothedValue<float> rateSmoothed;
    juce::LinearSmoothedValue<float> feedbackSmoothed;
    juce::LinearSmoothedValue<float> centerSmoothed;
    juce::LinearSmoothedValue<float> stereoSmoothed;
    juce::LinearSmoothedValue<float> toneSmoothed;
    juce::LinearSmoothedValue<float> mixSmoothed;

    float depthTarget = 0.5f;
    float rateTargetHz = 0.5f;
    float feedbackTarget = 0.0f;
    float centerTargetHz = 1000.0f;
    float stereoTarget = 0.0f;
    float toneTargetHz = 16000.0f;
    float mixTarget = 0.5f;

    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::ProcessSpec filterSpec { 48000.0, 512, 1 };
    std::vector<ChannelState> channels;

    static constexpr float crossfadeSeconds = 0.015f;
    static constexpr float stereoCentreOffsetPercent = 0.03f;
    static constexpr float sweepOctaves = 2.0f;

    float bloomParameterCached = 0.2f;

    int currentStages = 4;
    int targetStages = 4;

    std::array<CharacterProfile, 3> characters{};
    int currentCharacter = 1; // Type B default
    int targetCharacter = 1;
};
