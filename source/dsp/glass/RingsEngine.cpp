#include "RingsEngine.h"

// Simplified Rings-inspired modal resonator using JUCE filters
// This provides the glassy/bell-like character without the full Rings implementation

RingsEngine::RingsEngine() = default;
RingsEngine::~RingsEngine() = default;

struct RingsEngine::Impl
{
    // Modal resonator using multiple bandpass filters
    static constexpr int NUM_MODES = 6;
    std::array<juce::IIRFilter, NUM_MODES> filters;
    
    // Exciter processing
    juce::IIRFilter exciterHP;
    
    // Stereo processing
    juce::IIRFilter dcBlockerL, dcBlockerR;
    
    // Internal buffers
    std::vector<float> tempBuffer;
    
    RingsParams lastParams;
    bool prepared = false;
    
    void updateFilters(double sampleRate, const RingsParams& params)
    {
        if (!prepared) return;
        
        const float sr = static_cast<float>(sampleRate);
        const float f0 = juce::jlimit(20.0f, sr * 0.45f, params.f0Hz);
        
        // Create harmonic series with slight inharmonicity for glass character
        for (int i = 0; i < NUM_MODES; ++i) {
            // Harmonic series with slight stretch for metallic/glass sound
            float harmonic = (i + 1) * 1.02f; // 2% stretch
            float freq = f0 * harmonic;
            
            // Map damping to Q (higher damping = lower Q)
            float q = juce::jmap(params.damping, 0.0f, 1.0f, 20.0f, 2.0f);
            
            // Apply structure parameter as frequency detuning
            float detune = (params.structure - 0.5f) * 0.1f; // ±5% detune
            freq *= (1.0f + detune);
            
            // Apply brightness as Q emphasis for higher modes
            if (i >= 2) {
                q *= (1.0f + params.brightness * 0.5f);
            }
            
            freq = juce::jlimit(20.0f, sr * 0.45f, freq);
            
            filters[i].setCoefficients(juce::IIRCoefficients::makeBandPass(sr, freq, q));
        }
        
        // Exciter high-pass filter (4-6 kHz)
        exciterHP.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 5000.0f, 0.7f));
        
        // DC blockers
        dcBlockerL.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 20.0f, 0.7f));
        dcBlockerR.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 20.0f, 0.7f));
    }
};

void RingsEngine::prepare(double sr, int blockSize, int numChannels)
{
    sampleRate = sr > 0 ? sr : 48000.0;
    impl = std::make_unique<Impl>();
    
    // Initialize filters
    for (auto& filter : impl->filters) {
        filter.reset();
    }
    impl->exciterHP.reset();
    impl->dcBlockerL.reset();
    impl->dcBlockerR.reset();
    
    // Allocate temp buffer
    impl->tempBuffer.resize(blockSize);
    
    impl->prepared = true;
    impl->updateFilters(sampleRate, impl->lastParams);
}

void RingsEngine::reset()
{
    if (!impl) return;
    
    for (auto& filter : impl->filters) {
        filter.reset();
    }
    impl->exciterHP.reset();
    impl->dcBlockerL.reset();
    impl->dcBlockerR.reset();
    
    std::fill(impl->tempBuffer.begin(), impl->tempBuffer.end(), 0.0f);
}

void RingsEngine::setParams(const RingsParams& p)
{
    if (!impl) return;
    
    impl->lastParams = p;
    impl->updateFilters(sampleRate, p);
}

void RingsEngine::process(const float* excMono, int N, float* outL, float* outR)
{
    if (!impl || N <= 0 || !outL || !outR) return;
    
    const int numSamples = juce::jmin(N, static_cast<int>(impl->tempBuffer.size()));
    
    // Process exciter through high-pass filter
    for (int n = 0; n < numSamples; ++n) {
        float exc = excMono ? excMono[n] : 0.0f;
        impl->tempBuffer[n] = impl->exciterHP.processSingleSampleRaw(exc);
    }
    
    // Process through modal resonator
    for (int n = 0; n < numSamples; ++n) {
        float sum = 0.0f;
        
        // Sum all modal filters
        for (int i = 0; i < impl->NUM_MODES; ++i) {
            sum += impl->filters[i].processSingleSampleRaw(impl->tempBuffer[n]);
        }
        
        // Apply brightness as high-frequency emphasis
        float brightnessGain = 1.0f + impl->lastParams.brightness * 0.3f;
        sum *= brightnessGain;
        
        // Apply structure as inharmonicity (slight detuning)
        float structureDetune = (impl->lastParams.structure - 0.5f) * 0.05f;
        sum *= (1.0f + structureDetune);
        
        // Soft clipping for stability
        sum = std::tanh(sum * 0.7f) / 0.7f;
        
        // Stereo spread (simple pan law)
        float spread = impl->lastParams.structure; // Reuse structure for spread
        float leftGain = 0.7f + 0.3f * (1.0f - spread);
        float rightGain = 0.7f + 0.3f * spread;
        
        outL[n] = sum * leftGain;
        outR[n] = sum * rightGain;
    }
    
    // Apply DC blocking
    for (int n = 0; n < numSamples; ++n) {
        outL[n] = impl->dcBlockerL.processSingleSampleRaw(outL[n]);
        outR[n] = impl->dcBlockerR.processSingleSampleRaw(outR[n]);
    }
}
