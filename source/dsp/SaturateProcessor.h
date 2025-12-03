#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

#ifndef HEAT_OVERSAMPLE
    #define HEAT_OVERSAMPLE 4
#endif

static_assert(HEAT_OVERSAMPLE == 4 || HEAT_OVERSAMPLE == 8, "HEAT_OVERSAMPLE must be either 4 or 8");

/**
 * SaturateProcessor
 *
 * Modernised "Heat" engine featuring 8 analog-inspired drive models.
 * - Dual-render crossfade for clickless type changes
 * - 4x/8x oversampling with compile-time HEAT_OVERSAMPLE toggle
 * - ADAA anti-aliasing for memoryless waveshapers
 * - Per-model auto-gain, soft-knee post drive compressor, and tone/EQ staging
 */
class SaturateProcessor
{
public:
    SaturateProcessor() = default;
    ~SaturateProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void process(juce::AudioBuffer<float>& buffer,
                 int numSamples,
                 juce::AudioProcessorValueTreeState& apvts);

    void processWithSnapshot(juce::AudioBuffer<float>& buffer,
                             int numSamples,
                             float type,
                             float drive,
                             float color,
                             float shape,
                             float bias,
                             float output,
                             float mix,
                             bool stepChanged = false);

private:
    static constexpr int numModels = 8;
    static constexpr float crossfadeTimeSeconds = 0.02f; // 20 ms
    static constexpr float autoGainTauSeconds = 0.18f;   // slow ~180 ms
    static constexpr float compReleaseSeconds = 0.12f;

    //==============================================================================
    struct TiltFilter
    {
        void prepare(double fs, float pivotHz = 800.0f);
        void reset();
        void setTiltDb(float tiltDb);
        float process(float x);

    private:
        double sampleRate = 44100.0;
        float pivot = 800.0f;
        float alpha = 0.0f;
        float z = 0.0f;
        float lowGain = 1.0f;
        float highGain = 1.0f;
    };

    struct OnePole
    {
        void prepare(double fs, float cutoffHz, bool highpass);
        void reset();
        float process(float x);
        void setCutoff(float cutoffHz);

    private:
        double sampleRate = 44100.0;
        float cutoff = 1000.0f;
        bool isHighpass = false;
        float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
        float x1 = 0.0f, y1 = 0.0f;
        void updateCoeffs();
    };

    struct ToneFilter
    {
        void prepare(double fs);
        void reset();
        void setTone(float norm); // norm 0..1 -> 10k..22k
        float process(float x);

    private:
        juce::dsp::StateVariableTPTFilter<float> svf;
        double sampleRate = 44100.0;
        bool isPrepared = false; // Track if filter is properly prepared
        std::atomic<float> pendingToneNorm { 0.5f }; // Store pending tone update to apply safely
        std::atomic<bool> hasPendingUpdate { false }; // Flag to indicate pending update
        std::atomic<bool> isBypassed { false }; // CRITICAL: Bypass flag to prevent crashes from corrupted filter
        std::atomic<int> errorCount { 0 }; // Track consecutive errors to permanently bypass if too many
    };

    struct DiodeClipper
    {
        void prepare(double fs);
        void reset();
        void setParameters(float vf, float bias, float opampGbw);
        float process(float x);

    private:
        double sampleRate = 44100.0;
        float thermalVoltage = 0.026f;
        float saturationCurrent = 1.0e-12f;
        float forwardVoltage = 0.7f;
        float bias = 0.0f;
        float opampGbw = 5.0e4f;
        float lastOut = 0.0f;
        float lastDiode = 0.0f;
    };

    struct Wavefolder
    {
        float process(float x, float foldAmount);
    };

    struct Rectifier
    {
        float process(float x, float mix, float softness);
    };

    struct ModelChannelState
    {
        // General purpose states reused per model
        float adaaPrev = 0.0f;
        float hysteresis = 0.0f;
        float fuzzMem = 0.0f;
        float rectLP = 0.0f;
        float envelope = 0.0f;
        float compEnv = 0.0f;

        TiltFilter preTilt;
        TiltFilter postTilt;
        OnePole preHP;
        OnePole postHP;
        ToneFilter toneFilter;
        DiodeClipper diode;
    };

    struct ModelRuntime
    {
        std::array<ModelChannelState, 2> channels {};

        float lastDrive = 12.0f;
        float lastToneNorm = 0.5f;
        float lastPreTilt = 0.0f;
        float lastPostTilt = 0.0f;
        float lastHpCut = 30.0f;
        float lastCharacter = 0.5f;
        float lastCharacter2 = 0.0f;
        float lastBias = 0.0f;
        float lastComp = 0.3f;
        float lastOutputDb = 0.0f;
        float autoGain = 1.0f;
        float rmsInAccum = 0.0f;
        float rmsOutAccum = 0.0f;
        int rmsCount = 0;

        void reset();
    };

    struct ParameterSet
    {
        float driveDb = 12.0f;
        float driveLin = 4.0f;
        float bias = 0.0f;
        float toneNorm = 0.5f;
        float preTiltDb = 0.0f;
        float postTiltDb = 0.0f;
        float hpCutHz = 30.0f;
        float character = 0.5f;
        float character2 = 0.0f;
        float compAmount = 0.3f;
        float outputDb = 0.0f;
        float outputLin = 1.0f;
        float mix = 1.0f;
    };

    struct CrossfadeState
    {
        bool active = false;
        int remainingSamples = 0;
        int totalSamples = 0;
        int previousType = 0;
    };

    //==============================================================================
    void processInternal(juce::AudioBuffer<float>& buffer,
                         int numSamples,
                         ParameterSet liveParams,
                         int targetType,
                         bool stepChanged);

    void renderModelToBuffer(int modelIndex,
                             ModelRuntime& runtime,
                             juce::AudioBuffer<float>& workBuffer,
                             const ParameterSet& params,
                             bool updateRuntimeParams);

    void processOversampledBlock(int modelIndex,
                                 ModelRuntime& runtime,
                                 juce::dsp::AudioBlock<float>& osBlock,
                                 const ParameterSet& params);

    ParameterSet mapParameters(float type,
                               float drive,
                               float color,
                               float shape,
                               float bias,
                               float output,
                               float mix) const;

    void refreshFilters(ModelRuntime& runtime,
                        const ParameterSet& params,
                        double osSampleRate,
                        int modelIndex);

    void updateAutoGain(ModelRuntime& runtime, int osSamples, bool allowHeavyAttenuation);

    // Helper functions
    float applyWaveshape(float x, float shapeParam) const;
    float addHarmonics(float saturated, float driven, const ParameterSet& params) const;
    float blendDryWet(float dry, float wet, float mix) const;

    //==============================================================================
    double sampleRate = 44100.0;
    double osSampleRate = 44100.0 * HEAT_OVERSAMPLE;
    int maxBlockSize = 512;

    int currentType = 0;
    CrossfadeState crossfade;

    std::array<ModelRuntime, numModels> modelStates;

    // Oversamplers
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplerPrimary;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplerSecondary;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplerDry;

    // Working buffers
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> inputCopyBuffer;
    juce::AudioBuffer<float> secondaryWetBuffer;
    juce::AudioBuffer<float> dryMatchedBuffer;

    // Parameter smoothing to prevent clicks
    juce::LinearSmoothedValue<float> driveSmooth;
    juce::LinearSmoothedValue<float> colorSmooth;
    juce::LinearSmoothedValue<float> shapeSmooth;
    juce::LinearSmoothedValue<float> biasSmooth;
    juce::LinearSmoothedValue<float> outputSmooth;
    juce::LinearSmoothedValue<float> mixSmooth;
    
    // Track previous filter values to prevent clicks
    float prevPreTiltDb = 0.0f;
    float prevPostTiltDb = 0.0f;
    float prevHpCutHz = 30.0f;
    float prevToneNorm = 0.5f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturateProcessor)
};

