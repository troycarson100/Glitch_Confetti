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
    
    // Reset smoothing sample rates - minimal smoothing for immediate response
    depth.reset(sampleRate, 0.001);  // Very fast smoothing (1ms) for immediate depth response
    rate.reset(sampleRate, 0.001);   // Very fast smoothing (1ms) for immediate rate response
    feedback.reset(sampleRate, 0.0001);  // Minimal smoothing (0.1ms) for immediate feedback response
    center.reset(sampleRate, 0.001); // Very fast smoothing (1ms) for immediate center response
    bloom.reset(sampleRate, 0.001);  // Very fast smoothing (1ms) for immediate bloom response
    spread.reset(sampleRate, 0.001); // Very fast smoothing (1ms) for immediate spread response
    resonance.reset(sampleRate, 0.001); // Very fast smoothing (1ms) for immediate resonance response
    mix.reset(sampleRate, 0.001);    // Very fast smoothing (1ms) for immediate mix response
    
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
    
    // Tempo-synced division factors (in quarter notes)
    // Labels: "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"
    // 4 Bars = 16 quarter notes, 2 Bars = 8, 1 Bar = 4, 1/2 = 2, 1/4 = 1, etc.
    static const double divisionFactors[9] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };
    
    // Copy original (dry) buffer for later dry/wet mixing
    juce::AudioBuffer<float> dryBuffer(buffer);
    
    // Process slot 0 (main slot) - for now we only use one slot
    int slot = 0;
    
    // Get parameter values - only smooth non-critical parameters
    // Use target values for rate and feedback (immediate tempo sync and feedback response)
    // Use next values for others (smooth but fast response with minimal smoothing)
    float currentDepth = depth.getNextValue();       // Fast smooth depth response
    float currentRate = rate.getTargetValue();       // Immediate rate response (tempo sync)
    float currentFeedback = feedback.getTargetValue(); // Immediate feedback response (no delay)
    float currentCenter = center.getNextValue();     // Fast smooth center response
    float currentBloom = bloom.getNextValue();       // Fast smooth bloom response
    float currentSpread = spread.getNextValue();     // Fast smooth spread response
    float currentResonance = resonance.getNextValue(); // Fast smooth resonance response
    float currentMix = mix.getNextValue();           // Fast smooth mix response
    
    // If mix is at 0 (fully dry), bypass processing entirely
    if (currentMix <= 0.0f)
        return;
    
    // Convert rate value (0-1) to rate index (0-8)
    int rateIdx = juce::jlimit(0, 8, static_cast<int>(currentRate * 8.0f));
    
    // Map feedback [0,1] to JUCE Phaser feedback range [0, 0.6] (positive values only)
    // Keep feedback range lower to prevent excessive resonance
    float mappedFeedback = juce::jmap(currentFeedback, 0.0f, 1.0f, 0.0f, 0.6f);
    
    // Map resonance [0,1] to additional feedback boost ONLY when resonance > 0
    // When resonance is 0, no extra feedback is added (prevents unwanted resonance)
    float resonanceBoost = 0.0f;
    if (currentResonance > 0.001f) {
        resonanceBoost = currentResonance * 0.15f; // Add up to 0.15 extra feedback only when resonance is used
    }
    float resonanceFeedback = juce::jlimit(0.0f, 0.75f, mappedFeedback + resonanceBoost);
    
    // Calculate tempo-synced LFO rate in Hz
    // divisionFactors are in quarter notes, so multiply by quarter note duration
    double quarterSec = 60.0 / juce::jmax(1.0, bpm); // Seconds per quarter note
    double period = quarterSec * divisionFactors[rateIdx]; // Total period in seconds
    
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
    
    // Update phaser parameters (per-slot, per-channel)
    // Always update rate immediately - JUCE Phaser handles internal smoothing
    float targetRateHz = static_cast<float>(rateHz);
    phaserL[slot].setRate(targetRateHz);
    phaserR[slot].setRate(targetRateHz);
    
    // Debug output every 1000 blocks to verify rate calculation
    static int debugCounter = 0;
    if ((debugCounter++ % 1000) == 0) {
        DBG("[PHASEBLOOM] BPM=" << bpm << " rateIdx=" << rateIdx 
            << " division=" << divisionFactors[rateIdx] << " quarters"
            << " period=" << period << "s rateHz=" << rateHz);
    }
    
    // Update phaser parameters (per-slot, per-channel) - but rate is handled above
    // Boost depth for more prominent phasing (JUCE Phaser multiplies by 0.5 internally)
    float boostedDepth = juce::jmin(1.0f, currentDepth * 1.4f); // Boost depth by 40% for more prominence
    phaserL[slot].setDepth(boostedDepth);
    phaserL[slot].setCentreFrequency(currentCenter);
    phaserL[slot].setFeedback(resonanceFeedback);
    phaserL[slot].setMix(1.0f); // full wet inside phaser
    
    // phaserR[slot].setRate() is handled above with conditional update
    phaserR[slot].setDepth(boostedDepth);
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
            
            // Bloom effect: subtle saturation + harmonic generation for noticeable "bloom" without distortion
            if (currentBloom > 0.001f)
            {
                float bloomAmount = currentBloom;
                
                // Moderate saturation (up to 2x drive at max bloom) to prevent distortion
                float drive = 1.0f + bloomAmount * 1.0f; // 1.0x to 2.0x drive
                float saturatedL = std::tanh(wetL * drive);
                float saturatedR = std::tanh(wetR * drive);
                
                // Add subtle harmonic content (even harmonics for warmth)
                float harmonicsL = wetL * wetL * 0.15f * bloomAmount; // Subtle 2nd harmonic
                float harmonicsR = wetR * wetR * 0.15f * bloomAmount;
                
                // Blend: saturation + harmonics for rich "bloom" effect
                wetL = juce::jmap(bloomAmount, wetL, saturatedL + harmonicsL);
                wetR = juce::jmap(bloomAmount, wetR, saturatedR + harmonicsR);
            }
            
            // Less aggressive limiting to preserve effect prominence
            wetL = juce::jlimit(-0.95f, 0.95f, wetL);
            wetR = juce::jlimit(-0.95f, 0.95f, wetR);
            
            // Stereo spread: cross-feed with phase offset for clear, noticeable stereo width
            // This creates a much more obvious stereo widening effect
            if (currentSpread > 0.001f) {
                // Cross-feed amount: 0 = mono, 1 = full stereo width
                float crossFeed = currentSpread * 0.5f; // Up to 50% cross-feed for noticeable effect
                
                // Apply cross-feed: each channel gets a mix of itself and the opposite channel
                // This creates clear stereo separation that's easy to hear
                float tempL = wetL;
                float tempR = wetR;
                
                // Left gets: itself + inverted right (creates width)
                wetL = tempL + tempR * crossFeed;
                // Right gets: itself - inverted left (creates width)
                wetR = tempR - tempL * crossFeed;
                
                // Normalize to prevent level increase
                float norm = 1.0f / (1.0f + crossFeed);
                wetL *= norm;
                wetR *= norm;
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