#include "PhaseBloomEngine.h"
#include <cmath>

PhaseBloomEngine::PhaseBloomEngine()
{
    // Initialize smoothed values with 30ms smoothing time
    depth.reset(44100.0, 0.03);
    rate.reset(44100.0, 0.03);
    feedback.reset(44100.0, 0.03);
    center.reset(44100.0, 0.03);
    bloom.reset(44100.0, 0.03);
    spread.reset(44100.0, 0.03);
    resonance.reset(44100.0, 0.03);
    mix.reset(44100.0, 0.03);
    
    // Set default values
    depth.setCurrentAndTargetValue(0.5f);
    rate.setCurrentAndTargetValue(0.5f);        // 1/4 note default
    feedback.setCurrentAndTargetValue(0.3f);
    center.setCurrentAndTargetValue(1000.0f);   // 1kHz default
    bloom.setCurrentAndTargetValue(0.2f);
    spread.setCurrentAndTargetValue(0.8f);      // Wide stereo default
    resonance.setCurrentAndTargetValue(0.5f);
    mix.setCurrentAndTargetValue(0.5f);
}

void PhaseBloomEngine::prepare(double newSampleRate, int samplesPerBlock, int numChannels)
{
    sampleRate = newSampleRate;
    
    // Reset smoothing sample rates - faster smoothing for more responsive feel
    depth.reset(sampleRate, 0.01);
    rate.reset(sampleRate, 0.005);  // Very fast smoothing for rate
    feedback.reset(sampleRate, 0.01);
    center.reset(sampleRate, 0.01);
    bloom.reset(sampleRate, 0.01);
    spread.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    mix.reset(sampleRate, 0.01);
    
    // Reset state
    lfoPhase = 0.0f;
    
    // Prepare JUCE DSP phasers for all slots
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1; // Each phaser processes one channel
    
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        phaserL[i].reset();
        phaserR[i].reset();
        phaserL[i].prepare(spec);
        phaserR[i].prepare(spec);
        
        // TEMPORARILY DISABLE DELAY LINES TO PREVENT CRASHES
        // TODO: Re-implement bloom with safer approach
    }
}

void PhaseBloomEngine::reset()
{
    lfoPhase = 0.0f;
    
    // Reset all phasers
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        phaserL[i].reset();
        phaserR[i].reset();
        // TEMPORARILY DISABLE DELAY LINES TO PREVENT CRASHES
    }
    
    // Reset all smoothed parameters to prevent clicks
    depth.reset(sampleRate, 0.03);
    rate.reset(sampleRate, 0.03);
    feedback.reset(sampleRate, 0.03);
    center.reset(sampleRate, 0.03);
    bloom.reset(sampleRate, 0.03);
    spread.reset(sampleRate, 0.03);
    resonance.reset(sampleRate, 0.03);
    mix.reset(sampleRate, 0.03);
}

void PhaseBloomEngine::process(juce::AudioBuffer<float>& buffer, double hostBPM)
{
    if (!isEnabled || buffer.getNumChannels() < 2 || buffer.getNumSamples() <= 0)
        return;
    
    const int numSamples = buffer.getNumSamples();
    
    // Safety check to prevent crashes
    if (numSamples > 4096) {
        DBG("[PHASEBLOOM] Warning: Block size too large: " << numSamples);
        return;
    }
    
    // Get host BPM for tempo sync (default 120 if unavailable)
    double bpm = 120.0;
    if (hostBPM > 0.0)
        bpm = hostBPM;
    
    // Tempo-synced division factors (quarter notes) - corrected for proper musical timing
    static const double divisionFactors[9] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625 };
    
    // Copy original (dry) buffer for later dry/wet mixing
    juce::AudioBuffer<float> dryBuffer(buffer);
    
    // Process slot 0 (main slot) - for now we only use one slot
    int slot = 0;
    
    // Get parameter values - only smooth non-critical parameters
    float currentDepth = depth.getNextValue();
    float currentRate = rate.getNextValue();
    float currentFeedback = feedback.getNextValue(); // No smoothing for feedback to prevent lag
    float currentCenter = center.getNextValue();
    float currentBloom = bloom.getNextValue();
    float currentSpread = spread.getNextValue();
    float currentResonance = resonance.getNextValue();
    float currentMix = mix.getNextValue();
    
    // If mix is at 0 (fully dry), bypass processing entirely
    if (currentMix <= 0.0f)
        return;
    
    // Convert rate value (0-1) to rate index (0-8)
    int rateIdx = juce::jlimit(0, 8, static_cast<int>(currentRate * 8.0f));
    
    // Map resonance [0,1] to enhanced feedback with stability limiting
    float resonanceFeedback = juce::jmap(currentResonance, 0.0f, 1.0f, currentFeedback, currentFeedback * 1.2f);
    
    // Limit feedback to prevent instability and loudness issues
    resonanceFeedback = juce::jlimit(-0.7f, 0.7f, resonanceFeedback);
    
    // Calculate tempo-synced LFO rate in Hz with limiting for smoothness
    double quarterSec = 60.0 / juce::jmax(1.0, bpm); // Prevent division by zero
    double period = quarterSec * divisionFactors[rateIdx];
    
    // PHASEBLOOM FIX: Multiply period by 4 to make the effect rate 4x slower
    period *= 4.0;
    
    // Prevent division by zero and invalid values
    if (period <= 0.0 || !std::isfinite(period)) {
        period = 1.0; // Fallback to 1 second
    }
    
    double rateHz = 1.0 / period;
    
    // Check for NaN/infinity and limit rate to prevent harsh artifacts and instability
    if (!std::isfinite(rateHz)) {
        rateHz = 1.0; // Fallback to 1 Hz
    }
    rateHz = juce::jlimit(0.1, 20.0, rateHz);
    
    // Debug output every 1000 blocks to verify rate calculation
    static int debugCounter = 0;
    if ((debugCounter++ % 1000) == 0) {
        DBG("[PHASEBLOOM] BPM=" << bpm << " rateIdx=" << rateIdx 
            << " division=" << divisionFactors[rateIdx] 
            << " period=" << period << " rateHz=" << rateHz);
    }
    
    // Update phaser parameters (per-slot, per-channel)
    phaserL[slot].setRate(static_cast<float>(rateHz));
    phaserL[slot].setDepth(currentDepth);
    phaserL[slot].setCentreFrequency(currentCenter);
    phaserL[slot].setFeedback(resonanceFeedback);
    phaserL[slot].setMix(1.0f); // full wet inside phaser
    
    phaserR[slot].setRate(static_cast<float>(rateHz));
    phaserR[slot].setDepth(currentDepth);
    phaserR[slot].setCentreFrequency(currentCenter);
    phaserR[slot].setFeedback(resonanceFeedback);
    phaserR[slot].setMix(1.0f);
    
    // Create separate buffers for left and right channels
    juce::AudioBuffer<float> leftBuffer(1, numSamples);
    juce::AudioBuffer<float> rightBuffer(1, numSamples);
    
    // Copy input data to separate buffers
    leftBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    rightBuffer.copyFrom(0, 0, buffer, 1, 0, numSamples);
    
    // Create AudioBlocks for processing
    juce::dsp::AudioBlock<float> blockL(leftBuffer);
    juce::dsp::AudioBlock<float> blockR(rightBuffer);
    
    // Process the phaser effect
    phaserL[slot].process(juce::dsp::ProcessContextReplacing<float>(blockL));
    phaserR[slot].process(juce::dsp::ProcessContextReplacing<float>(blockR));
    
    // Copy processed data back to main buffer
    buffer.copyFrom(0, 0, leftBuffer, 0, 0, numSamples);
    buffer.copyFrom(1, 0, rightBuffer, 0, 0, numSamples);
    
        // Apply Bloom (tanh) and mixing sample-by-sample
        float invSpread = 1.0f - 2.0f * currentSpread; // for 0→1 spread (0..180° phase)
        
        // TEMPORARILY DISABLE DELAY-BASED BLOOM TO PREVENT CRASHES
        // TODO: Re-implement bloom with safer approach
        
        for (int n = 0; n < numSamples; ++n)
        {
            // Original dry samples
            float dryL = dryBuffer.getSample(0, n);
            float dryR = dryBuffer.getSample(1, n);
            // Wet output from phaser
            float wetL = buffer.getSample(0, n);
            float wetR = buffer.getSample(1, n);
            
            // Simple bloom effect with saturation only (no delay lines)
            if (currentBloom > 0.001f)
            {
                // Apply gentle saturation for bloom effect
                float bloomAmount = currentBloom * 0.8f;
                wetL = juce::jmap(bloomAmount, wetL, std::tanh(wetL * 0.9f));
                wetR = juce::jmap(bloomAmount, wetR, std::tanh(wetR * 0.9f));
            }
            
            // Soft limiting to prevent loudness issues
            wetL = juce::jlimit(-0.8f, 0.8f, wetL);
            wetR = juce::jlimit(-0.8f, 0.8f, wetR);
            
            // Stereo spread: apply phase offset between L and R channels
            // Use proper stereo spread that maintains balance (not simple inversion)
            if (currentSpread > 0.001f) {
                // Create phase offset for right channel (0 to 90 degrees)
                float phaseOffset = currentSpread * juce::MathConstants<float>::halfPi;
                // Apply phase shift to right channel using rotation
                float tempR = wetR;
                wetR = wetR * std::cos(phaseOffset) - wetL * std::sin(phaseOffset) * 0.3f;
                // Left channel gets slight opposite phase for balance
                wetL = wetL * std::cos(phaseOffset * 0.5f) + tempR * std::sin(phaseOffset * 0.5f) * 0.2f;
            }
            
            // Final dry/wet mix
            float outL = juce::jmap(currentMix, dryL, wetL);
            float outR = juce::jmap(currentMix, dryR, wetR);
            buffer.setSample(0, n, outL);
            buffer.setSample(1, n, outR);
        }
}

void PhaseBloomEngine::setDepth(float depthValue)
{
    depth.setTargetValue(juce::jlimit(0.0f, 1.0f, depthValue));
}

void PhaseBloomEngine::setRate(float rateValue)
{
    rate.setTargetValue(juce::jlimit(0.0f, 1.0f, rateValue));
}

void PhaseBloomEngine::setFeedback(float feedbackValue)
{
    // Limit feedback to prevent instability - max at 0.8 instead of 1.0
    feedback.setTargetValue(juce::jlimit(-0.8f, 0.8f, feedbackValue));
}

void PhaseBloomEngine::setCenter(float centerValue)
{
    center.setTargetValue(juce::jlimit(100.0f, 4000.0f, centerValue));
}

void PhaseBloomEngine::setBloom(float bloomValue)
{
    bloom.setTargetValue(juce::jlimit(0.0f, 1.0f, bloomValue));
}

void PhaseBloomEngine::setSpread(float spreadValue)
{
    spread.setTargetValue(juce::jlimit(0.0f, 1.0f, spreadValue));
}

void PhaseBloomEngine::setResonance(float resonanceValue)
{
    resonance.setTargetValue(juce::jlimit(0.0f, 1.0f, resonanceValue));
}

void PhaseBloomEngine::setMix(float mixValue)
{
    mix.setTargetValue(juce::jlimit(0.0f, 1.0f, mixValue));
}

void PhaseBloomEngine::setEnabled(bool enabled)
{
    isEnabled = enabled;
}

float PhaseBloomEngine::rateToHz(float rateValue, double hostBPM)
{
    // Convert rate value (0-1) to rate index (0-8)
    // 0 = slowest (4 bars), 1 = fastest (1/256 note)
    int rateIndex = juce::jlimit(0, 8, static_cast<int>(rateValue * 8.0f));
    
    // Get beat division from lookup table (don't reverse - faster on the right)
    float beats = RATE_DIVISIONS[rateIndex];
    
    // Convert to frequency in Hz
    float freqHz = 60.0f / (static_cast<float>(hostBPM) * beats);
    
    // Clamp to reasonable range
    return juce::jlimit(0.01f, 100.0f, freqHz);
}

juce::String PhaseBloomEngine::getRateLabel(float rateValue)
{
    int rateIndex = juce::jlimit(0, 8, static_cast<int>(rateValue * 8.0f));
    return juce::String(RATE_LABELS[rateIndex]); // Don't reverse - faster on the right
}