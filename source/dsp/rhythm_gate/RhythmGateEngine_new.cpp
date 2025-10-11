#include "RhythmGateEngine_new.h"
#include <cmath>
#include <algorithm>

void RhythmGateEngine::prepare(double sr, int maxBlock, int numCh)
{
    sampleRate = sr;
    
    // Initialize smoothers (per spec: 40-60ms for most, 15-25ms for release)
    shapeSm.reset(sr, 0.05);      // 50ms
    releaseSm.reset(sr, 0.02);    // 20ms (shorter for release)
    offsetSm.reset(sr, 0.05);     // 50ms
    mixSm.reset(sr, 0.04);        // 40ms
    
    reset();
}

void RhythmGateEngine::reset()
{
    freeRunPhase = 0.0;
    lastPpqPos = -1.0;
    
    shapeSm.setCurrentAndTargetValue(0.0f);     // Shape -1..+1, default 0 (linear)
    releaseSm.setCurrentAndTargetValue(20.0f);  // Release 5-80ms, default 20ms
    offsetSm.setCurrentAndTargetValue(0.5f);    // Offset 0..1, default 0.5 (center)
    mixSm.setCurrentAndTargetValue(0.75f);      // Mix 0..1, default 0.75
    
    envSlewL.reset(0.0f);
    envSlewR.reset(0.0f);
    
    // Build default envelope (Pattern 0)
    buildEnvelopeFromPattern(0);
}

void RhythmGateEngine::setTempoInfo(bool playing, double bpm_, double ppq, int tsNum_)
{
    isPlaying = playing;
    bpm = std::max(20.0, std::min(999.0, bpm_));
    ppqPos = ppq;
    tsNum = tsNum_;
}

void RhythmGateEngine::setParameters(int patternIdx, int divisionIdx, float offset01, float shape01,
                                     float releaseMs_, float mix01, bool syncOn)
{
    // Queue pattern change (build envelope on next block start)
    if (patternIdx != queuedPatternIndex) {
        queuedPatternIndex = std::clamp(patternIdx, 0, numPatterns - 1);
    }
    
    sync = syncOn;
    
    // Convert Shape from 0..1 UI to -1..+1 bipolar (0.5 = 0)
    float shapeBipolar = (shape01 - 0.5f) * 2.0f;
    
    // Set smoothed targets
    shapeSm.setTargetValue(std::clamp(shapeBipolar, -1.0f, 1.0f));
    releaseSm.setTargetValue(std::clamp(releaseMs_, 5.0f, 80.0f));
    offsetSm.setTargetValue(std::clamp(offset01, 0.0f, 1.0f));
    mixSm.setTargetValue(std::clamp(mix01, 0.0f, 1.0f));
}

void RhythmGateEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels < 2 || numSamples == 0)
        return;
    
    // Apply queued pattern change at block boundary
    if (queuedPatternIndex != patternIndex) {
        patternIndex = queuedPatternIndex;
        buildEnvelopeFromPattern(patternIndex);
    }
    
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);
    
    // Get smoothed parameter values for this block
    const float shapeValue = shapeSm.getCurrentValue();
    const float releaseValue = releaseSm.getCurrentValue();
    const float offsetValue = offsetSm.getCurrentValue();
    const float mixValue = mixSm.getCurrentValue();
    const float attackMs = std::min(releaseValue, 5.0f); // Attack capped at 5ms
    
    // Compute phase increment for free-run mode
    const double beatsPerCycle = 4.0; // 1 bar in 4/4
    const double phaseIncPerSample = (bpm > 0.0) 
        ? (1.0 / (beatsPerCycle * (60.0 / bpm) * sampleRate))
        : (1.0 / (beatsPerCycle * 2.0 * sampleRate));
    
    // Process each sample
    for (int i = 0; i < numSamples; ++i)
    {
        // Compute phase (0..1 within cycle)
        double phase = 0.0;
        
        if (sync && isPlaying && bpm > 0.0) {
            // Beat-locked: use PPQ position
            double beatInCycle = std::fmod(ppqPos, beatsPerCycle);
            if (beatInCycle < 0.0) beatInCycle += beatsPerCycle;
            phase = beatInCycle / beatsPerCycle;
            
            // Advance PPQ by one sample
            ppqPos += (bpm / 60.0) / sampleRate;
        } else {
            // Free-run mode
            phase = freeRunPhase;
            freeRunPhase = std::fmod(freeRunPhase + phaseIncPerSample, 1.0);
        }
        
        // Apply Offset (phase shift)
        phase = std::fmod(phase + static_cast<double>(offsetValue), 1.0);
        if (phase < 0.0) phase += 1.0;
        
        // Evaluate raw envelope at current phase
        float rawEnvL = evalEnvelope(static_cast<float>(phase), shapeValue);
        
        // Stereo micro-offset (±0.25ms at |offsetBP| > 0.2)
        float offsetBP = (offsetValue - 0.5f) * 2.0f;
        float stereoOffsetSamps = 0.0f;
        if (std::abs(offsetBP) > 0.2f) {
            stereoOffsetSamps = offsetBP * 0.00025f * static_cast<float>(sampleRate); // ±0.25ms max
        }
        double phaseR = std::fmod(phase + stereoOffsetSamps / sampleRate, 1.0);
        if (phaseR < 0.0) phaseR += 1.0;
        float rawEnvR = evalEnvelope(static_cast<float>(phaseR), shapeValue);
        
        // Apply Release smoothing (one-pole slew)
        float shapedEnvL = envSlewL.process(rawEnvL, attackMs, releaseValue, sampleRate);
        float shapedEnvR = envSlewR.process(rawEnvR, attackMs, releaseValue, sampleRate);
        
        // Advance smoothers
        shapeSm.skip(1);
        releaseSm.skip(1);
        offsetSm.skip(1);
        mixSm.skip(1);
        
        // Store dry
        float dryL = L[i];
        float dryR = R[i];
        
        // Apply envelope
        float wetL = dryL * shapedEnvL;
        float wetR = dryR * shapedEnvR;
        
        // Wet/dry mix
        L[i] = dryL * (1.0f - mixValue) + wetL * mixValue;
        R[i] = dryR * (1.0f - mixValue) + wetR * mixValue;
    }
}

float RhythmGateEngine::evalEnvelope(float phase, float shapeParam) const
{
    if (envelopeNodes.empty()) return 0.0f;
    
    // Find segment containing this phase
    int i1 = 0;
    while (i1 < (int)envelopeNodes.size() && envelopeNodes[i1].phase < phase) ++i1;
    int i0 = (i1 == 0) ? (int)envelopeNodes.size() - 1 : i1 - 1;
    
    const auto& P0 = envelopeNodes[i0];
    const auto& P1 = envelopeNodes[i1 % envelopeNodes.size()];
    
    // Compute segment span and local position
    float span = (P1.phase > P0.phase) ? (P1.phase - P0.phase) : (1.0f - P0.phase + P1.phase);
    float local = (phase >= P0.phase ? (phase - P0.phase) : (1.0f - P0.phase + phase));
    float frac = std::clamp((span > 1e-6f) ? (local / span) : 0.f, 0.f, 1.f);
    
    // Apply Shape curvature to interpolation
    float curved = applyShape(frac, shapeParam);
    
    return std::clamp(P0.value + (P1.value - P0.value) * curved, 0.f, 1.f);
}

void RhythmGateEngine::buildEnvelopeFromPattern(int patternIdx)
{
    int pIdx = std::clamp(patternIdx, 0, numPatterns - 1);
    const float* patternData = patterns[pIdx].data;
    
    envelopeNodes.clear();
    
    // Convert 16-step pattern to envelope nodes
    // Each step is 1/16 of a bar = 0.0625 in phase
    for (int i = 0; i < 16; ++i) {
        float phase = static_cast<float>(i) / 16.0f;
        float value = patternData[i];
        
        envelopeNodes.push_back({phase, value});
    }
    
    // Ensure we have at least 2 nodes for interpolation
    if (envelopeNodes.size() < 2) {
        envelopeNodes.clear();
        envelopeNodes.push_back({0.0f, 1.0f});
        envelopeNodes.push_back({0.5f, 0.0f});
    }
}

