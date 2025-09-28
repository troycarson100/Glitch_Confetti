#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

class FontManager
{
public:
    static FontManager& getInstance();
    
    // Load a font from BinaryData
    juce::Font getFont(const juce::String& fontName, float height, int style = juce::Font::plain);
    
    // Load a font from file (for development/testing)
    juce::Font loadFontFromFile(const juce::File& fontFile, float height, int style = juce::Font::plain);
    
    // Get a font by name (case-insensitive)
    juce::Font getFontByName(const juce::String& name, float height, int style = juce::Font::plain);
    
    // List available fonts
    juce::StringArray getAvailableFonts() const;
    
private:
    FontManager() = default;
    ~FontManager() = default;
    
    // Store loaded fonts
    std::map<juce::String, juce::Font> loadedFonts;
    
    // Load font from BinaryData
    juce::Font loadFontFromBinaryData(const juce::String& fontName, float height, int style);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FontManager)
};
