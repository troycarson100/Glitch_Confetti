#include "CompressEngine.h"

CompressEngine::CompressEngine()
{
    // Initialize tilt EQ coefficients (1kHz pivot frequency)
    tiltCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 1000.0);
    tiltLowPass.coefficients = tiltCoeffs;
    
    // Initialize noise filter coefficients
    noiseCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 4000.0);
    noiseFilter.coefficients = noiseCoeffs;
    
    // Initialize oversampler for nonlinear stages
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
}

CompressEngine::~CompressEngine() = default;

void CompressEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = static_cast<int>(spec.maximumBlockSize);
    
    // Prepare oversampler
    oversampler->initProcessing(static_cast<size_t>(blockSize));
    
    // Prepare tilt filter
    tiltLowPass.prepare(spec);
    tiltCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1000.0);
    tiltLowPass.coefficients = tiltCoeffs;
    
    // Prepare noise filter
    noiseFilter.prepare(spec);
    noiseCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 4000.0);
    noiseFilter.coefficients = noiseCoeffs;
    
    // Prepare dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    
    // Initialize compressor time constants
    const float attackTime = 0.01f; // 10ms attack
    const float releaseTime = 0.1f; // 100ms release
    attackCoeff = std::exp(-1.0f / (attackTime * sampleRate));
    releaseCoeff = std::exp(-1.0f / (releaseTime * sampleRate));
    
    reset();
}

void CompressEngine::reset()
{
    envelopeLevel = 0.0f;
    gainReductionDb.store(0.0f);
    sampleCounter = 0;
    heldSample = 0.0f;
    noiseEnv = 0.0f;
    
    tiltLowPass.reset();
    noiseFilter.reset();
    oversampler->reset();
    
    dryBuffer.clear();
}

void CompressEngine::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || buffer.getNumSamples() == 0)
        return;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Store dry signal for wet/dry mixing
    dryBuffer.makeCopyOf(buffer);
    
    // Process each effect in chain
    processCompressor(buffer);
    processDrive(buffer);
    processCrush(buffer);
    processTilt(buffer);
    processNoise(buffer);
    processWetDry(buffer);
}

void CompressEngine::processCompressor(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];
            const float inputLevel = std::abs(input);
            
            // Envelope detection with separate attack/release
            if (inputLevel > envelopeLevel)
                envelopeLevel = attackCoeff * envelopeLevel + (1.0f - attackCoeff) * inputLevel;
            else
                envelopeLevel = releaseCoeff * envelopeLevel + (1.0f - releaseCoeff) * inputLevel;
            
            // Convert to dB
            const float envDb = 20.0f * std::log10(std::max(envelopeLevel, 1e-6f));
            
            // Soft knee compression
            const float threshold = -20.0f; // Will be parameterized
            const float ratio = 4.0f; // 4:1 ratio
            const float kneeWidth = 6.0f; // 6dB knee
            
            float gainDb = 0.0f;
            if (envDb < threshold - kneeWidth / 2.0f)
            {
                gainDb = 0.0f; // No compression
            }
            else if (envDb > threshold + kneeWidth / 2.0f)
            {
                gainDb = (threshold - envDb) * (1.0f - 1.0f / ratio); // Full ratio
            }
            else
            {
                // Soft knee interpolation
                const float delta = envDb - (threshold - kneeWidth / 2.0f);
                gainDb = (1.0f - 1.0f / ratio) * delta * delta / (2.0f * kneeWidth);
            }
            
            // Convert gain to linear and apply
            const float gainLinear = std::pow(10.0f, gainDb / 20.0f);
            channelData[sample] = input * gainLinear;
            
            // Update gain reduction meter
            gainReductionDb.store(-gainDb);
        }
    }
}

void CompressEngine::processDrive(juce::AudioBuffer<float>& buffer)
{
    if (driveGain <= 1.0f)
        return;
    
    // Convert AudioBuffer to AudioBlock for oversampling
    juce::dsp::AudioBlock<float> audioBlock(buffer);
    
    // Upsample for oversampling
    auto oversampledBlock = oversampler->processSamplesUp(audioBlock);
    
    // Apply tanh saturation
    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
    {
        float* channelData = oversampledBlock.getChannelPointer(channel);
        for (size_t sample = 0; sample < oversampledBlock.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh(driveGain * channelData[sample]);
        }
    }
    
    // Downsample back to original rate
    oversampler->processSamplesDown(audioBlock);
}

void CompressEngine::processCrush(juce::AudioBuffer<float>& buffer)
{
    if (crushValue <= 0.0f)
        return;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float input = channelData[sample];
            
            // Sample rate reduction (stutter effect)
            if (sampleCounter % rateFactor == 0)
                heldSample = input;
            
            input = heldSample;
            sampleCounter++;
            
            // Bit reduction
            const float quantized = std::round(input * (1 << bitDepth)) / (1 << bitDepth);
            
            // Add dithering
            const float dither = (random.nextFloat() - 0.5f) * 2.0f / (1 << bitDepth);
            channelData[sample] = quantized + dither;
        }
    }
}

void CompressEngine::processTilt(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];
            
            // Process through tilt filter
            const float lp = tiltLowPass.processSample(input);
            const float hp = input - lp;
            
            // Apply tilt
            const float tilted = input + tiltValue * hp - tiltValue * lp;
            channelData[sample] = tilted;
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
            if (std::abs(channelData[sample]) > 0.01f) // Trigger on input activity
                noiseEnv = 1.0f;
            
            // Mix in noise
            channelData[sample] += noise * noiseLevel * noiseEnv;
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
void CompressEngine::setDrive(float driveDb)
{
    driveGain = std::pow(10.0f, driveDb / 20.0f);
}

void CompressEngine::setThreshold(float thresholdDb)
{
    // Threshold is used in processCompressor
    // This would need to be stored as a member variable for full implementation
}

void CompressEngine::setCrush(float crushVal)
{
    this->crushValue = crushVal;
    rateFactor = static_cast<int>(juce::jmap(crushVal, 0.0f, 1.0f, 1.0f, 20.0f));
    bitDepth = static_cast<int>(juce::jmap(crushVal, 0.0f, 1.0f, 24.0f, 4.0f));
}

void CompressEngine::setTilt(float tiltVal)
{
    this->tiltValue = tiltVal;
}

void CompressEngine::setNoise(float noiseVal)
{
    this->noiseLevel = noiseVal;
}

void CompressEngine::setNoiseDecay(float decayTime)
{
    this->noiseDecayTime = decayTime;
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
