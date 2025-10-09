#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioRingBuffer.h"

/**
 * Smooth scrolling waveform visualizer with glow effect
 * Displays post-FX output signal as a glowing light trail
 */
class OutputVisualizer : public juce::Component,
                         private juce::Timer
{
public:
    OutputVisualizer(const AudioRingBuffer& ringBuffer)
        : audioBuffer(ringBuffer)
    {
        startTimerHz(60); // 60 FPS for smooth animation
        
        // Pre-allocate sample buffer
        displaySamples.reserve(500);
    }
    
    ~OutputVisualizer() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // No background - transparent so only white waveform is visible
        const float centerY = bounds.getCentreY();
        
        // Get latest samples from ring buffer
        const int numSamples = juce::jmin(500, audioBuffer.size());
        audioBuffer.readLatest(displaySamples, numSamples);
        
        if (displaySamples.empty())
            return;
        
        // Create waveform path
        juce::Path waveform;
        const float width = bounds.getWidth();
        const float height = bounds.getHeight();
        const float scale = height * 0.4f; // Scale amplitude to 40% of height
        
        bool firstPoint = true;
        for (int i = 0; i < displaySamples.size(); ++i)
        {
            const float x = bounds.getX() + (i / (float)displaySamples.size()) * width;
            const float sample = juce::jlimit(-1.0f, 1.0f, displaySamples[i]);
            const float y = centerY - (sample * scale);
            
            if (firstPoint)
            {
                waveform.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                waveform.lineTo(x, y);
            }
        }
        
        // Apply fade-out effect based on recent activity
        updateFadeLevel();
        
        // Draw glow layer (wider, semi-transparent)
        g.setColour(juce::Colours::white.withAlpha(fadeLevel * 0.3f));
        g.strokePath(waveform, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Draw main waveform (crisp, brighter)
        g.setColour(juce::Colours::white.withAlpha(fadeLevel * 0.8f));
        g.strokePath(waveform, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Optional: Add subtle inner glow
        g.setColour(juce::Colours::white.withAlpha(fadeLevel * 0.5f));
        g.strokePath(waveform, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    void resized() override
    {
        // Component resized - nothing special to do
    }
    
private:
    void timerCallback() override
    {
        // Trigger repaint at 60 FPS
        repaint();
    }
    
    void updateFadeLevel()
    {
        // Calculate RMS of current display samples for fade effect
        float rms = 0.0f;
        for (float sample : displaySamples)
        {
            rms += sample * sample;
        }
        
        if (!displaySamples.empty())
            rms = std::sqrt(rms / displaySamples.size());
        
        // Target fade level based on signal presence
        const float targetFade = rms > 0.001f ? 1.0f : 0.0f;
        
        // Smooth fade transition
        const float fadeSpeed = targetFade > fadeLevel ? 0.3f : 0.05f; // Fast attack, slow release
        fadeLevel += (targetFade - fadeLevel) * fadeSpeed;
        fadeLevel = juce::jlimit(0.0f, 1.0f, fadeLevel);
    }
    
    const AudioRingBuffer& audioBuffer;
    std::vector<float> displaySamples;
    float fadeLevel = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputVisualizer)
};

