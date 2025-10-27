#include "FormantProcessor.h"

FormantProcessor::FormantProcessor()
{
    // No LFO needed for simplified vowel filter
}

void FormantProcessor::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    // Prepare filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
    spec.numChannels = 1;
    
    for (int i = 0; i < 3; ++i)
    {
        filtersL[i].reset();
        filtersL[i].prepare(spec);
        filtersR[i].reset();
        filtersR[i].prepare(spec);
    }
    
    // Initialize smoothed parameters
    const double smoothTime = 0.01; // 10ms smoothing
    resonanceSm.reset(sampleRate, smoothTime);
    intensitySm.reset(sampleRate, smoothTime);
    mixSm.reset(sampleRate, smoothTime);
    vowelSm.reset(sampleRate, smoothTime);
    
    // Set initial values
    resonanceSm.setCurrentAndTargetValue(12.0f);
    intensitySm.setCurrentAndTargetValue(6.0f);
    mixSm.setCurrentAndTargetValue(0.8f);
    vowelSm.setCurrentAndTargetValue(0);
    
    // Initialize formants
    currentFormants = getFormantsForVowel(0);
    
    // Initialize filters with dummy coefficients to prevent crash
    for (int i = 0; i < 3; ++i)
    {
        // Create dummy allpass coefficients (passes signal through unchanged)
        auto dummyCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f, 1.0f);
        *filtersL[i].coefficients = *dummyCoeffs;
        *filtersR[i].coefficients = *dummyCoeffs;
    }
    
    // Now update to real coefficients
    updateCoeffs();
    
    // Mark as prepared
    isPrepared = true;
}

void FormantProcessor::setTargets(const Targets& t)
{
    currentTargets = t;
    
    // Clamp values
    currentTargets.vowel = juce::jlimit(0, 4, t.vowel);
    currentTargets.resonance = juce::jlimit(0.5f, 20.0f, t.resonance);
    currentTargets.intensity = juce::jlimit(0.0f, 12.0f, t.intensity);
    currentTargets.mix = juce::jlimit(0.0f, 1.0f, t.mix);
}

void FormantProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!isPrepared || numSamples == 0 || sampleRate <= 0.0) return;
    
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 2) return;
    
    // Update smoothed parameters once per buffer
    processParams();
    
    // Get current formants for selected vowel
    int currentVowel = vowelSm.getCurrentValue();
    float currentQ = resonanceSm.getCurrentValue();
    float currentIntensity = intensitySm.getCurrentValue();
    
    // Only update filters if parameters changed
    if (currentVowel != lastVowel || 
        std::abs(currentQ - lastQ) > 0.01f ||
        std::abs(currentIntensity - lastIntensity) > 0.01f)
    {
        currentFormants = getFormantsForVowel(currentVowel);
        updateCoeffs();
        lastVowel = currentVowel;
        lastQ = currentQ;
        lastIntensity = currentIntensity;
    }
    
    // Process audio
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);
    
    const float intensityGain = juce::Decibels::decibelsToGain(intensitySm.getCurrentValue());
    const float mix = mixSm.getCurrentValue();
    
    // Safety check: ensure filters are ready
    if (filtersL[0].coefficients == nullptr || filtersR[0].coefficients == nullptr)
    {
        return; // Filters not initialized yet
    }
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryL = leftChannel[sample];
        const float dryR = rightChannel[sample];
        
        // Process left channel through 3 parallel bandpass filters
        float wetL = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            if (filtersL[i].coefficients != nullptr)
            {
                float filtered = filtersL[i].processSample(dryL);
                wetL += filtered;
            }
        }
        
        // Process right channel through 3 parallel bandpass filters
        float wetR = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            if (filtersR[i].coefficients != nullptr)
            {
                float filtered = filtersR[i].processSample(dryR);
                wetR += filtered;
            }
        }
        
        // Apply intensity gain
        wetL *= intensityGain;
        wetR *= intensityGain;
        
        // Mix dry and wet
        leftChannel[sample] = dryL * (1.0f - mix) + wetL * mix;
        rightChannel[sample] = dryR * (1.0f - mix) + wetR * mix;
    }
}

FormantProcessor::Formants FormantProcessor::getFormantsForVowel(int vowel) const
{
    // Clamp index
    vowel = juce::jlimit(0, 4, vowel);
    
    // Return formants for selected vowel (no interpolation, no gender shift)
    return kVowelTable[vowel];
}

void FormantProcessor::updateCoeffs()
{
    if (sampleRate <= 0.0) return; // Not initialized yet
    
    const float q = resonanceSm.getCurrentValue();
    
    // Get formant frequencies
    const float f1 = currentFormants.F1;
    const float f2 = currentFormants.F2;
    const float f3 = currentFormants.F3;
    
    // Clamp frequencies to valid range
    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float f1Clamped = juce::jlimit(20.0f, nyquist - 100.0f, f1);
    const float f2Clamped = juce::jlimit(20.0f, nyquist - 100.0f, f2);
    const float f3Clamped = juce::jlimit(20.0f, nyquist - 100.0f, f3);
    
    // Create bandpass filter coefficients with different Q for each formant
    // This creates more natural vowel coloration
    const float q1 = q * 1.2f;  // F1 slightly wider
    const float q2 = q;         // F2 standard
    const float q3 = q * 0.9f;  // F3 slightly narrower
    
    auto coeffs1 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f1Clamped, q1);
    auto coeffs2 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f2Clamped, q2);
    auto coeffs3 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f3Clamped, q3);
    
    // Update left channel filters with null check
    if (filtersL[0].coefficients != nullptr)
    {
        *filtersL[0].coefficients = *coeffs1;
    }
    if (filtersL[1].coefficients != nullptr)
    {
        *filtersL[1].coefficients = *coeffs2;
    }
    if (filtersL[2].coefficients != nullptr)
    {
        *filtersL[2].coefficients = *coeffs3;
    }
    
    // Update right channel filters with null check
    if (filtersR[0].coefficients != nullptr)
    {
        *filtersR[0].coefficients = *coeffs1;
    }
    if (filtersR[1].coefficients != nullptr)
    {
        *filtersR[1].coefficients = *coeffs2;
    }
    if (filtersR[2].coefficients != nullptr)
    {
        *filtersR[2].coefficients = *coeffs3;
    }
}

void FormantProcessor::processParams()
{
    // Update smoothed parameters
    resonanceSm.skip(1);
    intensitySm.skip(1);
    mixSm.skip(1);
    vowelSm.skip(1);
    
    // Set target values
    resonanceSm.setTargetValue(currentTargets.resonance);
    intensitySm.setTargetValue(currentTargets.intensity);
    mixSm.setTargetValue(currentTargets.mix);
    vowelSm.setTargetValue(currentTargets.vowel);
}

FormantProcessor::CurrentState FormantProcessor::getCurrentState() const
{
    CurrentState state;
    
    // Get current formants for selected vowel
    Formants formants = getFormantsForVowel(vowelSm.getCurrentValue());
    
    state.f1Hz = formants.F1;
    state.f2Hz = formants.F2;
    state.f3Hz = formants.F3;
    state.q = resonanceSm.getCurrentValue();
    state.emphasisDb = intensitySm.getCurrentValue();
    state.enabled = currentTargets.mix > 0.0f; // Consider enabled if mix > 0
    
    return state;
}
