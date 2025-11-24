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
    // First try to get from loaded fonts
    auto key = fontName + "_" + juce::String(height) + "_" + juce::String(style);
    auto it = loadedFonts.find(key);
    if (it != loadedFonts.end())
    {
        return it->second;
    }
    
    // Try to load from BinaryData
    juce::Font font = loadFontFromBinaryData(fontName, height, style);
    if (font.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
    {
        loadedFonts[key] = font;
        return font;
    }
    
    // Fallback to system font
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
    DBG("Loading font: " + fontName + " -> " + resourceName + " (dataSize: " + juce::String(dataSize) + ")");
    if (data != nullptr && dataSize > 0)
    {
        auto typeface = juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(dataSize));
        if (typeface != nullptr)
        {
            DBG("Successfully loaded font: " + fontName + " -> " + typeface->getName());
            return juce::Font(typeface).withHeight(height).withStyle(style);
        }
        else
        {
            DBG("Failed to create typeface for: " + fontName);
        }
    }
    else
    {
        DBG("No data found for font: " + fontName + " (resource: " + resourceName + ")");
    }
    
    // Fallback to system font
    DBG("Font not found in BinaryData: " + fontName + " (tried: " + resourceName + ")");
    return juce::Font(height, style);
}
