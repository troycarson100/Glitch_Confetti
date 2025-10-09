#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

/**
 * Lock-free spectrum frame for audio→UI communication
 */
struct SpectrumFrame
{
    static constexpr int numBins = 512; // Log-distributed bins for display
    double hostTime = 0.0;
    std::array<float, numBins> magsDb{};
    
    SpectrumFrame() 
    {
        magsDb.fill(-90.0f);
    }
};

/**
 * Real-time spectrum analyzer with trailing echo lines
 * Displays post-FX output with:
 * - Log-frequency bars/curve from bass→mid→high
 * - Glowing fill gradient
 * - Multiple fading trail lines for "echo" effect
 */
class OutputSpectrumView : public juce::Component,
                           private juce::Timer
{
public:
    static constexpr int MaxTrails = 8;
    static constexpr int ViewBins = 512;
    
    OutputSpectrumView()
    {
        startTimerHz(60); // 60 FPS for smooth animation
        
        // Initialize trail buffer
        for (auto& trail : trailFrames)
        {
            trail.magsDb.fill(-90.0f);
        }
        currentFrame.magsDb.fill(-90.0f);
    }
    
    ~OutputSpectrumView() override
    {
        stopTimer();
    }
    
    /**
     * Push a new spectrum frame from the audio thread
     * Thread-safe: uses atomic index
     */
    void pushFrame(const SpectrumFrame& frame)
    {
        int writePos = writeIndex.load(std::memory_order_relaxed);
        fifo[writePos] = frame;
        writeIndex.store((writePos + 1) % FifoSize, std::memory_order_release);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // === BACKGROUND ===
        // Dark charcoal with soft vignette
        juce::ColourGradient bgGradient(
            juce::Colour(0xFF1A1A1A), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour(0xFF0F0F0F), bounds.getCentreX(), bounds.getBottom(),
            true
        );
        g.setGradientFill(bgGradient);
        g.fillRect(bounds);
        
        // === DRAWING BOUNDS with margins to prevent cutoff ===
        // Add generous margins to keep all content within the visible frame
        const float leftMargin = 8.0f;
        const float rightMargin = 8.0f;
        const float topMargin = 20.0f;
        const float bottomMargin = 12.0f;
        
        auto drawBounds = bounds.reduced(leftMargin, topMargin);
        drawBounds = drawBounds.withTrimmedRight(rightMargin - leftMargin);
        drawBounds = drawBounds.withTrimmedBottom(bottomMargin - topMargin);
        
        // === GRID ===
        drawGrid(g, drawBounds);
        
        // === TRAIL ECHOES (oldest first) ===
        const std::array<float, MaxTrails> trailAlphas = {0.03f, 0.05f, 0.08f, 0.12f, 0.18f, 0.25f, 0.35f, 0.5f};
        
        for (int i = 0; i < MaxTrails; ++i)
        {
            int trailIdx = (currentTrailIndex - MaxTrails + i + MaxTrails) % MaxTrails;
            const auto& trail = trailFrames[trailIdx];
            
            float alpha = trailAlphas[i];
            float verticalOffset = (MaxTrails - i) * -1.5f; // Slight vertical decay
            
            drawSpectrumCurve(g, drawBounds, trail, alpha, verticalOffset, false);
        }
        
        // === LIVE CURVE (on top) ===
        drawSpectrumCurve(g, drawBounds, currentFrame, 1.0f, 0.0f, true);
    }
    
    void resized() override
    {
        // Component resized - frequency mapping will recalculate on next paint
    }
    
private:
    void timerCallback() override
    {
        // Pull new frames from FIFO
        bool gotNewFrame = false;
        int readPos = readIndex.load(std::memory_order_relaxed);
        int writePos = writeIndex.load(std::memory_order_acquire);
        
        while (readPos != writePos)
        {
            currentFrame = fifo[readPos];
            readIndex.store((readPos + 1) % FifoSize, std::memory_order_release);
            gotNewFrame = true;
            readPos = readIndex.load(std::memory_order_relaxed);
            writePos = writeIndex.load(std::memory_order_acquire);
        }
        
        // Push current frame into trail buffer
        if (gotNewFrame)
        {
            trailFrames[currentTrailIndex] = currentFrame;
            currentTrailIndex = (currentTrailIndex + 1) % MaxTrails;
        }
        
        // Always repaint for smooth trails
        repaint();
    }
    
    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        g.setColour(juce::Colour(0x10FFFFFF));
        
        // Vertical frequency rulers
        const std::array<float, 9> freqs = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f};
        for (float freq : freqs)
        {
            float x = freqToX(freq, bounds);
            g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        }
        
        // Horizontal dB lines
        const std::array<float, 5> dbLevels = {-60.0f, -30.0f, -12.0f, -6.0f, 0.0f};
        for (float db : dbLevels)
        {
            float y = dbToY(db, bounds);
            g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
        }
    }
    
    void drawSpectrumCurve(juce::Graphics& g, const juce::Rectangle<float>& bounds, 
                          const SpectrumFrame& frame, float alpha, float yOffset, bool withFill)
    {
        juce::Path curvePath;
        bool firstPoint = true;
        
        const float width = bounds.getWidth();
        const int numPoints = juce::jmin((int)width, ViewBins);
        
        // Build smooth curve through spectrum data
        for (int i = 0; i < numPoints; ++i)
        {
            float t = i / (float)(numPoints - 1);
            float freq = 20.0f * std::pow(1000.0f, t); // 20 Hz to 20 kHz log scale
            
            // Find bin for this frequency
            int binIdx = (int)(t * (ViewBins - 1));
            binIdx = juce::jlimit(0, ViewBins - 1, binIdx);
            
            float mag = frame.magsDb[binIdx];
            float x = bounds.getX() + i;
            float y = dbToY(mag, bounds) + yOffset;
            
            if (firstPoint)
            {
                curvePath.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                curvePath.lineTo(x, y);
            }
        }
        
        // Draw fill if requested (for live curve)
        if (withFill && !curvePath.isEmpty())
        {
            juce::Path fillPath = curvePath;
            fillPath.lineTo(bounds.getRight(), bounds.getBottom());
            fillPath.lineTo(bounds.getX(), bounds.getBottom());
            fillPath.closeSubPath();
            
            juce::ColourGradient fillGradient(
                juce::Colours::white.withAlpha(alpha * 0.3f), 
                bounds.getCentreX(), bounds.getY(),
                juce::Colour(0xFF4A9FD8).withAlpha(alpha * 0.1f), 
                bounds.getCentreX(), bounds.getBottom(),
                false
            );
            g.setGradientFill(fillGradient);
            g.fillPath(fillPath);
        }
        
        // Draw the curve stroke (more defined with sharper stroke)
        g.setColour(juce::Colours::white.withAlpha(alpha * 0.95f));
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Add glow layer (slightly more prominent)
        g.setColour(juce::Colours::white.withAlpha(alpha * 0.4f));
        g.strokePath(curvePath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    float freqToX(float freq, const juce::Rectangle<float>& bounds) const
    {
        // Log scale: 20 Hz to 20 kHz
        float logFreq = std::log10(juce::jlimit(20.0f, 20000.0f, freq));
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float t = (logFreq - logMin) / (logMax - logMin);
        return bounds.getX() + t * bounds.getWidth();
    }
    
    float dbToY(float db, const juce::Rectangle<float>& bounds) const
    {
        // -90 dB at bottom, with generous visual headroom to prevent hitting top
        // Scale down the visual amplitude by using larger dB range
        // This is VISUAL ONLY - doesn't affect audio
        // Margins are now handled in paint(), so use full bounds height
        
        // Use 140 dB range to compress the display more (prevents hitting top)
        float t = juce::jlimit(0.0f, 1.0f, (db + 90.0f) / 140.0f); // Much wider range = more compressed visually
        t = std::pow(t, 0.7f); // Even more compression to keep peaks lower
        
        // Scale down to 80% of available height for extra safety
        return bounds.getBottom() - (t * bounds.getHeight() * 0.8f);
    }
    
    // Lock-free FIFO for audio→UI communication
    static constexpr int FifoSize = 32;
    std::array<SpectrumFrame, FifoSize> fifo;
    std::atomic<int> writeIndex{0};
    std::atomic<int> readIndex{0};
    
    // Current and trail frames
    SpectrumFrame currentFrame;
    std::array<SpectrumFrame, MaxTrails> trailFrames;
    int currentTrailIndex = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSpectrumView)
};

