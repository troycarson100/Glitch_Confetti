#include "GranularEngine.h"

GranularEngine::GranularEngine()
{
    // Seed PRNG
    rngState = static_cast<uint32_t>(juce::Time::currentTimeMillis()) ^ 0xDEADBEEFu;
}

void GranularEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = sampleRate;
    channels = juce::jmin(numChannels, 2);
    
    // Allocate ring buffer (only if not already allocated)
    int newRingSize = static_cast<int>(std::ceil(sr * kCaptureSec));
    if (ringSize != newRingSize || ringBuffer.getNumSamples() == 0)
    {
        ringSize = newRingSize;
        ringBuffer.setSize(2, ringSize, false, true, false); // Don't preserve, clear it
        ringWritePos = 0;
    }
    
    // Reset smoothed values
    sizeSmooth.reset(sr, 0.05);      // 50ms smoothing
    pitchSmooth.reset(sr, 0.02);     // 20ms smoothing
    densitySmooth.reset(sr, 0.06);   // 60ms smoothing
    mixSmooth.reset(sr, 0.05);       // 50ms smoothing
    
    sizeSmooth.setCurrentAndTargetValue(currentSizeMs);
    pitchSmooth.setCurrentAndTargetValue(currentPitchSemi);
    densitySmooth.setCurrentAndTargetValue(currentDensityHz);
    mixSmooth.setCurrentAndTargetValue(0.5f);
    
    // Clear voices
    for (auto& v : voices)
        v.active = false;
    
    spawnAccumulator = 0.0f;
}

void GranularEngine::reset()
{
    ringBuffer.clear();
    ringWritePos = 0;
    
    for (auto& v : voices)
        v.active = false;
    
    spawnAccumulator = 0.0f;
}

void GranularEngine::setParameters(float sizeMs, float densityHz, float position01, float sprayMs,
                                   float pitchSemi, float randomAmt, float texture01, float mix01)
{
    // Store raw values for spawning
    currentSizeMs = juce::jlimit(5.0f, 200.0f, sizeMs);
    currentDensityHz = juce::jlimit(1.0f, 80.0f, densityHz);
    currentPosition01 = juce::jlimit(0.0f, 1.0f, position01);
    currentSprayMs = juce::jlimit(0.0f, 200.0f, sprayMs);
    currentPitchSemi = juce::jlimit(-24.0f, 24.0f, pitchSemi);
    currentRandomAmt = juce::jlimit(0.0f, 1.0f, randomAmt);
    currentTexture01 = juce::jlimit(0.0f, 1.0f, texture01);
    
    // Set smoothed parameter targets
    sizeSmooth.setTargetValue(currentSizeMs);
    pitchSmooth.setTargetValue(currentPitchSemi);
    densitySmooth.setTargetValue(currentDensityHz);
    mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix01));
}

void GranularEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || ringSize == 0)
        return;
    
    // Get current mix to check if we should process at all
    float currentMix = mixSmooth.getCurrentValue();
    
    // Skip expensive processing if mix is essentially zero
    if (currentMix < 0.001f)
    {
        // Just advance the smoothed values and return
        for (int n = 0; n < numSamples; ++n)
        {
            sizeSmooth.getNextValue();
            pitchSmooth.getNextValue();
            densitySmooth.getNextValue();
            mixSmooth.getNextValue();
        }
        return;
    }
    
    // Save dry signal
    juce::AudioBuffer<float> dry(buffer.getNumChannels(), numSamples);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dry.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    
    // Create wet buffer
    juce::AudioBuffer<float> wet(2, numSamples);
    wet.clear();
    
    // Write input to ring buffer and render grains
    for (int n = 0; n < numSamples; ++n)
    {
        // Write input to ring
        float inputL = buffer.getSample(0, n);
        float inputR = (buffer.getNumChannels() > 1) ? buffer.getSample(1, n) : inputL;
        
        ringBuffer.setSample(0, ringWritePos, inputL);
        ringBuffer.setSample(1, ringWritePos, inputR);
        
        if (++ringWritePos >= ringSize)
            ringWritePos = 0;
        
        // Spawn grains based on density
        float density = densitySmooth.getNextValue();
        float blockDuration = 1.0f / (float)sr;
        spawnAccumulator += density * blockDuration;
        
        while (spawnAccumulator >= 1.0f)
        {
            // Calculate spawn read position
            float posOffset = (1.0f - currentPosition01) * (float)ringSize;
            float sprayJitter = (rand01() - 0.5f) * 2.0f * currentSprayMs * 0.001f * (float)sr;
            sprayJitter += (rand01() - 0.5f) * 40.0f * currentRandomAmt * 0.001f * (float)sr; // Extra jitter
            
            float readPos = (float)ringWritePos - posOffset + sprayJitter;
            while (readPos < 0.0f) readPos += (float)ringSize;
            while (readPos >= (float)ringSize) readPos -= (float)ringSize;
            
            spawnGrain(readPos);
            spawnAccumulator -= 1.0f;
        }
        
        // Render all active voices for this sample
        float outL = 0.0f;
        float outR = 0.0f;
        
        for (auto& v : voices)
        {
            if (!v.active)
                continue;
            
            // Read from ring with Hermite interpolation
            int idx = static_cast<int>(v.readPos);
            float frac = v.readPos - (float)idx;
            
            float sampleL = hermite4(ringBuffer.getReadPointer(0), ringSize, v.readPos);
            float sampleR = hermite4(ringBuffer.getReadPointer(1), ringSize, v.readPos);
            
            // Apply window envelope
            float env = computeWindow(v.phase, currentTexture01);
            v.envLevel = env;
            
            // Apply panning
            outL += sampleL * env * v.panL;
            outR += sampleR * env * v.panR;
            
            // Advance grain
            v.readPos += v.increment;
            if (v.readPos >= (float)ringSize) v.readPos -= (float)ringSize;
            if (v.readPos < 0.0f) v.readPos += (float)ringSize;
            
            v.phase += 1.0f / v.duration;
            
            if (v.phase >= 1.0f)
                v.active = false;
        }
        
        // Soft clip wet output
        outL = std::tanh(outL * 0.8f);
        outR = std::tanh(outR * 0.8f);
        
        wet.setSample(0, n, outL);
        wet.setSample(1, n, outR);
    }
    
    // External wet/dry mix
    float mix = mixSmooth.getCurrentValue();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float dryVal = dry.getSample(ch, n);
            float wetVal = wet.getSample(juce::jmin(ch, 1), n);
            buffer.setSample(ch, n, dryVal * (1.0f - mix) + wetVal * mix);
        }
    }
}

void GranularEngine::spawnGrain(float baseReadPos)
{
    // Find free voice or steal lowest envelope
    int voiceIdx = findFreeVoice();
    if (voiceIdx < 0)
        return;
    
    auto& v = voices[voiceIdx];
    v.active = true;
    v.phase = 0.0f;
    v.readPos = baseReadPos;
    
    // Duration with jitter
    float size = sizeSmooth.getCurrentValue();
    float sizeJitter = 1.0f + (rand01() - 0.5f) * 0.5f * currentRandomAmt; // ±25% jitter
    v.duration = juce::jmax(128.0f, size * 0.001f * (float)sr * sizeJitter);
    
    // Pitch with jitter
    float pitch = pitchSmooth.getCurrentValue();
    float pitchJitter = (rand01() - 0.5f) * 6.0f * currentRandomAmt; // ±3 semitones
    v.increment = std::pow(2.0f, (pitch + pitchJitter) / 12.0f);
    
    // Random panning (subtle)
    float panJitter = (rand01() - 0.5f) * 0.4f; // ±0.2
    float pan = juce::jlimit(-1.0f, 1.0f, panJitter);
    float panAngle = (pan + 1.0f) * 0.5f * kTwoPi * 0.25f; // 0..π/2
    v.panL = std::cos(panAngle);
    v.panR = std::sin(panAngle);
    
    v.envLevel = 0.0f;
}

int GranularEngine::findFreeVoice()
{
    // First, try to find a free voice
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (!voices[i].active)
            return i;
    }
    
    // No free voices - steal the one with lowest envelope
    int stealIdx = 0;
    float minEnv = 1.0f;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[i].envLevel < minEnv)
        {
            minEnv = voices[i].envLevel;
            stealIdx = i;
        }
    }
    
    return stealIdx;
}

inline float GranularEngine::rand01()
{
    // Fast xorshift32
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(rngState) / static_cast<float>(0xFFFFFFFFu);
}

inline float GranularEngine::hermite4(const float* buf, int N, float rp)
{
    // 4-point Hermite interpolation
    int i = static_cast<int>(rp);
    float frac = rp - (float)i;
    
    auto at = [&](int offset) -> float {
        int idx = (i + offset) % N;
        if (idx < 0) idx += N;
        return buf[idx];
    };
    
    float y0 = at(-1);
    float y1 = at(0);
    float y2 = at(1);
    float y3 = at(2);
    
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

inline float GranularEngine::computeWindow(float phase, float texture)
{
    // Clamp phase
    phase = juce::jlimit(0.0f, 1.0f, phase);
    
    // Click guard (fade in/out at edges even for rect)
    float clickGuard = juce::jmin(1.0f, phase * 8.0f, (1.0f - phase) * 8.0f);
    
    // Compute windows
    float t = kTwoPi * phase;
    float hann = 0.5f - 0.5f * std::cos(t);
    float black = 0.42f - 0.5f * std::cos(t) + 0.08f * std::cos(2.0f * t);
    float rect = 1.0f;
    
    // Morph based on texture
    float window;
    if (texture <= 0.5f)
    {
        // Hann → Blackman
        float blend = texture * 2.0f;
        window = hann * (1.0f - blend) + black * blend;
    }
    else
    {
        // Blackman → Rect
        float blend = (texture - 0.5f) * 2.0f;
        window = black * (1.0f - blend) + rect * blend;
    }
    
    return window * clickGuard;
}

