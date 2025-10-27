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
    morphXSm.reset(sampleRate, smoothTime);
    morphYSm.reset(sampleRate, smoothTime);
    sharpnessSm.reset(sampleRate, smoothTime);
    emphasisSm.reset(sampleRate, smoothTime);
    shiftSm.reset(sampleRate, smoothTime);
    motionSm.reset(sampleRate, smoothTime);
    airSm.reset(sampleRate, smoothTime);
    mixSm.reset(sampleRate, smoothTime);
    
    // Set initial values
    morphXSm.setCurrentAndTargetValue(0.30f);
    morphYSm.setCurrentAndTargetValue(0.65f);
    sharpnessSm.setCurrentAndTargetValue(8.0f);
    emphasisSm.setCurrentAndTargetValue(6.0f);
    shiftSm.setCurrentAndTargetValue(1.0f);
    motionSm.setCurrentAndTargetValue(0.35f);
    airSm.setCurrentAndTargetValue(0.20f);
    mixSm.setCurrentAndTargetValue(0.5f);
    
    // Prepare SVFs
    prepareSVFs();
    
    // Initialize pink noise
    for (int i = 0; i < 7; ++i) pink[i] = 0.0f;
    rng.setSeedRandomly();
    
    // Initialize envelopes
    envRms = envSlow = envFast = 0.0f;
    envAlpha = 0.0f;
}

void Form2Processor::prepareSVFs()
{
    // Initialize all IIR filters as bandpass
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    
    for (int i = 0; i < 4; ++i)
    {
        filtersL[i].reset();
        filtersL[i].prepare(spec);
        filtersR[i].reset();
        filtersR[i].prepare(spec);
        
        // Initial bandpass coefficients (will be updated)
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1000.0f, 1.0f);
        *filtersL[i].coefficients = *coeffs;
        *filtersR[i].coefficients = *coeffs;
    }
}

void Form2Processor::updateFormants(float x, float y, float shift, float q, bool isRight)
{
    // Bilinear interpolation
    FF result = bilerp(x, y);
    
    // Apply shift (gender/size)
    float f1 = result.f1 * shift;
    float f2 = result.f2 * shift;
    float f3 = result.f3 * shift;
    float f4 = result.f4 * shift;
    
    // Stereo offset on F4
    if (isRight)
        f4 *= 1.02f;
    
    // Clamp to valid range
    const float nyquist = sampleRate * 0.5f;
    f1 = juce::jlimit(50.0f, nyquist - 100.0f, f1);
    f2 = juce::jlimit(50.0f, nyquist - 100.0f, f2);
    f3 = juce::jlimit(50.0f, nyquist - 100.0f, f3);
    f4 = juce::jlimit(50.0f, nyquist - 100.0f, f4);
    
    // Map sharpness to Q with proper range for bandpass filters
    // sharpness is 0.4-18.0, but Q for bandpass should be more like 0.8-2.5 (tighter for vowels)
    float sharpness = sharpnessSm.getCurrentValue();
    float qActual = juce::jmap(sharpness, 0.4f, 18.0f, 0.8f, 2.5f);
    // Apply stronger gain boost to compensate for narrow bandwidth (more resonant peaks need more gain)
    qGain = juce::jmap(qActual, 0.8f, 2.5f, 1.2f, 5.0f); // Boost up to 5x at high Q to keep volume consistent and emphasize vowels
    
    // Update filters (IIR::Filter uses setCoefficients)
    auto& fl = isRight ? filtersR : filtersL;
    auto c1 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f1, qActual);
    auto c2 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f2, qActual);
    auto c3 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f3, qActual);
    auto c4 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f4, qActual);
    
    *fl[0].coefficients = *c1;
    *fl[1].coefficients = *c2;
    *fl[2].coefficients = *c3;
    *fl[3].coefficients = *c4;
}

Form2Processor::FF Form2Processor::bilerp(float x, float y) const
{
    // Bilinear interpolation: (1-x)(1-y)*LL + x*(1-y)*LR + (1-x)*y*UL + x*y*UR
    float wx = 1.0f - x, wy = 1.0f - y;
    
    return {
        wx*wy*LL.f1 + x*wy*LR.f1 + wx*y*UL.f1 + x*y*UR.f1,
        wx*wy*LL.f2 + x*wy*LR.f2 + wx*y*UL.f2 + x*y*UR.f2,
        wx*wy*LL.f3 + x*wy*LR.f3 + wx*y*UL.f3 + x*y*UR.f3,
        wx*wy*LL.f4 + x*wy*LR.f4 + wx*y*UL.f4 + x*y*UR.f4
    };
}

float Form2Processor::nextPink()
{
    // 7-pole Voss-McCartney pink noise
    static constexpr float p1 = 0.997f, p2 = 0.99f, p3 = 0.9f, p4 = 0.7f;
    static constexpr float p5 = 0.5f, p6 = 0.3f, p7 = 0.1f;
    
    pink[0] *= p1; pink[0] += (rng.nextFloat() - 0.5f) * (1.0f - p1);
    pink[1] *= p2; pink[1] += (rng.nextFloat() - 0.5f) * (1.0f - p2);
    pink[2] *= p3; pink[2] += (rng.nextFloat() - 0.5f) * (1.0f - p3);
    pink[3] *= p4; pink[3] += (rng.nextFloat() - 0.5f) * (1.0f - p4);
    pink[4] *= p5; pink[4] += (rng.nextFloat() - 0.5f) * (1.0f - p5);
    pink[5] *= p6; pink[5] += (rng.nextFloat() - 0.5f) * (1.0f - p6);
    pink[6] *= p7; pink[6] += (rng.nextFloat() - 0.5f) * (1.0f - p7);
    
    return pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6];
}

void Form2Processor::updateEnvState(float numSamples)
{
    // Calculate alpha from block size (30-80ms time constant)
    float tc = 0.05f; // 50ms
    float alpha = 1.0f - std::exp(-1.0f / (sampleRate * tc / numSamples));
    
    // Update envelopes (simple 1-pole smoothing)
    envSlow += alpha * (envRms - envSlow);
    envFast += alpha * 0.3f * (envRms - envFast); // faster
}

void Form2Processor::setHostTempo(double bpm, bool hasTempo_)
{
    hostBpm = bpm;
    hasHostTempo = hasTempo_;
}

void Form2Processor::process(juce::dsp::AudioBlock<float>& block)
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = static_cast<int>(block.getNumSamples());
    
    if (numChannels < 2 || numSamples == 0) return;
    
    auto* leftChannel = block.getChannelPointer(0);
    auto* rightChannel = block.getChannelPointer(1);
    
    // Update smoothed parameters - skip entire block at once
    morphXSm.skip(numSamples);
    morphYSm.skip(numSamples);
    sharpnessSm.skip(numSamples);
    emphasisSm.skip(numSamples);
    shiftSm.skip(numSamples);
    motionSm.skip(numSamples);
    airSm.skip(numSamples);
    mixSm.skip(numSamples);
    
    // Get current values
    float mx = morphXSm.getCurrentValue();
    float my = morphYSm.getCurrentValue();
    float motionDepth = motionSm.getCurrentValue();
    float shift = shiftSm.getCurrentValue();
    float q = sharpnessSm.getCurrentValue();
    
    // LFO phase update
    double lfoHz = hasHostTempo ? (hostBpm / 240.0) : 0.5; // 1 bar if synced, else 0.5 Hz
    float lfoInc = static_cast<float>(lfoHz / sampleRate);
    
    // Apply motion ellipse
    float mxAct = mx + motionDepth * 0.06f * std::sin(lfoPhase * juce::MathConstants<float>::twoPi);
    float myAct = my + motionDepth * 0.04f * std::sin((lfoPhase + 0.25f) * juce::MathConstants<float>::twoPi);
    mxAct = juce::jlimit(0.0f, 1.0f, mxAct);
    myAct = juce::jlimit(0.0f, 1.0f, myAct);
    
    // Update formant positions (once per block)
    updateFormants(mxAct, myAct, shift, q, false); // L
    updateFormants(mxAct, myAct, shift, q, true);  // R
    
    // Update LFO phase
    lfoPhase += lfoInc * numSamples;
    lfoPhase = std::fmod(lfoPhase, 1.0f);
    
    // Block RMS calculation
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        sum += leftChannel[i] * leftChannel[i];
    }
    envRms = std::sqrt(sum / numSamples);
    
    // Update envelope state
    updateEnvState(numSamples);
    
    // Dynamic emphasis gains
    float gMax = juce::Decibels::decibelsToGain(emphasisSm.getCurrentValue());
    float k = 0.6f;
    float gEnv = juce::jmin(gMax, 1.0f + k * envSlow);
    float gEnvL = gEnv, gEnvR = gEnv;
    
    // Gate for air layer
    float gate = juce::jlimit(0.0f, 1.0f, 4.0f * (envFast - envSlow));
    
    // Air gain
    float airGain = airSm.getCurrentValue();
    float mix = mixSm.getCurrentValue();
    
    // Process audio
    for (int i = 0; i < numSamples; ++i)
    {
        float dryL = leftChannel[i];
        float dryR = rightChannel[i];
        
        // Process through 4 parallel IIR filters - sum the bandpass outputs
        float band1L = filtersL[0].processSample(dryL);
        float band2L = filtersL[1].processSample(dryL);
        float band3L = filtersL[2].processSample(dryL);
        float band4L = filtersL[3].processSample(dryL);
        
        float band1R = filtersR[0].processSample(dryR);
        float band2R = filtersR[1].processSample(dryR);
        float band3R = filtersR[2].processSample(dryR);
        float band4R = filtersR[3].processSample(dryR);
        
        // Sum and apply emphasis gain and Q gain compensation
        float wetL = (band1L + band2L + band3L + band4L) * gEnvL * qGain;
        float wetR = (band1R + band2R + band3R + band4R) * gEnvR * qGain;
        
        // Add air/breath layer
        float air = nextPink() * gate * airGain * 0.3f;
        wetL += air;
        wetR += air;
        
        // Mix dry and wet
        leftChannel[i] = dryL * (1.0f - mix) + wetL * mix;
        rightChannel[i] = dryR * (1.0f - mix) + wetR * mix;
    }
}

void Form2Processor::setMorphX(float vowelIndex)
{
    // Map discrete vowel index (0-4) to X position on vowel plane
    // 0=A, 1=E, 2=I, 3=O, 4=U
    const float vowelMap[5] = {0.8f, 0.0f, 1.0f, 0.0f, 0.2f}; // X positions
    const float charMap[5] = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f};  // Y positions
    
    int idx = juce::jlimit(0, 4, static_cast<int>(vowelIndex));
    float xPos = vowelMap[idx];
    float yPos = charMap[idx];
    
    // If pure U vowel (index 4), use special UU formant set
    if (idx == 4) {
        // Use UU directly (will be handled in updateFormants)
    }
    
    morphXSm.setTargetValue(xPos);
    morphYSm.setTargetValue(yPos);
}

