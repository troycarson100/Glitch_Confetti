#include "FontManager.h"
#include "BinaryData.h"

FontManager& FontManager::getInstance()
{
    // Leaky singleton: allocate once, never destroyed.
    static FontManager* instance = new FontManager();
    return *instance;
}

juce::Font FontManager::getFont(const juce::String& fontName, float height, int style)
{
    // Wrap in try-catch to prevent crashes during plugin scan
    try
    {
        // First try to get from loaded fonts
        auto key = fontName + "_" + juce::String(height) + "_" + juce::String(style);
        auto it = loadedFonts.find(key);
        if (it != loadedFonts.end())
        {
            return it->second;
        }
        
        // Try to load from BinaryData
        juce::Font font = loadFontFromBinaryData(fontName, height, style);
        
        // Check if we got a custom font (not default system font)
        // Only cache if it's actually a custom font to avoid caching system fonts
        if (font.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
        {
            loadedFonts[key] = font;
            return font;
        }
    }
    catch (...)
    {
        // If anything goes wrong, just return system font
    }
    
    // Fallback to system font (always safe)
    return juce::Font(height, style);
}

juce::Font FontManager::loadFontFromFile(const juce::File& fontFile, float height, int style)
{
    // For now, just return a system font
    // TODO: Implement proper font loading from file
    return juce::Font(height, style);
}

juce::Font FontManager::getFontByName(const juce::String& name, float height, int style)
{
    // Wrap in try-catch to prevent crashes during plugin scan
    try
    {
        // Try exact match first
        juce::Font font = getFont(name, height, style);
        if (font.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
        {
            return font;
        }
        
        // Try case-insensitive match
        for (const auto& pair : loadedFonts)
        {
            if (pair.first.containsIgnoreCase(name))
            {
                return pair.second;
            }
        }
    }
    catch (...)
    {
        // If anything goes wrong, just return system font
    }
    
    // Fallback to system font (always safe)
    return juce::Font(height, style);
}

juce::StringArray FontManager::getAvailableFonts() const
{
    juce::StringArray fonts;
    for (const auto& pair : loadedFonts)
    {
        fonts.add(pair.first);
    }
    return fonts;
}

juce::Font FontManager::loadFontFromBinaryData(const juce::String& fontName, float height, int style)
{
    // Wrap everything in try-catch to prevent crashes during plugin scan
    try
    {
        // Map user-friendly names to BinaryData resource names
        juce::String resourceName;
        if (fontName == "Akira Expanded")
            resourceName = "Akira_Expanded_otf";
        else if (fontName == "AlteHaasGroteskBold")
            resourceName = "AlteHaasGroteskBold_ttf";
        else if (fontName == "AlteHaasGroteskRegular")
            resourceName = "AlteHaasGroteskRegular_ttf";
        else
            resourceName = fontName; // Try direct name
        
        // Load from BinaryData
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(resourceName.toUTF8(), dataSize);
        
        // Only attempt to load if we have valid data
        if (data != nullptr && dataSize > 0)
        {
            // Try to create typeface - this can fail during plugin scan
            auto typeface = juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(dataSize));
            if (typeface != nullptr)
            {
                // Successfully loaded - create font with the typeface
                return juce::Font(typeface).withHeight(height).withStyle(style);
            }
        }
    }
    catch (...)
    {
        // If anything goes wrong (especially during plugin scan), just fall back to system font
        // Don't log or throw - silently fall back to prevent crashes
    }
    
    // Fallback to system font (always safe)
    return juce::Font(height, style);
}
