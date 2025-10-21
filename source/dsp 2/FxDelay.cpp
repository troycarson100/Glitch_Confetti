#include "FxDelay.h"

// Soft saturation helper
static inline float softSat(float x, float gain)
{
    float driven = x * gain;
    return std::tanh(driven) / gain;
}

FxDelay::FxDelay()
{
}

void FxDelay::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    
    // Ring buffer sized for 4 seconds max delay
    int ringSize = (int)juce::nextPowerOfTwo((int)std::ceil(4.0 * sr));
    ringL.assign(ringSize, 0.0f);
    ringR.assign(ringSize, 0.0f);
    mask = ringSize - 1;
    writePos = 0;
    
    // Musical slope limiting: 200ms per second max change rate
    // This prevents artifacts while keeping changes musical
    const float msPerSecond = 200.0f;
    delaySampMaxStep = (msPerSecond * 0.001f) * (float)sampleRate / (float)sampleRate;
    
    // Initialize current delay to a safe middle value
    delaySampCurrent = 250.0f * 0.001f * (float)sampleRate;
    
    // Initialize smoothers with musical timing
    timeMsSm.reset(sr, 0.020);     // 20ms for delay time
    fbSm.reset(sr, 0.015);         // 15ms for feedback
    mixSm.reset(sr, 0.010);        // 10ms for mix
    driveSm.reset(sr, 0.015);      // 15ms for drive
    hiCutHzSm.reset(sr, 0.040);    // 40ms for filters
    lowCutHzSm.reset(sr, 0.040);   // 40ms for filters
    wowDepthSm.reset(sr, 0.050);   // 50ms for modulation
    wowRateSm.reset(sr, 0.050);    // 50ms for modulation
    
    // Initialize LFO phases
    lfoPhase = 0.0;
    lfoPhase2 = 0.0;
    wowLP = 0.0f;
    
    // Initialize SVF
    svf.prepare(sr);
}

void FxDelay::setTargets(const Targets& t)
{
    // Set parameter targets with safety limits
    timeMsSm.setTargetValue(juce::jlimit(10.0f, 2000.0f, t.timeMs));
    fbSm.setTargetValue(juce::jlimit(0.0f, 0.95f, t.feedback));
    mixSm.setTargetValue(juce::jlimit(0.0f, 1.0f, t.mix));
    driveSm.setTargetValue(juce::jlimit(0.0f, 1.0f, t.drive));
    hiCutHzSm.setTargetValue(juce::jlimit(1000.0f, 20000.0f, t.hiCutHz));
    lowCutHzSm.setTargetValue(juce::jlimit(20.0f, 2000.0f, t.lowCutHz));
    wowDepthSm.setTargetValue(juce::jlimit(0.0f, 1.0f, t.wowDepth));
    wowRateSm.setTargetValue(juce::jlimit(0.1f, 8.0f, t.wowRate));
}

void FxDelay::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples <= 0) return;
    
    auto* inputL = buffer.getReadPointer(0);
    auto* inputR = (numChannels > 1) ? buffer.getReadPointer(1) : inputL;
    auto* outputL = buffer.getWritePointer(0);
    auto* outputR = (numChannels > 1) ? buffer.getWritePointer(1) : outputL;
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Update filter cutoffs
        svf.setHiCut(hiCutHzSm.getNextValue());
        svf.setLowCut(lowCutHzSm.getNextValue());
        
        // Get current delay time with slope limiting for artifact-free changes
        float targetDelaySamp = timeMsSm.getNextValue() * 0.001f * (float)sampleRate;
        targetDelaySamp = juce::jlimit(5.0f * (float)sampleRate * 0.001f, 
                                      2000.0f * (float)sampleRate * 0.001f, 
                                      targetDelaySamp);
        
        // Apply slope limiting to prevent artifacts
        float delayDiff = targetDelaySamp - delaySampCurrent;
        if (std::abs(delayDiff) > delaySampMaxStep) {
            delaySampCurrent += (delayDiff > 0.0f) ? delaySampMaxStep : -delaySampMaxStep;
        } else {
            delaySampCurrent = targetDelaySamp;
        }
        
        // Generate wow/flutter modulation
        float wowDepth = wowDepthSm.getNextValue();
        float wowRate = wowRateSm.getNextValue();
        float modSamp = 0.0f;
        
        if (wowDepth > 0.001f) {
            // Main wow LFO
            lfoPhase += 2.0f * juce::MathConstants<float>::pi * wowRate / (float)sampleRate;
            if (lfoPhase > 2.0f * juce::MathConstants<float>::pi) 
                lfoPhase -= 2.0f * juce::MathConstants<float>::pi;
            
            // Secondary flutter LFO
            lfoPhase2 += 2.0f * juce::MathConstants<float>::pi * (wowRate * 7.3f) / (float)sampleRate;
            if (lfoPhase2 > 2.0f * juce::MathConstants<float>::pi) 
                lfoPhase2 -= 2.0f * juce::MathConstants<float>::pi;
            
            float wow = (float)std::sin(lfoPhase) + 0.3f * (float)std::sin(lfoPhase * 1.7f);
            float flutter = 0.15f * (float)std::sin(lfoPhase2);
            
            // Low-pass filter the modulation for tape-like smoothness
            float rawMod = wow + flutter;
            wowLP += 0.02f * (rawMod - wowLP); // Simple one-pole LP
            
            modSamp = wowLP * wowDepth * 3.5f * (float)sampleRate * 0.001f; // ±3.5ms in samples
        }
        
        // Calculate read position with modulation
        float readPos = wrapF((float)writePos - (delaySampCurrent + modSamp));
        
        // Read delayed samples using 4-point Lagrange interpolation
        float wetL = lagrange4(ringL, readPos);
        float wetR = lagrange4(ringR, readPos);
        
        // Feedback processing chain: LP -> HP -> softSat -> DC block
        float fbAmt = fbSm.getNextValue();
        float loopL = svf.hp(svf.lp(wetL));
        float loopR = svf.hp(svf.lp(wetR));
        
        // Soft saturation with drive
        const float g = 1.0f + 24.0f * driveSm.getNextValue();
        loopL = dcL.process(softSat(loopL, g));
        loopR = dcR.process(softSat(loopR, g));
        
        // Get input samples
        float inputSampleL = inputL[sample];
        float inputSampleR = inputR[sample];
        
        // Write to delay line (input + feedback)
        ringL[writePos] = inputSampleL + fbAmt * loopL;
        ringR[writePos] = inputSampleR + fbAmt * loopR;
        writePos = (writePos + 1) & mask;
        
        // Dry/wet mixing with equal-power crossfade
        const float mix = mixSm.getNextValue();
        const float dryAmt = std::sqrt(juce::jlimit(0.0f, 1.0f, 1.0f - mix));
        const float wetAmt = std::sqrt(juce::jlimit(0.0f, 1.0f, mix));
        
        outputL[sample] = dryAmt * inputSampleL + wetAmt * wetL;
        outputR[sample] = dryAmt * inputSampleR + wetAmt * wetR;
    }
}

inline float FxDelay::lagrange4(const std::vector<float>& v, float idx) const
{
    // 4-point Lagrange interpolation for smooth, musical delay reads
    int i = (int)std::floor(idx);
    float f = idx - (float)i;
    
    // Get 4 points: i-1, i, i+1, i+2 (wrapped)
    int i0 = (i - 1) & mask;
    int i1 = i & mask;
    int i2 = (i + 1) & mask;
    int i3 = (i + 2) & mask;
    
    float y0 = v[i0];
    float y1 = v[i1];
    float y2 = v[i2];
    float y3 = v[i3];
    
    // Lagrange basis polynomials
    float c0 = -f * (f - 1.0f) * (f - 2.0f) / 6.0f;
    float c1 = (f + 1.0f) * (f - 1.0f) * (f - 2.0f) * 0.5f;
    float c2 = -(f + 1.0f) * f * (f - 2.0f) * 0.5f;
    float c3 = (f + 1.0f) * f * (f - 1.0f) / 6.0f;
    
    return c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3;
}

// SVF implementation
void FxDelay::SVF::prepare(double sr)
{
    this->sr = sr;
    gLP = gHP = 1.0f;
    z1LP = z1HP = 0.0f;
}

void FxDelay::SVF::setHiCut(float hz)
{
    // Low-pass for high-cut
    float fc = juce::jlimit(100.0f, (float)(sr * 0.45), hz);
    gLP = (float)std::tan(juce::MathConstants<float>::pi * fc / (float)sr);
}

void FxDelay::SVF::setLowCut(float hz)
{
    // High-pass for low-cut
    float fc = juce::jlimit(10.0f, (float)(sr * 0.45), hz);
    gHP = (float)std::tan(juce::MathConstants<float>::pi * fc / (float)sr);
}

float FxDelay::SVF::lp(float x)
{
    // Simple one-pole low-pass
    float v = (x - z1LP) * gLP / (1.0f + gLP);
    float y = v + z1LP;
    z1LP = y + v;
    return y;
}

float FxDelay::SVF::hp(float x)
{
    // Simple one-pole high-pass
    float v = (x - z1HP) * gHP / (1.0f + gHP);
    float y = x - (v + z1HP);
    z1HP = v + z1HP;
    return y;
}