#include "CompressEngine.h"

CompressEngine::CompressEngine()
{
    // Initialize drive oversampler
    driveOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        1, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
}

CompressEngine::~CompressEngine() = default;

void CompressEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = static_cast<int>(spec.maximumBlockSize);
    
    // Prepare JUCE compressor with default values
    juceCompressor.setAttack(attackMs);
    juceCompressor.setRelease(releaseMs);
    juceCompressor.setRatio(ratio);
    juceCompressor.setThreshold(thresholdDb);
    juceCompressor.prepare(spec);
    
    // Prepare smoothed gain for makeup compensation
    smoothedGain.reset(sampleRate, 0.05); // 50ms smoothing time
    smoothedGain.setCurrentAndTargetValue(1.0f);
    
    // Prepare drive oversampler
    driveOversampler->initProcessing(static_cast<size_t>(blockSize));
    
    // Noise filter removed - using lofi processing instead
    
    // Prepare dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    
    reset();
}

void CompressEngine::reset()
{
    sampleCounter = 0.0f;
    heldSample = 0.0f;
    
    juceCompressor.reset();
    driveOversampler->reset();
    smoothedGain.reset(sampleRate, 0.05);
    
    dryBuffer.clear();
}

void CompressEngine::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || buffer.getNumSamples() == 0)
    {
        DBG("[CompressEngine] Not processing - enabled: " << (enabled ? "true" : "false") << ", samples: " << buffer.getNumSamples());
        return;
    }
    
    DBG("[CompressEngine] Processing buffer with " << buffer.getNumSamples() << " samples");
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Store dry signal for wet/dry mixing
    dryBuffer.makeCopyOf(buffer);
    
    // Process each effect in chain
    processCompressor(buffer);
    processDrive(buffer);
    processLofi(buffer);
    processWetDry(buffer);
}

void CompressEngine::processCompressor(juce::AudioBuffer<float>& buffer)
{
    // Early bypass if threshold is at maximum (no compression)
    if (thresholdDb >= 0.0f) {
        gainReductionDb.store(0.0f);
        return;
    }
    
    // Update JUCE compressor parameters
    juceCompressor.setThreshold(thresholdDb);
    juceCompressor.setAttack(attackMs);
    juceCompressor.setRelease(releaseMs);
    juceCompressor.setRatio(ratio);
    
    // Store pre-compression levels for gain reduction calculation
    float preCompressionLevel = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            preCompressionLevel = juce::jmax(preCompressionLevel, std::abs(channelData[sample]));
        }
    }
    
    // Process with JUCE compressor
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    juceCompressor.process(context);
    
    // Calculate actual gain reduction
    float postCompressionLevel = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            postCompressionLevel = juce::jmax(postCompressionLevel, std::abs(channelData[sample]));
        }
    }
    
    // Calculate gain reduction in dB for UI feedback
    float actualGainReduction = 0.0f;
    if (preCompressionLevel > 0.0f && postCompressionLevel > 0.0f) {
        actualGainReduction = juce::Decibels::gainToDecibels(postCompressionLevel / preCompressionLevel);
        actualGainReduction = juce::jlimit(-30.0f, 0.0f, actualGainReduction); // Clamp to reasonable range
    }
    gainReductionDb.store(actualGainReduction);
    
    // Manual makeup gain will be applied in processWetDry method
}

void CompressEngine::processDrive(juce::AudioBuffer<float>& buffer)
{
    if (driveGain <= 1.0f)
        return;
    
    // Simple drive processing without oversampling to prevent crashes
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Apply drive gain and tanh saturation
            float driven = driveGain * channelData[sample];
            channelData[sample] = std::tanh(driven);
        }
    }
}


void CompressEngine::processLofi(juce::AudioBuffer<float>& buffer)
{
    if (lofiLevel <= 0.0f)
        return;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Map lofi level (0-1) to bit depth (24-8) and sample rate reduction (1-4)
    const int bitDepth = static_cast<int>(juce::jmap(lofiLevel, 0.0f, 1.0f, 24.0f, 8.0f));
    const int rateFactor = static_cast<int>(juce::jmap(lofiLevel, 0.0f, 1.0f, 1.0f, 4.0f));
    const float saturationAmount = juce::jmap(lofiLevel, 0.0f, 1.0f, 0.0f, 0.3f);
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float input = channelData[sample];
            
            // Sample rate reduction (hold samples)
            if (static_cast<int>(sampleCounter) % rateFactor == 0) {
                heldSample = input;
            }
            sampleCounter += 1.0f;
            
            // Apply held sample
            float processed = heldSample;
            
            // Bit depth reduction
            const float maxVal = static_cast<float>((1 << bitDepth) - 1);
            processed = std::round(processed * maxVal) / maxVal;
            
            // Subtle saturation
            if (saturationAmount > 0.0f) {
                processed = std::tanh(processed * (1.0f + saturationAmount)) / (1.0f + saturationAmount);
            }
            
            // Mix with original based on lofi level
            channelData[sample] = juce::jlimit(-1.0f, 1.0f, 
                input * (1.0f - lofiLevel) + processed * lofiLevel);
        }
    }
}

void CompressEngine::processWetDry(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Apply makeup gain first
    if (makeupGainDb != 0.0f) {
        const float makeupGain = juce::Decibels::decibelsToGain(makeupGainDb);
        smoothedGain.setTargetValue(makeupGain);
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                channelData[sample] *= smoothedGain.getNextValue();
            }
        }
    }
    
    // Apply wet/dry mix
    if (wetLevel >= 1.0f)
        return; // Already 100% wet
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        const float* dryData = dryBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] = (1.0f - wetLevel) * dryData[sample] + wetLevel * channelData[sample];
        }
    }
}

// Parameter setters
void CompressEngine::setThreshold(float thresholdValue)
{
    this->thresholdDb = thresholdValue;
}

void CompressEngine::setAttack(float attackValue)
{
    this->attackMs = attackValue;
}

void CompressEngine::setRelease(float releaseValue)
{
    this->releaseMs = releaseValue;
}

void CompressEngine::setRatio(float ratioValue)
{
    this->ratio = ratioValue;
}

void CompressEngine::setDrive(float driveDb)
{
    driveGain = std::pow(10.0f, driveDb / 20.0f);
}

void CompressEngine::setLofi(float lofiVal)
{
    this->lofiLevel = juce::jlimit(0.0f, 1.0f, lofiVal);
}

void CompressEngine::setMakeupGain(float makeupGainVal)
{
    this->makeupGainDb = juce::jlimit(-24.0f, 24.0f, makeupGainVal);
}

void CompressEngine::setWet(float wetVal)
{
    this->wetLevel = juce::jlimit(0.0f, 1.0f, wetVal);
}

void CompressEngine::setEnabled(bool enable)
{
    this->enabled = enable;
}
