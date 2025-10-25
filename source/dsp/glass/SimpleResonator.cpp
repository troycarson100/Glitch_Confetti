#include "SimpleResonator.h"

void SimpleResonator::prepare(double sr, int /*blockSize*/, int /*numChannels*/)
{
    sampleRate = (sr > 0 ? sr : 48000.0);
    
    // Prepare all mode filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    
    for (int i = 0; i < kNumModes; ++i) {
        modesL[i].prepare(spec);
        modesR[i].prepare(spec);
    }
    
    // Set default parameters
    SimpleResonatorParams defaultParams;
    setParams(defaultParams);
    
    reset();
}

void SimpleResonator::reset()
{
    for (int i = 0; i < kNumModes; ++i) {
        modesL[i].reset();
        modesR[i].reset();
    }
}

void SimpleResonator::setParams(const SimpleResonatorParams& p)
{
    const float f0 = juce::jlimit(20.f, 12000.f, p.f0Hz);
    const float bright = juce::jlimit(0.f, 1.f, p.brightness);
    const float damp = juce::jlimit(0.f, 1.f, p.damping);
    const float sprd = juce::jlimit(0.f, 1.f, p.spread);
    
    // Only update if changed significantly
    if (std::abs(f0 - currentF0) > 1.0f ||
        std::abs(bright - currentBrightness) > 0.02f ||
        std::abs(damp - currentDamping) > 0.02f ||
        std::abs(sprd - currentSpread) > 0.02f)
    {
        currentF0 = f0;
        currentBrightness = bright;
        currentDamping = damp;
        currentSpread = sprd;
        updateModes();
    }
}

void SimpleResonator::updateModes()
{
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    
    // Modal frequency ratios (glass-like inharmonic series)
    // f_k = f0 * pow(k+1, 1.35) for increasing inharmonicity
    for (int k = 0; k < kNumModes; ++k) {
        const float ratio = std::pow(static_cast<float>(k + 1), 1.35f);
        float modeFreq = currentF0 * ratio;
        
        // Brightness shifts modes upward
        modeFreq *= (1.0f + currentBrightness * 0.8f);
        
        // Clamp below Nyquist with margin
        modeFreq = juce::jlimit(20.f, nyquist - 300.f, modeFreq);
        
        // Q factor: higher for longer decays, grows with mode number
        // damping [0..1] → Q [300..15]
        const float baseQ = juce::jmap(currentDamping, 0.f, 1.f, 300.f, 15.f);
        const float modeQ = baseQ * (1.0f + k * 0.15f);  // Higher modes ring longer
        
        // Left channel: direct mode
        auto coeffsL = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, modeFreq, modeQ);
        modesL[k].coefficients = coeffsL;
        
        // Right channel: slight detuning for stereo width
        const float detune = currentSpread * 0.004f * ((k % 2 == 0) ? 1.0f : -1.0f);
        const float modeFreqR = modeFreq * (1.0f + detune);
        auto coeffsR = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, modeFreqR, modeQ);
        modesR[k].coefficients = coeffsR;
    }
}

void SimpleResonator::process(const float* excMono, int numSamples, float* outL, float* outR)
{
    if (numSamples <= 0) return;
    
    // Clear outputs
    juce::FloatVectorOperations::clear(outL, numSamples);
    juce::FloatVectorOperations::clear(outR, numSamples);
    
    // Process each mode
    juce::HeapBlock<float> tempL, tempR;
    tempL.allocate(static_cast<size_t>(numSamples), true);
    tempR.allocate(static_cast<size_t>(numSamples), true);
    
    for (int mode = 0; mode < kNumModes; ++mode) {
        // Copy exciter to temp buffers
        juce::FloatVectorOperations::copy(tempL.getData(), excMono, numSamples);
        juce::FloatVectorOperations::copy(tempR.getData(), excMono, numSamples);
        
        // Process through mode filters
        float* channelPtrL[] = { tempL.getData() };
        float* channelPtrR[] = { tempR.getData() };
        juce::dsp::AudioBlock<float> blockL(channelPtrL, 1, static_cast<size_t>(numSamples));
        juce::dsp::AudioBlock<float> blockR(channelPtrR, 1, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> contextL(blockL);
        juce::dsp::ProcessContextReplacing<float> contextR(blockR);
        
        modesL[mode].process(contextL);
        modesR[mode].process(contextR);
        
        // Modal amplitude (lower modes louder, realistic physics)
        const float amp = 1.0f / (1.0f + mode * 0.25f);
        
        // High-mode tilt controlled by brightness
        const float brightTilt = 1.0f + (mode / static_cast<float>(kNumModes)) * currentBrightness * 1.5f;
        const float finalAmp = amp * brightTilt;
        
        // Add to output
        juce::FloatVectorOperations::addWithMultiply(outL, tempL.getData(), finalAmp, numSamples);
        juce::FloatVectorOperations::addWithMultiply(outR, tempR.getData(), finalAmp, numSamples);
    }
    
    // Normalize output (6 modes summed)
    const float normGain = 0.3f;
    juce::FloatVectorOperations::multiply(outL, normGain, numSamples);
    juce::FloatVectorOperations::multiply(outR, normGain, numSamples);
    
    // Apply stereo spread via mid/side processing
    if (currentSpread > 0.01f) {
        const float widthGain = 1.0f + currentSpread * 1.2f;
        for (int n = 0; n < numSamples; ++n) {
            const float mid = (outL[n] + outR[n]) * 0.5f;
            const float side = (outL[n] - outR[n]) * 0.5f;
            outL[n] = mid + side * widthGain;
            outR[n] = mid - side * widthGain;
        }
    }
}
