#include "StkModalBarEngine.h"

void StkModalBarEngine::prepare(double sr, int /*blockSize*/, int /*numChannels*/)
{
    sampleRate = (sr > 0 ? sr : 48000.0);
    
    // Initialize all mode filters
    for (int i = 0; i < kNumModes; ++i) {
        modesL[i].reset();
        modesR[i].reset();
    }
    
    // Set default parameters
    StkModalParams defaultParams;
    setParams(defaultParams);
    
    reset();
    
    DBG("[Glass] StkModalBarEngine prepared sr=" << sampleRate);
}

void StkModalBarEngine::reset()
{
    for (int i = 0; i < kNumModes; ++i) {
        modesL[i].reset();
        modesR[i].reset();
    }
}

void StkModalBarEngine::setParams(const StkModalParams& p)
{
    const float f0 = juce::jlimit(20.f, 12000.f, p.f0Hz);
    const float bright = juce::jlimit(0.f, 1.f, p.brightness);
    const float damp = juce::jlimit(0.f, 1.f, p.damping);
    const float sprd = juce::jlimit(0.f, 1.f, p.spread);
    
    // Only update if parameters changed significantly
    if (std::abs(f0 - currentF0) > 0.5f ||
        std::abs(bright - currentBrightness) > 0.01f ||
        std::abs(damp - currentDamping) > 0.01f ||
        std::abs(sprd - currentSpread) > 0.01f)
    {
        currentF0 = f0;
        currentBrightness = bright;
        currentDamping = damp;
        currentSpread = sprd;
        updateModes();
    }
}

void StkModalBarEngine::updateModes()
{
    // Modal frequency ratios for glass/metal bar (inharmonic)
    // Based on research into struck bar physics and STK's ModalBar
    // Brightness shifts the ratios upward (brighter = more high partials)
    const float brightShift = 1.0f + currentBrightness * 2.0f;
    
    const float ratios[kNumModes] = {
        1.000f * brightShift,   // Fundamental
        2.572f * brightShift,   // Inharmonic partial
        4.644f * brightShift,   // Inharmonic partial  
        6.984f * brightShift,   // Inharmonic partial
        9.723f * brightShift,   // Inharmonic partial
        12.14f * brightShift,   // Inharmonic partial
        15.42f * brightShift,   // Inharmonic partial
        18.98f * brightShift    // Inharmonic partial
    };
    
    // Q factor controls decay time
    // Higher damping = lower Q = shorter decay
    // Map damping [0..1] → Q [300..15]
    const float baseQ = juce::jmap(currentDamping, 0.f, 1.f, 300.f, 15.f);
    
    // Set up resonant band-pass filters at modal frequencies
    for (int i = 0; i < kNumModes; ++i) {
        const float freq = juce::jlimit(20.f, 
                                       static_cast<float>(sampleRate * 0.48), 
                                       currentF0 * ratios[i]);
        
        // Higher modes decay faster (realistic physics)
        const float q = baseQ / (1.0f + i * 0.2f);
        
        // Left channel
        auto coeffsL = juce::IIRCoefficients::makeBandPass(sampleRate, freq, q);
        modesL[i].setCoefficients(coeffsL);
        
        // Right channel with slight detuning for stereo width
        const float detune = currentSpread * 0.003f * (i % 2 == 0 ? 1.0f : -1.0f);
        const float freqR = freq * (1.0f + detune);
        auto coeffsR = juce::IIRCoefficients::makeBandPass(sampleRate, freqR, q);
        modesR[i].setCoefficients(coeffsR);
    }
}

void StkModalBarEngine::process(const float* excMono, int numSamples, float* outL, float* outR)
{
    if (numSamples <= 0) return;
    
    // Clear output buffers
    juce::FloatVectorOperations::clear(outL, numSamples);
    juce::FloatVectorOperations::clear(outR, numSamples);
    
    // Process through each mode and sum
    juce::HeapBlock<float> tempL, tempR;
    tempL.allocate(static_cast<size_t>(numSamples), true);
    tempR.allocate(static_cast<size_t>(numSamples), true);
    
    for (int mode = 0; mode < kNumModes; ++mode) {
        // Copy exciter to temp buffers
        juce::FloatVectorOperations::copy(tempL.getData(), excMono, numSamples);
        juce::FloatVectorOperations::copy(tempR.getData(), excMono, numSamples);
        
        // Filter through this mode
        modesL[mode].processSamples(tempL.getData(), numSamples);
        modesR[mode].processSamples(tempR.getData(), numSamples);
        
        // Modal amplitude envelope (lower modes louder, like real bars)
        const float amp = 1.0f / (1.0f + mode * 0.3f);
        
        // Add to output
        juce::FloatVectorOperations::addWithMultiply(outL, tempL.getData(), amp, numSamples);
        juce::FloatVectorOperations::addWithMultiply(outR, tempR.getData(), amp, numSamples);
    }
    
    // Normalize to prevent clipping (8 modes summed)
    const float normGain = 0.25f;
    juce::FloatVectorOperations::multiply(outL, normGain, numSamples);
    juce::FloatVectorOperations::multiply(outR, normGain, numSamples);
    
    // Apply stereo width
    if (currentSpread > 0.01f) {
        // Mid/side processing for width control
        for (int n = 0; n < numSamples; ++n) {
            const float mid = (outL[n] + outR[n]) * 0.5f;
            const float side = (outL[n] - outR[n]) * 0.5f;
            const float widthGain = 1.0f + currentSpread * 1.5f;
            outL[n] = mid + side * widthGain;
            outR[n] = mid - side * widthGain;
        }
    }
}



