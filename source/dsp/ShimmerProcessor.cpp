#include "ShimmerProcessor.h"

//==============================================================================
// PitchBlock Implementation (Granular v3)
//==============================================================================

void PitchBlock::winPair(float phase01, float& wa, float& wb) const noexcept
{
    const float p = juce::MathConstants<float>::pi * juce::jlimit(0.0f, 1.0f, phase01);
    wa = sin2(p); 
    wb = cos2(p); // constant-power pair
}

void PitchBlock::updateFilters()
{
    const double osFs = fs * osFactor;
    lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(osFs, 12000.0);
    hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(osFs, 150.0);
}

void PitchBlock::advanceHead(Head& h)
{
    h.phase += h.step;
    if (h.phase >= 1.0)
    {
        h.phase -= 1.0;
        h.base = wrap(h.base + grainSmp); // hop forward one full grain — no discontinuity
    }
}

void PitchBlock::prepare(double sampleRate, int osFactorIn, int maxChannels, double grainLenSec, double xfadeSec)
{
    fs = sampleRate;
    setOSFactor(osFactorIn);
    setGrain(grainLenSec, xfadeSec);
    channels = juce::jlimit(1, maxChannels, maxChannels);

    const int maxGrainSamps = (int)std::ceil(grainSec * fs * osFactor) + 8;
    const int rbLen = juce::nextPowerOfTwo(juce::jmax(8192, 4 * maxGrainSamps));
    
    ring.resize((size_t)channels);
    for (auto& ch : ring) { 
        ch.setSize(1, rbLen); 
        ch.clear(); 
    }
    
    ringMask = rbLen - 1;
    writePos = 0;

    // heads 180° out of phase; start half-grain behind writer
    headA = Head{};
    headB = Head{ .phase = 0.5 };

    // Smoothers / FTZ
    ratioSm.reset(fs * osFactor, 0.010); // 10ms glide
    ratioSm.setCurrentAndTargetValue(1.0); // Start at unity
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();
    updateFilters();
    primed = false;
}

void PitchBlock::setOSFactor(int f)
{
    osFactor = juce::jlimit(1, 8, f);
    updateFilters();
}

void PitchBlock::setGrain(double lenS, double xfS)
{
    grainSec = juce::jlimit(0.150, 0.200, lenS);  // 150-200ms (was 100-120ms)
    xfadeSec = juce::jlimit(0.040, 0.060, xfS);   // 40-60ms crossfade (was 25-35ms)
    const int osSr = static_cast<int>(fs * osFactor);
    grainSmp = static_cast<int>(grainSec * osSr);
    xfadeSmp = static_cast<int>(xfadeSec * osSr);
}

void PitchBlock::processBuffer(juce::AudioBuffer<float>& inOut, float targetRatio, float amount)
{
    juce::ScopedNoDenormals _;
    const int n = inOut.getNumSamples();
    if (n == 0) return;  // Remove amount check - always process

    // ratio sanity (0.25x..4x). Remove unity bypass - let shimmer always pitch shift
    targetRatio = juce::jlimit(0.25f, 4.0f, targetRatio);

    // Prime ring with initial audio (once) to avoid under-run at first use
    if (!primed) { pushToRing(inOut); primed = true; }

    // Push current input chunk into ring
    pushToRing(inOut);

    // Temp output (pitched)
    juce::AudioBuffer<float> out(inOut.getNumChannels(), n);
    out.clear();

    // Glide ratio -> phase step (smoothed to prevent crackling)
    ratioSm.setTargetValue(targetRatio);
    double ratio = ratioSm.getNextValue();
    
    // Process ratio smoother across the block for smooth transitions
    const double ratioEnd = ratio;
    const double ratioStart = ratioSm.getCurrentValue();
    const double ratioStep = (ratioEnd - ratioStart) / (double)juce::jmax(1, n);
    
    // Use smoothed ratio for step calculation
    ratio = juce::jlimit(0.25, 4.0, ratio);
    const double step = ratio / (double)grainSmp;
    headA.step = headB.step = step;

    // Set read bases ~half & full grain behind write head
    const int behindHalf = wrap(writePos - (grainSmp/2));
    headA.base = behindHalf;
    headB.base = wrap(behindHalf - (grainSmp/2));

    // Synthesize dual-head with constant-power windows
    for (int i = 0; i < n; ++i)
    {
        float wa, wb; 
        winPair((float)headA.phase, wa, wb); // sin² / cos²

        for (int ch = 0; ch < inOut.getNumChannels(); ++ch)
        {
            const float a = readHermite(ring[(size_t)ch], headA.base, headA.phase);
            const float b = readHermite(ring[(size_t)ch], headB.base, headB.phase);
            const float y = a * wa + b * wb;
            out.addSample(ch, i, y * amount);
        }

        advanceHead(headA);
        advanceHead(headB);
    }

    // Gentle cleanup in OS domain
    juce::dsp::AudioBlock<float> outBlk(out);
    lp.process(juce::dsp::ProcessContextReplacing<float>(outBlk));
    hp.process(juce::dsp::ProcessContextReplacing<float>(outBlk));

    // Overwrite input with pitched result
    inOut.makeCopyOf(out, true);
}

void PitchBlock::pushToRing(const juce::AudioBuffer<float>& in)
{
    const int n = in.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        const int w = wrap(writePos + i);
        for (int ch = 0; ch < (int)ring.size(); ++ch)
            ring[(size_t)ch].setSample(0, w, in.getSample(ch, i));
    }
    writePos = wrap(writePos + n);
}

// Hermite interpolation helper
float PitchBlock::hermite4(float xm1, float x0, float x1, float x2, float frac)
{
    const float c0 = x0;
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f*x0 + 2.0f*x1 - 0.5f*x2;
    const float c3 = 0.5f*(x2 - xm1) + 1.5f*(x0 - x1);
    return ((c3*frac + c2)*frac + c1)*frac + c0;
}

float PitchBlock::readHermite(const juce::AudioBuffer<float>& rb, int base, double phase) const
{
    const int len = rb.getNumSamples();
    const double pos = (double)base + juce::jlimit(0.0, 1.0, phase) * (double)grainSmp;
    const int i0 = ((int)pos) & (len - 1);
    const float t = (float)(pos - std::floor(pos));

    const int im1 = (i0 - 1) & (len - 1);
    const int i1  = (i0 + 1) & (len - 1);
    const int i2  = (i0 + 2) & (len - 1);

    const float xm1 = rb.getSample(0, im1);
    const float x0  = rb.getSample(0, i0);
    const float x1  = rb.getSample(0, i1);
    const float x2  = rb.getSample(0, i2);

    return hermite4(xm1, x0, x1, x2, t);
}

//==============================================================================
// ReverbTank Implementation
//==============================================================================

void ReverbTank::prepare(const juce::dsp::ProcessSpec& spec)
{
    fs = spec.sampleRate;
    maxBlock = (int)spec.maximumBlockSize;
    
    predelay.setMaximumDelayInSamples(static_cast<int>(spec.sampleRate * 0.15)); // 150ms max
    predelay.prepare(spec);
    
    for (auto& d : comb) d.prepare(spec);
    for (auto& a : ap) a.prepare(spec);
    
    // Initialize feedback buffer for shimmer injection
    feedbackBuffer.setSize(2, maxBlock);
    feedbackBuffer.clear();
    feedbackGain = 0.0f;
}

void ReverbTank::setPredelayMs(float ms)
{
    predelayMs = juce::jlimit(0.0f, 120.0f, ms);
}

void ReverbTank::setSize(float s)
{
    size = juce::jlimit(0.10f, 1.0f, s);
}

void ReverbTank::setDecaySeconds(float s)
{
    decay = juce::jlimit(0.10f, 20.0f, s);
}

void ReverbTank::setColor(float c)
{
    color = juce::jlimit(0.0f, 1.0f, c);
}

void ReverbTank::process(juce::dsp::AudioBlock<float>& in, juce::AudioBuffer<float>& wetOut)
{
    const int numSamples = (int)in.getNumSamples();
    const int numChannels = (int)in.getNumChannels();
    
    wetOut.setSize(numChannels, numSamples, false, false, true);
    wetOut.clear();
    
    // Set predelay
    predelay.setDelay(static_cast<int>(predelayMs * 0.001 * fs));
    
    // Compute feedback coefficients from decay time (RT60)
    float fb = 0.0f;
    if (decay > 0.1f)
    {
        const float avgDelaySec = size * 0.05f;
        const float ratio = decay / juce::jmax(avgDelaySec, 0.001f);
        fb = std::pow(10.0f, -3.0f / ratio);
        fb = juce::jlimit(0.0f, 0.99f, fb);
        // Boost feedback for denser, bigger reverb
        fb = juce::jmin(0.99f, fb * 1.15f);
    }
    
    // Color: tilt EQ via damping (0=bright, 1=dark)
    const float dampCutoff = juce::jmap(color, 0.0f, 1.0f, 12000.0f, 2000.0f);
    
    // Longer comb times for more obvious shimmer bloom
    const float combTimes[8] = {
        0.040f * size, 0.047f * size, 0.053f * size, 0.061f * size,
        0.071f * size, 0.083f * size, 0.097f * size, 0.113f * size  // Increased from max 0.083
    };
    
    for (int i = 0; i < 8; ++i) {
        const int delaySamps = juce::jlimit(4, (int)(fs * 0.1), (int)(combTimes[i] * fs));
        comb[i].delay.setDelay(delaySamps);
        // Reduce damping for denser, longer tail
        comb[i].damping = juce::jmap(dampCutoff, 2000.0f, 12000.0f, 0.98f, 0.6f);  // Was 0.95-0.5, now 0.98-0.6
    }
    
    // Longer allpass times for more diffusion
    const float apTimes[4] = { 0.008f, 0.011f, 0.015f, 0.021f };  // Was 0.005-0.0123, now longer
    for (int i = 0; i < 4; ++i) {
        const int delaySamps = juce::jlimit(1, (int)(fs * 0.02), (int)(apTimes[i] * fs));
        ap[i].delay.setDelay(delaySamps);
        // Higher allpass coefficient for denser diffusion
        ap[i].coeff = 0.65f;  // Was 0.5f, now 0.65 for more feedback through allpass
    }
    
    // Process sample by sample
    for (int n = 0; n < numSamples; ++n)
    {
        // Read input (mono sum for reverb input)
        float inSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            inSample += in.getChannelPointer(ch)[n];
        inSample /= (float)numChannels;
        
        // Predelay (shimmer feedback injected later into comb inputs, not here)
        predelay.pushSample(0, inSample);
        float pd = (float)predelay.popSample(0);
        
        // ===== FDN: Feedback Delay Network with 8 combs =====
        // Read comb outputs
        float combOuts[8];
        for (int i = 0; i < 8; ++i) {
            float delayed = (float)comb[i].delay.popSample(0);
            comb[i].readPos = comb[i].readPos * comb[i].damping + delayed * (1.0f - comb[i].damping);
            combOuts[i] = comb[i].readPos;
        }
        
        // Hadamard mixing matrix (8×8 for diffuse feedback)
        float mixed[8];
        mixed[0] = 0.25f * (combOuts[0] + combOuts[1] + combOuts[2] + combOuts[3] + combOuts[4] + combOuts[5] + combOuts[6] + combOuts[7]);
        mixed[1] = 0.25f * (combOuts[0] - combOuts[1] + combOuts[2] - combOuts[3] + combOuts[4] - combOuts[5] + combOuts[6] - combOuts[7]);
        mixed[2] = 0.25f * (combOuts[0] + combOuts[1] - combOuts[2] - combOuts[3] + combOuts[4] + combOuts[5] - combOuts[6] - combOuts[7]);
        mixed[3] = 0.25f * (combOuts[0] - combOuts[1] - combOuts[2] + combOuts[3] + combOuts[4] - combOuts[5] - combOuts[6] + combOuts[7]);
        mixed[4] = 0.25f * (combOuts[0] + combOuts[1] + combOuts[2] + combOuts[3] - combOuts[4] - combOuts[5] - combOuts[6] - combOuts[7]);
        mixed[5] = 0.25f * (combOuts[0] - combOuts[1] + combOuts[2] - combOuts[3] - combOuts[4] + combOuts[5] - combOuts[6] + combOuts[7]);
        mixed[6] = 0.25f * (combOuts[0] + combOuts[1] - combOuts[2] - combOuts[3] - combOuts[4] - combOuts[5] + combOuts[6] + combOuts[7]);
        mixed[7] = 0.25f * (combOuts[0] - combOuts[1] - combOuts[2] + combOuts[3] - combOuts[4] + combOuts[5] + combOuts[6] - combOuts[7]);
        
        // Write back to combs with feedback + shimmer injection
        // Inject shimmer feedback into comb inputs (distributed across all combs)
        float shimmerInj = 0.0f;
        if (feedbackGain > 0.0001f && n < feedbackBuffer.getNumSamples())
        {
            float fbSample = 0.0f;
            for (int ch = 0; ch < feedbackBuffer.getNumChannels(); ++ch)
                fbSample += feedbackBuffer.getSample(ch, n);
            fbSample /= (float)feedbackBuffer.getNumChannels();
            shimmerInj = feedbackGain * fbSample;
        }
        
        // Distribute shimmer across all combs for diffusion (alternating signs)
        // Gentle injection for musical shimmer (30% of feedback signal)
        const float shimmerDist[8] = { 1.0f, -0.8f, 0.9f, -0.85f, 0.95f, -0.88f, 0.92f, -0.9f };
        float shimmerInjScaled = shimmerInj * 0.3f;  // 30% injection for gentle shimmer bloom
        for (int i = 0; i < 8; ++i) {
            float combIn = pd + fb * mixed[i] + shimmerInjScaled * shimmerDist[i];
            combIn = juce::jlimit(-2.0f, 2.0f, combIn);
            comb[i].delay.pushSample(0, combIn);
        }
        
        // ===== Allpass cascade for extra diffusion (enhanced for density) =====
        float diffused = 0.7f * (mixed[0] + mixed[2] + mixed[4] + mixed[6]);  // More combs -> denser
        
        for (int i = 0; i < 4; ++i) {
            float delayed = (float)ap[i].delay.popSample(0);
            float apOut = -ap[i].coeff * diffused + delayed;
            ap[i].delay.pushSample(0, diffused + ap[i].coeff * apOut);
            diffused = apOut;
        }
        
        // ===== Stereo output with enhanced spread for bigger, denser sound =====
        float outL = 0.75f * diffused + 0.5f * (mixed[0] + mixed[2] + mixed[4] + mixed[6]);  // More comb mixing
        float outR = 0.75f * diffused + 0.5f * (mixed[1] + mixed[3] + mixed[5] + mixed[7]);
        
        // Moderate boost for bigger reverb (reduced from 1.3x to 1.15x for less harshness)
        outL *= 1.15f;
        outR *= 1.15f;
        
        // Gentle saturation for musical sound (reduced aggressiveness)
        outL = std::tanh(outL * 0.7f);  // Was 0.5f, now 0.7f for smoother
        outR = std::tanh(outR * 0.7f);
        
        wetOut.setSample(0, n, outL);
        if (numChannels > 1)
            wetOut.setSample(1, n, outR);
    }
}

void ReverbTank::injectFeedback(const juce::AudioBuffer<float>& regen, float guard)
{
    feedbackBuffer.makeCopyOf(regen);
    feedbackGain = guard;
}

//==============================================================================
// ShimmerProcessor Implementation
//==============================================================================

ShimmerProcessor::ShimmerProcessor()
{
}

int ShimmerProcessor::currentOSIndexToFactor(int idx) const
{
    return (idx == 0) ? 1 : (idx == 1) ? 2 : (idx == 2) ? 4 : 8;
}

void ShimmerProcessor::setOSFactor(int f)
{
    osFactor = f;
    pitA.setOSFactor(f);
    pitB.setOSFactor(f);
    pitC.setOSFactor(f);
}

float ShimmerProcessor::cent(float c)
{
    return std::pow(2.0f, c / 1200.0f);
}

void ShimmerProcessor::setModeRatios()
{
    if (mode != prevMode)
    {
        modeCrossfadeRemaining = kModeCrossfadeSamples;
        prevMode = mode;
    }
    
    switch (mode)
    {
        case ShimmerMode::A:
            ratioA = 2.0f;
            ratioB = ratioC = 1.0f;
            detCents = 0;
            break;
        case ShimmerMode::B:
            ratioA = 1.498307f;
            ratioB = ratioC = 1.0f;
            detCents = 0;
            break;
        case ShimmerMode::C:
            ratioA = 2.0f;
            ratioB = 0.5f;
            ratioC = 1.0f;
            detCents = 0;
            break;
        case ShimmerMode::D:
            ratioA = 2.0f;
            ratioB = 2.0f * cent(+9);
            ratioC = 2.0f * cent(-9);
            break;
        case ShimmerMode::E:
            ratioA = cent(+6);
            ratioB = cent(-6);
            ratioC = cent(+11);
            break;
    }
}

float ShimmerProcessor::feedbackGuardGain(float shim, float decaySec) const
{
    // Moderate feedback for stable, musical shimmer (professional shimmer uses gentle feedback)
    const float base = juce::jmap(juce::jlimit(0.0f, 1.0f, shim), 0.0f, 1.0f, 0.1f, 0.6f);  // 0.1-0.6 range
    return base * (decaySec > 15.0f ? 0.9f : 1.0f);
}

float ShimmerProcessor::softLimit(float x) const
{
    return x / (1.0f + 0.5f * std::abs(x));
}

void ShimmerProcessor::softLimitInPlace(juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* data = buf.getWritePointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            data[i] = softLimit(data[i]);
    }
}

void ShimmerProcessor::processShimmerEQ(juce::AudioBuffer<float>& buf)
{
    juce::dsp::AudioBlock<float> block(buf);
    
    // Apply HP filter FIRST to prevent low-end buildup in feedback loop
    shimmerHP.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    // Apply chorus to smooth pitch artifacts
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    shimmerChorus.process(ctx);
    
    // Then apply gentle LP to prevent harshness
    shimmerLP.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    // Boost air
    shimmerAir.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void ShimmerProcessor::sumInPlace(juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        a.addFrom(ch, 0, b, ch, 0, a.getNumSamples());
}

void ShimmerProcessor::processPitchModes(juce::AudioBuffer<float>& buf, float amount)
{
    setModeRatios();
    const float pitchAmount = amount;
    
    if (modeCrossfadeRemaining > 0)
    {
        juce::AudioBuffer<float> oldMode(buf.getNumChannels(), buf.getNumSamples());
        oldMode.makeCopyOf(buf);
        
        switch (mode)
        {
            case ShimmerMode::A:
                pitA.processBuffer(buf, 2.0f, pitchAmount);
                break;
            case ShimmerMode::B:
                pitA.processBuffer(buf, 1.498307f, pitchAmount);
                break;
            case ShimmerMode::C:
            {
                juce::AudioBuffer<float> tmp(buf.getNumChannels(), buf.getNumSamples());
                tmp.makeCopyOf(buf);
                pitA.processBuffer(buf, 2.0f, pitchAmount * 0.6f);
                pitB.processBuffer(tmp, 0.5f, pitchAmount * 0.6f);
                sumInPlace(buf, tmp);
            }
            break;
            case ShimmerMode::D:
            {
                juce::AudioBuffer<float> t1(buf.getNumChannels(), buf.getNumSamples());
                t1.makeCopyOf(buf);
                pitA.processBuffer(buf, ratioA, pitchAmount * 0.5f);
                pitB.processBuffer(t1, ratioB, pitchAmount * 0.25f);
                sumInPlace(buf, t1);
                juce::AudioBuffer<float> t2(buf.getNumChannels(), buf.getNumSamples());
                t2.makeCopyOf(buf);
                pitC.processBuffer(t2, ratioC, pitchAmount * 0.25f);
                sumInPlace(buf, t2);
            }
            break;
            case ShimmerMode::E:
            {
                juce::AudioBuffer<float> t(buf.getNumChannels(), buf.getNumSamples());
                t.makeCopyOf(buf);
                pitA.processBuffer(buf, ratioA, pitchAmount * 0.35f);
                pitB.processBuffer(t, ratioB, pitchAmount * 0.35f);
                sumInPlace(buf, t);
                t.makeCopyOf(buf);
                pitC.processBuffer(t, ratioC, pitchAmount * 0.30f);
                sumInPlace(buf, t);
            }
            break;
        }
        
        const int samplesToFade = juce::jmin(modeCrossfadeRemaining, buf.getNumSamples());
        for (int i = 0; i < samplesToFade; ++i)
        {
            const float t = (float)i / (float)samplesToFade;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const float newSample = buf.getSample(ch, i);
                const float oldSample = oldMode.getSample(ch, i);
                buf.setSample(ch, i, oldSample * (1.0f - t) + newSample * t);
            }
        }
        modeCrossfadeRemaining -= samplesToFade;
    }
    else
    {
        switch (mode)
        {
            case ShimmerMode::A:
                pitA.processBuffer(buf, 2.0f, pitchAmount);
                break;
            case ShimmerMode::B:
                pitA.processBuffer(buf, 1.498307f, pitchAmount);
                break;
            case ShimmerMode::C:
            {
                juce::AudioBuffer<float> tmp(buf.getNumChannels(), buf.getNumSamples());
                tmp.makeCopyOf(buf);
                pitA.processBuffer(buf, 2.0f, pitchAmount * 0.6f);
                pitB.processBuffer(tmp, 0.5f, pitchAmount * 0.6f);
                sumInPlace(buf, tmp);
            }
            break;
            case ShimmerMode::D:
            {
                juce::AudioBuffer<float> t1(buf.getNumChannels(), buf.getNumSamples());
                t1.makeCopyOf(buf);
                pitA.processBuffer(buf, ratioA, pitchAmount * 0.5f);
                pitB.processBuffer(t1, ratioB, pitchAmount * 0.25f);
                sumInPlace(buf, t1);
                juce::AudioBuffer<float> t2(buf.getNumChannels(), buf.getNumSamples());
                t2.makeCopyOf(buf);
                pitC.processBuffer(t2, ratioC, pitchAmount * 0.25f);
                sumInPlace(buf, t2);
            }
            break;
            case ShimmerMode::E:
            {
                juce::AudioBuffer<float> t(buf.getNumChannels(), buf.getNumSamples());
                t.makeCopyOf(buf);
                pitA.processBuffer(buf, ratioA, pitchAmount * 0.35f);
                pitB.processBuffer(t, ratioB, pitchAmount * 0.35f);
                sumInPlace(buf, t);
                t.makeCopyOf(buf);
                pitC.processBuffer(t, ratioC, pitchAmount * 0.30f);
                sumInPlace(buf, t);
            }
            break;
        }
    }
}

void ShimmerProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    fs = spec.sampleRate;
    blockSize = (int)spec.maximumBlockSize;
    
    setOSFactor(currentOSIndexToFactor(osIndex));
    
    tank.prepare(spec);
    pitA.prepare(fs, osFactor, (int)spec.numChannels, 0.180, 0.050);  // 180ms grain, 50ms xfade
    pitB.prepare(fs, osFactor, (int)spec.numChannels, 0.180, 0.050);
    pitC.prepare(fs, osFactor, (int)spec.numChannels, 0.180, 0.050);
    
    sizeSm.reset(fs, 0.03);
    decaySm.reset(fs, 0.03);
    colorSm.reset(fs, 0.03);
    shimSm.reset(fs, 0.03);
    mixSm.reset(fs, 0.03);
    predelaySm.reset(fs, 0.03);
    
    wet.setSize((int)spec.numChannels, blockSize);
    regen.setSize((int)spec.numChannels, blockSize);
    regenStore.setSize((int)spec.numChannels, blockSize);
    regenStore.clear();
    
    // Prepare shimmer EQ filters - very gentle to preserve shimmer body
    shimmerHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(fs, 150.0);   // Was 300Hz - now 150Hz
    shimmerLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(fs, 16000.0);  // Was 15kHz - now 16kHz
    shimmerAir.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(fs, 5000.0, 0.7f, juce::Decibels::decibelsToGain(12.0f));  // +12dB air (was +9dB)
    
    // Prepare shimmer chorus for lush, artifact-free shimmer
    shimmerChorus.prepare(spec);
    shimmerChorus.setRate(0.8f);      // Slow modulation
    shimmerChorus.setDepth(0.25f);    // Subtle depth
    shimmerChorus.setFeedback(0.15f); // Light feedback
    shimmerChorus.setMix(0.4f);       // Blend with dry pitched signal
    shimmerChorus.setCentreDelay(7.0f); // 7ms center delay
}

void ShimmerProcessor::setParams(float size, float decay, float color, float predelayMs, float shimAmt, int modeIndex, int osIndexIn, float mix, float pitchTuneIn)
{
    sizeSm.setTargetValue(size);
    decaySm.setTargetValue(decay);
    colorSm.setTargetValue(color);
    predelaySm.setTargetValue(predelayMs);
    shimSm.setTargetValue(shimAmt);
    mixSm.setTargetValue(mix);
    pitchTune = juce::jlimit(0.5f, 2.0f, pitchTuneIn);  // -12 to +12 semitones
    
    if (osIndex != osIndexIn)
    {
        osIndex = osIndexIn;
        setOSFactor(currentOSIndexToFactor(osIndex));
    }
    if (mode != (ShimmerMode)modeIndex)
    {
        mode = (ShimmerMode)modeIndex;
        setModeRatios();
    }
}

void ShimmerProcessor::process(juce::dsp::AudioBlock<float>& block)
{
    juce::ScopedNoDenormals _;
    const int numSamples = (int)block.getNumSamples();
    const int numChannels = (int)block.getNumChannels();
    
    // Save dry signal
    juce::AudioBuffer<float> dry(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* dryPtr = dry.getWritePointer(ch);
        auto* blockPtr = block.getChannelPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            dryPtr[i] = blockPtr[i];
    }
    
    // Force audition defaults on first run
    if (firstRun)
    {
        sizeSm.setCurrentAndTargetValue(0.75f);     // Large plate
        decaySm.setCurrentAndTargetValue(12.0f);    // 12s decay
        colorSm.setCurrentAndTargetValue(0.50f);    // Balanced tone
        predelaySm.setCurrentAndTargetValue(40.0f); // 40ms predelay
        shimSm.setCurrentAndTargetValue(0.50f);     // 50% shimmer
        mixSm.setCurrentAndTargetValue(0.80f);      // 80% wet
        mode = ShimmerMode::A;
        setModeRatios();
        firstRun = false;
    }
    
    // Get smoothed parameters
    float mix = mixSm.getNextValue();
    if (dbgSoloWet)
        mix = 1.0f;
    
    // Bypass if mix is 0
    if (mix < 0.001f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* outPtr = block.getChannelPointer(ch);
            auto* dryPtr = dry.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                outPtr[i] = dryPtr[i];
        }
        return;
    }
    
    const float preMs = predelaySm.getNextValue();
    const float sizeVal = sizeSm.getNextValue();
    const float colorVal = colorSm.getNextValue();
    const float decayVal = decaySm.getNextValue();
    const float shim = shimSm.getNextValue();
    
    // Set reverb parameters
    tank.setPredelayMs(preMs);
    tank.setSize(sizeVal);
    tank.setColor(colorVal);
    tank.setDecaySeconds(decayVal);
    
    // A) Inject stored feedback from PREVIOUS block (one-block-late)
    if (regenStore.getNumSamples() == numSamples)
    {
        const float guard = feedbackGuardGain(shim, decayVal);
        tank.injectFeedback(regenStore, guard);
        regenStore.clear();
    }
    
    // B) Process reverb: dry → predelay → tank.process() → wet
    tank.process(block, wet);
    
    // C) Build shimmer regen from THIS wet, pitch it, EQ it, store for NEXT block
    // REMOVED: Pitched signal should NOT be in output - only in feedback for natural bloom
    if (shim > 0.0001f)
    {
        regen.makeCopyOf(wet);
        processPitchModes(regen, 1.0f);
        processShimmerEQ(regen);
        softLimitInPlace(regen);
        
        regenStore.setSize(numChannels, numSamples, false, false, true);
        regenStore.makeCopyOf(regen);
    }
    else
    {
        regenStore.clear();
    }
    
    // D) Output: mix(dry, wet, Mix) - Wet is pure reverb, shimmer blooms in feedback
    const float wetGain = 1.0f;  // No gain boost - let shimmer bloom naturally
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = block.getChannelPointer(ch);
        auto* wetData = wet.getReadPointer(ch);
        auto* dryData = dry.getReadPointer(ch);
        
        // DC blocking filter to prevent low-end buildup (one-pole HP @ 20Hz)
        static float dcState[2] = {0.0f, 0.0f};
        const float dcCoeff = 0.995f;  // HP filter coefficient
        
        for (int n = 0; n < numSamples; ++n)
        {
            const float wetScaled = wetData[n] * wetGain;
            float output = dryData[n] * (1.0f - mix) + wetScaled * mix;
            
            // DC blocking to prevent low-end stuck signal
            dcState[ch] = output + dcCoeff * (dcState[ch] - output);
            output -= dcState[ch];
            
            out[n] = output;
        }
    }
}

void ShimmerProcessor::processWithSnapshot(juce::AudioBuffer<float>& buffer, int numSamples, float mode, float size, float decay, float color, float predelay, float shimAmt, float osMode, float mix, bool stepChanged)
{
    // Implementation for snapshot-based processing
    juce::dsp::AudioBlock<float> block(buffer);
    setParams(size, decay, color, predelay, shimAmt, (int)mode, (int)osMode, mix);
    process(block);
}
