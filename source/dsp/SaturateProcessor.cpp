#include "SaturateProcessor.h"
#include <cmath>

// Model implementations

struct SatSpiral2 : public ISat {
    float density = 0.5f;
    float asym = 0.4f;
    float bias = 0.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float density_, float asym_, float bias_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        density = density_;
        asym = asym_;
        bias = bias_;
    }
    
    float process(float x) override {
        float s = gain * (x + bias);
        float even = std::tanh(s);
        float odd = s / (1.0f + std::abs(s)); // Soft odd harmonics
        float y = juce::jmap(density, even, 0.6f * even + 0.4f * odd);
        
        // Polarity skew
        if (asym > 0.01f) {
            y = (1.0f - asym) * y + asym * ((s >= 0 ? y : -y));
        }
        
        return y;
    }
};

struct SatDensity2 : public ISat {
    float thickness = 0.5f;
    float focus = 0.5f;
    float locut = 100.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float thickness_, float focus_, float locut_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        thickness = thickness_;
        focus = focus_;
        locut = locut_;
    }
    
    float process(float x) override {
        float s = gain * x;
        
        // Two-stage soft clip stack
        float stage1 = std::tanh(s * (0.7f + 0.3f * thickness));
        float stage2 = std::tanh(stage1 * (0.8f + 0.2f * focus));
        
        return stage2;
    }
};

struct SatDrive : public ISat {
    float hardness = 0.5f;
    float asym = 0.5f;
    float locut = 100.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float hardness_, float asym_, float locut_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        hardness = hardness_;
        asym = asym_;
        locut = locut_;
    }
    
    float process(float x) override {
        float s = gain * x;
        float exponent = juce::jmap(hardness, 1.0f, 2.5f);
        float y = s / std::pow(1.0f + std::abs(s), exponent);
        
        // Asymmetry
        if (asym > 0.01f) {
            y = (1.0f - asym) * y + asym * ((x >= 0 ? y : -std::abs(y)));
        }
        
        return y;
    }
};

struct SatPurestDrive : public ISat {
    float saturation = 0.5f;
    float airGain = 0.0f;
    float locut = 100.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float sat_, float air_, float locut_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        saturation = sat_;
        airGain = juce::Decibels::decibelsToGain(air_);
        locut = locut_;
    }
    
    float process(float x) override {
        float s = gain * x;
        
        // Ultra-clean saturation
        float y = s / (1.0f + std::abs(s) * (0.3f + 0.7f * saturation));
        
        // Subtle high shelf
        if (airGain > 1.01f || airGain < 0.99f) {
            // Simple high-frequency boost
            float air = y * airGain;
            y = y + 0.1f * (air - y);
        }
        
        return y;
    }
};

struct SatMojo : public ISat {
    float warmth = 0.5f;
    float presence = 0.5f;
    float hicut = 12.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float warmth_, float presence_, float hicut_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        warmth = warmth_;
        presence = presence_;
        hicut = hicut_;
    }
    
    float process(float x) override {
        float s = gain * x;
        float y = std::tanh(s * (0.6f + 0.4f * warmth));
        
        // Gentle presence boost
        if (presence > 0.01f) {
            y = y * (1.0f + presence * 0.2f);
        }
        
        return y;
    }
};

struct SatConsole : public ISat {
    float trim = 0.0f;
    float focus = 0.5f;
    float hicut = 12.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float trim_, float focus_, float hicut_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        trim = juce::Decibels::decibelsToGain(trim_);
        focus = focus_;
        hicut = hicut_;
    }
    
    float process(float x) override {
        float s = gain * trim * x;
        
        // Subtle nonlinear
        float y = s / (1.0f + std::abs(s) * 0.5f);
        
        // Frequency pre/de-emphasis based on focus
        y = y * (1.0f + focus * 0.15f);
        
        return y;
    }
};

struct SatCoils : public ISat {
    float iron = 0.5f;
    float asym = 0.5f;
    float hiBump = 0.0f;
    float gain = 1.0f;
    
    void setParams(float drive, float iron_, float asym_, float hiBump_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        iron = iron_;
        asym = asym_;
        hiBump = juce::Decibels::decibelsToGain(hiBump_);
    }
    
    float process(float x) override {
        float s = gain * x;
        float y = std::tanh(s * (0.7f + 0.3f * iron));
        
        // Asymmetry
        if (asym > 0.01f) {
            y = (1.0f - asym) * y + asym * ((x >= 0 ? y : -std::abs(y)));
        }
        
        // 3-5kHz bump
        if (hiBump > 1.01f) {
            y = y * (1.0f + 0.15f * (hiBump - 1.0f));
        }
        
        return y;
    }
};

struct SatTubey : public ISat {
    float evenBlend = 0.5f;
    float sag = 0.0f;
    float locut = 100.0f;
    float gain = 1.0f;
    float envelope = 0.0f;
    static constexpr float envCoeff = 0.995f; // One-pole envelope follower
    
    void setParams(float drive, float even_, float sag_, float locut_) override {
        gain = juce::Decibels::decibelsToGain(drive);
        evenBlend = even_;
        sag = sag_;
        locut = locut_;
    }
    
    float process(float x) override {
        float s = gain * x;
        
        // Even-harmonic bias
        float y;
        if (evenBlend > 0.5f) {
            // More even harmonics (symmetric)
            y = std::tanh(s);
        } else {
            // More odd harmonics (asymmetric)
            y = s / (1.0f + std::abs(s));
        }
        y = juce::jmap(evenBlend, s / (1.0f + std::abs(s)), std::tanh(s));
        
        // Compressor-like sag
        if (sag > 0.01f) {
            float envTarget = std::abs(y);
            envelope = envelope * envCoeff + envTarget * (1.0f - envCoeff);
            float sagAmount = 1.0f - sag * envelope * 0.3f;
            y *= sagAmount;
        }
        
        return y;
    }
    
    void reset() override {
        envelope = 0.0f;
    }
};

// SaturateProcessor implementation

SaturateProcessor::SaturateProcessor()
{
}

void SaturateProcessor::prepare(double sampleRate_, int maxBlockSize_)
{
    sampleRate = sampleRate_;
    maxBlockSize = maxBlockSize_;
    
    // Create oversampler (8× max = 2^3)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, // 2 channels
        3, // log2(8) = 3
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,  // stereo
        false  // non-latency mode
    );
    
    oversampler->initProcessing(static_cast<size_t>(maxBlockSize));
    currentOsFactor = 8; // Track as factor (1, 2, 4, 8)
    
    // Setup input HPF (25 Hz at native rate, will be updated for OS)
    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate,
        25.0f
    );
    inputHPF.coefficients = hpfCoeffs;
    inputHPF.reset();
    
    // Create models
    createModels();
    
    // Initialize smoothing
    const double smoothTime = 0.02; // 20ms
    driveSm.reset(sampleRate, smoothTime);
    colorSm.reset(sampleRate, smoothTime);
    shapeSm.reset(sampleRate, smoothTime);
    biasSm.reset(sampleRate, smoothTime);
    outSm.reset(sampleRate, smoothTime);
    mixSm.reset(sampleRate, smoothTime);
    
    driveSm.setCurrentAndTargetValue(12.0f);
    colorSm.setCurrentAndTargetValue(0.5f);
    shapeSm.setCurrentAndTargetValue(0.4f);
    biasSm.setCurrentAndTargetValue(0.0f);
    outSm.setCurrentAndTargetValue(0.0f);
    mixSm.setCurrentAndTargetValue(1.0f);
}

void SaturateProcessor::createModels()
{
    models[0] = std::make_unique<SatSpiral2>();
    models[1] = std::make_unique<SatDensity2>();
    models[2] = std::make_unique<SatDrive>();
    models[3] = std::make_unique<SatPurestDrive>();
    models[4] = std::make_unique<SatMojo>();
    models[5] = std::make_unique<SatConsole>();
    models[6] = std::make_unique<SatCoils>();
    models[7] = std::make_unique<SatTubey>();
    
    currentModel = std::make_unique<SatSpiral2>();
    currentType = 0;
}

void SaturateProcessor::switchModel(size_t type)
{
    if (type == static_cast<size_t>(currentType) || type >= 8) return;
    
    currentType = static_cast<int>(type);
    
    // Ensure model exists before moving
    if (models[type] != nullptr) {
        currentModel = std::move(models[type]);
        
        // Create new model instance for next time
        createModels();
    } else {
        // Fallback: create default model if something went wrong
        currentModel = std::make_unique<SatSpiral2>();
    }
    
    // Start crossfade
    crossfadeValue = 0.0f;
    crossfadeTarget = 1.0f;
}

void SaturateProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples, juce::AudioProcessorValueTreeState& apvts)
{
    if (numSamples == 0 || sampleRate <= 0.0) return;
    if (buffer.getNumChannels() < 2) return;
    
    juce::ScopedNoDenormals noDenormals;
    
    // Read parameters
    auto* typeParam = apvts.getRawParameterValue("satType");
    auto* driveParam = apvts.getRawParameterValue("satDrive");
    auto* colorParam = apvts.getRawParameterValue("satColor");
    auto* shapeParam = apvts.getRawParameterValue("satShape");
    auto* biasParam = apvts.getRawParameterValue("satBias");
    auto* outParam = apvts.getRawParameterValue("satOut");
    auto* osModeParam = apvts.getRawParameterValue("satOsMode");
    auto* mixParam = apvts.getRawParameterValue("satMix");
    
    // Get parameters and convert from normalized to actual values
    int type = typeParam ? juce::jlimit(0, 7, static_cast<int>(typeParam->load())) : 0;
    float drive = driveParam ? juce::jlimit(0.0f, 36.0f, driveParam->load()) : 12.0f;
    float color = colorParam ? juce::jlimit(0.0f, 1.0f, colorParam->load()) : 0.5f;
    float shape = shapeParam ? juce::jlimit(0.0f, 1.0f, shapeParam->load()) : 0.4f;
    float bias = biasParam ? juce::jlimit(-0.2f, 0.2f, biasParam->load()) : 0.0f;
    int osMode = osModeParam ? juce::jlimit(0, 3, static_cast<int>(osModeParam->load())) : 2;
    float out = outParam ? juce::jlimit(-24.0f, 12.0f, outParam->load()) : 0.0f;
    float mix = mixParam ? juce::jlimit(0.0f, 1.0f, mixParam->load()) : 1.0f;
    
    // Switch model if needed
    if (type != currentType) {
        if (type >= 0 && type < 8) {
            switchModel(static_cast<size_t>(type));
        }
    }
    
    // Update oversampling factor (don't recreate, just track)
    int osFactor = 1 << osMode; // 1, 2, 4, 8
    currentOsFactor = osFactor; // Just track, JUCE handles switching
    
    // Update smoothed parameters
    driveSm.setTargetValue(drive);
    colorSm.setTargetValue(color);
    shapeSm.setTargetValue(shape);
    biasSm.setTargetValue(bias);
    outSm.setTargetValue(out);
    mixSm.setTargetValue(mix);
    
    // Get current smoothed values
    float driveVal = driveSm.getCurrentValue();
    float colorVal = colorSm.getCurrentValue();
    float shapeVal = shapeSm.getCurrentValue();
    float biasVal = biasSm.getCurrentValue();
    float outVal = outSm.getCurrentValue();
    float mixVal = mixSm.getCurrentValue();
    
    // Update model parameters - ensure model exists
    if (!currentModel) {
        // Fallback: create default model
        currentModel = std::make_unique<SatSpiral2>();
        createModels();
    }
    currentModel->setParams(driveVal, colorVal, shapeVal, biasVal);
    
    // Convert out to linear gain
    float outGain = juce::Decibels::decibelsToGain(outVal);
    
    // Copy dry signal for mixing
    juce::AudioBuffer<float> dryBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }
    
    // Create DSP context for oversampling
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Process through oversampling
    auto osBlock = oversampler->processSamplesUp(block);
    
    // Process sample-by-sample on oversampled data
    int osNumSamples = static_cast<int>(osBlock.getNumSamples());
    
    for (int i = 0; i < osNumSamples; ++i) {
        // HPF
        float dryL = osBlock.getChannelPointer(0)[i];
        float dryR = osBlock.getChannelPointer(1)[i];
        
        float filteredL = inputHPF.processSample(dryL);
        float filteredR = inputHPF.processSample(dryR);
        
        // Saturate - ensure model exists
        float wetL, wetR;
        if (currentModel) {
            wetL = currentModel->process(filteredL);
            wetR = currentModel->process(filteredR);
        } else {
            wetL = filteredL;
            wetR = filteredR;
        }
        
        // Apply output gain
        wetL *= outGain;
        wetR *= outGain;
        
        // Store back
        osBlock.getChannelPointer(0)[i] = wetL;
        osBlock.getChannelPointer(1)[i] = wetR;
        
        // Update smoothed parameters (once per OS sample)
        driveSm.skip(1);
        colorSm.skip(1);
        shapeSm.skip(1);
        biasSm.skip(1);
        outSm.skip(1);
        mixSm.skip(1);
    }
    
    // Downsample
    oversampler->processSamplesDown(block);
    
    // Mix with dry
    auto* outLeft = buffer.getWritePointer(0);
    auto* outRight = buffer.getWritePointer(1);
    
    for (int i = 0; i < numSamples; ++i) {
        float dryL = dryBuffer.getSample(0, i);
        float dryR = dryBuffer.getSample(1, i);
        
        float wetL = buffer.getSample(0, i);
        float wetR = buffer.getSample(1, i);
        
        outLeft[i] = dryL * (1.0f - mixVal) + wetL * mixVal;
        outRight[i] = dryR * (1.0f - mixVal) + wetR * mixVal;
    }
}

void SaturateProcessor::processWithSnapshot(juce::AudioBuffer<float>& buffer, int numSamples, float type, float drive, float color, float shape, float bias, float output, float mix, bool stepChanged)
{
    if (numSamples == 0 || sampleRate <= 0.0) return;
    if (buffer.getNumChannels() < 2) return;
    
    juce::ScopedNoDenormals noDenormals;
    
    // Clamp and convert parameters
    int typeInt = juce::jlimit(0, 7, static_cast<int>(type));
    float driveVal = juce::jlimit(0.0f, 36.0f, drive);
    float colorVal = juce::jlimit(0.0f, 1.0f, color);
    float shapeVal = juce::jlimit(0.0f, 1.0f, shape);
    float biasVal = juce::jlimit(-0.2f, 0.2f, bias);
    float outVal = juce::jlimit(-24.0f, 12.0f, output);
    float mixVal = juce::jlimit(0.0f, 1.0f, mix);
    
    // Switch model if needed - ensure we have a valid model
    if (typeInt != currentType) {
        if (typeInt >= 0 && typeInt < 8) {
            switchModel(static_cast<size_t>(typeInt));
        } else {
            // Invalid type - ensure we have a valid model
            if (!currentModel) {
                currentModel = std::make_unique<SatSpiral2>();
                createModels();
            }
        }
    }
    
    // Safety check: ensure currentModel is valid
    if (!currentModel) {
        currentModel = std::make_unique<SatSpiral2>();
        createModels();
    }
    
    // Update oversampling (always 8×)
    int osMode = 3;
    int osFactor = 1 << osMode;
    currentOsFactor = osFactor;
    
    // Update smoothed parameters - reset smoothing on step change for instant response when needed
    // But still use smoothing to prevent clicks during transitions
    if (stepChanged) {
        // On step change, reset smoothing to prevent clicks
        // Keep current smoothing time but start from current values
        driveSm.setCurrentAndTargetValue(driveVal);
        colorSm.setCurrentAndTargetValue(colorVal);
        shapeSm.setCurrentAndTargetValue(shapeVal);
        biasSm.setCurrentAndTargetValue(biasVal);
        outSm.setCurrentAndTargetValue(outVal);
        mixSm.setCurrentAndTargetValue(mixVal);
    } else {
        // Normal smoothing between blocks
        driveSm.setTargetValue(driveVal);
        colorSm.setTargetValue(colorVal);
        shapeSm.setTargetValue(shapeVal);
        biasSm.setTargetValue(biasVal);
        outSm.setTargetValue(outVal);
        mixSm.setTargetValue(mixVal);
    }
    
    // Get current smoothed values
    float driveCurrent = driveSm.getCurrentValue();
    float colorCurrent = colorSm.getCurrentValue();
    float shapeCurrent = shapeSm.getCurrentValue();
    float biasCurrent = biasSm.getCurrentValue();
    float outCurrent = outSm.getCurrentValue();
    float mixCurrent = mixSm.getCurrentValue();
    
    // Update model parameters - ensure model exists
    if (!currentModel) {
        // Fallback: create default model
        currentModel = std::make_unique<SatSpiral2>();
        createModels();
    }
    currentModel->setParams(driveCurrent, colorCurrent, shapeCurrent, biasCurrent);
    
    // Convert out to linear gain
    float outGain = juce::Decibels::decibelsToGain(outCurrent);
    
    // Copy dry signal for mixing
    juce::AudioBuffer<float> dryBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }
    
    // Create DSP context for oversampling
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Process through oversampling
    auto osBlock = oversampler->processSamplesUp(block);
    
    // Process sample-by-sample on oversampled data
    int osNumSamples = static_cast<int>(osBlock.getNumSamples());
    
    for (int i = 0; i < osNumSamples; ++i) {
        // HPF
        float dryL = osBlock.getChannelPointer(0)[i];
        float dryR = osBlock.getChannelPointer(1)[i];
        
        float filteredL = inputHPF.processSample(dryL);
        float filteredR = inputHPF.processSample(dryR);
        
        // Saturate - ensure model exists
        float wetL, wetR;
        if (currentModel) {
            wetL = currentModel->process(filteredL);
            wetR = currentModel->process(filteredR);
        } else {
            wetL = filteredL;
            wetR = filteredR;
        }
        
        // Apply output gain
        wetL *= outGain;
        wetR *= outGain;
        
        // Store back
        osBlock.getChannelPointer(0)[i] = wetL;
        osBlock.getChannelPointer(1)[i] = wetR;
        
        // Update smoothed parameters (once per OS sample)
        driveSm.skip(1);
        colorSm.skip(1);
        shapeSm.skip(1);
        biasSm.skip(1);
        outSm.skip(1);
        mixSm.skip(1);
    }
    
    // Downsample
    oversampler->processSamplesDown(block);
    
    // Mix with dry
    auto* outLeft = buffer.getWritePointer(0);
    auto* outRight = buffer.getWritePointer(1);
    
    for (int i = 0; i < numSamples; ++i) {
        float dryL = dryBuffer.getSample(0, i);
        float dryR = dryBuffer.getSample(1, i);
        
        float wetL = buffer.getSample(0, i);
        float wetR = buffer.getSample(1, i);
        
        outLeft[i] = dryL * (1.0f - mixCurrent) + wetL * mixCurrent;
        outRight[i] = dryR * (1.0f - mixCurrent) + wetR * mixCurrent;
    }
}

