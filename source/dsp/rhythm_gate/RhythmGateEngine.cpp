#include "RhythmGateEngine.h"
#include <cmath>
#include <algorithm>

void RhythmGateEngine::prepare(double sr, int maxBlock, int numCh)
{
    sampleRate = sr;
    
    // Allocate ring buffer for ~2 bars at 60 BPM worst case
    // 2 bars at 60 BPM = 4 seconds
    ringLen = static_cast<int>(std::ceil(sr * 4.0));
    ring.setSize(2, ringLen); // Stereo
    ring.clear();
    ringWrite = 0;
    
    // Initialize smoothers (40-80ms smoothing)
    shapeSm.reset(sr, 0.05);      // 50ms
    pitchSemiSm.reset(sr, 0.06);  // 60ms
    glitchSm.reset(sr, 0.04);     // 40ms
    mixSm.reset(sr, 0.08);        // 80ms
    
    reset();
}

void RhythmGateEngine::reset()
{
    ring.clear();
    ringWrite = 0;
    stepPhase = 0.0f;
    curStep = 0;
    samplesIntoStep = 0;
    freePhase = 0.0f;
    lastPpqPos = -1.0;
    
    for (int i = 0; i < 16; ++i)
        stepReversed[i] = false;
    
    glitchRetriggerCount = 0;
    glitchSamplesUntilRetrigger = 0;
    
    shapeSm.setCurrentAndTargetValue(0.35f);
    pitchSemiSm.setCurrentAndTargetValue(0.0f);
    glitchSm.setCurrentAndTargetValue(0.0f);
    mixSm.setCurrentAndTargetValue(0.75f);
}

void RhythmGateEngine::setTempoInfo(bool playing, double bpm_, double ppq, int tsNum_)
{
    isPlaying = playing;
    bpm = std::max(20.0, std::min(999.0, bpm_));
    ppqPos = ppq;
    tsNum = tsNum_;
}

void RhythmGateEngine::setParameters(int patternIdx, int divisionIdx, float offset01, float shape01,
                                     float pitchSemi, float reverse01, float glitch01, float mix01, bool syncOn)
{
    // Queue pattern change to apply on next step boundary
    queuedPatternIndex = std::clamp(patternIdx, 0, numPatterns - 1);
    
    division = std::clamp(divisionIdx, 0, 5);
    offset = offset01;
    sync = syncOn;
    probReverse = reverse01;
    
    // Set smoothed targets
    shapeSm.setTargetValue(std::clamp(shape01, 0.0f, 1.0f));
    pitchSemiSm.setTargetValue(std::clamp(pitchSemi, -12.0f, 12.0f));
    glitchSm.setTargetValue(std::clamp(glitch01, 0.0f, 1.0f));
    mixSm.setTargetValue(std::clamp(mix01, 0.0f, 1.0f));
}

void RhythmGateEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels < 2 || numSamples == 0)
        return;
    
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);
    
    // Write input to ring buffer
    writeRing(L, R, numSamples);
    
    // Compute samples per step for current settings
    samplesPerStep = divisionToSamplesPerStep();
    if (samplesPerStep < 64) samplesPerStep = 64; // Safety minimum
    
    // Process sample-by-sample
    for (int i = 0; i < numSamples; ++i)
    {
        // Advance step timing
        samplesIntoStep++;
        stepPhase = static_cast<float>(samplesIntoStep) / static_cast<float>(samplesPerStep);
        
        if (samplesIntoStep >= samplesPerStep || stepPhase >= 1.0f)
        {
            // Step boundary - wrap to next step
            curStep = (curStep + 1) % stepCount;
            samplesIntoStep = 0;
            stepPhase = 0.0f;
            
            // Apply queued pattern change at step boundary
            if (queuedPatternIndex != patternIndex)
            {
                patternIndex = queuedPatternIndex;
            }
            
            // Decide reverse for this step
            stepReversed[curStep] = (rand01() < probReverse);
        }
        
        // Get pattern value at current step (with offset)
        int offsetSteps = static_cast<int>(offset * stepCount);
        int readStep = (curStep + offsetSteps) % stepCount;
        float stepAmp = patternValueAt(readStep, stepPhase);
        
        // Shape the gate envelope
        float gEnv = shapedGate(stepAmp, stepPhase);
        
        // Get smoothed parameter values
        float currentPitchSemi = pitchSemiSm.getNextValue();
        float currentGlitch = glitchSm.getNextValue();
        float currentMix = mixSm.getNextValue();
        shapeSm.skip(1); // Advance shape smoother (used in shapedGate)
        
        // Store dry input
        float dryL = L[i];
        float dryR = R[i];
        
        // Process wet signal
        float wetL, wetR;
        
        if (gEnv < 0.001f)
        {
            // Gate closed - silence
            wetL = 0.0f;
            wetR = 0.0f;
        }
        else
        {
            // Gate open - apply effects
            bool needsRingRead = (std::abs(currentPitchSemi) > 0.01f) || stepReversed[curStep] || (currentGlitch > 0.01f);
            
            if (needsRingRead)
            {
                // Calculate ring read position (simple delay compensation)
                float ringReadPos = static_cast<float>((ringWrite - samplesPerStep + ringLen) % ringLen);
                float ratio = std::pow(2.0f, currentPitchSemi / 12.0f);
                
                // Read from ring with varispeed and optional reverse
                float tempL[1], tempR[1];
                readRingVarispeed(tempL, tempR, 1, ringReadPos, ratio, stepReversed[curStep], 2.0f, 2.0f);
                
                wetL = tempL[0] * gEnv;
                wetR = tempR[0] * gEnv;
            }
            else
            {
                // Simple gating
                wetL = dryL * gEnv;
                wetR = dryR * gEnv;
            }
            
            // Soft limit wet signal
            softLimit(wetL, -2.0f, -1.0f);
            softLimit(wetR, -2.0f, -1.0f);
        }
        
        // Mix wet/dry
        L[i] = dryL * (1.0f - currentMix) + wetL * currentMix;
        R[i] = dryR * (1.0f - currentMix) + wetR * currentMix;
    }
}

int RhythmGateEngine::divisionToSamplesPerStep() const
{
    if (!sync || !isPlaying || bpm < 20.0)
    {
        // Free-running mode: derive rate from division as if 120 BPM
        double baseBpm = 120.0;
        double beatsPerBar = 4.0;
        double secondsPerBeat = 60.0 / baseBpm;
        
        // Division multipliers: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32
        double divMult[] = {1.0, 2.0, 4.0, 8.0, 16.0, 32.0};
        double stepsPerBeat = divMult[std::clamp(division, 0, 5)];
        double secondsPerStep = secondsPerBeat / stepsPerBeat;
        
        return static_cast<int>(sampleRate * secondsPerStep);
    }
    else
    {
        // Sync to host tempo
        double secondsPerBeat = 60.0 / bpm;
        
        // Division multipliers: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32
        double divMult[] = {1.0, 2.0, 4.0, 8.0, 16.0, 32.0};
        double stepsPerBeat = divMult[std::clamp(division, 0, 5)];
        double secondsPerStep = secondsPerBeat / stepsPerBeat;
        
        return static_cast<int>(sampleRate * secondsPerStep);
    }
}

float RhythmGateEngine::patternValueAt(int step, float localPhase) const
{
    int idx = std::clamp(step, 0, stepCount - 1);
    int pIdx = std::clamp(patternIndex, 0, numPatterns - 1);
    
    return patterns[pIdx].data[idx];
}

float RhythmGateEngine::shapedGate(float stepAmp, float localPhase) const
{
    if (stepAmp < 0.001f)
        return 0.0f;
    
    // Get current shape value
    float shape = shapeSm.getCurrentValue();
    
    // Attack/decay times based on shape (0 = fast/exp, 1 = smooth/sine)
    float attackMs = std::max(0.5f, shape * 10.0f);
    float decayMs = std::max(0.5f, shape * 10.0f);
    
    float attackSamples = (attackMs / 1000.0f) * static_cast<float>(sampleRate);
    float decaySamples = (decayMs / 1000.0f) * static_cast<float>(sampleRate);
    
    float totalSamples = static_cast<float>(samplesPerStep);
    float attackPhase = attackSamples / totalSamples;
    float decayPhase = decaySamples / totalSamples;
    
    float env = 1.0f;
    
    // Attack
    if (localPhase < attackPhase && attackPhase > 0.0f)
    {
        float t = localPhase / attackPhase;
        // Crossfade between exp (shape=0) and sine (shape=1)
        float expCurve = 1.0f - std::exp(-5.0f * t);
        float sineCurve = std::sin(t * juce::MathConstants<float>::halfPi);
        env = expCurve * (1.0f - shape) + sineCurve * shape;
    }
    // Decay
    else if (localPhase > (1.0f - decayPhase) && decayPhase > 0.0f)
    {
        float t = (1.0f - localPhase) / decayPhase;
        float expCurve = 1.0f - std::exp(-5.0f * t);
        float sineCurve = std::sin(t * juce::MathConstants<float>::halfPi);
        env = expCurve * (1.0f - shape) + sineCurve * shape;
    }
    
    // Micro-fade at edges (2ms) to prevent clicks
    float fadeSamples = (2.0f / 1000.0f) * static_cast<float>(sampleRate);
    float fadePhase = fadeSamples / totalSamples;
    
    if (localPhase < fadePhase && fadePhase > 0.0f)
    {
        env *= (localPhase / fadePhase);
    }
    else if (localPhase > (1.0f - fadePhase) && fadePhase > 0.0f)
    {
        env *= ((1.0f - localPhase) / fadePhase);
    }
    
    return stepAmp * env;
}

void RhythmGateEngine::writeRing(const float* L, const float* R, int n)
{
    for (int i = 0; i < n; ++i)
    {
        ring.setSample(0, ringWrite, L[i]);
        ring.setSample(1, ringWrite, R[i]);
        ringWrite = (ringWrite + 1) % ringLen;
    }
}

void RhythmGateEngine::readRingVarispeed(float* outL, float* outR, int n, float startPos, float ratio, bool reverse,
                                         float fadeInMs, float fadeOutMs)
{
    float fadeInSamples = (fadeInMs / 1000.0f) * static_cast<float>(sampleRate);
    float fadeOutSamples = (fadeOutMs / 1000.0f) * static_cast<float>(sampleRate);
    
    float readPos = startPos;
    float increment = reverse ? -ratio : ratio;
    
    for (int i = 0; i < n; ++i)
    {
        // Hermite interpolation from ring buffer
        float sL = hermite(ring.getReadPointer(0), ringLen, readPos);
        float sR = hermite(ring.getReadPointer(1), ringLen, readPos);
        
        // Apply micro-fades at start/end
        float fade = 1.0f;
        if (i < fadeInSamples && fadeInSamples > 0.0f)
            fade = static_cast<float>(i) / fadeInSamples;
        else if (i > (n - fadeOutSamples) && fadeOutSamples > 0.0f)
            fade = static_cast<float>(n - i) / fadeOutSamples;
        
        outL[i] = sL * fade;
        outR[i] = sR * fade;
        
        readPos += increment;
        while (readPos < 0.0f) readPos += static_cast<float>(ringLen);
        while (readPos >= static_cast<float>(ringLen)) readPos -= static_cast<float>(ringLen);
    }
}

float RhythmGateEngine::hermite(const float* buf, int len, float rp) const
{
    // 4-point Hermite interpolation
    int idx = static_cast<int>(std::floor(rp));
    float frac = rp - static_cast<float>(idx);
    
    auto getSample = [&](int offset) -> float {
        int i = (idx + offset + len) % len;
        return buf[i];
    };
    
    float xm1 = getSample(-1);
    float x0  = getSample(0);
    float x1  = getSample(1);
    float x2  = getSample(2);
    
    float c0 = x0;
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void RhythmGateEngine::softLimit(float& sample, float kneeDb, float ceilingDb)
{
    float kneeLinear = juce::Decibels::decibelsToGain(kneeDb);
    float ceilingLinear = juce::Decibels::decibelsToGain(ceilingDb);
    
    float absVal = std::abs(sample);
    
    if (absVal > kneeLinear)
    {
        // Soft knee compression
        float over = absVal - kneeLinear;
        float range = ceilingLinear - kneeLinear;
        
        if (range > 0.0f)
        {
            // Soft knee curve
            float ratio = over / range;
            float compressed = kneeLinear + range * std::tanh(ratio);
            
            sample = (sample > 0.0f ? 1.0f : -1.0f) * compressed;
        }
    }
}

