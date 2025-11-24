#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
// Forward declaration removed - not needed

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
     * Set whether formant filter overlay should be displayed
     */
    void setFormantOverlayEnabled(bool enabled) { formantOverlayEnabled = enabled; }
    
    /**
     * Set formant filter parameters for overlay calculation
     */
    void setFormantParameters(float f1Hz, float f2Hz, float f3Hz, float q, float emphasisDb)
    {
        formantF1 = f1Hz;
        formantF2 = f2Hz;
        formantF3 = f3Hz;
        formantQ = q;
        formantEmphasisDb = emphasisDb;
    }
    
    // Callback for dragging formant peaks
    std::function<void(int formantIndex, float newFreq, float newQ)> onFormantDragged;
    
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
        
        // No background - transparent
        // Background removed to show underlying UI
        
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
        
        // === FORMANT FILTER OVERLAY (when enabled) ===
        if (formantOverlayEnabled)
        {
            drawFormantOverlay(g, drawBounds);
        }
    }
    
    void resized() override
    {
        // Component resized - frequency mapping will recalculate on next paint
    }
    
private:
    void timerCallback() override
    {
        // Fix: Check if component and MessageManager are still valid before repainting
        // This prevents crashes when timer callback fires during component destruction
        auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        if (mm == nullptr || getParentComponent() == nullptr || !isVisible())
            return;
        
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
    
    void drawFormantOverlay(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        // Color-coded formant peaks with morphing
        const juce::Colour f1Color(0xFF4A9FD8); // Blue for F1 (lowest frequency)
        const juce::Colour f2Color(0xFF4AD84A); // Green for F2 (mid frequency)  
        const juce::Colour f3Color(0xFFD84A4A); // Red for F3 (highest frequency)
        
        const float overlayAlpha = 0.85f; // Increased opacity for better visibility
        
        // Draw morphing formant response with color transitions
        drawConnectedFormantResponse(g, bounds, f1Color.withAlpha(overlayAlpha), f2Color.withAlpha(overlayAlpha), f3Color.withAlpha(overlayAlpha));
        
        // Draw draggable peak markers with individual Q values
        drawFormantPeak(g, bounds, formantF1, formantQ1, f1Color.withAlpha(overlayAlpha));
        drawFormantPeak(g, bounds, formantF2, formantQ2, f2Color.withAlpha(overlayAlpha));
        drawFormantPeak(g, bounds, formantF3, formantQ3, f3Color.withAlpha(overlayAlpha));
    }
    
    void drawConnectedFormantResponse(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                                     juce::Colour f1Color, juce::Colour f2Color, juce::Colour f3Color)
    {
        const float width = bounds.getWidth();
        const int numPoints = (int)width;
        const float strokeThickness = 8.0f;
        const float glowThickness = 16.0f;
        
        // Create path segments with interpolated colors
        for (int i = 0; i < numPoints - 1; ++i)
        {
            float t1 = i / (float)(numPoints - 1);
            float t2 = (i + 1) / (float)(numPoints - 1);
            float freq1 = 20.0f * std::pow(1000.0f, t1); // 20 Hz to 20 kHz log scale
            float freq2 = 20.0f * std::pow(1000.0f, t2);
            
            // Calculate combined formant response with individual Q values
            float f1Response1 = calculateBandpassResponse(freq1, formantF1, formantQ1);
            float f2Response1 = calculateBandpassResponse(freq1, formantF2, formantQ2);
            float f3Response1 = calculateBandpassResponse(freq1, formantF3, formantQ3);
            float combinedResponse1 = (f1Response1 + f2Response1 + f3Response1) * juce::Decibels::decibelsToGain(formantEmphasisDb);
            
            float f1Response2 = calculateBandpassResponse(freq2, formantF1, formantQ1);
            float f2Response2 = calculateBandpassResponse(freq2, formantF2, formantQ2);
            float f3Response2 = calculateBandpassResponse(freq2, formantF3, formantQ3);
            float combinedResponse2 = (f1Response2 + f2Response2 + f3Response2) * juce::Decibels::decibelsToGain(formantEmphasisDb);
            
            // Convert to dB
            float responseDb1 = 20.0f * std::log10(combinedResponse1 + 1e-12f);
            float responseDb2 = 20.0f * std::log10(combinedResponse2 + 1e-12f);
            responseDb1 = juce::jlimit(-90.0f, 24.0f, responseDb1);
            responseDb2 = juce::jlimit(-90.0f, 24.0f, responseDb2);
            
            float x1 = bounds.getX() + i;
            float y1 = dbToY(responseDb1, bounds);
            float x2 = bounds.getX() + (i + 1);
            float y2 = dbToY(responseDb2, bounds);
            
            // Interpolate color based on frequency position
            juce::Colour segmentColor;
            if (t1 < 0.33f)
            {
                // Transition from F1 (blue) to F2 (green)
                float interp = t1 / 0.33f;
                segmentColor = f1Color.interpolatedWith(f2Color, interp);
            }
            else if (t1 < 0.66f)
            {
                // Transition from F2 (green) to F3 (red)
                float interp = (t1 - 0.33f) / 0.33f;
                segmentColor = f2Color.interpolatedWith(f3Color, interp);
            }
            else
            {
                // Mostly F3 (red) at high frequencies
                float interp = (t1 - 0.66f) / 0.34f;
                segmentColor = f3Color.interpolatedWith(f3Color, interp); // Stay red
            }
            
            // Draw glow
            g.setColour(segmentColor.withAlpha(0.5f));
            g.drawLine(x1, y1, x2, y2, glowThickness);
            
            // Draw main line
            g.setColour(segmentColor.withAlpha(0.95f));
            g.drawLine(x1, y1, x2, y2, strokeThickness);
        }
    }
    
    void drawFormantPeak(juce::Graphics& g, const juce::Rectangle<float>& bounds, 
                        float centerFreq, float q, juce::Colour color)
    {
        if (centerFreq <= 0.0f || q <= 0.0f) return;
        
        // Draw the bandpass filter response curve to show Q width
        juce::Path peakPath;
        bool firstPoint = true;
        
        const float width = bounds.getWidth();
        const int numPoints = (int)width;
        
        // Calculate bandwidth from Q factor (higher Q = narrower peak)
        float bandwidth = centerFreq / q;
        float freqLow = juce::jmax(20.0f, centerFreq - bandwidth * 2.0f);
        float freqHigh = juce::jmin(20000.0f, centerFreq + bandwidth * 2.0f);
        
        for (int i = 0; i < numPoints; ++i)
        {
            float t = i / (float)(numPoints - 1);
            float freq = 20.0f * std::pow(1000.0f, t); // 20 Hz to 20 kHz log scale
            
            // Only draw frequencies within the peak range
            if (freq < freqLow || freq > freqHigh) continue;
            
            // Calculate bandpass filter response
            float response = calculateBandpassResponse(freq, centerFreq, q);
            
            // Apply emphasis gain
            response *= juce::Decibels::decibelsToGain(formantEmphasisDb);
            
            // Convert to dB
            float responseDb = 20.0f * std::log10(response + 1e-12f);
            responseDb = juce::jlimit(-90.0f, 24.0f, responseDb);
            
            float x = bounds.getX() + i;
            float y = dbToY(responseDb, bounds);
            
            if (firstPoint)
            {
                peakPath.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                peakPath.lineTo(x, y);
            }
        }
        
        // Draw the peak curve with Q-based width
        g.setColour(color);
        g.strokePath(peakPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Draw glow effect with Q-based width
        g.setColour(color.withAlpha(0.4f));
        g.strokePath(peakPath, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Draw interactive dot at the peak center
        float centerX = freqToX(centerFreq, bounds);
        float centerY = dbToY(0.0f, bounds);
        
        // Draw glow effect
        g.setColour(color.withAlpha(0.6f));
        g.fillEllipse(centerX - 12, centerY - 12, 24, 24);
        
        // Draw main dot
        g.setColour(color);
        g.fillEllipse(centerX - 10, centerY - 10, 20, 20);
        
        // Add white outline to dot
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawEllipse(centerX - 10, centerY - 10, 20, 20, 2.0f);
    }
    
    void drawFormantLegend(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        // Draw legend at the top of the spectrum analyzer
        const float legendY = bounds.getY() + 5;
        const float legendX = bounds.getX() + 10;
        
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(12.0f);
        
        // No text labels - just visual
    }
    
    float calculateBandpassResponse(float freq, float centerFreq, float q) const
    {
        // Simplified band-pass filter response calculation
        float normalizedFreq = freq / centerFreq;
        float bandwidth = 1.0f / q;
        
        // Calculate response magnitude (simplified Butterworth band-pass)
        float response = 1.0f / std::sqrt(1.0f + std::pow((normalizedFreq - 1.0f) / (bandwidth * 0.5f), 2.0f));
        
        return response;
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
    
    // Formant filter overlay state
    bool formantOverlayEnabled = false;
    float formantF1 = 730.0f;  // Default A vowel F1
    float formantF2 = 1090.0f; // Default A vowel F2
    float formantF3 = 2440.0f; // Default A vowel F3
    float formantQ = 6.0f;     // Default Q factor
    float formantQ1 = 12.0f;   // Q for F1
    float formantQ2 = 12.0f;   // Q for F2
    float formantQ3 = 12.0f;   // Q for F3
    float formantEmphasisDb = 6.0f; // Default emphasis
    
    // Mouse interaction state
    int draggedFormantIndex = -1; // -1 = none, 0 = F1, 1 = F2, 2 = F3
    float initialMouseY = 0.0f;
    bool isFormantOverlayEnabled() const { return formantOverlayEnabled && onFormantDragged; }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!formantOverlayEnabled || !onFormantDragged) return;
        
        auto bounds = this->getLocalBounds();
        
        // Check if mouse is near any formant peak
        float x = event.position.x;
        float y = event.position.y;
        initialMouseY = y; // Store initial Y for vertical drag
        const float dotSize = 22.0f; // Size of interactive dot
        
        // Get bounds for spectrum drawing
        auto drawBounds = bounds.toFloat();
        
        for (int i = 0; i < 3; ++i)
        {
            float freq = (i == 0) ? formantF1 : (i == 1) ? formantF2 : formantF3;
            float centerX = freqToX(freq, drawBounds);
            float centerY = dbToY(0.0f, drawBounds);
            
            float dist = std::sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
            if (dist < dotSize)
            {
                draggedFormantIndex = i;
                return;
            }
        }
        
        draggedFormantIndex = -1;
    }
    
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!formantOverlayEnabled || !onFormantDragged || draggedFormantIndex < 0) return;
        
        auto bounds = this->getLocalBounds();
        float x = event.position.x;
        float y = event.position.y;
        
        // Convert X position to frequency (horizontal drag)
        float xNorm = (x - bounds.getX()) / bounds.getWidth();
        float freq = 20.0f * std::pow(1000.0f, xNorm); // 20 Hz to 20 kHz log scale
        freq = juce::jlimit(20.0f, 20000.0f, freq);
        
        // Convert Y position to Q/resonance (vertical drag - inverted: lower Y = higher Q)
        float yNorm = (y - bounds.getY()) / bounds.getHeight();
        float q = 0.5f + (1.0f - yNorm) * 19.5f; // 0.5 to 20.0 Q, higher on screen = higher Q
        q = juce::jlimit(0.5f, 20.0f, q);
        
        // Update frequency
        if (draggedFormantIndex == 0) {
            formantF1 = freq;
            formantQ1 = q;
        }
        else if (draggedFormantIndex == 1) {
            formantF2 = freq;
            formantQ2 = q;
        }
        else if (draggedFormantIndex == 2) {
            formantF3 = freq;
            formantQ3 = q;
        }
        
        // Call callback with both frequency and Q
        onFormantDragged(draggedFormantIndex, freq, q);
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        draggedFormantIndex = -1;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSpectrumView)
};

