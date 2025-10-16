#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * ReduxBank - Bitcrusher/Redux DSP processor
 * 
 * Implements a classic bitcrusher effect with the following processing chain:
 * Input → Pre-Filter → Drive → Downsampling (Sample Rate + Jitter) → Bit Depth Reduction → Post-Filter → Frequency Emphasis → Mix → Output
 */
class ReduxBank
{
public:
    struct Params
    {
        float mix = 0.5f;                    // Dry/wet blend (0.0 to 1.0)
        int bitDepth = 8;                    // Bit resolution (1 to 24 bits)
        int sampleRateReduction = 1;         // Hold every Nth sample (1 to 32)
        float jitter = 0.0f;                 // Randomize held sample timing (0.0 to 1.0)
        float preFilterCutoff = 20000.0f;    // Pre-filter cutoff (20 Hz - 20 kHz)
        float postFilterCutoff = 20000.0f;   // Post-filter cutoff (20 Hz - 20 kHz)
        float drive = 1.0f;                  // Pre-saturation gain (0.0 to 10.0)
        float emphasisFreq = 0.5f;           // Frequency emphasis (0.0 to 1.0)
        
        double sampleRate = 44100.0;
    };
    
    ReduxBank() = default;
    ~ReduxBank() = default;
    
    void prepare(double sampleRate, int maxBlockSize)
    {
        p.sampleRate = sampleRate;
        
        // Prepare filters
        preFilter.prepare({ sampleRate, (juce::uint32)maxBlockSize, 1 });
        postFilter.prepare({ sampleRate, (juce::uint32)maxBlockSize, 1 });
        emphasisFilter.prepare({ sampleRate, (juce::uint32)maxBlockSize, 1 });
        
        // Reset state
        reset();
    }
    
    void reset()
    {
        holdCounter = 0;
        heldSample = 0.0f;
        preFilter.reset();
        postFilter.reset();
        emphasisFilter.reset();
    }
    
    void setParams(float mix, int bitDepth, int sampleRateReduction, float jitter,
                   float preFilterCutoff, float postFilterCutoff, float drive, float emphasisFreq)
    {
        p.mix = juce::jlimit(0.0f, 1.0f, mix);
        p.bitDepth = juce::jlimit(1, 24, bitDepth);
        p.sampleRateReduction = juce::jlimit(1, 32, sampleRateReduction);
        p.jitter = juce::jlimit(0.0f, 1.0f, jitter);
        p.preFilterCutoff = juce::jlimit(20.0f, 20000.0f, preFilterCutoff);
        p.postFilterCutoff = juce::jlimit(20.0f, 20000.0f, postFilterCutoff);
        p.drive = juce::jlimit(0.0f, 10.0f, drive);
        p.emphasisFreq = juce::jlimit(0.0f, 1.0f, emphasisFreq);
        
        updateFilters();
    }
    
    float processSample(float inputSample)
    {
        if (p.mix <= 0.0f)
            return inputSample; // Dry signal only
        
        const float dry = inputSample;
        
        // 1. Pre-Filter
        float filtered = preFilter.processSample(inputSample);
        
        // 2. Drive (saturation)
        const float driven = std::tanh(p.drive * filtered);
        
        // 3. Sample Rate Reduction with Jitter
        if (--holdCounter <= 0)
        {
            heldSample = driven;
            
            // Calculate hold counter with jitter
            int baseCounter = p.sampleRateReduction;
            if (p.jitter > 0.0f)
            {
                int jitterRange = static_cast<int>(p.jitter * 8.0f); // Max 8 samples jitter
                int offset = juce::Random::getSystemRandom().nextInt(juce::Range<int>(-jitterRange, jitterRange + 1));
                baseCounter = juce::jlimit(1, 32, baseCounter + offset);
            }
            holdCounter = baseCounter;
        }
        
        // 4. Bit Depth Reduction
        const float quantized = applyBitDepthReduction(heldSample);
        
        // 5. Post-Filter
        const float postFiltered = postFilter.processSample(quantized);
        
        // 6. Frequency Emphasis (High-pass tilt)
        const float emphasized = emphasisFilter.processSample(postFiltered);
        
        // 7. Mix dry and wet
        const float wet = emphasized;
        const float output = dry * (1.0f - p.mix) + wet * p.mix;
        
        return juce::jlimit(-0.98f, 0.98f, output);
    }
    
    void process(juce::dsp::AudioBlock<float>& block)
    {
        auto* leftChannel = block.getChannelPointer(0);
        auto* rightChannel = block.getNumChannels() > 1 ? block.getChannelPointer(1) : nullptr;
        
        const int numSamples = static_cast<int>(block.getNumSamples());
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            leftChannel[sample] = processSample(leftChannel[sample]);
            if (rightChannel)
                rightChannel[sample] = processSample(rightChannel[sample]);
        }
    }
    
private:
    Params p;
    
    // DSP state
    int holdCounter = 0;
    float heldSample = 0.0f;
    
    // Filters
    juce::dsp::IIR::Filter<float> preFilter;
    juce::dsp::IIR::Filter<float> postFilter;
    juce::dsp::IIR::Filter<float> emphasisFilter;
    
    float applyBitDepthReduction(float sample)
    {
        if (p.bitDepth >= 24)
            return sample; // No reduction
        
        // Convert to 0-1 range
        const float x01 = 0.5f * (sample + 1.0f);
        
        // Calculate quantization levels
        const int levels = (1 << p.bitDepth) - 1;
        
        // Quantize
        const float quantized01 = std::floor(x01 * levels + 0.5f) / levels;
        
        // Convert back to -1 to 1 range
        return quantized01 * 2.0f - 1.0f;
    }
    
    void updateFilters()
    {
        // Pre-filter: Low-pass before crushing
        auto preCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(p.sampleRate, p.preFilterCutoff);
        preFilter.coefficients = preCoeffs;
        
        // Post-filter: Low-pass after crushing
        auto postCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(p.sampleRate, p.postFilterCutoff);
        postFilter.coefficients = postCoeffs;
        
        // Emphasis filter: High-pass for brightness control
        // Map emphasisFreq (0-1) to cutoff (20-2000 Hz)
        const float emphasisCutoff = juce::jmap(p.emphasisFreq, 0.0f, 1.0f, 20.0f, 2000.0f);
        auto emphasisCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(p.sampleRate, emphasisCutoff);
        emphasisFilter.coefficients = emphasisCoeffs;
    }
};