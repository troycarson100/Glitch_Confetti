#include "FxAutoPan.h"

//==============================================================================
FxAutoPan::FxAutoPan()
{
    // Initialize smoothed values with default targets
    rateSm.setCurrentAndTargetValue(1.0f);
    phaseSm.setCurrentAndTargetValue(180.0f);
    waveTypeSm.setCurrentAndTargetValue(0);
    waveShapeSm.setCurrentAndTargetValue(0.5f);
    invertedSm.setCurrentAndTargetValue(false);
    amountSm.setCurrentAndTargetValue(0.5f);
}

void FxAutoPan::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    
    // Reset LFO phase
    lfoPhase = 0.0;
    lfoPhaseRadians = 0.0;
    
    // Set smoothing time for parameter changes (100ms for rate to prevent clipping)
    const float smoothingTimeMs = 100.0f;
    const int smoothingSamples = (int)(smoothingTimeMs * sampleRate / 1000.0);
    
    rateSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    phaseSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    waveTypeSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    waveShapeSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    invertedSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    amountSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    widthSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
    mixSm.reset(sampleRate, smoothingTimeMs / 1000.0f);
}

void FxAutoPan::setTargets(const Targets& t)
{
    rateSm.setTargetValue(t.rateHz);
    phaseSm.setTargetValue(t.phaseDegrees);
    waveTypeSm.setTargetValue(t.waveType);
    waveShapeSm.setTargetValue(t.waveShape);
    invertedSm.setTargetValue(t.inverted);
    amountSm.setTargetValue(t.amount);
    widthSm.setTargetValue(t.width);
    mixSm.setTargetValue(t.mix);
}

void FxAutoPan::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 2 || numSamples <= 0) return;
    
    auto* outputL = buffer.getWritePointer(0);
    auto* outputR = buffer.getWritePointer(1);
    
    // Get current parameter values (smoothed)
    float currentRate = rateSm.getNextValue();
    float currentPhase = phaseSm.getNextValue();
    int currentWaveType = waveTypeSm.getNextValue();
    float currentWaveShape = waveShapeSm.getNextValue();
    bool currentInverted = invertedSm.getNextValue();
    float currentAmount = amountSm.getNextValue();
    float currentWidth = widthSm.getNextValue();
    float currentMix = mixSm.getNextValue();
    
    // Clamp rate to prevent extreme values - much more conservative
    currentRate = juce::jlimit(0.01f, 10.0f, currentRate);
    
    // Check for significant rate changes and bypass processing if needed
    float rateChange = std::abs(currentRate - lastRate);
    if (rateChange > 0.5f) { // Much more sensitive - bypass for any rate change > 0.5Hz
        bypassCounter = (int)(sampleRate * 0.2); // Bypass for 200ms (longer)
        lfoPhase = 0.0; // Reset phase
    }
    lastRate = currentRate;
    
    // If we're in bypass mode, don't process the auto pan
    if (bypassCounter > 0) {
        bypassCounter -= numSamples;
        return; // Skip auto pan processing
    }
    
    // Calculate phase increment for the entire block
    double phaseIncrement = 2.0 * juce::MathConstants<double>::pi * currentRate / sampleRate;
    
    // Add phase offset (convert degrees to radians)
    double phaseOffset = currentPhase * juce::MathConstants<double>::pi / 180.0;
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Update LFO phase
        lfoPhase += phaseIncrement;
        
        // Wrap phase to [0, 2π)
        if (lfoPhase >= 2.0 * juce::MathConstants<double>::pi) {
            lfoPhase -= 2.0 * juce::MathConstants<double>::pi;
        }
        if (lfoPhase < 0.0) {
            lfoPhase += 2.0 * juce::MathConstants<double>::pi;
        }
        
        // Add phase offset
        double phaseWithOffset = lfoPhase + phaseOffset;
        
        // Wrap offset phase
        if (phaseWithOffset >= 2.0 * juce::MathConstants<double>::pi) {
            phaseWithOffset -= 2.0 * juce::MathConstants<double>::pi;
        }
        if (phaseWithOffset < 0.0) {
            phaseWithOffset += 2.0 * juce::MathConstants<double>::pi;
        }
        
        // Generate LFO value (-1 to +1)
        float lfoValue = generateLFOValue(currentWaveType, currentWaveShape, phaseWithOffset);
        
        // Clamp amount to prevent extreme panning
        currentAmount = juce::jlimit(0.0f, 0.8f, currentAmount); // Limit to 80% max
        
        // Apply pan amount
        float panPosition = lfoValue * currentAmount;
        
        // Apply inversion if enabled
        if (currentInverted) {
            panPosition = -panPosition;
        }
        
        // --- SURGICAL PATCH: Improved panning with mono source and proper crossfades ---
        
        // Inputs
        const float inL = outputL[sample];
        const float inR = outputR[sample]; // if mono, duplicate beforehand
        
        // Pan pos x in [-1, 1] (you already clamp panPosition to ±0.8)
        const float x = juce::jlimit(-1.0f, 1.0f, panPosition);
        
        // Equal-power gains
        const float theta = (x + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi; // map [-1,1] -> [0, π/2]
        const float gL = std::cos(theta);
        const float gR = std::sin(theta);
        
        // Build a mono "pan source" to avoid stereo peak-doubling
        const float mono = 0.5f * (inL + inR);
        const float panL = mono * gL;
        const float panR = mono * gR;
        
        // Width: 0 = keep original stereo; 1 = fully replace with panned mono
        // (Make widthParam a smoothed 0..1 parameter; 1.0f is classic autopan)
        const float width = currentWidth; // e.g. widthSmooth.getNextValue();
        const float wetL  = inL + width * (panL - inL); // Manual lerp: a + t * (b - a)
        const float wetR  = inR + width * (panR - inR);
        
        // True dry/wet crossfade (prevents "dry+wet at 0 dB" summing)
        const float mix = currentMix; // 0..1, smoothed
        outputL[sample] = inL + mix * (wetL - inL); // Manual lerp: a + t * (b - a)
        outputR[sample] = inR + mix * (wetR - inR);
        
        // No hard limiting needed; equal-power + mono-source + proper crossfades keep levels sane.
    }
}

float FxAutoPan::generateLFOValue(int waveType, float waveShape, double phase)
{
    switch (waveType) {
        case 0: // Sine
            return (float)std::sin(phase);
            
        case 1: // Triangle
            {
                double triangle = (2.0 / juce::MathConstants<double>::pi) * std::asin(std::sin(phase));
                return (float)triangle;
            }
            
        case 2: // Ramp Down (sawtooth)
            {
                double saw = (2.0 * (phase / (2.0 * juce::MathConstants<double>::pi))) - 1.0;
                saw = saw - std::floor(saw + 0.5); // Wrap to [-1, 1]
                return (float)saw;
            }
            
        case 3: // Ramp Up (inverse sawtooth)
            {
                double saw = (2.0 * (phase / (2.0 * juce::MathConstants<double>::pi))) - 1.0;
                saw = saw - std::floor(saw + 0.5); // Wrap to [-1, 1]
                return (float)(-saw); // Invert for ramp up
            }
            
        case 4: // Random
            {
                // Generate random value, but smooth it over time for musical results
                static float randomTarget = 0.0f;
                static float currentRandom = 0.0f;
                
                // Change random target occasionally (every ~200 samples at 44.1kHz for smoother changes)
                static int randomCounter = 0;
                if (++randomCounter >= 200) {
                    randomTarget = random.nextFloat() * 2.0f - 1.0f;
                    randomCounter = 0;
                }
                
                // Smooth towards random target with very slow rate to prevent sudden changes
                currentRandom += 0.002f * (randomTarget - currentRandom);
                
                // Clamp the result to ensure it stays within bounds
                return juce::jlimit(-1.0f, 1.0f, currentRandom);
            }
            
        default:
            return (float)std::sin(phase);
    }
}

float FxAutoPan::applyPanAmount(float panValue, float amount)
{
    // Scale the pan value by the amount parameter
    // amount = 0: no panning (center)
    // amount = 1: full panning range
    return panValue * amount;
}
