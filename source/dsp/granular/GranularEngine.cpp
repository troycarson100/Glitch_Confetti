#include "GranularEngine.h"

GranularEngine::GranularEngine()
{
    // Seed PRNG with time-based value
    rngState = static_cast<uint32_t>(juce::Time::currentTimeMillis()) ^ 0xA5A5A5A5u;
}

void GranularEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = sampleRate;
    channels = juce::jmin(numChannels, 2);
    
    // Allocate ring buffer (8 seconds for long textures)
    int newRingSize = static_cast<int>(std::ceil(sr * kCaptureSec));
    if (ringSize != newRingSize || ringBuffer.getNumSamples() == 0)
    {
        ringSize = newRingSize;
        ringBuffer.setSize(2, ringSize, false, true, false);
        ringWritePos = 0;
    }
    
    // Reset smoothed parameters (slower smoothing for musical modulation)
    sizeSmooth.reset(sr, 0.06);       // 60ms smoothing
    pitchSmooth.reset(sr, 0.04);      // 40ms smoothing
    densitySmooth.reset(sr, 0.08);    // 80ms smoothing
    mixSmooth.reset(sr, 0.05);        // 50ms smoothing
    textureSmooth.reset(sr, 0.10);    // 100ms smoothing for smooth morphs
    
    sizeSmooth.setCurrentAndTargetValue(currentSizeMs);
    pitchSmooth.setCurrentAndTargetValue(currentPitchSemi);
    densitySmooth.setCurrentAndTargetValue(currentDensityHz);
    mixSmooth.setCurrentAndTargetValue(0.5f);
    textureSmooth.setCurrentAndTargetValue(0.2f);
    
    // Clear voices
    for (auto& v : voices)
        v.active = false;
    
    spawnAccumulator = 0.0f;
    
    // Reset AGC
    wetRms = 0.0f;
    agcGain = 1.0f;
}

void GranularEngine::reset()
{
    ringBuffer.clear();
    ringWritePos = 0;
    
    for (auto& v : voices)
        v.active = false;
    
    spawnAccumulator = 0.0f;
    wetRms = 0.0f;
    agcGain = 1.0f;
}

void GranularEngine::setParameters(float sizeMs, float densityHz, float position01, float sprayMs,
                                   float pitchSemi, float randomAmt, float texture01, float mix01)
{
    // Store raw values
    currentSizeMs = juce::jlimit(5.0f, 200.0f, sizeMs);
    currentDensityHz = juce::jlimit(1.0f, 80.0f, densityHz);
    currentPosition01 = juce::jlimit(0.0f, 1.0f, position01);
    currentSprayMs = juce::jlimit(0.0f, 200.0f, sprayMs);
    currentPitchSemi = juce::jlimit(-24.0f, 24.0f, pitchSemi);
    currentRandomAmt = juce::jlimit(0.0f, 1.0f, randomAmt);
    currentTexture01 = juce::jlimit(0.0f, 1.0f, texture01);
    
    // Set smoothed targets
    sizeSmooth.setTargetValue(currentSizeMs);
    pitchSmooth.setTargetValue(currentPitchSemi);
    densitySmooth.setTargetValue(currentDensityHz);
    mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix01));
    textureSmooth.setTargetValue(currentTexture01);
}

void GranularEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || ringSize == 0)
        return;
    
    // Check mix early exit
    float targetMix = mixSmooth.getTargetValue();
    float currentMix = mixSmooth.getCurrentValue();
    
    if (currentMix < 0.001f && targetMix < 0.001f)
    {
        // Bypass: advance smoothing and return
        for (int n = 0; n < numSamples; ++n)
        {
            sizeSmooth.skip(1);
            pitchSmooth.skip(1);
            densitySmooth.skip(1);
            mixSmooth.skip(1);
            textureSmooth.skip(1);
        }
        return;
    }
    
    // Save dry signal
    juce::AudioBuffer<float> dry(buffer.getNumChannels(), numSamples);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dry.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    
    // Wet buffer (stereo)
    juce::AudioBuffer<float> wet(2, numSamples);
    wet.clear();
    
    // Process per-sample
    for (int n = 0; n < numSamples; ++n)
    {
        // Write input to ring
        float inputL = buffer.getSample(0, n);
        float inputR = (buffer.getNumChannels() > 1) ? buffer.getSample(1, n) : inputL;
        
        ringBuffer.setSample(0, ringWritePos, inputL);
        ringBuffer.setSample(1, ringWritePos, inputR);
        
        if (++ringWritePos >= ringSize)
            ringWritePos = 0;
        
        // Get smoothed parameters
        float size = sizeSmooth.getNextValue();
        float density = densitySmooth.getNextValue();
        float pitch = pitchSmooth.getNextValue();
        float texture = textureSmooth.getNextValue();
        float mix = mixSmooth.getNextValue();
        
        // Adaptive density: enforce overlap target (makes it more musical)
        const float kOverlapTarget = 6.0f;
        float sizeSec = size * 0.001f;
        float minDensity = kOverlapTarget / juce::jmax(0.01f, sizeSec);
        float effectiveDensity = juce::jmax(density, minDensity * 0.5f); // Nudge toward target
        
        // Spawn grains based on effective density
        float blockDuration = 1.0f / (float)sr;
        spawnAccumulator += effectiveDensity * blockDuration;
        
        while (spawnAccumulator >= 1.0f)
        {
            // Calculate spawn position with gaussian spray
            float posOffset = (1.0f - currentPosition01) * (float)ringSize;
            
            // Gaussian spray (more natural than uniform)
            float spray1 = (rand01() - 0.5f) * 2.0f;
            float spray2 = (rand01() - 0.5f) * 2.0f;
            float gaussianSpray = (spray1 + spray2) * 0.5f; // Approximate gaussian
            float sprayJitter = gaussianSpray * currentSprayMs * 0.001f * (float)sr;
            
            // Add subtle forward drift when position > 0.8 (surf the live input)
            float drift = 0.0f;
            if (currentPosition01 > 0.8f)
            {
                drift = (currentPosition01 - 0.8f) * 5.0f * (rand01() - 0.3f); // Slight forward bias
            }
            
            float readPos = (float)ringWritePos - posOffset + sprayJitter + drift;
            while (readPos < 0.0f) readPos += (float)ringSize;
            while (readPos >= (float)ringSize) readPos -= (float)ringSize;
            
            spawnGrain(readPos, size, pitch, texture);
            spawnAccumulator -= 1.0f;
        }
        
        // Render all active voices
        float outL = 0.0f;
        float outR = 0.0f;
        
        for (auto& v : voices)
        {
            if (!v.active)
                continue;
            
            // Read with improved interpolation
            float sampleL = lagrangeInterp(ringBuffer.getReadPointer(0), ringSize, v.readPos);
            float sampleR = lagrangeInterp(ringBuffer.getReadPointer(1), ringSize, v.readPos);
            
            // Apply envelope
            float env = computeMusicalWindow(v.phase, v.windowType);
            v.envLevel = env;
            
            // Apply panning with micro-decorrelation
            float delayedSampleL = sampleL;
            float delayedSampleR = sampleR;
            
            // Micro stereo decorrelation (0-0.6ms delay, windowed)
            if (v.microDelay > 0.0f)
            {
                float decorr = v.microDelay * env; // Window it in/out
                delayedSampleR = lagrangeInterp(ringBuffer.getReadPointer(1), ringSize, 
                                                v.readPos - decorr);
            }
            
            outL += delayedSampleL * env * v.panL;
            outR += delayedSampleR * env * v.panR;
            
            // Advance grain
            v.readPos += v.increment;
            if (v.readPos >= (float)ringSize) v.readPos -= (float)ringSize;
            if (v.readPos < 0.0f) v.readPos += (float)ringSize;
            
            v.phase += 1.0f / v.duration;
            
            if (v.phase >= 1.0f)
                v.active = false;
        }
        
        // Energy normalization + adaptive gain control
        float energyNorm = 1.0f / std::sqrt(juce::jmax(4.0f, effectiveDensity * sizeSec));
        
        // Simple RMS tracking for AGC
        float wetMag = std::sqrt(outL * outL + outR * outR);
        wetRms = wetRms * 0.999f + wetMag * 0.001f; // ~100ms time constant
        
        // Adaptive gain: target RMS ~0.2 (-14 dBFS), max 6dB cut
        float targetGain = 0.2f / juce::jmax(0.01f, wetRms);
        targetGain = juce::jlimit(0.5f, 1.0f, targetGain);
        agcGain = agcGain * 0.9995f + targetGain * 0.0005f; // 200ms smooth
        
        // Apply normalization and AGC
        outL *= energyNorm * agcGain * 2.5f; // Boost for audibility
        outR *= energyNorm * agcGain * 2.5f;
        
        // Soft limiter (knee at -2dB, ceiling -1dB)
        auto softLimit = [](float x) {
            const float thresh = 0.794f; // -2dB
            const float ceiling = 0.891f; // -1dB
            if (std::abs(x) > thresh)
            {
                float sign = (x > 0.0f) ? 1.0f : -1.0f;
                float excess = std::abs(x) - thresh;
                float compressed = thresh + excess * 0.3f; // Soft knee
                x = sign * juce::jmin(compressed, ceiling);
            }
            return x;
        };
        
        outL = softLimit(outL);
        outR = softLimit(outR);
        
        wet.setSample(0, n, outL);
        wet.setSample(1, n, outR);
    }
    
    // External wet/dry mix (pure crossfade)
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        int wetCh = juce::jmin(ch, 1);
        auto* bufPtr = buffer.getWritePointer(ch);
        const auto* dryPtr = dry.getReadPointer(ch);
        const auto* wetPtr = wet.getReadPointer(wetCh);
        
        // Get final mix value
        float finalMix = mixSmooth.getCurrentValue();
        
        for (int n = 0; n < numSamples; ++n)
        {
            // Pure crossfade: mix=0 → all dry, mix=1 → all wet
            bufPtr[n] = dryPtr[n] * (1.0f - finalMix) + wetPtr[n] * finalMix;
        }
    }
}

void GranularEngine::spawnGrain(float baseReadPos, float size, float pitch, float texture)
{
    int voiceIdx = findFreeVoice();
    if (voiceIdx < 0)
        return;
    
    auto& v = voices[voiceIdx];
    v.active = true;
    v.phase = 0.0f;
    v.readPos = baseReadPos;
    
    // Duration with musical jitter (±10% for subtle variation)
    float sizeJitter = 1.0f + (rand01() - 0.5f) * 0.2f * currentRandomAmt;
    v.duration = juce::jmax(256.0f, size * 0.001f * (float)sr * sizeJitter);
    
    // Pitch ratio with musical jitter (±0.5 semitone for organic feel)
    float pitchJitter = (rand01() - 0.5f) * 1.0f * currentRandomAmt;
    v.increment = std::pow(2.0f, (pitch + pitchJitter) / 12.0f);
    
    // Subtle stereo panning (±0.35 for width without extreme separation)
    float panJitter = (rand01() - 0.5f) * 0.7f * currentRandomAmt; // ±0.35 max
    float pan = juce::jlimit(-1.0f, 1.0f, panJitter);
    
    // Equal-power panning
    float panAngle = (pan + 1.0f) * 0.25f * kTwoPi * 0.5f; // Map to 0..π/2
    v.panL = std::sqrt(std::cos(panAngle));
    v.panR = std::sqrt(std::sin(panAngle));
    
    // Micro stereo decorrelation (0-0.6ms delay, creates width)
    v.microDelay = (rand01() * 0.6f * 0.001f * (float)sr) * currentRandomAmt;
    
    // Window type selection based on texture
    v.windowType = texture;
    
    v.envLevel = 0.0f;
}

int GranularEngine::findFreeVoice()
{
    // First pass: find truly free voice
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (!voices[i].active)
            return i;
    }
    
    // Second pass: steal voice with lowest projected power (look ahead 10ms)
    int stealIdx = 0;
    float minProjectedPower = 1.0f;
    const float lookaheadSamples = 0.01f * (float)sr; // 10ms
    
    for (int i = 0; i < kMaxVoices; ++i)
    {
        float futurePhase = voices[i].phase + lookaheadSamples / voices[i].duration;
        futurePhase = juce::jlimit(0.0f, 1.0f, futurePhase);
        float projectedEnv = computeMusicalWindow(futurePhase, voices[i].windowType);
        float projectedPower = projectedEnv * projectedEnv;
        
        if (projectedPower < minProjectedPower)
        {
            minProjectedPower = projectedPower;
            stealIdx = i;
        }
    }
    
    return stealIdx;
}

inline float GranularEngine::rand01()
{
    // Xorshift32 (fast PRNG)
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(rngState) / static_cast<float>(0xFFFFFFFFu);
}

inline float GranularEngine::lagrangeInterp(const float* buf, int N, float rp)
{
    // 5-point Lagrange interpolation (original implementation)
    // Smoother than 4-point Hermite, no overshoot
    int i = static_cast<int>(rp);
    float frac = rp - (float)i;
    
    auto at = [&](int offset) -> float {
        int idx = (i + offset + N) % N;
        return buf[idx];
    };
    
    // Lagrange basis functions for 5 points
    float y_2 = at(-2);
    float y_1 = at(-1);
    float y0  = at(0);
    float y1  = at(1);
    float y2  = at(2);
    
    float f = frac;
    float f2 = f * f;
    float f3 = f2 * f;
    float f4 = f3 * f;
    
    // Lagrange coefficients (derived from polynomial fit)
    float c0 = y0;
    float c1 = (-y_2 + 8.0f*y_1 - 8.0f*y1 + y2) / 12.0f;
    float c2 = (y_2 - 2.0f*y_1 + 2.0f*y1 - y2) / 2.0f;
    float c3 = (-y_2 + 2.0f*y_1 - 2.0f*y0 + 2.0f*y1 - y2) / 6.0f;
    float c4 = (y_2 - 4.0f*y_1 + 6.0f*y0 - 4.0f*y1 + y2) / 24.0f;
    
    return c0 + c1*f + c2*f2 + c3*f3 + c4*f4;
}

inline float GranularEngine::computeMusicalWindow(float phase, float morphParam)
{
    // Clamp phase
    phase = juce::jlimit(0.0f, 1.0f, phase);
    
    // 2-4ms click guard at edges
    const float guardSamples = 0.003f * (float)sr; // 3ms
    float clickGuard = juce::jmin(1.0f, phase * guardSamples, (1.0f - phase) * guardSamples);
    
    // Compute window family (original implementations, not copied)
    float t = kTwoPi * phase;
    
    // Soft sine (raised cosine with slight plateau)
    float softSine = 0.5f - 0.5f * std::cos(t);
    softSine = std::pow(softSine, 0.9f); // Slight plateau in middle
    
    // Tukey window (α = 0.5)
    float tukey;
    const float alpha = 0.5f;
    if (phase < alpha / 2.0f)
    {
        tukey = 0.5f * (1.0f - std::cos(kTwoPi * phase / alpha));
    }
    else if (phase > 1.0f - alpha / 2.0f)
    {
        tukey = 0.5f * (1.0f - std::cos(kTwoPi * (1.0f - phase) / alpha));
    }
    else
    {
        tukey = 1.0f;
    }
    
    // Blackman-Harris (4-term, airy tail)
    float bh = 0.35875f 
             - 0.48829f * std::cos(t)
             + 0.14128f * std::cos(2.0f * t)
             - 0.01168f * std::cos(3.0f * t);
    
    // Hann (smooth, classic)
    float hann = 0.5f - 0.5f * std::cos(t);
    
    // Morph between windows based on morphParam
    float window;
    if (morphParam < 0.33f)
    {
        // Soft Sine → Hann
        float blend = morphParam / 0.33f;
        window = softSine * (1.0f - blend) + hann * blend;
    }
    else if (morphParam < 0.66f)
    {
        // Hann → Tukey
        float blend = (morphParam - 0.33f) / 0.33f;
        window = hann * (1.0f - blend) + tukey * blend;
    }
    else
    {
        // Tukey → Blackman-Harris
        float blend = (morphParam - 0.66f) / 0.34f;
        window = tukey * (1.0f - blend) + bh * blend;
    }
    
    // Energy normalization (keep loudness consistent across window types)
    float energyScale = 1.0f;
    if (morphParam < 0.33f)
        energyScale = 1.0f;
    else if (morphParam < 0.66f)
        energyScale = 0.95f; // Tukey is slightly louder
    else
        energyScale = 0.85f; // BH is loudest
    
    return window * clickGuard * energyScale;
}
