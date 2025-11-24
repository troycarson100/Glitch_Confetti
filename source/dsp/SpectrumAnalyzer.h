#pragma once
#include <juce_dsp/juce_dsp.h>
#include "../ui/OutputSpectrumView.h"
#include <array>
#include <cmath>
#include <atomic> // Fix: track analyzer active state safely
// Forward declaration for shutdown flag access
class PluginEditor;

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
        
        // Initialize filters to bypass (all-pass)
        lowCutFreq.store(20.0f);
        highCutFreq.store(20000.0f);
        hpB0 = 1.0f; hpB1 = 0.0f; hpB2 = 0.0f;
        hpA1 = 0.0f; hpA2 = 0.0f;
        hpX1 = 0.0f; hpX2 = 0.0f; hpY1 = 0.0f; hpY2 = 0.0f;
        lpB0 = 1.0f; lpB1 = 0.0f; lpB2 = 0.0f;
        lpA1 = 0.0f; lpA2 = 0.0f;
        lpX1 = 0.0f; lpX2 = 0.0f; lpY1 = 0.0f; lpY2 = 0.0f;
        
        hopCounter = 0;
        accumulatorPos = 0;
        isActive.store(true); // Fix: track whether analyzer should run
    }
    
    ~SpectrumAnalyzer()
    {
        disable(); // Fix: guarantee async callbacks can't outlive this object
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
        
        // Update filter coefficients
        updateFilters();
    }
    
    void setOutputView(OutputSpectrumView* view)
    {
        outputView = view;
    }
    
    void setFilterFrequencies(float lowCut, float highCut)
    {
        if (!isActive.load()) return; // Fix: ignore updates once analyzer is torn down
        
        lowCutFreq.store(lowCut);
        highCutFreq.store(highCut);
        
        // Update filter coefficients on message thread (safe)
        // Only call async if MessageManager exists and object is still active
        auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        if (mm != nullptr && isActive.load()) {
            mm->callAsync([this]() {
                if (isActive.load()) { // Double-check before executing
                    updateFilters();
                }
            });
        }
    }
    
    void disable()
    {
        isActive.store(false);      // Fix: gate processBlock/setFilterFrequencies during shutdown
        outputView = nullptr;       // Fix: drop UI pointer so no repaint occurs post-destruction
    }
    
    /**
     * Process a stereo buffer (post-FX output)
     * Creates filtered copy for analysis only (doesn't modify output audio)
     */
    void processBlock(const float* const* channelData, int numChannels, int numSamples)
    {
        if (!isActive.load() || outputView == nullptr || currentSampleRate <= 0.0)
            return;
        
        // Process samples for FFT (with optional filtering for visualization only)
        for (int i = 0; i < numSamples; ++i)
        {
            // Convert stereo to mono
            float mono = 0.0f;
            if (numChannels >= 2)
                mono = 0.5f * (channelData[0][i] + channelData[1][i]);
            else if (numChannels == 1)
                mono = channelData[0][i];
            
            // Apply filters to the mono signal for visualization
            // This doesn't affect the output audio, only what we analyze
            mono = applyFiltersToSample(mono);
            
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
            
            // Convert to dB (increased ceiling to +24 dB for headroom)
            float db = 20.0f * std::log10(mag + 1e-12f);
            
            // Safety check: prevent NaN/infinity values that could cause white spectrum
            if (!std::isfinite(db)) {
                db = -90.0f; // Silent
            }
            
            db = juce::jlimit(-90.0f, 24.0f, db);
            
            // Apply filter attenuation (12 dB/oct slopes)
            float filterGain = calculateFilterGain(freq);
            db += filterGain; // Subtract attenuation in dB
            
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
    
    void updateFilters()
    {
        if (currentSampleRate <= 0.0) return;
        
        // Update filter coefficients (simple biquad)
        float lowCut = lowCutFreq.load();
        float highCut = highCutFreq.load();
        
        // Calculate biquad coefficients for highpass (12dB/oct = 2-pole)
        if (lowCut > 20.0f)
        {
            float w0 = 2.0f * juce::MathConstants<float>::pi * lowCut / (float)currentSampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * 0.707f); // Q = 0.707 (Butterworth)
            
            float a0 = 1.0f + alpha;
            hpB0 = (1.0f + cosw0) / (2.0f * a0);
            hpB1 = -(1.0f + cosw0) / a0;
            hpB2 = (1.0f + cosw0) / (2.0f * a0);
            hpA1 = -2.0f * cosw0 / a0;
            hpA2 = (1.0f - alpha) / a0;
        }
        else
        {
            // Bypass (all-pass)
            hpB0 = 1.0f; hpB1 = 0.0f; hpB2 = 0.0f;
            hpA1 = 0.0f; hpA2 = 0.0f;
        }
        
        // Calculate biquad coefficients for lowpass
        if (highCut < 20000.0f)
        {
            float w0 = 2.0f * juce::MathConstants<float>::pi * highCut / (float)currentSampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * 0.707f); // Q = 0.707 (Butterworth)
            
            float a0 = 1.0f + alpha;
            lpB0 = (1.0f - cosw0) / (2.0f * a0);
            lpB1 = (1.0f - cosw0) / a0;
            lpB2 = (1.0f - cosw0) / (2.0f * a0);
            lpA1 = -2.0f * cosw0 / a0;
            lpA2 = (1.0f - alpha) / a0;
        }
        else
        {
            // Bypass (all-pass)
            lpB0 = 1.0f; lpB1 = 0.0f; lpB2 = 0.0f;
            lpA1 = 0.0f; lpA2 = 0.0f;
        }
    }
    
    float applyFiltersToSample(float input)
    {
        // Apply highpass filter (biquad)
        float hpOut = hpB0 * input + hpB1 * hpX1 + hpB2 * hpX2 - hpA1 * hpY1 - hpA2 * hpY2;
        hpX2 = hpX1; hpX1 = input;
        hpY2 = hpY1; hpY1 = hpOut;
        
        // Apply lowpass filter (biquad) to highpass output
        float lpOut = lpB0 * hpOut + lpB1 * lpX1 + lpB2 * lpX2 - lpA1 * lpY1 - lpA2 * lpY2;
        lpX2 = lpX1; lpX1 = hpOut;
        lpY2 = lpY1; lpY1 = lpOut;
        
        return lpOut;
    }
    
    float calculateFilterGain(float freq) const
    {
        // 12 dB/octave slopes for LP/HP filters (for visual display only)
        const float lowCut = lowCutFreq.load();
        const float highCut = highCutFreq.load();
        
        float gain = 0.0f; // dB
        
        // Highpass: -12dB/octave below lowCut
        if (freq < lowCut)
        {
            float octavesBelow = std::log2(lowCut / juce::jmax(1.0f, freq));
            gain -= octavesBelow * 12.0f;
        }
        
        // Lowpass: -12dB/octave above highCut
        if (freq > highCut)
        {
            float octavesAbove = std::log2(freq / highCut);
            gain -= octavesAbove * 12.0f;
        }
        
        return juce::jmax(-90.0f, gain); // Don't go below noise floor
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
    
    // Filter frequencies (atomic for thread-safe UI control)
    std::atomic<float> lowCutFreq{20.0f};
    std::atomic<float> highCutFreq{20000.0f};
    
    // Active flag to prevent async callbacks during destruction
    std::atomic<bool> isActive{true};
    
    // Biquad filter coefficients (for visualization filtering only)
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f;
    float hpA1 = 0.0f, hpA2 = 0.0f;
    float hpX1 = 0.0f, hpX2 = 0.0f, hpY1 = 0.0f, hpY2 = 0.0f;
    
    float lpB0 = 1.0f, lpB1 = 0.0f, lpB2 = 0.0f;
    float lpA1 = 0.0f, lpA2 = 0.0f;
    float lpX1 = 0.0f, lpX2 = 0.0f, lpY1 = 0.0f, lpY2 = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

