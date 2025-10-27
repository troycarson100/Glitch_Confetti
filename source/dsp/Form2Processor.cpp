#include "Form2Processor.h"
#include <cmath>

Form2Processor::Form2Processor()
{
}

void Form2Processor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const float smoothTime = 0.03f; // 30ms smoothing
    
    // Initialize smoothed parameters
    vowelSm.reset(sampleRate, smoothTime);
    emphasisSm.reset(sampleRate, smoothTime);
    sharpnessSm.reset(sampleRate, smoothTime);
    shiftSm.reset(sampleRate, smoothTime);
    brightnessSm.reset(sampleRate, smoothTime);
    motionSm.reset(sampleRate, smoothTime);
    airSm.reset(sampleRate, smoothTime);
    mixSm.reset(sampleRate, smoothTime);
    
    // Set initial values with high defaults for obvious vowels
    vowelSm.setCurrentAndTargetValue(0.0f); // A
    emphasisSm.setCurrentAndTargetValue(12.0f); // +12 dB
    sharpnessSm.setCurrentAndTargetValue(10.0f); // Q=10
    shiftSm.setCurrentAndTargetValue(1.0f);
    brightnessSm.setCurrentAndTargetValue(3.0f); // +3 dB
    motionSm.setCurrentAndTargetValue(0.0f);
    airSm.setCurrentAndTargetValue(0.0f);
    mixSm.setCurrentAndTargetValue(1.0f); // 100% wet
    
    // Initialize envelope
    envFast = envSlow = 0.0f;
    
    // Initialize pink noise
    for (int i = 0; i < 7; ++i) pink[i] = 0.0f;
    rng.setSeedRandomly();
    
    // Initialize LFO
    lfoPhase = 0.0f;
    
    // Prepare State-Variable filters
    juce::dsp::ProcessSpec svfSpec;
    svfSpec.sampleRate = sampleRate;
    svfSpec.maximumBlockSize = 512;
    svfSpec.numChannels = 1;
    
    for (int i = 0; i < 4; ++i)
    {
        filtersL[i].reset();
        filtersL[i].prepare(svfSpec);
        filtersL[i].setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        filtersL[i].setCutoffFrequency(1000.0f);
        filtersL[i].setResonance(10.0f);
        
        filtersR[i].reset();
        filtersR[i].prepare(svfSpec);
        filtersR[i].setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        filtersR[i].setCutoffFrequency(1000.0f);
        filtersR[i].setResonance(10.0f);
    }
}

void Form2Processor::setHostTempo(double bpm, bool hasTempo_)
{
    hostBpm = bpm;
    hasHostTempo = hasTempo_;
}

float Form2Processor::interpolateVowel(float v) const
{
    // Clamp vowel to 0-4 range
    v = juce::jlimit(0.0f, 4.0f, v);
    
    // Linear interpolation between vowel points
    int idx = static_cast<int>(v);
    float t = v - idx;
    
    if (idx >= 4)
    {
        idx = 4;
        t = 0.0f;
    }
    
    const F123& v1 = kVowels[idx];
    const F123& v2 = kVowels[juce::jmin(idx + 1, 4)];
    
    return v1.f1 * (1.0f - t) + v2.f1 * t; // Interpolate F1
}

float Form2Processor::computeF4(float f2, float f3) const
{
    // F4 as average: F4 = 0.5*F2 + 0.5*F3
    float f4 = 0.5f * f2 + 0.5f * f3;
    return juce::jlimit(2000.0f, 4000.0f, f4); // Clamp to 2-4 kHz
}

void Form2Processor::updateFormants(float v, float shift, float q)
{
    // Clamp vowel
    v = juce::jlimit(0.0f, 4.0f, v);
    
    // Get base formants
    int idx = static_cast<int>(v);
    float t = v - idx;
    
    if (idx >= 4)
    {
        idx = 4;
        t = 0.0f;
    }
    
    const F123& v1 = kVowels[idx];
    const F123& v2 = kVowels[juce::jmin(idx + 1, 4)];
    
    // Interpolate F1, F2, F3
    float f1 = (v1.f1 * (1.0f - t) + v2.f1 * t) * shift;
    float f2 = (v1.f2 * (1.0f - t) + v2.f2 * t) * shift;
    float f3 = (v1.f3 * (1.0f - t) + v2.f3 * t) * shift;
    float f4 = computeF4(f2, f3) * shift;
    
    // Clamp frequencies to valid range
    const float nyquist = sampleRate * 0.48f;
    f1 = juce::jlimit(80.0f, nyquist, f1);
    f2 = juce::jlimit(80.0f, nyquist, f2);
    f3 = juce::jlimit(80.0f, nyquist, f3);
    f4 = juce::jlimit(2000.0f, nyquist, f4);
    
    // Stereo offset on F4
    float f4R = f4 * 1.02f;
    
    // Update all filters
    for (int i = 0; i < 4; ++i)
    {
        float freq = (i == 0) ? f1 : (i == 1) ? f2 : (i == 2) ? f3 : f4;
        filtersL[i].setCutoffFrequency(freq);
        filtersL[i].setResonance(q);
        
        float freqR = (i == 3) ? f4R : freq;
        filtersR[i].setCutoffFrequency(freqR);
        filtersR[i].setResonance(q);
    }
}

float Form2Processor::nextPink()
{
    // 7-pole pink noise
    static constexpr float p[7] = {0.997f, 0.99f, 0.9f, 0.7f, 0.5f, 0.3f, 0.1f};
    
    for (int i = 0; i < 7; ++i)
    {
        pink[i] *= p[i];
        pink[i] += (rng.nextFloat() - 0.5f) * (1.0f - p[i]);
    }
    
    return pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6];
}

void Form2Processor::updateEnvelope(float sample)
{
    // Fast attack, slow release envelope
    float absSample = std::abs(sample);
    envFast = 0.9f * envFast + 0.1f * absSample;
    envSlow = 0.99f * envSlow + 0.01f * absSample;
}

void Form2Processor::process(juce::dsp::AudioBlock<float>& block)
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = static_cast<int>(block.getNumSamples());
    
    if (numChannels < 2 || numSamples == 0) return;
    
    auto* leftChannel = block.getChannelPointer(0);
    auto* rightChannel = block.getChannelPointer(1);
    
    // Smooth parameters across block
    vowelSm.skip(numSamples);
    emphasisSm.skip(numSamples);
    sharpnessSm.skip(numSamples);
    shiftSm.skip(numSamples);
    brightnessSm.skip(numSamples);
    motionSm.skip(numSamples);
    airSm.skip(numSamples);
    mixSm.skip(numSamples);
    
    // Get current values
    float vowel = vowelSm.getCurrentValue();
    float emphasis = emphasisSm.getCurrentValue();
    float q = sharpnessSm.getCurrentValue();
    float shift = shiftSm.getCurrentValue();
    float brightness = brightnessSm.getCurrentValue();
    float motion = motionSm.getCurrentValue();
    float air = airSm.getCurrentValue();
    float mix = mixSm.getCurrentValue();
    
    // Compute LFO for motion
    double lfoHz = hasHostTempo ? (hostBpm / 240.0) : 0.5;
    float lfoInc = static_cast<float>(lfoHz / sampleRate);
    
    // Apply motion modulation to vowel
    float vowelMod = vowel + motion * 0.15f * std::sin(lfoPhase * juce::MathConstants<float>::twoPi);
    vowelMod = juce::jlimit(0.0f, 4.0f, vowelMod);
    
    // Update LFO phase
    lfoPhase += lfoInc * numSamples;
    lfoPhase = std::fmod(lfoPhase, 1.0f);
    
    // Update formants once per block
    updateFormants(vowelMod, shift, q);
    
    // Convert dB gains to linear
    float emphasisGain = juce::Decibels::decibelsToGain(emphasis);
    float brightnessGain = juce::Decibels::decibelsToGain(brightness);
    
    // Process audio
    for (int i = 0; i < numSamples; ++i)
    {
        float dryL = leftChannel[i];
        float dryR = rightChannel[i];
        
        // Update envelope for air gate
        updateEnvelope(dryL);
        
        // Process through formant filters
        float band1L = filtersL[0].processSample(0, dryL);
        float band2L = filtersL[1].processSample(0, dryL);
        float band3L = filtersL[2].processSample(0, dryL);
        float band4L = filtersL[3].processSample(0, dryL);
        
        float band1R = filtersR[0].processSample(0, dryR);
        float band2R = filtersR[1].processSample(0, dryR);
        float band3R = filtersR[2].processSample(0, dryR);
        float band4R = filtersR[3].processSample(0, dryR);
        
        // Apply per-band gains and sum
        float wetL = (band1L + band2L + band3L) * emphasisGain + band4L * brightnessGain;
        float wetR = (band1R + band2R + band3R) * emphasisGain + band4R * brightnessGain;
        
        // Add air layer (transient-gated pink noise, band-passed ~7 kHz)
        float gate = juce::jlimit(0.0f, 1.0f, 4.0f * (envFast - envSlow));
        float airSample = nextPink() * gate * air * 0.3f;
        
        wetL += airSample;
        wetR += airSample;
        
        // Apply overall mix
        leftChannel[i] = dryL * (1.0f - mix) + wetL * mix * 0.5f;
        rightChannel[i] = dryR * (1.0f - mix) + wetR * mix * 0.5f;
    }
}