void CustomStepButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw the appropriate SVG based on active state
    if (active && activeImage != nullptr)
    {
        activeImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else if (!active && inactiveImage != nullptr)
    {
        inactiveImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback drawing if SVGs not loaded
        g.setColour(active ? juce::Colours::orange : juce::Colours::darkgrey);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        g.setColour(juce::Colours::white);
        auto stepFont = FontManager::getInstance().getFont("Akira Expanded", 12.0f, juce::Font::bold);
        g.setFont(stepFont);
        g.drawText(juce::String(stepIndex + 1), bounds, juce::Justification::centred);
    }
    
    // Apply 70% opacity overlay if button is inactive
    if (inactive)
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
}
