#include "SaturateProcessor.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;
    constexpr float epsilon = 1.0e-9f;

    inline float adaaTanh(float x, float& xPrev)
    {
        const float dx = x - xPrev;
        if (std::abs(dx) > 1.0e-6f)
        {
            const float result = (std::log(std::cosh(x)) - std::log(std::cosh(xPrev))) / dx;
            xPrev = x;
            return juce::jlimit(-1.0f, 1.0f, result);
        }

        const float mid = 0.5f * (x + xPrev);
        xPrev = x;
        return std::tanh(mid);
    }

    inline float adaaAtan(float x, float& xPrev)
    {
        const float dx = x - xPrev;
        if (std::abs(dx) > 1.0e-6f)
        {
            const float result = (std::log(1.0f + x * x) - std::log(1.0f + xPrev * xPrev)) / (2.0f * dx);
            xPrev = x;
            return juce::jlimit(-1.0f, 1.0f, result);
        }

        const float mid = 0.5f * (x + xPrev);
        xPrev = x;
        return std::atan(mid);
    }

    inline float softClipCubic(float x, float amount)
    {
        const float a = juce::jlimit(0.0f, 1.0f, amount);
        const float k = juce::jmap(a, 0.6f, 1.2f);
        const float y = x - k * x * x * x;
        return juce::jlimit(-3.0f, 3.0f, y);
    }

    inline float saturateAsymmetric(float x, float bias, float hardness)
    {
        const float shifted = x + bias;
        const float scale = juce::jmap(hardness, 0.1f, 1.25f);
        const float pos = std::tanh(shifted * scale);
        const float neg = std::tanh(shifted * (scale * 0.6f));
        return shifted >= 0.0f ? pos : neg;
    }

    inline float dbToLinear(float dbValue)
    {
        return juce::Decibels::decibelsToGain(dbValue, -80.0f);
    }

    inline float equalPowerFadeIn(float phase)
    {
        return std::sin(halfPi * juce::jlimit(0.0f, 1.0f, phase));
    }

    inline float equalPowerFadeOut(float phase)
    {
        return std::cos(halfPi * juce::jlimit(0.0f, 1.0f, phase));
    }

} // namespace

//==============================================================================
void SaturateProcessor::TiltFilter::prepare(double fs, float pivotHz)
{
    sampleRate = fs;
    pivot = pivotHz;
    const double omega = 2.0 * pi * pivot / sampleRate;
    alpha = std::exp(-omega);
    z = 0.0f;
    setTiltDb(0.0f);
}

void SaturateProcessor::TiltFilter::reset()
{
    z = 0.0f;
}

void SaturateProcessor::TiltFilter::setTiltDb(float tiltDb)
{
    const float half = 0.5f * tiltDb;
    lowGain = dbToLinear(half);
    highGain = dbToLinear(-half);
}

float SaturateProcessor::TiltFilter::process(float x)
{
    const float low = static_cast<float>(alpha) * z + (1.0f - static_cast<float>(alpha)) * x;
    const float high = x - low;
    z = low;
    return low * lowGain + high * highGain;
}

//==============================================================================
void SaturateProcessor::OnePole::prepare(double fs, float cutoffHz, bool highpass)
{
    sampleRate = fs;
    isHighpass = highpass;
    setCutoff(cutoffHz);
    reset();
}

void SaturateProcessor::OnePole::reset()
{
    x1 = 0.0f;
    y1 = 0.0f;
}

void SaturateProcessor::OnePole::setCutoff(float cutoffHz)
{
    cutoff = juce::jlimit(10.0f, 20000.0f, cutoffHz);
    updateCoeffs();
}

void SaturateProcessor::OnePole::updateCoeffs()
{
    const float k = std::tan(juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
    const float norm = 1.0f / (1.0f + k);

    if (isHighpass)
    {
        b0 = norm;
        b1 = -norm;
    }
    else
    {
        b0 = k * norm;
        b1 = b0;
    }

    a1 = (1.0f - k) * norm;
}

float SaturateProcessor::OnePole::process(float x)
{
    const float y = b0 * x + b1 * x1 + a1 * y1;
    x1 = x;
    y1 = y;
        return y;
    }

//==============================================================================
void SaturateProcessor::ToneFilter::prepare(double fs)
{
    sampleRate = fs;
    svf.reset();
    svf.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    svf.setCutoffFrequency(15000.0f);
    svf.setResonance(0.707f);
}

void SaturateProcessor::ToneFilter::reset()
{
    svf.reset();
}

void SaturateProcessor::ToneFilter::setTone(float norm)
{
    const float hz = juce::jmap(juce::jlimit(0.0f, 1.0f, norm), 10000.0f, 22000.0f);
    svf.setCutoffFrequency(hz);
}

float SaturateProcessor::ToneFilter::process(float x)
{
    return svf.processSample(0, x);
}

//==============================================================================
void SaturateProcessor::DiodeClipper::prepare(double fs)
{
    sampleRate = fs;
    reset();
}

void SaturateProcessor::DiodeClipper::reset()
{
    lastOut = 0.0f;
    lastDiode = 0.0f;
}

void SaturateProcessor::DiodeClipper::setParameters(float vf, float biasValue, float gbw)
{
    forwardVoltage = juce::jlimit(0.2f, 1.2f, vf);
    bias = juce::jlimit(-0.5f, 0.5f, biasValue);
    opampGbw = juce::jlimit(1.0e3f, 2.0e5f, gbw);
}

float SaturateProcessor::DiodeClipper::process(float x)
{
    const float R = 5600.0f;
    const float C = 22e-9f;
    const float dt = 1.0f / static_cast<float>(sampleRate);
    const float alpha = dt / (R * C + dt);
    const float pole = (1.0f - alpha) * lastOut + alpha * x;

    float v = lastDiode;
    const float vt = thermalVoltage;
    const float Is = std::max(saturationCurrent, 1.0e-15f);

    constexpr float maxExponent = 40.0f; // clamp to avoid overflow in exp()

    for (int i = 0; i < 4; ++i)
    {
        const float posArg = juce::jlimit(-maxExponent, maxExponent, (v + bias + forwardVoltage) / vt);
        const float negArg = juce::jlimit(-maxExponent, maxExponent, -(v + bias - forwardVoltage) / vt);

        const float expV = std::exp(posArg);
        const float expN = std::exp(negArg);
        const float Id = Is * (expV - expN);
        const float Gd = (Is / vt) * (expV + expN);

        const float f = v + R * Id - pole;
        const float df = 1.0f + R * Gd;

        v -= f / df;
    }

    lastDiode = juce::jlimit(-3.0f, 3.0f, v);
    const float sinhArg = juce::jlimit(-maxExponent, maxExponent, (v + bias) / vt);
    const float diodeCurrent = saturationCurrent * std::sinh(sinhArg);
    lastOut = juce::jlimit(-1.5f, 1.5f, pole - R * diodeCurrent);
    return lastOut;
}

//==============================================================================
float SaturateProcessor::Wavefolder::process(float x, float foldAmount)
{
    const float a = juce::jmap(juce::jlimit(0.0f, 1.0f, foldAmount), 1.0f, 4.0f);
    float y = x;
    for (int i = 0; i < 3; ++i)
    {
        y = std::fabs(y * a + 0.5f) - 0.5f;
        y = juce::jlimit(-1.0f, 1.0f, y);
    }
        return y;
    }
    
float SaturateProcessor::Rectifier::process(float x, float mix, float softness)
{
    const float sign = x >= 0.0f ? 1.0f : -1.0f;
    const float half = 0.5f * (x + std::fabs(x));
    const float full = std::fabs(x);
    const float rectMix = juce::jlimit(0.0f, 1.0f, mix);
    const float rectified = juce::jmap(rectMix, half, full);
    const float k = juce::jmap(juce::jlimit(0.0f, 1.0f, softness), 1.5f, 0.5f);
    return std::tanh(rectified * k) * sign;
}

//==============================================================================
void SaturateProcessor::ModelRuntime::reset()
{
    for (auto& ch : channels)
    {
        ch.adaaPrev = 0.0f;
        ch.hysteresis = 0.0f;
        ch.fuzzMem = 0.0f;
        ch.rectLP = 0.0f;
        ch.envelope = 0.0f;
        ch.compEnv = 0.0f;
        ch.preTilt.reset();
        ch.postTilt.reset();
        ch.preHP.reset();
        ch.toneFilter.reset();
        ch.diode.reset();
    }

    lastDrive = 12.0f;
    lastToneNorm = 0.5f;
    lastPreTilt = 0.0f;
    lastPostTilt = 0.0f;
    lastHpCut = 30.0f;
    lastCharacter = 0.5f;
    lastCharacter2 = 0.0f;
    lastBias = 0.0f;
    lastComp = 0.3f;
    lastOutputDb = 0.0f;
    autoGain = 1.0f;
    rmsInAccum = 0.0f;
    rmsOutAccum = 0.0f;
    rmsCount = 0;
}

//==============================================================================
void SaturateProcessor::prepare(double fs, int blockSize)
{
    sampleRate = fs;
    maxBlockSize = blockSize;
    osSampleRate = sampleRate * HEAT_OVERSAMPLE;

    const int osPower = (HEAT_OVERSAMPLE == 8) ? 3 : 2;

    auto createOversampler = [osPower](int channels)
    {
        return std::make_unique<juce::dsp::Oversampling<float>>(channels,
                                                                osPower,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                true,
                                                                false);
    };

    oversamplerPrimary = createOversampler(2);
    oversamplerSecondary = createOversampler(2);
    oversamplerDry = createOversampler(2);

    oversamplerPrimary->initProcessing(static_cast<size_t>(maxBlockSize));
    oversamplerSecondary->initProcessing(static_cast<size_t>(maxBlockSize));
    oversamplerDry->initProcessing(static_cast<size_t>(maxBlockSize));

    dryBuffer.setSize(2, maxBlockSize);
    inputCopyBuffer.setSize(2, maxBlockSize);
    secondaryWetBuffer.setSize(2, maxBlockSize);
    dryMatchedBuffer.setSize(2, maxBlockSize);

    for (auto& runtime : modelStates)
    {
        runtime.reset();
        for (auto& ch : runtime.channels)
        {
            ch.preTilt.prepare(osSampleRate, 750.0f);
            ch.postTilt.prepare(osSampleRate, 1600.0f);
            ch.preHP.prepare(osSampleRate, 28.0f, true);
            ch.postHP.prepare(osSampleRate, 18.0f, true);
            ch.toneFilter.prepare(osSampleRate);
            ch.diode.prepare(osSampleRate);
        }
    }

    reset();
}

void SaturateProcessor::reset()
{
    if (oversamplerPrimary) oversamplerPrimary->reset();
    if (oversamplerSecondary) oversamplerSecondary->reset();
    if (oversamplerDry) oversamplerDry->reset();

    for (auto& runtime : modelStates)
        runtime.reset();

    currentType = 0;
    crossfade = {};
}

//==============================================================================
SaturateProcessor::ParameterSet SaturateProcessor::mapParameters(float type,
                                                                 float drive,
                                                                 float color,
                                                                 float shape,
                                                                 float bias,
                                                                 float output,
                                                                 float mix) const
{
    ParameterSet p;
    p.driveDb = drive;
    p.driveLin = dbToLinear(drive);
    p.bias = bias * 0.5f;
    p.mix = juce::jlimit(0.0f, 1.0f, mix);
    p.outputDb = output;
    p.outputLin = dbToLinear(output);

    const float tone = juce::jlimit(0.0f, 1.0f, color);
    const float shapeNorm = juce::jlimit(0.0f, 1.0f, shape);
    const int modelIndex = juce::jlimit(0, numModels - 1, static_cast<int>(std::roundf(type)));

    switch (modelIndex)
    {
        case 0: // Clean
        {
            p.toneNorm = juce::jmap(tone, 0.18f, 0.92f);                 // wide sweetening range
            p.preTiltDb = juce::jmap(shapeNorm, -4.0f, 4.0f);            // tilt EQ for colour
            p.postTiltDb = -p.preTiltDb * 0.4f;
            p.hpCutHz = juce::jmap(tone, 20.0f, 42.0f);                  // tighten lows as tone rises
            p.bias = juce::jmap(bias, -0.55f, 0.55f);                    // stronger bias throw
            p.character = juce::jmap(shapeNorm, 1.0f, 3.2f);             // odd-order strength
            p.character2 = juce::jmap(tone, 0.0f, 0.65f);                // even harmonic blend
            p.compAmount = juce::jmap(shapeNorm, 0.1f, 1.0f);            // reuse as user blend
            break;
        }

        case 1: // Tape / transformer
            p.toneNorm = juce::jmap(tone, 0.25f, 0.6f);
            p.preTiltDb = juce::jmap(shapeNorm, -1.0f, 5.0f);
            p.postTiltDb = -p.preTiltDb * 0.65f;
            p.hpCutHz = juce::jmap(shapeNorm, 28.0f, 50.0f);
            p.character = juce::jmap(shapeNorm, 0.6f, 1.4f);
            p.character2 = juce::jmap(tone, 0.02f, 0.12f);
            p.compAmount = juce::jmap(tone, 0.3f, 0.5f);
            break;

        case 2: // Diode clipper
            p.toneNorm = juce::jmap(tone, 0.35f, 0.75f);
            p.preTiltDb = juce::jmap(shapeNorm, -2.0f, 2.0f);
            p.postTiltDb = juce::jmap(shapeNorm, -1.2f, 1.2f);
            p.hpCutHz = juce::jmap(shapeNorm, 26.0f, 38.0f);
            p.character = juce::jmap(shapeNorm, 0.35f, 0.85f);      // diode Vf
            p.character2 = juce::jmap(tone, 2.0e4f, 9.0e4f);        // op-amp GBW
            p.compAmount = juce::jmap(tone, 0.35f, 0.55f);
            break;

        case 3: // Op-amp soft clip
            p.toneNorm = juce::jmap(tone, 0.4f, 0.9f);
            p.preTiltDb = juce::jmap(shapeNorm, -1.5f, 1.5f);
            p.postTiltDb = juce::jmap(shapeNorm, -0.8f, 0.8f);
            p.hpCutHz = 28.0f;
            p.character = juce::jmap(shapeNorm, 6000.0f, 16000.0f);  // GBW
            p.character2 = 0.0f;
            p.compAmount = juce::jmap(tone, 0.25f, 0.55f);
            break;

        case 4: // Wavefolder
            p.toneNorm = juce::jmap(tone, 0.35f, 0.8f);
            p.preTiltDb = juce::jmap(shapeNorm, -2.5f, 2.5f);
            p.postTiltDb = juce::jmap(tone, -1.5f, 2.0f);
            p.hpCutHz = juce::jmap(shapeNorm, 30.0f, 55.0f);
            p.character = juce::jmap(shapeNorm, 0.6f, 1.8f);        // fold amount
            p.character2 = juce::jmap(tone, 0.0f, 0.35f);           // sparkle mix
            p.compAmount = juce::jmap(tone, 0.35f, 0.6f);
            break;

        case 5: // Centaur-style
            p.toneNorm = juce::jmap(tone, 0.4f, 0.7f);
            p.preTiltDb = juce::jmap(shapeNorm, -3.0f, 1.5f);
            p.postTiltDb = juce::jmap(tone, -1.0f, 2.0f);
            p.hpCutHz = juce::jmap(shapeNorm, 35.0f, 120.0f);
            p.character = juce::jmap(shapeNorm, 650.0f, 1600.0f);   // mid frequency
            p.character2 = juce::jmap(tone, 0.5f, 1.2f);            // mid Q
            p.compAmount = juce::jmap(tone, 0.25f, 0.5f);
            break;

        case 6: // Fuzz / BJT
            p.toneNorm = juce::jmap(tone, 0.3f, 0.65f);
            p.preTiltDb = juce::jmap(shapeNorm, -5.0f, 0.5f);
            p.postTiltDb = juce::jmap(tone, -1.0f, 1.5f);
            p.hpCutHz = juce::jmap(shapeNorm, 40.0f, 120.0f);
            p.character = juce::jmap(shapeNorm, 0.5f, 1.4f);        // gain hardness
            p.character2 = juce::jmap(tone, 0.2f, 0.6f);            // leakage/sag
            p.compAmount = juce::jmap(tone, 0.3f, 0.65f);
            break;

        case 7: // Rectifier / X-Dist
        default:
            p.toneNorm = juce::jmap(tone, 0.45f, 0.85f);
            p.preTiltDb = juce::jmap(shapeNorm, -1.5f, 1.5f);
            p.postTiltDb = juce::jmap(tone, -0.5f, 2.5f);
            p.hpCutHz = juce::jmap(shapeNorm, 28.0f, 60.0f);
            p.character = juce::jlimit(0.0f, 1.0f, shapeNorm);      // rect mix
            p.character2 = juce::jmap(tone, 0.4f, 0.9f);            // softness
            p.compAmount = juce::jmap(tone, 0.35f, 0.6f);
            break;
    }

    return p;
}

void SaturateProcessor::refreshFilters(ModelRuntime& runtime,
                                       const ParameterSet& params,
                                       double /*fs*/,
                                       int modelIndex)
{
    for (auto& ch : runtime.channels)
    {
        ch.preTilt.setTiltDb(params.preTiltDb);
        ch.postTilt.setTiltDb(params.postTiltDb);
        ch.preHP.setCutoff(params.hpCutHz);
        ch.toneFilter.setTone(params.toneNorm);

        if (modelIndex == 2)
            ch.diode.setParameters(params.character, params.bias, params.character2);
        else
            ch.diode.setParameters(0.65f, params.bias, 4.0e4f);
    }

    runtime.lastDrive = params.driveDb;
    runtime.lastToneNorm = params.toneNorm;
    runtime.lastPreTilt = params.preTiltDb;
    runtime.lastPostTilt = params.postTiltDb;
    runtime.lastHpCut = params.hpCutHz;
    runtime.lastCharacter = params.character;
    runtime.lastCharacter2 = params.character2;
    runtime.lastBias = params.bias;
    runtime.lastComp = params.compAmount;
    runtime.lastOutputDb = params.outputDb;
}

//==============================================================================
void SaturateProcessor::process(juce::AudioBuffer<float>& buffer,
                                int numSamples,
                                juce::AudioProcessorValueTreeState& apvts)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    auto* typeParam = apvts.getRawParameterValue("satType");
    auto* driveParam = apvts.getRawParameterValue("satDrive");
    auto* colorParam = apvts.getRawParameterValue("satColor");
    auto* shapeParam = apvts.getRawParameterValue("satShape");
    auto* biasParam = apvts.getRawParameterValue("satBias");
    auto* outParam = apvts.getRawParameterValue("satOut");
    auto* mixParam = apvts.getRawParameterValue("satMix");
    
    const int type = typeParam ? juce::jlimit(0, numModels - 1, static_cast<int>(typeParam->load())) : 0;

    const float drive = driveParam ? driveParam->load() : 12.0f;
    const float color = colorParam ? colorParam->load() : 0.5f;
    const float shape = shapeParam ? shapeParam->load() : 0.5f;
    const float bias = biasParam ? biasParam->load() : 0.0f;
    const float output = outParam ? outParam->load() : 0.0f;
    const float mix = mixParam ? mixParam->load() : 1.0f;

    ParameterSet params = mapParameters(static_cast<float>(type),
                                        drive,
                                        color,
                                        shape,
                                        bias,
                                        output,
                                        mix);

    processInternal(buffer, numSamples, params, type, false);
}

void SaturateProcessor::processWithSnapshot(juce::AudioBuffer<float>& buffer,
                                            int numSamples,
                                            float type,
                                            float drive,
                                            float color,
                                            float shape,
                                            float bias,
                                            float output,
                                            float mix,
                                            bool stepChanged)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    const int typeIndex = juce::jlimit(0, numModels - 1, static_cast<int>(type));

    ParameterSet params = mapParameters(type,
                                        drive,
                                        color,
                                        shape,
                                        bias,
                                        output,
                                        mix);

    processInternal(buffer, numSamples, params, typeIndex, stepChanged);
}

//==============================================================================
void SaturateProcessor::processInternal(juce::AudioBuffer<float>& buffer,
                                        int numSamples,
                                        ParameterSet liveParams,
                                        int targetType,
                                        bool stepChanged)
{
    juce::ScopedNoDenormals guard;

    dryBuffer.makeCopyOf(buffer, true);
    inputCopyBuffer.makeCopyOf(buffer, true);

    if (targetType != currentType)
    {
        crossfade.active = true;
        crossfade.previousType = currentType;
        crossfade.totalSamples = static_cast<int>(crossfadeTimeSeconds * sampleRate);
        crossfade.remainingSamples = crossfade.totalSamples;
        currentType = targetType;
        oversamplerSecondary->reset();
    }

    auto& runtime = modelStates[currentType];
    refreshFilters(runtime, liveParams, osSampleRate, targetType);

    renderModelToBuffer(currentType, runtime, buffer, liveParams, true);

    if (crossfade.active && crossfade.remainingSamples > 0)
    {
        const int prevType = crossfade.previousType;
        auto& prevRuntime = modelStates[prevType];
        ParameterSet storedParams;
        storedParams.driveDb = prevRuntime.lastDrive;
        storedParams.driveLin = dbToLinear(prevRuntime.lastDrive);
        storedParams.bias = prevRuntime.lastBias;
        storedParams.toneNorm = prevRuntime.lastToneNorm;
        storedParams.preTiltDb = prevRuntime.lastPreTilt;
        storedParams.postTiltDb = prevRuntime.lastPostTilt;
        storedParams.hpCutHz = prevRuntime.lastHpCut;
        storedParams.character = prevRuntime.lastCharacter;
        storedParams.character2 = prevRuntime.lastCharacter2;
        storedParams.compAmount = prevRuntime.lastComp;
        storedParams.mix = liveParams.mix;
        storedParams.outputDb = prevRuntime.lastOutputDb;
        storedParams.outputLin = dbToLinear(prevRuntime.lastOutputDb);

        secondaryWetBuffer.makeCopyOf(inputCopyBuffer, true);
        refreshFilters(prevRuntime, storedParams, osSampleRate, prevType);
        renderModelToBuffer(prevType, prevRuntime, secondaryWetBuffer, storedParams, false);

        auto* wetL = buffer.getWritePointer(0);
        auto* wetR = buffer.getWritePointer(1);
        auto* oldL = secondaryWetBuffer.getWritePointer(0);
        auto* oldR = secondaryWetBuffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float phase = 1.0f - static_cast<float>(crossfade.remainingSamples) / static_cast<float>(crossfade.totalSamples);
            const float fadeIn = equalPowerFadeIn(phase);
            const float fadeOut = equalPowerFadeOut(phase);
            wetL[i] = wetL[i] * fadeIn + oldL[i] * fadeOut;
            wetR[i] = wetR[i] * fadeIn + oldR[i] * fadeOut;
            --crossfade.remainingSamples;
            if (crossfade.remainingSamples <= 0)
                break;
        }

        if (crossfade.remainingSamples <= 0)
        {
            crossfade.active = false;
            oversamplerSecondary->reset();
        }
    }

    dryMatchedBuffer.makeCopyOf(dryBuffer, true);
    juce::dsp::AudioBlock<float> dryBlock(dryMatchedBuffer);
    auto dryUp = oversamplerDry->processSamplesUp(dryBlock);
    juce::ignoreUnused(dryUp);
    oversamplerDry->processSamplesDown(dryBlock);

    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);
    auto* dryL = dryMatchedBuffer.getWritePointer(0);
    auto* dryR = dryMatchedBuffer.getWritePointer(1);

    const float mixVal = juce::jlimit(0.0f, 1.0f, liveParams.mix);
    const float dryGain = 1.0f - mixVal;
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = dryL[i] * dryGain + outL[i] * mixVal;
        outR[i] = dryR[i] * dryGain + outR[i] * mixVal;
    }
}

//==============================================================================
void SaturateProcessor::renderModelToBuffer(int modelIndex,
                                            ModelRuntime& runtime,
                                            juce::AudioBuffer<float>& workBuffer,
                                            const ParameterSet& params,
                                            bool updateRuntimeParams)
{
    juce::dsp::AudioBlock<float> block(workBuffer);
    auto osBlock = updateRuntimeParams ? oversamplerPrimary->processSamplesUp(block)
                                       : oversamplerSecondary->processSamplesUp(block);

    processOversampledBlock(modelIndex, runtime, osBlock, params);

    if (updateRuntimeParams)
        oversamplerPrimary->processSamplesDown(block);
    else
        oversamplerSecondary->processSamplesDown(block);
}

//==============================================================================
void SaturateProcessor::processOversampledBlock(int modelIndex,
                                                ModelRuntime& runtime,
                                                juce::dsp::AudioBlock<float>& osBlock,
                                                const ParameterSet& params)
{
    auto* left = osBlock.getChannelPointer(0);
    auto* right = osBlock.getChannelPointer(1);
    const int osSamples = static_cast<int>(osBlock.getNumSamples());

    auto& leftState = runtime.channels[0];
    auto& rightState = runtime.channels[1];

    runtime.rmsInAccum = 0.0f;
    runtime.rmsOutAccum = 0.0f;
    runtime.rmsCount = osSamples;

    const int modelIndexClamped = juce::jlimit(0, numModels - 1, modelIndex);
    const bool isCleanModel = (modelIndexClamped == 0);
    const float driveBlendGlobal = juce::jlimit(0.0f, 1.0f, params.driveDb / 24.0f);

    for (int i = 0; i < osSamples; ++i)
    {
        const float driveLin = params.driveLin;
        const float rawBias = params.bias;
        const float compValue = params.compAmount;
        const float characterValue = params.character;

        float l = left[i];
        float r = right[i];

        runtime.rmsInAccum += 0.5f * (l * l + r * r);

        l = leftState.preHP.process(l);
        r = rightState.preHP.process(r);

        l = leftState.preTilt.process(l);
        r = rightState.preTilt.process(r);

        l = juce::jlimit(-4.0f, 4.0f, l * driveLin + rawBias);
        r = juce::jlimit(-4.0f, 4.0f, r * driveLin + rawBias);

        const float driveBlend = driveBlendGlobal;

        switch (modelIndexClamped)
        {
            case 0: // Clean
            {
                auto processCleanStage = [&](float& sample, ModelChannelState& state)
                {
                    if (driveBlend <= 0.0001f)
                    {
                        state.envelope *= 0.995f;
                        return;
                    }

                    const float oddGainBase = juce::jlimit(1.0f, 2.2f, params.character);
                    const float oddGain = juce::jmap(driveBlend, 0.0f, 1.0f, 1.0f, oddGainBase);
                    const float userBlend = juce::jlimit(0.0f, 1.0f, params.compAmount);

                    state.envelope = 0.995f * state.envelope + 0.005f * sample;
                    const float dynamicBias = juce::jlimit(-0.35f, 0.35f,
                                                           params.bias + state.envelope * (0.08f * userBlend));
                    const float biasedSample = juce::jlimit(-2.5f, 2.5f, sample + dynamicBias);

                    const float oddComponent = adaaTanh(biasedSample * oddGain, state.adaaPrev);

                    const float cubicSoft = softClipCubic(biasedSample, 0.6f);
                    const float triodeApprox = juce::jlimit(-1.8f, 1.8f,
                                                            (biasedSample * (1.5f + 0.5f * biasedSample * biasedSample)) /
                                                            (1.5f + std::abs(biasedSample)));
                    const float evenBlend = juce::jlimit(0.0f, 0.6f, params.character2);
                    float evenComponent = juce::jmap(evenBlend, 0.0f, 0.6f, cubicSoft, triodeApprox);

                    float result = juce::jlimit(-1.2f, 1.2f,
                                                0.9f * oddComponent + 0.1f * (driveBlend * evenComponent));
                    float processed = juce::jlimit(-1.0f, 1.0f, 0.75f * result + 0.25f * sample);
                    const float stageMix = juce::jlimit(0.0f, 1.0f, driveBlend * userBlend);
                    sample = juce::jlimit(-1.0f, 1.0f, sample + stageMix * (processed - sample));
                };

                processCleanStage(l, leftState);
                processCleanStage(r, rightState);
                break;
            }
            case 1: // Tape / transformer
            {
                const float cubic = juce::jlimit(0.4f, 2.0f, characterValue);
                l = softClipCubic(l, cubic);
                r = softClipCubic(r, cubic);
                leftState.hysteresis = 0.97f * leftState.hysteresis + 0.03f * l;
                rightState.hysteresis = 0.97f * rightState.hysteresis + 0.03f * r;
                l = 0.75f * l + 0.25f * leftState.hysteresis;
                r = 0.75f * r + 0.25f * rightState.hysteresis;
                break;
            }
            case 2: // Diode asym clip
            {
                l = leftState.diode.process(l);
                r = rightState.diode.process(r);
                break;
            }
            case 3: // Op-amp soft clip
            {
                const float gbw = juce::jlimit(2000.0f, 20000.0f, characterValue);
                const float slew = 1.0f / juce::jmax(1000.0f, gbw);
                l = adaaAtan(l * (1.0f + slew), leftState.adaaPrev);
                r = adaaAtan(r * (1.0f + slew), rightState.adaaPrev);
                break;
            }
            case 4: // Wavefolder
            {
                const float fold = juce::jlimit(0.4f, 2.0f, characterValue);
                l = Wavefolder{}.process(l, fold);
                r = Wavefolder{}.process(r, fold);
                break;
            }
            case 5: // Centaur style
            {
                const float midFreq = juce::jlimit(200.0f, 3000.0f, params.character);
                const float q = juce::jlimit(0.3f, 2.0f, params.character2);
                float midL = leftState.postTilt.process(l);
                float midR = rightState.postTilt.process(r);
                juce::ignoreUnused(midFreq);
                l = 0.7f * l + 0.3f * midL;
                r = 0.7f * r + 0.3f * midR;
                const float asymBias = rawBias * 0.5f;
                l = saturateAsymmetric(l, asymBias, q);
                r = saturateAsymmetric(r, asymBias, q);
                break;
            }
            case 6: // Fuzz / BJT
            {
                leftState.fuzzMem = 0.92f * leftState.fuzzMem + 0.08f * l;
                rightState.fuzzMem = 0.92f * rightState.fuzzMem + 0.08f * r;
                const float fuzzHard = juce::jlimit(0.4f, 1.6f, characterValue);
                const float asymBias = rawBias * 1.5f;
                l = saturateAsymmetric(l, asymBias, fuzzHard);
                r = saturateAsymmetric(r, asymBias, fuzzHard);
                l = 0.8f * l + 0.2f * leftState.fuzzMem;
                r = 0.8f * r + 0.2f * rightState.fuzzMem;
                break;
            }
            case 7: // Rectifier / X-Dist
            {
                const float rectMix = characterValue;
                const float softness = juce::jlimit(0.0f, 1.0f, params.toneNorm);
                l = Rectifier{}.process(l, rectMix, softness);
                r = Rectifier{}.process(r, rectMix, softness);
                break;
            }
            default:
                break;
        }

        l = leftState.postTilt.process(l);
        r = rightState.postTilt.process(r);

        l = leftState.toneFilter.process(l);
        r = rightState.toneFilter.process(r);

        l = leftState.postHP.process(l);
        r = rightState.postHP.process(r);

        if (!isCleanModel)
        {
            const float thresh = juce::jmap(compValue, 0.1f, 0.6f);
            const float knee = juce::jmap(compValue, 2.0f, 6.0f);

            auto applySoftLimiter = [&](float input, ModelChannelState& state) {
                const float absIn = std::abs(input);
                const float envCoeff = std::exp(-1.0f / (compReleaseSeconds * static_cast<float>(osSampleRate)));
                state.compEnv = envCoeff * state.compEnv + (1.0f - envCoeff) * absIn;
                if (state.compEnv <= thresh)
                    return input;

                const float over = state.compEnv / thresh;
                const float gain = std::pow(over, -knee);
                return input * juce::jlimit(0.0f, 1.0f, gain);
            };

            l = applySoftLimiter(l, leftState);
            r = applySoftLimiter(r, rightState);
        }

        left[i] = juce::jlimit(-1.0f, 1.0f, l);
        right[i] = juce::jlimit(-1.0f, 1.0f, r);
        runtime.rmsOutAccum += 0.5f * (left[i] * left[i] + right[i] * right[i]);
    }

    updateAutoGain(runtime, osSamples, !isCleanModel);

    const float finalGain = runtime.autoGain * params.outputLin;

    for (int i = 0; i < osSamples; ++i)
    {
        left[i] = juce::jlimit(-1.0f, 1.0f, left[i] * finalGain);
        right[i] = juce::jlimit(-1.0f, 1.0f, right[i] * finalGain);
    }
}

//==============================================================================
void SaturateProcessor::updateAutoGain(ModelRuntime& runtime, int osSamples, bool allowHeavyAttenuation)
{
    if (osSamples <= 0)
        return;

    const float rmsIn = std::sqrt(runtime.rmsInAccum / static_cast<float>(osSamples) + epsilon);
    const float rmsOut = std::sqrt(runtime.rmsOutAccum / static_cast<float>(osSamples) + epsilon);
    float target = 1.0f;
    if (rmsOut > 0.0f)
        target = juce::jlimit(0.25f, 4.0f, rmsIn / rmsOut);

    const float minGain = allowHeavyAttenuation ? 0.25f : 0.75f;
    target = juce::jlimit(minGain, 4.0f, target);
    if (!std::isfinite(target))
        target = 1.0f;

    const float coeff = std::exp(-1.0f / (autoGainTauSeconds * static_cast<float>(osSampleRate)));
    runtime.autoGain = coeff * runtime.autoGain + (1.0f - coeff) * target;
}
