#include "PanIndicator.h"
#include "../FontManager.h"

PanIndicator::PanIndicator()
{
    setInterceptsMouseClicks(false, false); // Don't intercept mouse clicks
}

void PanIndicator::setPanPosition(float position)
{
    panPosition = juce::jlimit(-1.0f, 1.0f, position);
    repaint();
}

void PanIndicator::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(juce::Colour(0xFF2B2D31));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Border
    g.setColour(juce::Colour(0xFF40444B));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    
    // L/C/R labels - positioned at left, center, and right
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
    
    // L on the left side
    auto leftBounds = bounds.reduced(8.0f, 4.0f).removeFromLeft(20);
    g.drawText("L", leftBounds, juce::Justification::centred);
    
    // C perfectly centered in the middle of the entire bar
    auto centerBounds = bounds.reduced(8.0f, 4.0f);
    centerBounds.setX(centerBounds.getCentreX() - 10);
    centerBounds.setWidth(20);
    g.drawText("C", centerBounds, juce::Justification::centred);
    
    // R on the right side
    auto rightBounds = bounds.reduced(8.0f, 4.0f).removeFromRight(20);
    g.drawText("R", rightBounds, juce::Justification::centred);
    
    // Pan position indicator - always white and smoother
    float indicatorWidth = 12.0f; // Wider for smoother appearance
    float indicatorHeight = bounds.getHeight() - 6.0f;
    
    // Create movement area (leave space for labels)
    float movementArea = bounds.getWidth() - 60.0f; // Leave 30px on each side for L/R labels
    float centerX = bounds.getX() + 30.0f + (movementArea * 0.5f); // Start from left margin + half movement area
    
    // Convert pan position (-1 to +1) to X position with linear movement
    float panX = centerX + (panPosition * (movementArea * 0.5f));
    float indicatorX = juce::jlimit(bounds.getX() + 30.0f, bounds.getRight() - indicatorWidth - 30.0f, panX - indicatorWidth * 0.5f);
    
    // Always white indicator with subtle glow
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillRoundedRectangle(indicatorX, bounds.getY() + 3.0f, indicatorWidth, indicatorHeight, 4.0f);
    
    // Add a subtle white glow effect
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillRoundedRectangle(indicatorX - 1.5f, bounds.getY() + 1.5f, indicatorWidth + 3.0f, indicatorHeight + 3.0f, 5.0f);
}

void PanIndicator::resized()
{
    // Nothing special needed for resized
}
