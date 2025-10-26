#include "FormantProcessor.h"

FormantProcessor::FormantProcessor()
{
    // Initialize LFO with slower rate
    lfo.setFrequency(2.0); // Reduced from 5.5 Hz to 2.0 Hz
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
    
    // Reset LFO
    lfo.reset();
    lfo.setFrequency(2.0); // Reduced from kLfoRate (5.5 Hz) to 2.0 Hz
    lfoPhase = 0.0;
    
    // Initialize smoothed parameters
    const double smoothTime = 0.01; // 10ms smoothing
    morphSm.reset(sampleRate, smoothTime);
    qSm.reset(sampleRate, smoothTime);
    emphasisSm.reset(sampleRate, smoothTime);
    genderSm.reset(sampleRate, smoothTime);
    vibDepthSm.reset(sampleRate, smoothTime);
    mixSm.reset(sampleRate, smoothTime);
    vowelASm.reset(sampleRate, smoothTime);
    vowelBSm.reset(sampleRate, smoothTime);
    
    // Set initial values
    morphSm.setCurrentAndTargetValue(0.0f);
    qSm.setCurrentAndTargetValue(6.0f);
    emphasisSm.setCurrentAndTargetValue(6.0f);
    genderSm.setCurrentAndTargetValue(1.0f);
    vibDepthSm.setCurrentAndTargetValue(1.0f); // Reduced from 5.0f to 1.0f
    mixSm.setCurrentAndTargetValue(0.5f);
    vowelASm.setCurrentAndTargetValue(0);
    vowelBSm.setCurrentAndTargetValue(1);
    
    // Initialize formants
    currentFormants = getInterpolatedFormants(0, 1, 0.0f, 1.0f);
    updateCoeffs();
}

void FormantProcessor::setTargets(const Targets& t)
{
    currentTargets = t;
    
    // Clamp values
    currentTargets.vowelA = juce::jlimit(0, 4, t.vowelA);
    currentTargets.vowelB = juce::jlimit(0, 4, t.vowelB);
    currentTargets.morph = juce::jlimit(0.0f, 1.0f, t.morph);
    currentTargets.q = juce::jlimit(0.3f, 20.0f, t.q);
    currentTargets.emphasis = juce::jlimit(0.0f, 12.0f, t.emphasis);
    currentTargets.gender = juce::jlimit(0.5f, 2.0f, t.gender);
    currentTargets.vibDepth = juce::jlimit(0.0f, 30.0f, t.vibDepth);
    currentTargets.mix = juce::jlimit(0.0f, 1.0f, t.mix);
}

void FormantProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 2) return;
    
    // Process smoothed parameters
    processParams();
    
    // Get current formants
    currentFormants = getInterpolatedFormants(
        vowelASm.getCurrentValue(),
        vowelBSm.getCurrentValue(),
        morphSm.getCurrentValue(),
        genderSm.getCurrentValue()
    );
    
    // Calculate vibrato modulation
    float vibratoMod = 0.0f;
    if (vibDepthSm.getCurrentValue() > 0.0f)
    {
        const float lfoValue = lfo.processSample(1.0f);
        const float cents = vibDepthSm.getCurrentValue();
        const float ratio = std::pow(2.0f, cents / 1200.0f);
        vibratoMod = (ratio - 1.0f) * lfoValue;
    }
    
    // Update filter coefficients with vibrato (only once per buffer)
    updateCoeffs(vibratoMod);
    
    // Process audio
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);
    
    const float emphasisGain = juce::Decibels::decibelsToGain(emphasisSm.getCurrentValue());
    const float mix = mixSm.getCurrentValue();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryL = leftChannel[sample];
        const float dryR = rightChannel[sample];
        
        // Process left channel through 3 parallel band-pass filters
        float wetL = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            float filtered = filtersL[i].processSample(dryL);
            wetL += filtered;
        }
        
        // Process right channel through 3 parallel band-pass filters
        float wetR = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            float filtered = filtersR[i].processSample(dryR);
            wetR += filtered;
        }
        
        // Apply emphasis gain
        wetL *= emphasisGain;
        wetR *= emphasisGain;
        
        // Mix dry and wet
        leftChannel[sample] = dryL * (1.0f - mix) + wetL * mix;
        rightChannel[sample] = dryR * (1.0f - mix) + wetR * mix;
    }
}

FormantProcessor::Formants FormantProcessor::getInterpolatedFormants(int vowelA, int vowelB, float morph, float gender)
{
    // Clamp indices
    vowelA = juce::jlimit(0, 4, vowelA);
    vowelB = juce::jlimit(0, 4, vowelB);
    morph = juce::jlimit(0.0f, 1.0f, morph);
    
    // Get formant data for both vowels
    const Formants& formantsA = kVowelTable[vowelA];
    const Formants& formantsB = kVowelTable[vowelB];
    
    // Interpolate between vowels
    Formants interpolated;
    interpolated.F1 = formantsA.F1 + morph * (formantsB.F1 - formantsA.F1);
    interpolated.F2 = formantsA.F2 + morph * (formantsB.F2 - formantsA.F2);
    interpolated.F3 = formantsA.F3 + morph * (formantsB.F3 - formantsA.F3);
    
    // Apply gender shift
    interpolated.F1 *= gender;
    interpolated.F2 *= gender;
    interpolated.F3 *= gender;
    
    return interpolated;
}

void FormantProcessor::updateCoeffs(float vibratoMod)
{
    const float q = qSm.getCurrentValue();
    const float vibratoRatio = 1.0f + vibratoMod;
    
    // Calculate formant frequencies with vibrato
    const float f1 = currentFormants.F1 * vibratoRatio;
    const float f2 = currentFormants.F2 * vibratoRatio;
    const float f3 = currentFormants.F3 * vibratoRatio;
    
    // Clamp frequencies to valid range
    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float f1Clamped = juce::jlimit(20.0f, nyquist - 100.0f, f1);
    const float f2Clamped = juce::jlimit(20.0f, nyquist - 100.0f, f2);
    const float f3Clamped = juce::jlimit(20.0f, nyquist - 100.0f, f3);
    
    // Create band-pass filter coefficients using RBJ Audio EQ Cookbook
    auto coeffs1 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f1Clamped, q);
    auto coeffs2 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f2Clamped, q);
    auto coeffs3 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f3Clamped, q);
    
    // Update left channel filters
    *filtersL[0].coefficients = *coeffs1;
    *filtersL[1].coefficients = *coeffs2;
    *filtersL[2].coefficients = *coeffs3;
    
    // Update right channel filters
    *filtersR[0].coefficients = *coeffs1;
    *filtersR[1].coefficients = *coeffs2;
    *filtersR[2].coefficients = *coeffs3;
}

void FormantProcessor::processParams()
{
    // Update smoothed parameters
    morphSm.skip(1);
    qSm.skip(1);
    emphasisSm.skip(1);
    genderSm.skip(1);
    vibDepthSm.skip(1);
    mixSm.skip(1);
    vowelASm.skip(1);
    vowelBSm.skip(1);
    
    // Set target values
    morphSm.setTargetValue(currentTargets.morph);
    qSm.setTargetValue(currentTargets.q);
    emphasisSm.setTargetValue(currentTargets.emphasis);
    genderSm.setTargetValue(currentTargets.gender);
    vibDepthSm.setTargetValue(currentTargets.vibDepth);
    mixSm.setTargetValue(currentTargets.mix);
    vowelASm.setTargetValue(currentTargets.vowelA);
    vowelBSm.setTargetValue(currentTargets.vowelB);
}
