#include "CompressEngine.h"

CompressEngine::CompressEngine()
{
    // Initialize drive oversampler
    driveOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        1, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    
    // Initialize noise filter coefficients
    noiseCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 1000.0);
    noiseFilter.coefficients = noiseCoeffs;
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
    
    // Prepare noise filter
    noiseFilter.prepare(spec);
    noiseCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1000.0);
    noiseFilter.coefficients = noiseCoeffs;
    
    // Prepare dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    
    reset();
}

void CompressEngine::reset()
{
    noiseEnv = 0.0f;
    
    juceCompressor.reset();
    noiseFilter.reset();
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
    processNoise(buffer);
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
    
    // Calculate gain reduction in dB
    float actualGainReduction = 0.0f;
    if (preCompressionLevel > 0.0f && postCompressionLevel > 0.0f) {
        actualGainReduction = juce::Decibels::gainToDecibels(postCompressionLevel / preCompressionLevel);
        actualGainReduction = juce::jlimit(-30.0f, 0.0f, actualGainReduction); // Clamp to reasonable range
    }
    gainReductionDb.store(actualGainReduction);
    
    // Apply auto gain compensation - compensate for the gain reduction
    float makeupGain = std::pow(10.0f, -actualGainReduction / 20.0f); // Convert dB to linear gain
    makeupGain = juce::jlimit(0.1f, 10.0f, makeupGain); // Clamp to reasonable range
    
    // Apply makeup gain smoothly
    smoothedGain.setTargetValue(makeupGain);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= smoothedGain.getNextValue();
        }
    }
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


void CompressEngine::processNoise(juce::AudioBuffer<float>& buffer)
{
    if (noiseLevel <= 0.0f)
        return;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Generate noise
            float noise = (random.nextFloat() - 0.5f) * 2.0f;
            
            // Filter noise for tone
            noise = noiseFilter.processSample(noise);
            
            // Apply envelope decay
            noiseEnv *= std::exp(-1.0f / (noiseDecayTime * sampleRate));
            if (std::abs(channelData[sample]) > 0.05f) // Increased threshold to prevent crush from triggering noise
                noiseEnv = 1.0f;
            
            // Clamp noise envelope to prevent runaway
            noiseEnv = juce::jlimit(0.0f, 1.0f, noiseEnv);
            
            // Mix in noise and clamp output to prevent blowup
            float mixed = channelData[sample] + noise * noiseLevel * noiseEnv;
            channelData[sample] = juce::jlimit(-1.0f, 1.0f, mixed);
        }
    }
}

void CompressEngine::processWetDry(juce::AudioBuffer<float>& buffer)
{
    if (wetLevel >= 1.0f)
        return; // Already 100% wet
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
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

void CompressEngine::setNoise(float noiseVal)
{
    this->noiseLevel = noiseVal;
}

void CompressEngine::setNoiseTone(float toneFreq)
{
    noiseCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, toneFreq);
    noiseFilter.coefficients = noiseCoeffs;
}

void CompressEngine::setWet(float wetVal)
{
    this->wetLevel = juce::jlimit(0.0f, 1.0f, wetVal);
}

void CompressEngine::setEnabled(bool enable)
{
    this->enabled = enable;
}
