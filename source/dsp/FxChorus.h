#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

//==============================================================================
// FxChorus - Multi-voice chorus with stereo spreading
//==============================================================================
class FxChorus
{
public:
    FxChorus() = default;
    
    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;
        
        // Smoothing for all parameters (30-50ms for click-free changes)
        const double smoothingMs = 40.0;
        const double smoothingSec = smoothingMs / 1000.0;
        
        rateSmooth.reset(sr, smoothingSec);
        depthSmooth.reset(sr, smoothingSec);
        voicesSmooth.reset(sr, smoothingSec);
        delayTimeSmooth.reset(sr, smoothingSec);
        feedbackSmooth.reset(sr, smoothingSec);
        widthSmooth.reset(sr, smoothingSec);
        toneSmooth.reset(sr, smoothingSec);
        mixSmooth.reset(sr, smoothingSec);
        
        // Initialize 4 delay lines (max delay = 100ms for safety)
        const int maxDelaySamples = (int)(sr * 0.1); // 100ms
        for (int i = 0; i < 4; ++i)
        {
            delayLines[i].resize(maxDelaySamples);
            std::fill(delayLines[i].begin(), delayLines[i].end(), 0.0f);
            writePos[i] = 0;
            lfoPhase[i] = i * 0.25; // Phase offsets: 0°, 90°, 180°, 270°
        }
        
        // Tone filter (simple one-pole for tilt)
        toneFilterL = 0.0f;
        toneFilterR = 0.0f;
    }
    
    void setTargets(float rateHz, float depthPercent, float numVoices, float delayMs, 
                    float feedbackPercent, float widthPercent, float tone, float mixPercent)
    {
        rateSmooth.setTargetValue(juce::jlimit(0.1f, 10.0f, rateHz));
        depthSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, depthPercent / 100.0f));
        voicesSmooth.setTargetValue(juce::jlimit(1.0f, 4.0f, numVoices));
        delayTimeSmooth.setTargetValue(juce::jlimit(5.0f, 50.0f, delayMs));
        feedbackSmooth.setTargetValue(juce::jlimit(0.0f, 0.8f, feedbackPercent / 100.0f));
        widthSmooth.setTargetValue(juce::jlimit(0.0f, 2.0f, widthPercent / 100.0f));
        toneSmooth.setTargetValue(juce::jlimit(-1.0f, 1.0f, tone));
        mixSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, mixPercent / 100.0f));
    }
    
    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        
        if (numChannels == 0 || numSamples == 0 || sr <= 0.0) return;
        
        auto* L = buffer.getWritePointer(0);
        auto* R = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
        
        for (int n = 0; n < numSamples; ++n)
        {
            // Get smoothed parameters
            const float rate = rateSmooth.getNextValue();
            const float depth = depthSmooth.getNextValue();
            const float voices = voicesSmooth.getNextValue();
            const float baseDelay = delayTimeSmooth.getNextValue();
            const float feedback = feedbackSmooth.getNextValue();
            const float width = widthSmooth.getNextValue();
            const float tone = toneSmooth.getNextValue();
            const float mix = mixSmooth.getNextValue();
            
            // Input
            const float inL = L[n];
            const float inR = R ? R[n] : inL;
            const float mono = (inL + inR) * 0.5f; // Mono sum for chorus input
            
            // Determine how many voices to use
            const int numVoices = juce::jlimit(1, 4, (int)std::round(voices));
            
            // Accumulate chorus output
            float chorusL = 0.0f;
            float chorusR = 0.0f;
            
            for (int v = 0; v < numVoices; ++v)
            {
                // Update LFO for this voice
                lfoPhase[v] += rate / sr;
                if (lfoPhase[v] >= 1.0) lfoPhase[v] -= 1.0;
                
                const float lfo = std::sin(lfoPhase[v] * juce::MathConstants<float>::twoPi);
                
                // Modulated delay time (base delay ± depth)
                const float depthMs = depth * 2.5f; // Max modulation range ±2.5ms
                const float modDelayMs = baseDelay + (lfo * depthMs);
                const float modDelaySamples = (modDelayMs / 1000.0f) * (float)sr;
                
                // Read from delay line with interpolation
                const int delayInt = (int)modDelaySamples;
                const float delayFrac = modDelaySamples - delayInt;
                
                const int readPos1 = (writePos[v] - delayInt - 1 + (int)delayLines[v].size()) % (int)delayLines[v].size();
                const int readPos2 = (readPos1 - 1 + (int)delayLines[v].size()) % (int)delayLines[v].size();
                
                const float sample1 = delayLines[v][readPos1];
                const float sample2 = delayLines[v][readPos2];
                const float delayOut = sample1 + delayFrac * (sample2 - sample1); // Linear interpolation
                
                // Write to delay line with feedback
                const float feedbackSample = mono + (delayOut * feedback);
                const float clippedFeedback = juce::jlimit(-1.0f, 1.0f, feedbackSample); // Prevent runaway
                delayLines[v][writePos[v]] = clippedFeedback;
                writePos[v] = (writePos[v] + 1) % (int)delayLines[v].size();
                
                // Route voices to L/R based on voice index
                // Voice 0,2 → Left, Voice 1,3 → Right
                if (v % 2 == 0) {
                    chorusL += delayOut;
                } else {
                    chorusR += delayOut;
                }
            }
            
            // Normalize by number of voices and apply width
            const float voiceGain = 1.0f / std::sqrt((float)numVoices); // Energy compensation
            chorusL *= voiceGain;
            chorusR *= voiceGain;
            
            // Stereo width control (0 = mono, 1 = normal, 2 = ultra-wide)
            const float mid = (chorusL + chorusR) * 0.5f;
            const float side = (chorusL - chorusR) * 0.5f * width;
            chorusL = mid + side;
            chorusR = mid - side;
            
            // Tone control (simple one-pole tilt filter)
            const float toneCoeff = 0.1f; // Gentle filtering
            if (tone < 0.0f) {
                // Darken (low-pass)
                const float lpAmount = -tone;
                toneFilterL = toneFilterL + toneCoeff * (chorusL - toneFilterL);
                toneFilterR = toneFilterR + toneCoeff * (chorusR - toneFilterR);
                chorusL = juce::jmap(lpAmount, chorusL, toneFilterL);
                chorusR = juce::jmap(lpAmount, chorusR, toneFilterR);
            } else if (tone > 0.0f) {
                // Brighten (high-pass approximation via reduced LP)
                toneFilterL = toneFilterL + toneCoeff * (chorusL - toneFilterL);
                toneFilterR = toneFilterR + toneCoeff * (chorusR - toneFilterR);
                const float hpL = chorusL - toneFilterL * tone;
                const float hpR = chorusR - toneFilterR * tone;
                chorusL = chorusL + hpL * 0.5f; // Boost highs
                chorusR = chorusR + hpR * 0.5f;
            }
            
            // Dry/wet mix (equal-power-ish crossfade)
            L[n] = juce::jmap(mix, inL, chorusL);
            if (R) R[n] = juce::jmap(mix, inR, chorusR);
        }
    }
    
private:
    double sr = 44100.0;
    
    // Smoothed parameters
    juce::SmoothedValue<float> rateSmooth;
    juce::SmoothedValue<float> depthSmooth;
    juce::SmoothedValue<float> voicesSmooth;
    juce::SmoothedValue<float> delayTimeSmooth;
    juce::SmoothedValue<float> feedbackSmooth;
    juce::SmoothedValue<float> widthSmooth;
    juce::SmoothedValue<float> toneSmooth;
    juce::SmoothedValue<float> mixSmooth;
    
    // Delay lines (4 voices max)
    std::array<std::vector<float>, 4> delayLines;
    std::array<int, 4> writePos {0, 0, 0, 0};
    std::array<double, 4> lfoPhase {0.0, 0.25, 0.5, 0.75}; // Phase-offset LFOs
    
    // Tone filter state
    float toneFilterL = 0.0f;
    float toneFilterR = 0.0f;
};

