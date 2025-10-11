#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& state)
    : processor(proc), apvts(state)
{
}

void PresetManager::initialize()
{
    // Ensure user presets folder exists
    auto folder = getUserPresetsFolder();
    if (!folder.exists())
        folder.createDirectory();
    
    // Scan all presets from disk
    scanPresets();
    
    DBG("[PRESETS] Initialized - found " << allPresets.size() << " presets");
}

juce::File PresetManager::getUserPresetsFolder() const
{
    juce::File root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    
    #ifdef JUCE_MAC
    root = root.getChildFile("Audio").getChildFile("Presets");
    #endif
    
    root = root.getChildFile("Stepper").getChildFile("Stepper");
    return root;
}

void PresetManager::scanPresets()
{
    allPresets.clear();
    
    auto rootFolder = getUserPresetsFolder();
    if (!rootFolder.exists())
        return;
    
    // Scan all category folders
    for (const auto& categoryDir : rootFolder.findChildFiles(juce::File::findDirectories, false))
    {
        juce::String categoryName = categoryDir.getFileName();
        
        // Scan all preset files in this category
        for (const auto& presetFile : categoryDir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            // Read preset metadata from file
            auto xml = juce::XmlDocument::parse(presetFile);
            if (xml != nullptr)
            {
                juce::String name = xml->getStringAttribute("name", presetFile.getFileNameWithoutExtension());
                bool isFavorite = xml->getBoolAttribute("favorite", false);
                
                allPresets.add(PresetInfo(name, categoryName, presetFile, isFavorite));
            }
        }
    }
    
    DBG("[PRESETS] Scanned " << allPresets.size() << " presets");
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray categories;
    
    // Always include Favorites first
    categories.add("Favorites");
    
    // Add all unique categories from presets
    for (const auto& preset : allPresets)
    {
        if (!categories.contains(preset.category))
            categories.add(preset.category);
    }
    
    return categories;
}

juce::Array<PresetInfo> PresetManager::getPresetsForCategory(const juce::String& category) const
{
    juce::Array<PresetInfo> result;
    
    if (category == "Favorites")
    {
        // Return all favorite presets
        for (const auto& preset : allPresets)
        {
            if (preset.isFavorite)
                result.add(preset);
        }
    }
    else
    {
        // Return presets in this category
        for (const auto& preset : allPresets)
        {
            if (preset.category == category)
                result.add(preset);
        }
    }
    
    return result;
}

juce::Result PresetManager::savePreset(const juce::String& name, const juce::String& category)
{
    if (name.isEmpty())
        return juce::Result::fail("Preset name cannot be empty");
    
    if (category.isEmpty() || category == "Favorites")
        return juce::Result::fail("Please select a valid category");
    
    // Create category folder if needed
    auto categoryFolder = getUserPresetsFolder().getChildFile(category);
    if (!categoryFolder.exists())
        categoryFolder.createDirectory();
    
    // Create preset file
    juce::File presetFile = categoryFolder.getChildFile(name + ".xml");
    
    // Get current plugin state
    juce::ValueTree state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    
    if (xml == nullptr)
        return juce::Result::fail("Failed to create state XML");
    
    // Add metadata
    xml->setAttribute("name", name);
    xml->setAttribute("category", category);
    xml->setAttribute("favorite", false); // New presets not favorited by default
    
    // Write to file
    bool success = xml->writeTo(presetFile);
    
    if (success)
    {
        // Add to our in-memory list
        allPresets.add(PresetInfo(name, category, presetFile, false));
        currentPresetName = name;
        stateModified = false;
        
        DBG("[PRESETS] Saved preset: " << name << " in category: " << category);
        return juce::Result::ok();
    }
    
    return juce::Result::fail("Failed to write preset file");
}

juce::Result PresetManager::loadPreset(const juce::File& presetFile)
{
    if (!presetFile.existsAsFile())
        return juce::Result::fail("Preset file does not exist");
    
    auto xml = juce::XmlDocument::parse(presetFile);
    if (xml == nullptr)
        return juce::Result::fail("Failed to parse preset file");
    
    // Extract metadata
    juce::String name = xml->getStringAttribute("name", presetFile.getFileNameWithoutExtension());
    
    // Convert XML to ValueTree and restore state
    juce::ValueTree tree = juce::ValueTree::fromXml(*xml);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        currentPresetName = name;
        stateModified = false;
        
        DBG("[PRESETS] Loaded preset: " << name);
        return juce::Result::ok();
    }
    
    return juce::Result::fail("Invalid preset data");
}

juce::Result PresetManager::loadPreset(const juce::String& category, const juce::String& name)
{
    auto* preset = findPreset(category, name);
    if (preset == nullptr)
        return juce::Result::fail("Preset not found");
    
    return loadPreset(preset->file);
}

juce::Result PresetManager::setPresetFavorite(const juce::String& category, const juce::String& name, bool isFavorite)
{
    auto* preset = findPreset(category, name);
    if (preset == nullptr)
        return juce::Result::fail("Preset not found");
    
    // Update in-memory
    preset->isFavorite = isFavorite;
    
    // Update in file
    return updatePresetFavoriteInFile(preset->file, isFavorite);
}

juce::Result PresetManager::deletePreset(const juce::String& category, const juce::String& name)
{
    auto* preset = findPreset(category, name);
    if (preset == nullptr)
        return juce::Result::fail("Preset not found");
    
    // Delete file
    bool deleted = preset->file.deleteFile();
    
    if (deleted)
    {
        // Remove from in-memory list
        for (int i = 0; i < allPresets.size(); ++i)
        {
            if (allPresets[i].file == preset->file)
            {
                allPresets.remove(i);
                break;
            }
        }
        
        DBG("[PRESETS] Deleted preset: " << name);
        return juce::Result::ok();
    }
    
    return juce::Result::fail("Failed to delete preset file");
}

juce::Result PresetManager::createCategory(const juce::String& categoryName)
{
    if (categoryName.isEmpty() || categoryName == "Favorites")
        return juce::Result::fail("Invalid category name");
    
    auto categoryFolder = getUserPresetsFolder().getChildFile(categoryName);
    
    if (categoryFolder.exists())
        return juce::Result::fail("Category already exists");
    
    bool created = categoryFolder.createDirectory();
    
    if (created)
    {
        DBG("[PRESETS] Created category: " << categoryName);
        return juce::Result::ok();
    }
    
    return juce::Result::fail("Failed to create category folder");
}

juce::Result PresetManager::deleteCategory(const juce::String& categoryName)
{
    if (categoryName == "Favorites")
        return juce::Result::fail("Cannot delete Favorites category");
    
    auto categoryFolder = getUserPresetsFolder().getChildFile(categoryName);
    
    if (!categoryFolder.exists())
        return juce::Result::fail("Category does not exist");
    
    // Check if empty
    int numFiles = categoryFolder.findChildFiles(juce::File::findFiles, false).size();
    if (numFiles > 0)
        return juce::Result::fail("Category is not empty - delete or move presets first");
    
    bool deleted = categoryFolder.deleteFile();
    
    if (deleted)
    {
        DBG("[PRESETS] Deleted category: " << categoryName);
        return juce::Result::ok();
    }
    
    return juce::Result::fail("Failed to delete category folder");
}

PresetInfo* PresetManager::findPreset(const juce::String& category, const juce::String& name)
{
    for (auto& preset : allPresets)
    {
        if (preset.name == name)
        {
            // For Favorites, just match by name
            if (category == "Favorites" && preset.isFavorite)
                return &preset;
            // For other categories, match both category and name
            else if (preset.category == category)
                return &preset;
        }
    }
    return nullptr;
}

juce::Result PresetManager::updatePresetFavoriteInFile(const juce::File& file, bool isFavorite)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return juce::Result::fail("Failed to parse preset file");
    
    xml->setAttribute("favorite", isFavorite ? 1 : 0);
    
    bool success = xml->writeTo(file);
    
    if (success)
    {
        DBG("[PRESETS] Updated favorite status for: " << file.getFileNameWithoutExtension());
        return juce::Result::ok();
    }
    
    return juce::Result::fail("Failed to write preset file");
}

