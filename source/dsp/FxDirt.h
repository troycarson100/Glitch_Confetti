#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * FxDirt - Musical saturation/distortion with color, asymmetry, and texture
 * Processing order: Pre-EQ → Drive → Bias → Waveshaper → De-bias → Post-tone → Dry/Wet
 */
class FxDirt
{
public:
    FxDirt() = default;
    
    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;
        
        // Smoothing for all parameters (30ms to avoid zipper)
        const double smoothMs = 30.0;
        driveSmooth.reset(sr, smoothMs / 1000.0);
        colorSmooth.reset(sr, smoothMs / 1000.0);
        asymSmooth.reset(sr, smoothMs / 1000.0);
        textureSmooth.reset(sr, smoothMs / 1000.0);
        lowCutSmooth.reset(sr, smoothMs / 1000.0);
        highCutSmooth.reset(sr, smoothMs / 1000.0);
        toneSmooth.reset(sr, smoothMs / 1000.0);
        mixSmooth.reset(sr, smoothMs / 1000.0);
        
        // Prepare filters
        for (int ch = 0; ch < 2; ++ch)
        {
            lowCutFilters[ch].setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 60.0));
            highCutFilters[ch].setCoefficients(juce::IIRCoefficients::makeLowPass(sr, 12000.0));
            colorFilters[ch].setCoefficients(juce::IIRCoefficients::makeHighShelf(sr, 1000.0, 0.707, 1.0));
            toneFilters[ch].setCoefficients(juce::IIRCoefficients::makeHighShelf(sr, 1500.0, 0.707, 1.0));
        }
        
        // DC blocker (to remove any residual DC after processing)
        dcBlocker.prepare({sr, (juce::uint32)samplesPerBlock, 2});
        *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 20.0);
    }
    
    void setTargets(float driveDb, float color, float asym, float texture,
                   float lowCutHz, float highCutHz, float tone, float mix)
    {
        driveSmooth.setTargetValue(juce::Decibels::decibelsToGain(juce::jlimit(0.0f, 36.0f, driveDb)));
        colorSmooth.setTargetValue(juce::jlimit(-1.0f, 1.0f, color));
        asymSmooth.setTargetValue(juce::jlimit(-1.0f, 1.0f, asym));
        textureSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, texture));
        lowCutSmooth.setTargetValue(juce::jlimit(20.0f, 300.0f, lowCutHz));
        highCutSmooth.setTargetValue(juce::jlimit(3000.0f, 22000.0f, highCutHz));
        toneSmooth.setTargetValue(juce::jlimit(-1.0f, 1.0f, tone));
        mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mix));
    }
    
    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        
        if (numSamples == 0 || numChannels == 0)
            return;
        
        // Store dry signal for mix
        juce::AudioBuffer<float> dryBuffer(numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        
        auto* L = buffer.getWritePointer(0);
        auto* R = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
        
        for (int n = 0; n < numSamples; ++n)
        {
            // Get smoothed parameters
            const float drive = driveSmooth.getNextValue();
            const float color = colorSmooth.getNextValue();
            const float asym = asymSmooth.getNextValue();
            const float texture = textureSmooth.getNextValue();
            const float lowCut = lowCutSmooth.getNextValue();
            const float highCut = highCutSmooth.getNextValue();
            const float tone = toneSmooth.getNextValue();
            const float mix = mixSmooth.getNextValue();
            
            // Update filter coefficients if needed (check every 64 samples)
            if (n % 64 == 0)
            {
                updateFilters(lowCut, highCut, color, tone);
            }
            
            // Process left channel
            float sampleL = L[n];
            sampleL = processChannel(sampleL, 0, drive, color, asym, texture, tone);
            L[n] = sampleL;
            
            // Process right channel
            if (R != nullptr)
            {
                float sampleR = R[n];
                sampleR = processChannel(sampleR, 1, drive, color, asym, texture, tone);
                R[n] = sampleR;
            }
        }
        
        // Apply DC blocker
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        dcBlocker.process(context);
        
        // Dry/wet mix (true crossfade)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float mixVal = mixSmooth.getCurrentValue();
                buffer.setSample(ch, n, juce::jmap(mixVal, dryBuffer.getSample(ch, n), buffer.getSample(ch, n)));
            }
        }
    }
    
private:
    float processChannel(float sample, int channelIndex, float drive, float color, 
                        float asym, float texture, float tone)
    {
        // 1. Pre-EQ color tilt
        sample = colorFilters[channelIndex].processSingleSampleRaw(sample);
        
        // 2. Low-cut and high-cut
        sample = lowCutFilters[channelIndex].processSingleSampleRaw(sample);
        sample = highCutFilters[channelIndex].processSingleSampleRaw(sample);
        
        // 3. Apply drive
        sample *= drive;
        
        // 4. Add asymmetry bias (±0.15 max for musical effect)
        const float bias = asym * 0.15f;
        sample += bias;
        
        // 5. Waveshaper with texture morphing
        sample = applyWaveshaper(sample, texture);
        
        // 6. Remove bias to re-center (keeps harmonic asymmetry without DC)
        sample -= bias;
        
        // 7. Post-tone tilt
        sample = toneFilters[channelIndex].processSingleSampleRaw(sample);
        
        // 8. Soft safety limiter (tanh ceiling, not the main effect)
        sample = std::tanh(sample * 0.8f) / 0.8f;
        
        return sample;
    }
    
    float applyWaveshaper(float x, float texture)
    {
        // Morph between soft (warm) → medium (edgy) → hard (gnarly)
        // texture: 0.0 = soft tanh, 0.5 = arctan, 1.0 = soft clip
        
        if (texture < 0.5f)
        {
            // Blend tanh → arctan
            const float t = texture * 2.0f; // 0..1
            const float soft = std::tanh(x * 1.5f) / 1.5f;
            const float medium = (2.0f / juce::MathConstants<float>::pi) * std::atan(x * 2.0f);
            return juce::jmap(t, soft, medium);
        }
        else
        {
            // Blend arctan → soft clip
            const float t = (texture - 0.5f) * 2.0f; // 0..1
            const float medium = (2.0f / juce::MathConstants<float>::pi) * std::atan(x * 2.0f);
            const float hard = juce::jlimit(-0.95f, 0.95f, x * 1.2f);
            return juce::jmap(t, medium, hard);
        }
    }
    
    void updateFilters(float lowCutHz, float highCutHz, float color, float tone)
    {
        // Update filter coefficients (called periodically, not every sample)
        for (int ch = 0; ch < 2; ++ch)
        {
            lowCutFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeHighPass(sr, lowCutHz));
            
            highCutFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeLowPass(sr, highCutHz));
            
            // Color tilt: negative = darker (cut highs), positive = brighter (boost highs)
            const float colorGain = juce::Decibels::decibelsToGain(color * 6.0f); // ±6 dB
            colorFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeHighShelf(sr, 1000.0, 0.707, colorGain));
            
            // Tone tilt: post-shaper tilt
            const float toneGain = juce::Decibels::decibelsToGain(tone * 4.0f); // ±4 dB
            toneFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeHighShelf(sr, 1500.0, 0.707, toneGain));
        }
    }
    
    double sr = 44100.0;
    
    // Smoothed parameters
    juce::SmoothedValue<float> driveSmooth;
    juce::SmoothedValue<float> colorSmooth;
    juce::SmoothedValue<float> asymSmooth;
    juce::SmoothedValue<float> textureSmooth;
    juce::SmoothedValue<float> lowCutSmooth;
    juce::SmoothedValue<float> highCutSmooth;
    juce::SmoothedValue<float> toneSmooth;
    juce::SmoothedValue<float> mixSmooth;
    
    // Filters (stereo)
    juce::IIRFilter lowCutFilters[2];
    juce::IIRFilter highCutFilters[2];
    juce::IIRFilter colorFilters[2];
    juce::IIRFilter toneFilters[2];
    
    // DC blocker
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlocker;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxDirt)
};

