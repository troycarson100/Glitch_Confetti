#include "DubDelayProcessor.h"

DubDelayProcessor::DubDelayProcessor()
{
    random.setSeedRandomly();
}

void DubDelayProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    
    // Allocate ring buffers (20 seconds max delay for tempo sync, power-of-two size)
    // (4 beats @ 20 BPM dotted ≈ 18s, so 20s covers all cases)
    int bufferSize = 1;
    while (bufferSize < static_cast<int>(sampleRate * 20.5)) {
        bufferSize *= 2;
    }
    bufferMask = bufferSize - 1;
    
    delayBufferL.resize(bufferSize, 0.0f);
    delayBufferR.resize(bufferSize, 0.0f);
    writePos = 0;
    
    // Initialize smoothed parameters (100ms ramp time for time to prevent scratchy sounds)
    timeMsSmooth.reset(sampleRate, 0.300); // Much longer ramp for smooth time changes (300ms)
    feedbackSmooth.reset(sampleRate, 0.015);
    toneCutoffSmooth.reset(sampleRate, 0.015);
    driveSmooth.reset(sampleRate, 0.015);
    wowFlutterSmooth.reset(sampleRate, 0.015);
    regenDampSmooth.reset(sampleRate, 0.015);
    mixSmooth.reset(sampleRate, 0.015);
    
    // More aggressive smoothing for delay samples to prevent read position jumps
    delaySampsSmoothL.reset(sampleRate, 0.200); // 200ms smoothing for read position
    delaySampsSmoothR.reset(sampleRate, 0.200); // 200ms smoothing for read position
    
    // Set initial values
    timeMsSmooth.setCurrentAndTargetValue(450.0f);
    feedbackSmooth.setCurrentAndTargetValue(0.45f);
    toneCutoffSmooth.setCurrentAndTargetValue(6500.0f);
    driveSmooth.setCurrentAndTargetValue(0.15f);
    wowFlutterSmooth.setCurrentAndTargetValue(0.35f);
    regenDampSmooth.setCurrentAndTargetValue(0.25f);
    mixSmooth.setCurrentAndTargetValue(0.35f);
    
    // Initialize delay samples smoothers (faster smoothing for read position)
    delaySampsSmoothL.setCurrentAndTargetValue(450.0f * static_cast<float>(sampleRate) * 0.001f);
    delaySampsSmoothR.setCurrentAndTargetValue(450.0f * static_cast<float>(sampleRate) * 0.001f);
    
    // Prepare filters
    hpfL.setCutoff(40.0f, sampleRate);  // Fixed 40 Hz HPF in feedback path
    hpfR.setCutoff(40.0f, sampleRate);
    
    toneLPF_L.prepare({sampleRate, static_cast<juce::uint32>(maxBlockSize), 2});
    toneLPF_R.prepare({sampleRate, static_cast<juce::uint32>(maxBlockSize), 2});
    toneLPF_L.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    toneLPF_R.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    toneLPF_L.setCutoffFrequency(6500.0f);
    toneLPF_R.setCutoffFrequency(6500.0f);
    
    shelfL.prepare(sampleRate);
    shelfR.prepare(sampleRate);
    shelfL.setParams(4000.0f, -3.0f);  // Initial mild damping
    shelfR.setParams(4000.0f, -3.0f);
    
    // Reset modulation
    wowPhase = 0.0;
    flutterPhase = 0.0;
    flutterPhaseR = juce::MathConstants<double>::halfPi;  // 90° offset for stereo
    randomWalk = 0.0f;
    randomWalkSmooth = 0.0f;
}

void DubDelayProcessor::setTargets(const Targets& t)
{
    timeMsSmooth.setTargetValue(juce::jlimit(1.0f, 2000.0f, t.timeMs));
    feedbackSmooth.setTargetValue(juce::jlimit(0.0f, 0.98f, t.feedback));
    toneCutoffSmooth.setTargetValue(juce::jlimit(200.0f, 20000.0f, t.toneHz));
    driveSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, t.drive));
    wowFlutterSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, t.wowFlutterDepth));
    regenDampSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, t.regenDamp));
    mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, t.mix));
    pingPongEnabled = t.pingPong;
}

void DubDelayProcessor::setTargetDelaySec(float seconds)
{
    // Clamp to safe range (0.001s to 20s)
    seconds = juce::jlimit(0.001f, 20.0f, seconds);
    float newTimeMs = seconds * 1000.0f;
    
    // Detect big jump: if ratio > 2.0, trigger crossfade
    float currentTimeMs = timeMsSmooth.getTargetValue();
    float ratio = juce::jmax(newTimeMs, currentTimeMs) / juce::jmin(newTimeMs, currentTimeMs);
    
    if (ratio > 2.0f && !isCrossfading)
    {
        // Start crossfade (10-20ms)
        isCrossfading = true;
        crossfadeTotalSamples = static_cast<int>(sr * 0.015); // 15ms crossfade
        crossfadeSamplesRemaining = crossfadeTotalSamples;
        
        // Store current read positions as "A"
        float currentDelayMs = timeMsSmooth.getCurrentValue();
        float delaySamples = (currentDelayMs / 1000.0f) * static_cast<float>(sr);
        crossfadeReadPosA_L = static_cast<float>(writePos) - delaySamples;
        crossfadeReadPosA_R = static_cast<float>(writePos) - delaySamples;
        
        // Compute new read positions as "B"
        float newDelaySamples = (newTimeMs / 1000.0f) * static_cast<float>(sr);
        crossfadeReadPosB_L = static_cast<float>(writePos) - newDelaySamples;
        crossfadeReadPosB_R = static_cast<float>(writePos) - newDelaySamples;
    }
    
    // Always update the target
    timeMsSmooth.setTargetValue(newTimeMs);
    previousTimeSec = seconds;
}

void DubDelayProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 2) return;
    
    auto* dataL = buffer.getWritePointer(0);
    auto* dataR = buffer.getWritePointer(1);
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Get smoothed parameter values
        const float timeMs = timeMsSmooth.getNextValue();
        const float fbUser = feedbackSmooth.getNextValue();
        const float toneCutoff = toneCutoffSmooth.getNextValue();
        const float drive = driveSmooth.getNextValue();
        const float wfDepth = wowFlutterSmooth.getNextValue();
        const float regenDamp = regenDampSmooth.getNextValue();
        const float mixParam = mixSmooth.getNextValue();
        
        // Update tone LPF cutoff
        toneLPF_L.setCutoffFrequency(toneCutoff);
        toneLPF_R.setCutoffFrequency(toneCutoff);
        
        // Update regen damp shelf (−12 dB max at full regenDamp)
        const float dampGainDb = -12.0f * regenDamp;
        shelfL.setParams(4000.0f, dampGainDb);
        shelfR.setParams(4000.0f, dampGainDb);
        
        // Compute auto-gain compensation
        const float loopAtten = computeLoopAttenuation(toneCutoff, regenDamp);
        const float fbTarget = std::min(0.98f, fbUser / std::max(0.1f, loopAtten));
        
        // Slew feedback compensation (20ms time constant)
        const float fbSlew = 1.0f - std::exp(-1.0f / (0.020f * static_cast<float>(sr)));
        compensatedFeedback += fbSlew * (fbTarget - compensatedFeedback);
        const float fb = compensatedFeedback;
        
        // Wow/Flutter modulation
        // Wow: slow LFO (0.25 Hz) + random walk
        const double wowFreq = 0.25;
        wowPhase += wowFreq / sr;
        if (wowPhase >= 1.0) wowPhase -= 1.0;
        const float wowLFO = std::sin(juce::MathConstants<float>::twoPi * static_cast<float>(wowPhase));
        
        // Random walk for tape-like drift
        if (random.nextFloat() < 0.001f) {  // Update occasionally
            randomWalk += (random.nextFloat() - 0.5f) * 0.5f;
            randomWalk = juce::jlimit(-1.0f, 1.0f, randomWalk);
        }
        randomWalkSmooth += 0.0001f * (randomWalk - randomWalkSmooth);  // Very slow
        
        // Flutter: faster LFO (5 Hz)
        const double flutterFreq = 5.0;
        flutterPhase += flutterFreq / sr;
        if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        flutterPhaseR += flutterFreq / sr;
        if (flutterPhaseR >= 1.0) flutterPhaseR -= 1.0;
        
        const float flutterL = std::sin(juce::MathConstants<float>::twoPi * static_cast<float>(flutterPhase));
        const float flutterR = std::sin(juce::MathConstants<float>::twoPi * static_cast<float>(flutterPhaseR));
        
        // Total modulation (max 6ms depth)
        const float maxModMs = 6.0f;
        const float modL = wfDepth * maxModMs * (0.7f * wowLFO + 0.2f * randomWalkSmooth + 0.1f * flutterL);
        const float modR = wfDepth * maxModMs * (0.7f * wowLFO + 0.2f * randomWalkSmooth + 0.1f * flutterR);
        
        // Calculate delay time in samples with modulation
        // For ping-pong: offset the channels to create stereo effect
        float timeL = timeMs + modL;
        float timeR = timeMs + modR;
        
        if (pingPongEnabled) {
            // Add stereo offset for ping-pong effect (25% of delay time)
            const float stereoOffset = timeMs * 0.25f;
            timeL += stereoOffset;
            timeR -= stereoOffset;
        }
        
        const float targetDelaySampsL = juce::jlimit(1.0f, static_cast<float>(bufferMask), 
                                                      timeL * static_cast<float>(sr) * 0.001f);
        const float targetDelaySampsR = juce::jlimit(1.0f, static_cast<float>(bufferMask), 
                                                      timeR * static_cast<float>(sr) * 0.001f);
        
        // Smooth the delay samples to prevent read position jumps
        delaySampsSmoothL.setTargetValue(targetDelaySampsL);
        delaySampsSmoothR.setTargetValue(targetDelaySampsR);
        const float delaySampsL = delaySampsSmoothL.getNextValue();
        const float delaySampsR = delaySampsSmoothR.getNextValue();
        
        // Read from delay lines with fractional interpolation
        const float readPosL = static_cast<float>(writePos) - delaySampsL;
        const float readPosR = static_cast<float>(writePos) - delaySampsR;
        
        float delayedL = lagrange3(delayBufferL, readPosL);
        float delayedR = lagrange3(delayBufferR, readPosR);
        
        // Input signal
        float inL = dataL[i];
        float inR = dataR[i];
        
        // Pre-delay drive (soft clip with makeup gain)
        if (drive > 0.01f) {
            inL = softClip(inL, drive);
            inR = softClip(inR, drive);
        }
        
        // Store clean delayed signal for output (before feedback processing)
        float wetL = delayedL;
        float wetR = delayedR;
        
        // Feedback path processing
        // HPF (fixed 40 Hz to remove DC/sub buildup)
        delayedL = hpfL.process(delayedL);
        delayedR = hpfR.process(delayedR);
        
        // Tone LPF
        delayedL = toneLPF_L.processSample(0, delayedL);
        delayedR = toneLPF_R.processSample(0, delayedR);
        
        // Regen damp shelf
        delayedL = shelfL.process(delayedL);
        delayedR = shelfR.process(delayedR);
        
        // Apply feedback gain (reduced by 40% to prevent overbearing feedback)
        delayedL *= fb * 0.6f;
        delayedR *= fb * 0.6f;
        
        // Ping-pong cross-feedback
        float fbL, fbR;
        if (pingPongEnabled) {
            // Cross-feed: L→R and R→L
            fbL = delayedR;   // Right feedback goes to left delay
            fbR = delayedL;   // Left feedback goes to right delay
        } else {
            // Same-channel feedback
            fbL = delayedL;
            fbR = delayedR;
        }
        
        // Write to delay buffers (input + feedback)
        delayBufferL[writePos] = inL + fbL;
        delayBufferR[writePos] = inR + fbR;
        
        // Advance write position
        writePos = (writePos + 1) & bufferMask;
        
        // Equal-power crossfade (wet/dry mix) with boosted wet signal for prominence
        const float wetGain = std::sin(juce::MathConstants<float>::halfPi * mixParam) * 1.5f; // Boost wet by 1.5x
        const float dryGain = std::cos(juce::MathConstants<float>::halfPi * mixParam);
        
        dataL[i] = dryGain * dataL[i] + wetGain * wetL;
        dataR[i] = dryGain * dataR[i] + wetGain * wetR;
        
        // Denormal protection
        if (std::abs(dataL[i]) < 1e-15f) dataL[i] = 0.0f;
        if (std::abs(dataR[i]) < 1e-15f) dataR[i] = 0.0f;
    }
}

float DubDelayProcessor::computeLoopAttenuation(float toneCutoff, float regenDamp)
{
    // Estimate effective loop gain reduction from filters
    const float k = 4000.0f;
    const float lpAtten = toneCutoff / (toneCutoff + k);
    
    // Shelf attenuation (−12 dB max)
    const float shelfAtten = std::pow(10.0f, (-12.0f * regenDamp * 0.7f) / 20.0f);
    
    // HPF constant (slight bass reduction)
    const float hpConst = 0.98f;
    
    const float loopAtten = hpConst * lpAtten * shelfAtten;
    return juce::jlimit(0.1f, 1.0f, loopAtten);
}

float DubDelayProcessor::softClip(float x, float drive)
{
    // Soft clip: tanh with drive-dependent gain (1 to 10)
    const float g = 1.0f + 9.0f * drive;
    const float y = std::tanh(g * x);
    
    // Makeup gain to maintain unity on small signals
    const float makeup = 1.0f / std::tanh(g * 0.5f);  // Normalize at 0.5 input level
    
    return juce::jlimit(-0.98f, 0.98f, y * makeup * 0.5f);
}

float DubDelayProcessor::lagrange3(const std::vector<float>& buffer, float readPos)
{
    // Wrap read position to buffer range
    while (readPos < 0.0f) readPos += static_cast<float>(bufferMask + 1);
    while (readPos >= static_cast<float>(bufferMask + 1)) readPos -= static_cast<float>(bufferMask + 1);
    
    const int i0 = static_cast<int>(readPos);
    const float frac = readPos - static_cast<float>(i0);
    
    // Get 4 samples for 3rd-order interpolation
    const int i1 = (i0 - 1) & bufferMask;
    const int i2 = i0 & bufferMask;
    const int i3 = (i0 + 1) & bufferMask;
    const int i4 = (i0 + 2) & bufferMask;
    
    const float y1 = buffer[i1];
    const float y2 = buffer[i2];
    const float y3 = buffer[i3];
    const float y4 = buffer[i4];
    
    // Lagrange 3rd order
    const float c0 = y2;
    const float c1 = 0.5f * (y3 - y1);
    const float c2 = y1 - 2.5f * y2 + 2.0f * y3 - 0.5f * y4;
    const float c3 = 0.5f * (y4 - y1) + 1.5f * (y2 - y3);
    
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void DubDelayProcessor::HighShelf::prepare(double sampleRate)
{
    sr = sampleRate;
    z1 = z2 = 0.0f;
}

void DubDelayProcessor::HighShelf::setParams(float freqHz, float gainDb)
{
    // Biquad high-shelf coefficients
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = juce::MathConstants<float>::twoPi * freqHz / static_cast<float>(sr);
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float alpha = sinw0 / 2.0f * std::sqrt(2.0f);  // Q = 0.707
    
    const float b0_temp = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha);
    const float b1_temp = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
    const float b2_temp = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha);
    const float a0_temp = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha;
    const float a1_temp = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
    const float a2_temp = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha;
    
    // Normalize
    a0 = b0_temp / a0_temp;
    a1 = b1_temp / a0_temp;
    a2 = b2_temp / a0_temp;
    b1 = a1_temp / a0_temp;
    b2 = a2_temp / a0_temp;
}

float DubDelayProcessor::HighShelf::process(float x)
{
    const float y = a0 * x + a1 * z1 + a2 * z2 - b1 * z1 - b2 * z2;
    
    // Shift delay line
    z2 = z1;
    z1 = x;
    
    return y;
}

