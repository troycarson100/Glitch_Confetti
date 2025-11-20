#include "FormantProcessor.h"
#include <algorithm>
#include <cmath>

FormantProcessor::FormantProcessor()
{
}

void FormantProcessor::prepare(double sampleRate_, int maxBlockSize_)
{
    sampleRate = sampleRate_;
    maxBlockSize = maxBlockSize_;
    
    // Initialize 4 IIR biquad filters per channel (L/R)
    for (int i = 0; i < 4; ++i) {
        // Initialize with safe default coefficients (1kHz bandpass)
        auto defaultCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            sampleRate, 1000.0f, 0.707f);
        
        filtersL[i].coefficients = defaultCoeffs;
        filtersR[i].coefficients = defaultCoeffs;
        filtersL[i].reset();
        filtersR[i].reset();
    }
    
    // Initialize smoothed parameters (20ms smoothing)
    const double smoothTime = 0.02;
    
    vowelSm.reset(sampleRate, smoothTime);
    qSm.reset(sampleRate, smoothTime);
    emphasisSm.reset(sampleRate, smoothTime);
    shiftSm.reset(sampleRate, smoothTime);
    brightnessSm.reset(sampleRate, smoothTime);
    motionSm.reset(sampleRate, smoothTime);
    airSm.reset(sampleRate, smoothTime);
    mixSm.reset(sampleRate, smoothTime);
    
    // Set defaults for audibility
    vowelSm.setCurrentAndTargetValue(0.0f);      // A
    qSm.setCurrentAndTargetValue(2.0f);         // Moderately sharp
    emphasisSm.setCurrentAndTargetValue(3.0f);   // +3 dB emphasis
    shiftSm.setCurrentAndTargetValue(1.0f);       // No shift
    brightnessSm.setCurrentAndTargetValue(0.0f); // 0 dB F4
    motionSm.setCurrentAndTargetValue(0.0f);     // No motion
    airSm.setCurrentAndTargetValue(0.0f);        // No air
    mixSm.setCurrentAndTargetValue(1.0f);        // Full wet
    
    isPrepared = true;
}

void FormantProcessor::setHostTempo(double bpm, bool hasTempo)
{
    // Not used in simplified version
    (void)bpm;
    (void)hasTempo;
}

FormantProcessor::Formants FormantProcessor::getFormantsForVowel(float vowel) const
{
    // Clamp to range
    vowel = juce::jlimit(0.0f, 4.0f, vowel);
    
    // Get integer index and fractional part
    int idx = static_cast<int>(vowel);
    float frac = vowel - static_cast<float>(idx);
    
    // Clamp index to valid range
    idx = juce::jlimit(0, 4, idx);
    int nextIdx = juce::jmin(4, idx + 1);
    
    // Get formants
    const Formants& f0 = kVowels[idx];
    const Formants& f1 = kVowels[nextIdx];
    
    // Linear interpolation
    Formants result;
    result.F1 = f0.F1 + frac * (f1.F1 - f0.F1);
    result.F2 = f0.F2 + frac * (f1.F2 - f0.F2);
    result.F3 = f0.F3 + frac * (f1.F3 - f0.F3);
    
    return result;
}

float FormantProcessor::computeF4(float f2, float f3) const
{
    return juce::jlimit(2000.0f, 4000.0f, 0.45f * f2 + 0.55f * f3);
}

juce::dsp::IIR::Coefficients<float>::Ptr FormantProcessor::makeBandpassCoefficients(float centerFreq, float bandwidth) const
{
    // Calculate Q from bandwidth
    float Q = centerFreq / bandwidth;
    Q = juce::jlimit(0.1f, 100.0f, Q);
    
    // Create bandpass coefficients using JUCE's built-in method
    return juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, centerFreq, Q);
}

void FormantProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples, juce::AudioProcessorValueTreeState& apvts)
{
    if (!isPrepared || numSamples == 0 || sampleRate <= 0.0) return;
    if (buffer.getNumChannels() < 2) return;
    
    // Enable flush-to-zero for stability
    juce::ScopedNoDenormals noDenormals;
    
    // Read parameters from APVTS
    auto* vowelParam = apvts.getRawParameterValue("vowel");
    auto* resonanceParam = apvts.getRawParameterValue("resonance");
    auto* intensityParam = apvts.getRawParameterValue("intensity");
    auto* shiftParam = apvts.getRawParameterValue("formantShift");
    auto* brightnessParam = apvts.getRawParameterValue("formantBrightness");
    auto* motionParam = apvts.getRawParameterValue("formantMotion");
    auto* airParam = apvts.getRawParameterValue("formantAir");
    auto* mixParam = apvts.getRawParameterValue("mix");
    
    if (vowelParam) vowelSm.setTargetValue(vowelParam->load());
    if (resonanceParam) qSm.setTargetValue(resonanceParam->load());
    if (intensityParam) emphasisSm.setTargetValue(intensityParam->load());
    if (shiftParam) shiftSm.setTargetValue(shiftParam->load());
    if (brightnessParam) brightnessSm.setTargetValue(brightnessParam->load());
    if (motionParam) motionSm.setTargetValue(motionParam->load());
    if (airParam) airSm.setTargetValue(airParam->load());
    if (mixParam) mixSm.setTargetValue(mixParam->load());
    
    // Get smoothed parameters
    float vowel = vowelSm.getCurrentValue();
    float q = qSm.getCurrentValue();  // Sharpness multiplier
    float emphasis = emphasisSm.getCurrentValue();
    float shift = shiftSm.getCurrentValue();
    float brightness = brightnessSm.getCurrentValue();
    float motion = motionSm.getCurrentValue();
    float air = airSm.getCurrentValue();
    float mix = mixSm.getCurrentValue();
    
    // Calculate morphed vowel formants
    // Motion determines how far to morph from center vowel
    float delta = motion * 0.75f;
    float vA = juce::jlimit(0.0f, 4.0f, vowel - delta);
    float vB = juce::jlimit(0.0f, 4.0f, vowel + delta);
    
    // Interpolate formants for target vowels A and B
    Formants formantsA = getFormantsForVowel(vA);
    Formants formantsA_shifted = formantsA;
    formantsA_shifted.F1 = juce::jlimit(80.0f, 18000.0f, formantsA.F1 * shift);
    formantsA_shifted.F2 = juce::jlimit(80.0f, 18000.0f, formantsA.F2 * shift);
    formantsA_shifted.F3 = juce::jlimit(80.0f, 18000.0f, formantsA.F3 * shift);
    
    Formants formantsB = getFormantsForVowel(vB);
    Formants formantsB_shifted = formantsB;
    formantsB_shifted.F1 = juce::jlimit(80.0f, 18000.0f, formantsB.F1 * shift);
    formantsB_shifted.F2 = juce::jlimit(80.0f, 18000.0f, formantsB.F2 * shift);
    formantsB_shifted.F3 = juce::jlimit(80.0f, 18000.0f, formantsB.F3 * shift);
    
    // Compute F4 for both
    float f4A = juce::jlimit(80.0f, 18000.0f, computeF4(formantsA_shifted.F2, formantsA_shifted.F3));
    float f4B = juce::jlimit(80.0f, 18000.0f, computeF4(formantsB_shifted.F2, formantsB_shifted.F3));
    
    // Calculate morph balance (0 = all A, 1 = all B)
    // Use motion parameter to blend dynamically if motion > 0
    float morphBalance = motion > 0.01f ? 0.5f : 0.0f; // Static blend for now
    
    // Interpolate formant frequencies
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    
    float f1 = lerp(formantsA_shifted.F1, formantsB_shifted.F1, morphBalance);
    float f2 = lerp(formantsA_shifted.F2, formantsB_shifted.F2, morphBalance);
    float f3 = lerp(formantsA_shifted.F3, formantsB_shifted.F3, morphBalance);
    float f4 = lerp(f4A, f4B, morphBalance);
    
    // Apply Q sharpness multiplier (q is 0.4-18, where 1.0 = standard sharpness)
    // q < 1 = wider (flatter), q > 1 = narrower (sharper)
    float bandwidths[4] = {kBandwidths[0], kBandwidths[1], kBandwidths[2], kBandwidths[3]};
    for (int i = 0; i < 4; ++i) {
        bandwidths[i] /= juce::jmax(0.1f, q); // Narrower bandwidth = higher Q = sharper
    }
    
    // Update filter coefficients ONCE PER BLOCK
    auto coeffs0 = makeBandpassCoefficients(f1, bandwidths[0]);
    auto coeffs1 = makeBandpassCoefficients(f2, bandwidths[1]);
    auto coeffs2 = makeBandpassCoefficients(f3, bandwidths[2]);
    auto coeffs3 = makeBandpassCoefficients(f4, bandwidths[3]);
    
    filtersL[0].coefficients = coeffs0;
    filtersL[1].coefficients = coeffs1;
    filtersL[2].coefficients = coeffs2;
    filtersL[3].coefficients = coeffs3;
    
    filtersR[0].coefficients = coeffs0;
    filtersR[1].coefficients = coeffs1;
    filtersR[2].coefficients = coeffs2;
    filtersR[3].coefficients = coeffs3;
    
    // Convert gains
    float emphasisGain = juce::Decibels::decibelsToGain(emphasis);
    float brightnessGain = juce::Decibels::decibelsToGain(brightness);
    
    // Skip smoothed parameter updates for this block
    vowelSm.skip(numSamples);
    qSm.skip(numSamples);
    emphasisSm.skip(numSamples);
    shiftSm.skip(numSamples);
    brightnessSm.skip(numSamples);
    motionSm.skip(numSamples);
    airSm.skip(numSamples);
    mixSm.skip(numSamples);
    
    // Process audio sample-by-sample
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);
    
    for (int i = 0; i < numSamples; ++i) {
        float dryL = leftChannel[i];
        float dryR = rightChannel[i];
        
        // Process through parallel filter bank
        float wetL = 0.0f;
        float wetR = 0.0f;
        
        for (int j = 0; j < 4; ++j) {
            float filteredL = filtersL[j].processSample(dryL);
            float filteredR = filtersR[j].processSample(dryR);
            
            // Apply gains
            if (j < 3) {
                // F1, F2, F3 get emphasis gain
                filteredL *= emphasisGain;
                filteredR *= emphasisGain;
            } else {
                // F4 gets brightness gain
                filteredL *= brightnessGain;
                filteredR *= brightnessGain * 0.98f; // Slight stereo width
            }
            
            wetL += filteredL;
            wetR += filteredR;
        }
        
        // Gain compensation: parallel bandpass filters reduce overall level
        // Compensate for energy loss from 4 parallel filters
        // Base compensation: ~+6dB to account for parallel filter energy reduction
        float baseCompensation = juce::Decibels::decibelsToGain(6.0f);
        
        // Adjust compensation based on Q (sharper filters = more compensation needed)
        // Q range is 0.4-18, where 1.0 = standard
        float qCompensation = juce::jmap(q, 0.4f, 18.0f, 1.0f, 1.3f); // More compensation for sharper filters
        
        // Adjust for emphasis (higher emphasis = less compensation needed since we're already boosting)
        float emphasisCompensation = juce::jmap(emphasis, -6.0f, 18.0f, 1.2f, 0.8f);
        
        // Combined compensation gain
        float compensationGain = baseCompensation * qCompensation * emphasisCompensation;
        
        wetL *= compensationGain;
        wetR *= compensationGain;
        
        // Guard against overload with high emphasis
        if (emphasis > 12.0f) {
            float safetyScale = juce::Decibels::decibelsToGain(-3.0f); // Reduced safety scale since we have compensation
            wetL *= safetyScale;
            wetR *= safetyScale;
        }
        
        // Final dry/wet mix
        leftChannel[i] = dryL * (1.0f - mix) + wetL * mix;
        rightChannel[i] = dryR * (1.0f - mix) + wetR * mix;
    }
}

FormantProcessor::CurrentState FormantProcessor::getCurrentState() const
{
    CurrentState state;
    
    // Get current formants
    float vowel = vowelSm.getCurrentValue();
    float motion = motionSm.getCurrentValue();
    float shift = shiftSm.getCurrentValue();
    float q = qSm.getCurrentValue();
    
    float delta = motion * 0.75f;
    float vA = juce::jlimit(0.0f, 4.0f, vowel - delta);
    float vB = juce::jlimit(0.0f, 4.0f, vowel + delta);
    
    Formants formantsA = getFormantsForVowel(vA);
    Formants formantsB = getFormantsForVowel(vB);
    
    // Apply shift
    formantsA.F1 *= shift;
    formantsA.F2 *= shift;
    formantsA.F3 *= shift;
    formantsB.F1 *= shift;
    formantsB.F2 *= shift;
    formantsB.F3 *= shift;
    
    // Average
    float morphBalance = motion > 0.01f ? 0.5f : 0.0f;
    state.f1Hz = formantsA.F1 + morphBalance * (formantsB.F1 - formantsA.F1);
    state.f2Hz = formantsA.F2 + morphBalance * (formantsB.F2 - formantsA.F2);
    state.f3Hz = formantsA.F3 + morphBalance * (formantsB.F3 - formantsA.F3);
    
    // Calculate Q from bandwidth
    state.q = (state.f1Hz / kBandwidths[0]) / q;
    state.emphasisDb = emphasisSm.getCurrentValue();
    state.enabled = mixSm.getCurrentValue() > 0.0f;
    
    return state;
}
