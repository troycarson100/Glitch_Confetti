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
    // Reset lofi state variables
    heldL = 0.0f;
    heldR = 0.0f;
    lastHeldL = 0.0f;
    lastHeldR = 0.0f;
    counterL = 0.0f;
    counterR = 0.0f;
    lowpassL = 0.0f;
    lowpassR = 0.0f;
    
    juceCompressor.reset();
    driveOversampler->reset();
    smoothedGain.reset(sampleRate, 0.05);
    
    dryBuffer.clear();
}

void CompressEngine::process(juce::AudioBuffer<float>& buffer)
{
    static int processCallCounter = 0;
    if (processCallCounter++ % 1000 == 0) { // Print every 1000 calls to avoid spam
        DBG("[CompressEngine] process() called - enabled: " << (enabled ? "true" : "false") << ", samples: " << buffer.getNumSamples());
    }
    
    if (!enabled || buffer.getNumSamples() == 0)
    {
        if (processCallCounter % 1000 == 0) { // Only log when we're already logging
            DBG("[CompressEngine] Not processing - enabled: " << (enabled ? "true" : "false") << ", samples: " << buffer.getNumSamples());
        }
        return;
    }
    
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
    
    // Debug output for compressor parameters
    static int paramDebugCounter = 0;
    if (paramDebugCounter++ % 100 == 0) { // Print every 100 calls to avoid spam
        DBG("[CompressEngine] Threshold: " << thresholdDb << "dB, Attack: " << attackMs << "ms, Release: " << releaseMs << "ms, Ratio: " << ratio);
    }
    
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
    
    // Calculate actual gain reduction in dB for UI feedback
    float actualGainReduction = 0.0f;
    if (preCompressionLevel > 1e-6f && postCompressionLevel > 1e-6f) {
        float preDb = juce::Decibels::gainToDecibels(preCompressionLevel + 1e-6f);
        float postDb = juce::Decibels::gainToDecibels(postCompressionLevel + 1e-6f);
        actualGainReduction = juce::jlimit(0.0f, 30.0f, preDb - postDb); // Positive gain reduction
    }
    
    // Debug output for gain reduction
    static int debugCounter = 0;
    if (debugCounter++ % 100 == 0) { // Print every 100 calls to avoid spam
        DBG("[CompressEngine] Threshold: " << thresholdDb << "dB, Pre: " << preCompressionLevel << " Post: " << postCompressionLevel << " GainReduction: " << actualGainReduction << "dB");
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
    
    // Pre-calc (do once per block): map slider to downsample factor and bit steps
    // Make lofi effect much more subtle at low values
    // Downsample hold period: 1 (no hold) up to 16 samples (moderate hold) - reduced from 32
    // Use exponential curve to make low values much more subtle
    const float holdPeriod = juce::jmap(lofiLevel * lofiLevel, 1.0f, 16.0f);
    
    // Bit quantization steps: from many steps at low lofi to few at high lofi
    // Use exponential curve to make low values much more subtle
    // At 1% lofi: ~65536 steps (16-bit), at 10%: ~10000 steps (13-bit), at 100%: 64 steps (6-bit) - improved from 8
    const float bitSteps = juce::jmap(lofiLevel * lofiLevel, 65536.0f, 64.0f);
    
    // Low-pass filter setup: cutoff falls with more lofi for smoothness
    // Use exponential curve to make low values much more subtle
    // At 1% lofi: 20kHz (no filtering), at 10%: ~18kHz, at 100%: 4kHz - improved from 2kHz
    const float cutoff = juce::jmap(lofiLevel * lofiLevel, 20000.0f, 4000.0f);
    const float RC = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoff);
    const float dt = 1.0f / sampleRate;
    const float alpha = dt / (RC + dt);  // one-pole filter coefficient
    
    // Process each sample for stereo
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Read input (assuming stereo)
        float inL = buffer.getReadPointer(0)[sample];
        float inR = (numChannels > 1) ? buffer.getReadPointer(1)[sample] : inL;
        
        // --- Downsampling (sample&hold with interpolation) ---
        // Left channel:
        counterL += 1.0f;
        if (counterL >= holdPeriod)
        {
            counterL -= holdPeriod;
            lastHeldL = heldL;
            heldL = inL;
        }
        float phaseL = counterL / holdPeriod;
        float outL = lastHeldL + (heldL - lastHeldL) * phaseL;
        
        // Right channel (same logic):
        counterR += 1.0f;
        if (counterR >= holdPeriod)
        {
            counterR -= holdPeriod;
            lastHeldR = heldR;
            heldR = inR;
        }
        float phaseR = counterR / holdPeriod;
        float outR = lastHeldR + (heldR - lastHeldR) * phaseR;
        
        // --- Bit-depth quantization ---
        // Clamp to [-1,1] to avoid overflow, then quantize amplitude:
        outL = juce::jlimit(-1.0f, 1.0f, outL);
        outR = juce::jlimit(-1.0f, 1.0f, outR);
        
        float quantL = std::round(outL * bitSteps) / bitSteps;
        float quantR = std::round(outR * bitSteps) / bitSteps;
        
        // --- Low-pass filtering to remove aliasing ---
        // Simple one-pole (smoothing). Alpha is small at low cutoff (more smoothing).
        lowpassL += alpha * (quantL - lowpassL);
        lowpassR += alpha * (quantR - lowpassR);
        
        // --- Soft saturation for warmth ---
        // Make saturation much more subtle at low lofi values
        // Use exponential curve to make low values much more subtle
        const float warmGain = 1.0f + 0.2f * (lofiLevel * lofiLevel);  // drive up to 1.2x (reduced from 1.3x)
        float satL = std::tanh(lowpassL * warmGain);
        float satR = std::tanh(lowpassR * warmGain);
        
        // Write back to buffer (to be mixed downstream by the plugin's mix parameter)
        buffer.getWritePointer(0)[sample] = satL;
        if (numChannels > 1)
            buffer.getWritePointer(1)[sample] = satR;
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
    DBG("[CompressEngine] setThreshold called with: " << thresholdValue << "dB");
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
    DBG("[CompressEngine] setEnabled called with: " << (enable ? "true" : "false") << ", current enabled state: " << (this->enabled ? "true" : "false"));
}
