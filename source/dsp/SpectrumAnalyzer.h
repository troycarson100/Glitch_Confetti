#pragma once
#include <juce_dsp/juce_dsp.h>
#include "../ui/OutputSpectrumView.h"
#include <array>
#include <cmath>

/**
 * Real-time FFT spectrum analyzer for post-FX output
 * - 4096 FFT size with 75% overlap (1024 hop)
 * - Blackman-Harris window
 * - Per-bin smoothing (40ms attack, 150ms release)
 * - Lock-free push to UI component
 */
class SpectrumAnalyzer
{
public:
    static constexpr int FFTOrder = 12; // 2^12 = 4096
    static constexpr int FFTSize = 1 << FFTOrder;
    static constexpr int HopSize = 1024; // 75% overlap
    static constexpr int NumBins = SpectrumFrame::numBins;
    
    SpectrumAnalyzer()
    {
        // Initialize FFT
        fft = std::make_unique<juce::dsp::FFT>(FFTOrder);
        
        // Pre-allocate buffers (no allocs in audio thread)
        fftBuffer.resize(FFTSize * 2, 0.0f); // Complex (real + imag)
        windowBuffer.resize(FFTSize, 0.0f);
        accumulatorBuffer.resize(FFTSize, 0.0f);
        magnitudes.fill(0.0f);
        smoothedMags.fill(-90.0f);
        
        // Build Blackman-Harris window (92 dB sidelobe rejection)
        buildBlackmanHarrisWindow();
        
        hopCounter = 0;
        accumulatorPos = 0;
    }
    
    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        
        // Calculate smoothing coefficients
        // Attack: 40ms, Release: 150ms
        float attackTime = 0.040f; // seconds
        float releaseTime = 0.150f;
        
        // Convert time constant to alpha per hop
        // alpha = 1 - exp(-hopInterval / timeConstant)
        float hopInterval = HopSize / (float)sampleRate;
        
        attackAlpha = 1.0f - std::exp(-hopInterval / attackTime);
        releaseAlpha = 1.0f - std::exp(-hopInterval / releaseTime);
        
        attackAlpha = juce::jlimit(0.0f, 1.0f, attackAlpha);
        releaseAlpha = juce::jlimit(0.0f, 1.0f, releaseAlpha);
    }
    
    void setOutputView(OutputSpectrumView* view)
    {
        outputView = view;
    }
    
    /**
     * Process a stereo buffer (post-FX output)
     * Converts to mono, accumulates for FFT, and pushes frames to UI
     */
    void processBlock(const float* const* channelData, int numChannels, int numSamples)
    {
        if (outputView == nullptr || currentSampleRate <= 0.0)
            return;
        
        // Process samples in chunks
        for (int i = 0; i < numSamples; ++i)
        {
            // Convert stereo to mono
            float mono = 0.0f;
            if (numChannels >= 2)
                mono = 0.5f * (channelData[0][i] + channelData[1][i]);
            else if (numChannels == 1)
                mono = channelData[0][i];
            
            // Accumulate into circular buffer
            accumulatorBuffer[accumulatorPos] = mono;
            accumulatorPos = (accumulatorPos + 1) % FFTSize;
            
            // Count samples for hop
            hopCounter++;
            
            // Trigger FFT at hop boundaries
            if (hopCounter >= HopSize)
            {
                hopCounter = 0;
                performFFTAndPush();
            }
        }
    }
    
private:
    void buildBlackmanHarrisWindow()
    {
        // Blackman-Harris 92 dB window coefficients
        const float a0 = 0.35875f;
        const float a1 = 0.48829f;
        const float a2 = 0.14128f;
        const float a3 = 0.01168f;
        
        for (int i = 0; i < FFTSize; ++i)
        {
            float t = i / (float)(FFTSize - 1);
            float w = a0 
                    - a1 * std::cos(2.0f * juce::MathConstants<float>::pi * t)
                    + a2 * std::cos(4.0f * juce::MathConstants<float>::pi * t)
                    - a3 * std::cos(6.0f * juce::MathConstants<float>::pi * t);
            windowBuffer[i] = w;
        }
    }
    
    void performFFTAndPush()
    {
        // Copy windowed data from circular accumulator to FFT buffer
        for (int i = 0; i < FFTSize; ++i)
        {
            int readIdx = (accumulatorPos + i) % FFTSize;
            fftBuffer[i] = accumulatorBuffer[readIdx] * windowBuffer[i];
        }
        
        // Perform FFT (in-place)
        fft->performRealOnlyForwardTransform(fftBuffer.data(), true);
        
        // Compute magnitudes and convert to dB
        const int numFFTBins = FFTSize / 2;
        
        // Build log-frequency distributed bins for display
        for (int displayBin = 0; displayBin < NumBins; ++displayBin)
        {
            // Map display bin to FFT bin range (log scale 20 Hz - 20 kHz)
            float t = displayBin / (float)(NumBins - 1);
            float freq = 20.0f * std::pow(1000.0f, t); // 20 to 20k Hz
            
            // Convert frequency to FFT bin
            float fftBin = freq * FFTSize / (float)currentSampleRate;
            int binLow = juce::jlimit(0, numFFTBins - 1, (int)fftBin);
            int binHigh = juce::jlimit(0, numFFTBins - 1, binLow + 1);
            
            // Get magnitude (take max for log-distributed bins)
            float magLow = computeBinMagnitude(binLow);
            float magHigh = computeBinMagnitude(binHigh);
            float mag = juce::jmax(magLow, magHigh);
            
            // Convert to dB
            float db = 20.0f * std::log10(mag + 1e-12f);
            db = juce::jlimit(-90.0f, 6.0f, db);
            
            magnitudes[displayBin] = db;
        }
        
        // Smooth magnitudes with attack/release
        for (int i = 0; i < NumBins; ++i)
        {
            float target = magnitudes[i];
            float current = smoothedMags[i];
            
            // Use attack if rising, release if falling
            float alpha = (target > current) ? attackAlpha : releaseAlpha;
            smoothedMags[i] += alpha * (target - current);
        }
        
        // Push frame to UI
        SpectrumFrame frame;
        frame.hostTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        for (int i = 0; i < NumBins; ++i)
        {
            frame.magsDb[i] = smoothedMags[i];
        }
        
        outputView->pushFrame(frame);
    }
    
    float computeBinMagnitude(int binIdx) const
    {
        // FFT output is [real0, imag0, real1, imag1, ...]
        float real = fftBuffer[binIdx * 2];
        float imag = fftBuffer[binIdx * 2 + 1];
        return std::sqrt(real * real + imag * imag);
    }
    
    // FFT engine
    std::unique_ptr<juce::dsp::FFT> fft;
    
    // Buffers (pre-allocated, no audio thread mallocs)
    std::vector<float> fftBuffer;
    std::vector<float> windowBuffer;
    std::vector<float> accumulatorBuffer;
    
    // Magnitude storage
    std::array<float, NumBins> magnitudes;
    std::array<float, NumBins> smoothedMags;
    
    // Processing state
    int hopCounter = 0;
    int accumulatorPos = 0;
    double currentSampleRate = 0.0;
    
    // Smoothing coefficients
    float attackAlpha = 0.3f;
    float releaseAlpha = 0.1f;
    
    // Output view (not owned)
    OutputSpectrumView* outputView = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

