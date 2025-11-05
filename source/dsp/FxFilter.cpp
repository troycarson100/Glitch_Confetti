#include "FxFilter.h"

//==============================================================================
// SVFProc Implementation
//==============================================================================

SVFProc::SVFProc(int filterType) : type(filterType), use24dB(false)
{
    // Initialize filters
}

void SVFProc::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->spec = spec;
    sampleRate = spec.sampleRate;
    svf1.prepare(spec);
    svf2.prepare(spec);
    cutoffSm.reset(sampleRate, 0.005);  // 5ms smoothing for faster response
    cutoffSm.setCurrentAndTargetValue(1200.0f);
}

float SVFProc::mapResToQ(float res)
{
    // Musical mapping: Q = 0.5 + 11*res^2
    return 0.5f + 11.0f * res * res;
}

float SVFProc::getGainTrim(float q)
{
    // Equal-loudness trim at high resonance
    if (q > 5.0f) {
        return 1.0f / (1.0f + 0.1f * (q - 5.0f));
    }
    return 1.0f;
}

void SVFProc::set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs)
{
    use24dB = (slopeSel == 1);
    currentQ = mapResToQ(res);
    currentRes = res;  // Store resonance for BP mode dry/wet mixing
    cutoffSm.setTargetValue(cutoffHz);
    
    // Update filter coefficients
    float cut = cutoffSm.getCurrentValue();
    svf1.setCutoffFrequency(cut);
    svf1.setResonance(currentQ);
    
    // Set filter type
    if (type == 0) {
        svf1.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    } else if (type == 1) {
        svf1.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    } else {
        svf1.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }
    
    if (use24dB) {
        svf2.setCutoffFrequency(cut);
        svf2.setResonance(currentQ);
        if (type == 0) {
            svf2.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        } else if (type == 1) {
            svf2.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        } else {
            svf2.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        }
    }
}

void SVFProc::process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out)
{
    juce::ScopedNoDenormals nd;
    
    auto numSamples = (int)in.getNumSamples();
    auto numChannels = (int)in.getNumChannels();
    
    // Process each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* input = in.getChannelPointer(ch);
        auto* output = out.getChannelPointer(ch);
        
        // Update cutoff per sample for smoothness
        for (int i = 0; i < numSamples; ++i)
        {
            float cut = cutoffSm.getNextValue();
            svf1.setCutoffFrequency(cut);
            if (use24dB) {
                svf2.setCutoffFrequency(cut);
            }
            
            float sample = input[i];
            float drySample = sample;  // Store dry for BP mode mixing
            
            // Process through SVF (type already set in set() method)
            sample = svf1.processSample(ch, sample);
            if (use24dB) {
                sample = svf2.processSample(ch, sample);
            }
            
            // For bandpass mode: mix in dry signal when resonance is low for more transparency
            if (type == 2) {  // BP mode
                // Lower resonance = more dry signal mixed in (less apparent filtering)
                // When res is 0, mix 50% dry. When res is 1, mix 0% dry (full filtering)
                float dryMix = 0.5f * (1.0f - currentRes);
                float wetMix = 1.0f - dryMix;
                sample = dryMix * drySample + wetMix * sample;
            }
            
            // Apply gain trim for equal loudness
            float gainTrim = getGainTrim(currentQ);
            output[i] = sample * gainTrim;
        }
    }
}

//==============================================================================
// CombProc Implementation
//==============================================================================

CombProc::CombProc(int sign) : combSign(sign)
{
}

void CombProc::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    
    // Allocate delay buffer - max 8000 Hz needs fs/40 = ~1200 samples at 48kHz
    bufferSize = (int)juce::nextPowerOfTwo((int)(sampleRate / 40.0));  // Min 40 Hz
    delayBufferL.assign(bufferSize, 0.0f);
    delayBufferR.assign(bufferSize, 0.0f);
    mask = bufferSize - 1;
    writePos = 0;
    readPosL = 0.0f;
    readPosR = 0.0f;
    
    // Stability LP filter - 12-16 kHz cutoff
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 14000.0f);
    stabilityLP.prepare(spec);
    stabilityLP.coefficients = coeffs;
    lpCoeffs = coeffs;
}

float CombProc::readDelay(const std::vector<float>& buffer, float readIndex) const
{
    // 3rd-order Lagrange interpolation
    int i = (int)readIndex;
    float frac = readIndex - i;
    
    int i0 = (i - 1) & mask;
    int i1 = i & mask;
    int i2 = (i + 1) & mask;
    int i3 = (i + 2) & mask;
    
    float y0 = buffer[i0];
    float y1 = buffer[i1];
    float y2 = buffer[i2];
    float y3 = buffer[i3];
    
    // Lagrange3rd coefficients
    float c0 = (-1.0f/6.0f) * y0 + 0.5f * y1 - 0.5f * y2 + (1.0f/6.0f) * y3;
    float c1 = 0.5f * y0 - y1 + 0.5f * y2;
    float c2 = (-1.0f/3.0f) * y0 + 0.5f * y1 + 0.5f * y2 - (1.0f/6.0f) * y3;
    float c3 = y1;
    
    return ((c0 * frac + c1) * frac + c2) * frac + c3;
}

void CombProc::updateDelayLine(float tuneHz, float spread)
{
    // Store tune Hz and spread
    currentTuneHz = juce::jlimit(40.0f, 8000.0f, tuneHz);
    spreadCents = spread;
    
    // Convert tune Hz to delay length in samples
    currentDelay = sampleRate / currentTuneHz;
    currentDelay = juce::jlimit(1.0f, (float)(bufferSize - 1), currentDelay);
}

void CombProc::set(float cutoffHz, float res, float slopeOrDepth, int slopeSel, float driveDb, float spreadCents, float fs)
{
    feedback = juce::jlimit(0.0f, 0.95f, res);
    // For Comb, slopeSel (0 or 1) becomes depth (0.0 or 1.0)
    depth = (slopeSel == 1) ? 1.0f : 0.0f;
    updateDelayLine(cutoffHz, spreadCents);
}

void CombProc::process(juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>& out)
{
    juce::ScopedNoDenormals nd;
    
    auto numSamples = (int)in.getNumSamples();
    auto numChannels = (int)in.getNumChannels();
    
    if (numChannels == 0 || numSamples == 0) return;
    
    auto* inputL = in.getChannelPointer(0);
    auto* inputR = (numChannels > 1) ? in.getChannelPointer(1) : inputL;
    auto* outputL = out.getChannelPointer(0);
    auto* outputR = (numChannels > 1) ? out.getChannelPointer(1) : outputL;
    
    // Apply stereo spread (cents -> Hz multiplier, then to delay)
    float spreadMultL = std::pow(2.0f, spreadCents / 1200.0f);
    float spreadMultR = std::pow(2.0f, -spreadCents / 1200.0f);
    
    float tuneL = currentTuneHz * spreadMultL;
    float tuneR = currentTuneHz * spreadMultR;
    
    float delayL = sampleRate / juce::jmax(40.0f, tuneL);
    float delayR = sampleRate / juce::jmax(40.0f, tuneR);
    
    // Clamp delays
    delayL = juce::jlimit(1.0f, (float)(bufferSize - 1), delayL);
    delayR = juce::jlimit(1.0f, (float)(bufferSize - 1), delayR);
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Calculate read positions
        float readIdxL = (float)writePos - delayL;
        float readIdxR = (float)writePos - delayR;
        
        // Wrap
        while (readIdxL < 0) readIdxL += bufferSize;
        while (readIdxL >= bufferSize) readIdxL -= bufferSize;
        while (readIdxR < 0) readIdxR += bufferSize;
        while (readIdxR >= bufferSize) readIdxR -= bufferSize;
        
        // Read delayed samples
        float delayedL = readDelay(delayBufferL, readIdxL);
        float delayedR = readDelay(delayBufferR, readIdxR);
        
        // Apply feedback (with sign for Comb- vs Comb+)
        float fbL = delayedL * feedback * (float)combSign;
        float fbR = delayedR * feedback * (float)combSign;
        
        // Apply feed-forward depth (if > 0)
        float ffL = (depth > 0.0f) ? delayedL * depth : 0.0f;
        float ffR = (depth > 0.0f) ? delayedR * depth : 0.0f;
        
        // Get input samples
        float inL = inputL[i];
        float inR = inputR[i];
        
        // Write to delay buffer: input + feedback
        delayBufferL[writePos] = inL + fbL;
        delayBufferR[writePos] = inR + fbR;
        
        // Advance write position
        writePos = (writePos + 1) & mask;
        
        // Output: delayed + feed-forward + some dry signal for more apparent comb effect
        // Mix in dry signal to make comb filtering more noticeable
        float dryMix = 0.3f;  // 30% dry to make comb effect more apparent
        float wetMix = 1.0f - dryMix;
        float outL = dryMix * inL + wetMix * (delayedL + ffL);
        float outR = dryMix * inR + wetMix * (delayedR + ffR);
        
        // Apply stability LP
        outL = stabilityLP.processSample(outL);
        outR = stabilityLP.processSample(outR);
        
        // Boost output slightly to make comb filters more apparent
        float combBoost = 1.2f;
        outputL[i] = outL * combBoost;
        outputR[i] = outR * combBoost;
    }
}

//==============================================================================
// FxFilter Implementation
//==============================================================================

FxFilter::FxFilter()
{
    cutoffSm.reset(48000.0, 0.005);  // 5ms smoothing for faster response
    cutoffSm.setCurrentAndTargetValue(1200.0f);
    resSm.reset(48000.0, 0.015);
    resSm.setCurrentAndTargetValue(0.35f);
}

void FxFilter::prepare(double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    block = maxBlockSize;
    
    specCached.sampleRate = sampleRate;
    specCached.maximumBlockSize = (juce::uint32)maxBlockSize;
    specCached.numChannels = 2;
    
    cutoffSm.reset(sampleRate, 0.005);  // 5ms smoothing for faster response
    cutoffSm.setCurrentAndTargetValue(1200.0f);
    resSm.reset(sampleRate, 0.015);
    resSm.setCurrentAndTargetValue(0.35f);
    
    tmpA.setSize(2, maxBlockSize);
    tmpB.setSize(2, maxBlockSize);
    
    makeFilter(0);  // Default LP
    if (cur) {
        cur->prepare(specCached);
    }
}

float FxFilter::applyKeyTracking(float baseCutoff, float keytrackVal, int midiNote)
{
    if (keytrackVal < 0.001f) return baseCutoff;
    
    // 1 semitone per 10 units = 0.01 per unit
    float semitoneOffset = (float)(midiNote - 60) * 0.01f * keytrackVal;
    return baseCutoff * std::pow(2.0f, semitoneOffset / 12.0f);
}

void FxFilter::makeFilter(int type)
{
    if (type <= 2) {
        newF = std::make_unique<SVFProc>(type);  // LP=0, HP=1, BP=2
    } else {
        int sign = (type == 3) ? -1 : +1;  // Comb-=3, Comb+=4
        newF = std::make_unique<CombProc>(sign);
    }
    
    if (!cur) {
        cur = std::make_unique<SVFProc>(0);  // Default LP
    }
}

void FxFilter::setTargets(const FilterTargets& targets)
{
    targetType = juce::jlimit(0, 4, targets.type);
    slope = targets.slope;
    drive = targets.drive;
    spreadCents = targets.spread;
    keytrack = targets.keytrack;
    mix = targets.mix;
    
    // Apply key tracking to cutoff
    float effectiveCutoff = applyKeyTracking(targets.cutoff, targets.keytrack, currentMIDINote);
    cutoffSm.setTargetValue(effectiveCutoff);
    resSm.setTargetValue(targets.res);
}

void FxFilter::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::ScopedNoDenormals nd;
    
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples <= 0) return;
    
    // Store dry signal for mix (before any processing)
    juce::AudioBuffer<float> dryBuffer(numChannels, numSamples);
    for (int c = 0; c < numChannels; ++c) {
        dryBuffer.copyFrom(c, 0, buffer, c, 0, numSamples);
    }
    
    // Handle type change with crossfade
    if (currentType != targetType) {
        makeFilter(targetType);
        if (newF) {
            newF->prepare(specCached);
            ramp.start(fs, 20.0);  // 20ms crossfade
        }
        currentType = targetType;
    }
    
    // Update params block-rate
    // Note: Cutoff smoothing happens per-sample in SVFProc, so we use the current target value directly
    // This gives faster response to knob changes while SVFProc handles smooth per-sample transitions
    float cut = cutoffSm.getTargetValue();  // Use target directly for faster response
    cutoffSm.skip(numSamples);  // Advance smoothing state without using it
    float r = resSm.getNextValue();
    
    // Apply drive pre-filter (drive is part of the effect, not dry signal)
    float gPre = juce::Decibels::decibelsToGain(drive);
    for (int c = 0; c < numChannels; ++c) {
        buffer.applyGain(c, 0, numSamples, gPre);
    }
    
    // Create audio blocks
    juce::dsp::AudioBlock<float> inputBlock(buffer);
    juce::dsp::AudioBlock<float> outputBlockA(tmpA);
    juce::dsp::AudioBlock<float> outputBlockB(tmpB);
    
    // Process current filter
    if (cur) {
        cur->set(cut, r, (float)slope, slope, drive, spreadCents, fs);
        cur->process(inputBlock, outputBlockA);
    } else {
        outputBlockA.copyFrom(inputBlock);
    }
    
    if (ramp.isActive()) {
        // Process new filter
        if (newF) {
            newF->set(cut, r, (float)slope, slope, drive, spreadCents, fs);
            newF->process(inputBlock, outputBlockB);
        } else {
            outputBlockB.copyFrom(inputBlock);
        }
        
        // Crossfade
        for (int c = 0; c < numChannels; ++c) {
            auto* out = buffer.getWritePointer(c);
            auto* A = tmpA.getReadPointer(c);
            auto* B = tmpB.getReadPointer(c);
            
            for (int i = 0; i < numSamples; ++i) {
                float g = ramp.next();
                out[i] = (1.0f - g) * A[i] + g * B[i];
            }
        }
        
        // Swap in new filter after crossfade
        if (!ramp.isActive()) {
            cur.swap(newF);
            newF.reset();
        }
    } else {
        // No crossfade, just copy A -> output
        for (int c = 0; c < numChannels; ++c) {
            buffer.copyFrom(c, 0, tmpA, c, 0, numSamples);
        }
    }
    
    // Soft limit
    for (int c = 0; c < numChannels; ++c) {
        auto* p = buffer.getWritePointer(c);
        for (int i = 0; i < numSamples; ++i) {
            float x = p[i];
            p[i] = x / (1.0f + 0.5f * std::abs(x));
        }
    }
    
    // Apply dry/wet mix
    float wetMix = mix;
    float dryMix = 1.0f - mix;
    
    for (int c = 0; c < numChannels; ++c) {
        auto* out = buffer.getWritePointer(c);
        auto* dry = dryBuffer.getReadPointer(c);
        
        for (int i = 0; i < numSamples; ++i) {
            out[i] = dryMix * dry[i] + wetMix * out[i];
        }
    }
}

