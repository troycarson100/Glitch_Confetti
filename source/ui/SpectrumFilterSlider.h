#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * Dual-slider for spectrum LP/HP filtering
 * Drag from left = highpass, drag from right = lowpass
 */
class SpectrumFilterSlider : public juce::Component
{
public:
    SpectrumFilterSlider()
    {
        lowCutFreq = 20.0f;    // Full spectrum at start
        highCutFreq = 20000.0f;
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // No background - transparent
        // Background removed to show underlying UI
        
        // Calculate positions (log scale 20 Hz to 20 kHz)
        float lowCutX = freqToX(lowCutFreq, bounds);
        float highCutX = freqToX(highCutFreq, bounds);
        
        // Active range (pass band) - brighter
        juce::Rectangle<float> passband(lowCutX, bounds.getY(), 
                                        highCutX - lowCutX, bounds.getHeight());
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.fillRoundedRectangle(passband, 3.0f);
        
        // Cut regions (darker/red tint)
        g.setColour(juce::Colour(0xFF2A1A1A));
        juce::Rectangle<float> lowCut(bounds.getX(), bounds.getY(),
                                      lowCutX - bounds.getX(), bounds.getHeight());
        g.fillRoundedRectangle(lowCut, 3.0f);
        
        juce::Rectangle<float> highCut(highCutX, bounds.getY(),
                                       bounds.getRight() - highCutX, bounds.getHeight());
        g.fillRoundedRectangle(highCut, 3.0f);
        
        // Draw handles (3x thicker, white)
        const float handleWidth = 12.0f; // 3x thicker (was 4.0f)
        const float handleHeight = bounds.getHeight() + 4.0f;
        
        // Low cut handle (highpass) - white
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRoundedRectangle(lowCutX - handleWidth * 0.5f, bounds.getY() - 2.0f,
                              handleWidth, handleHeight, 3.0f);
        
        // High cut handle (lowpass) - white
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRoundedRectangle(highCutX - handleWidth * 0.5f, bounds.getY() - 2.0f,
                              handleWidth, handleHeight, 3.0f);
        
        // Frequency labels
        g.setFont(10.0f);
        if (lowCutFreq > 20.0f)
        {
            g.setColour(juce::Colours::orange);
            g.drawText(formatFreq(lowCutFreq), lowCutX - 30, bounds.getY() - 15, 60, 12,
                      juce::Justification::centred);
        }
        
        if (highCutFreq < 20000.0f)
        {
            g.setColour(juce::Colours::cyan);
            g.drawText(formatFreq(highCutFreq), highCutX - 30, bounds.getY() - 15, 60, 12,
                      juce::Justification::centred);
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();
        float mouseX = (float)e.x;
        
        float lowCutX = freqToX(lowCutFreq, bounds);
        float highCutX = freqToX(highCutFreq, bounds);
        
        // Determine which handle is closer
        float distToLow = std::abs(mouseX - lowCutX);
        float distToHigh = std::abs(mouseX - highCutX);
        
        if (distToLow < distToHigh && distToLow < 20.0f)
            draggingLowCut = true;
        else if (distToHigh < 20.0f)
            draggingHighCut = true;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();
        float mouseX = juce::jlimit(bounds.getX(), bounds.getRight(), (float)e.x);
        
        if (draggingLowCut)
        {
            lowCutFreq = xToFreq(mouseX, bounds);
            lowCutFreq = juce::jlimit(20.0f, 20000.0f, lowCutFreq); // Allow full range
            
            if (onFilterChange)
                onFilterChange(lowCutFreq, highCutFreq);
            
            repaint();
        }
        else if (draggingHighCut)
        {
            highCutFreq = xToFreq(mouseX, bounds);
            highCutFreq = juce::jlimit(20.0f, 20000.0f, highCutFreq); // Allow full range
            
            if (onFilterChange)
                onFilterChange(lowCutFreq, highCutFreq);
            
            repaint();
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        draggingLowCut = false;
        draggingHighCut = false;
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        // Reset to full spectrum
        lowCutFreq = 20.0f;
        highCutFreq = 20000.0f;
        
        if (onFilterChange)
            onFilterChange(lowCutFreq, highCutFreq);
        
        repaint();
    }
    
    float getLowCutFreq() const { return lowCutFreq; }
    float getHighCutFreq() const { return highCutFreq; }
    
    std::function<void(float, float)> onFilterChange;
    
private:
    float freqToX(float freq, const juce::Rectangle<float>& bounds) const
    {
        // Log scale: 20 Hz to 20 kHz
        float logFreq = std::log10(juce::jlimit(20.0f, 20000.0f, freq));
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float t = (logFreq - logMin) / (logMax - logMin);
        return bounds.getX() + t * bounds.getWidth();
    }
    
    float xToFreq(float x, const juce::Rectangle<float>& bounds) const
    {
        float t = (x - bounds.getX()) / bounds.getWidth();
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float logFreq = logMin + t * (logMax - logMin);
        return std::pow(10.0f, logFreq);
    }
    
    juce::String formatFreq(float freq) const
    {
        if (freq >= 1000.0f)
            return juce::String(freq / 1000.0f, 1) + "k";
        else
            return juce::String((int)freq);
    }
    
    float lowCutFreq = 20.0f;
    float highCutFreq = 20000.0f;
    bool draggingLowCut = false;
    bool draggingHighCut = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumFilterSlider)
};

