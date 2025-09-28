#include "FxDelay.h"

FxDelay::FxDelay()
{
}

void FxDelay::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    
    // RE-201 style tape buffer - 4 seconds at max sample rate
    bufferSize = (int) juce::nextPowerOfTwo((int) std::ceil(4.0 * sampleRate));
    tapeBufferL.assign(bufferSize, 0.0f);
    tapeBufferR.assign(bufferSize, 0.0f);
    mask = bufferSize - 1;
    writePos = 0;
    
    // Initialize parameter smoothers
    timeMsSm.reset(sampleRate, 0.03);    // 30ms smoothing
    feedbackSm.reset(sampleRate, 0.02);  // 20ms smoothing
    mixSm.reset(sampleRate, 0.02);       // 20ms smoothing
    driveSm.reset(sampleRate, 0.03);     // 30ms smoothing
    hiCutSm.reset(sampleRate, 0.02);     // 20ms smoothing
    loCutSm.reset(sampleRate, 0.02);     // 20ms smoothing
    wowDepthSm.reset(sampleRate, 0.05);  // 50ms smoothing
    wowRateSm.reset(sampleRate, 0.05);   // 50ms smoothing
    
    // Initialize filters
    hiCutFilterL.setCutoff(20000.0f);
    hiCutFilterR.setCutoff(20000.0f);
    loCutFilterL.setCutoff(20.0f);
    loCutFilterR.setCutoff(20.0f);
    
    DBG("[RE-201] Buffer size: " << bufferSize << " samples (" << (bufferSize/sampleRate) << "s)");
}

void FxDelay::setTargets(const DelayTargets& targets)
{
    // Map UI parameters to internal ranges
    timeMsSm.setTargetValue(juce::jlimit(5.0f, 2000.0f, targets.timeMs));
    feedbackSm.setTargetValue(juce::jlimit(0.0f, 0.95f, targets.feedback));
    mixSm.setTargetValue(juce::jlimit(0.0f, 1.0f, targets.mix));
    driveSm.setTargetValue(juce::jlimit(0.0f, 1.0f, targets.drive));
    hiCutSm.setTargetValue(juce::jlimit(1000.0f, 20000.0f, targets.hiCutHz));
    loCutSm.setTargetValue(juce::jlimit(20.0f, 2000.0f, targets.lowCutHz));
    wowDepthSm.setTargetValue(juce::jlimit(0.0f, 1.0f, targets.wowDepth));
    wowRateSm.setTargetValue(juce::jlimit(0.05f, 8.0f, targets.wowRate));
    
    // Update filter cutoffs
    hiCutFilterL.setCutoff(targets.hiCutHz);
    hiCutFilterR.setCutoff(targets.hiCutHz);
    loCutFilterL.setCutoff(targets.lowCutHz);
    loCutFilterR.setCutoff(targets.lowCutHz);
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
    
    // Get smoothed parameters
    const float timeMs = timeMsSm.getNextValue();
    const float feedback = feedbackSm.getNextValue();
    const float mix = mixSm.getNextValue();
    const float drive = driveSm.getNextValue();
    
    // Convert delay time to samples
    float baseDelaySamples = timeMs * 0.001f * currentSampleRate;
    baseDelaySamples = juce::jlimit(5.0f, (float)(bufferSize - 10), baseDelaySamples);
    
    // Process each sample
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Generate wow and flutter modulation
        float modulation = generateWowFlutter();
        float delaySamples = baseDelaySamples + modulation;
        
        // Calculate read position
        float readPos = wrapBuffer(writePos - delaySamples);
        
        // Read delayed signal with interpolation
        float delayedL = readDelay(tapeBufferL, readPos);
        float delayedR = readDelay(tapeBufferR, readPos);
        
        // Get input samples
        float inputSampleL = inputL[sample];
        float inputSampleR = inputR[sample];
        
        // Apply feedback filters (RE-201 style)
        float filteredL = delayedL;
        float filteredR = delayedR;
        
        // High cut filter
        filteredL = hiCutFilterL.processLowPass(filteredL, currentSampleRate);
        filteredR = hiCutFilterR.processLowPass(filteredR, currentSampleRate);
        
        // Low cut filter
        filteredL = loCutFilterL.processHighPass(filteredL, currentSampleRate);
        filteredR = loCutFilterR.processHighPass(filteredR, currentSampleRate);
        
        // Apply tape saturation in feedback loop
        float saturatedL = tapeSatL.process(filteredL, drive);
        float saturatedR = tapeSatR.process(filteredR, drive);
        
        // Write to delay buffer: input + feedback
        tapeBufferL[writePos] = inputSampleL + feedback * saturatedL;
        tapeBufferR[writePos] = inputSampleR + feedback * saturatedR;
        
        // Advance write position
        writePos = (writePos + 1) & mask;
        
        // Mix dry and wet signals
        outputL[sample] = (1.0f - mix) * inputSampleL + mix * delayedL;
        outputR[sample] = (1.0f - mix) * inputSampleR + mix * delayedR;
    }
}

float FxDelay::generateWowFlutter()
{
    const float wowDepth = wowDepthSm.getCurrentValue();
    const float wowRate = wowRateSm.getCurrentValue();
    
    if (wowDepth < 0.001f) return 0.0f;
    
    // Generate wow and flutter modulation
    const float phaseIncrement = 2.0f * juce::MathConstants<float>::pi * wowRate / currentSampleRate;
    lfoPhase += phaseIncrement;
    
    // Combine multiple LFOs for realistic tape modulation
    float wow = std::sin(lfoPhase) * 0.7f;
    wow += std::sin(lfoPhase * 1.7f) * 0.3f;  // Secondary harmonic
    wow += std::sin(lfoPhase * 0.3f) * 0.2f;  // Sub-harmonic
    
    // Add some flutter (higher frequency)
    float flutter = std::sin(lfoPhase * 23.0f) * 0.1f;
    
    // Scale by depth and convert to samples
    float modulation = (wow + flutter) * wowDepth * 2.5f; // ±2.5ms max
    
    return modulation * currentSampleRate * 0.001f; // Convert to samples
}