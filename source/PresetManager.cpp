#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& state)
    : processor(proc), apvts(state)
{
}

void PresetManager::refresh()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    allPresets.clear();
    
    auto root = getUserPresetsRoot();
    if (!root.exists())
        root.createDirectory();
    
    // Scan all folders in the root
    for (auto& groupFolder : root.findChildFiles(juce::File::findDirectories, false))
    {
        juce::String groupName = groupFolder.getFileName();
        
        // Scan all .xml files in this group
        for (auto& presetFile : groupFolder.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            auto info = parsePresetFile(presetFile);
            if (info.name.isNotEmpty())
                allPresets.add(info);
        }
    }
    
    DBG("[PresetManager] Scanned " + juce::String(allPresets.size()) + " presets");
}

juce::StringArray PresetManager::getGroups() const
{
    juce::StringArray groups;
    
    // Always add "FAVORITES" first
    groups.add("FAVORITES");
    
    // Collect unique group names from scanned presets
    for (const auto& preset : allPresets)
    {
        if (preset.group.isNotEmpty() && !groups.contains(preset.group))
            groups.add(preset.group);
    }
    
    return groups;
}

juce::Array<PresetInfo> PresetManager::getPresetsInGroup(const juce::String& group) const
{
    juce::Array<PresetInfo> result;
    
    if (group == "FAVORITES")
    {
        // Virtual group: return all favorite presets
        for (const auto& preset : allPresets)
        {
            if (preset.favorite)
                result.add(preset);
        }
    }
    else
    {
        // Return presets in this group
        for (const auto& preset : allPresets)
        {
            if (preset.group == group)
                result.add(preset);
        }
    }
    
    return result;
}

juce::Result PresetManager::saveCurrentStateAsPreset(const juce::String& name, const juce::String& group)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    if (name.isEmpty())
        return juce::Result::fail("Preset name cannot be empty");
    
    if (group.isEmpty() || group == "FAVORITES")
        return juce::Result::fail("Please select a valid group");
    
    // Create group folder if needed (use full path creation)
    auto rootFolder = getUserPresetsRoot();
    if (!rootFolder.exists())
    {
        auto result = rootFolder.createDirectory();
        if (!result.wasOk())
            return juce::Result::fail("Failed to create presets root directory: " + result.getErrorMessage());
    }
    
    auto groupFolder = rootFolder.getChildFile(group);
    if (!groupFolder.exists())
    {
        auto result = groupFolder.createDirectory();
        if (!result.wasOk())
            return juce::Result::fail("Failed to create group folder: " + result.getErrorMessage());
    }
    
    // Create preset file
    auto presetFile = groupFolder.getChildFile(name + ".xml");
    
    // Get complete plugin state via getStateInformation (includes EffectRouter, sequencers, etc.)
    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);
    
    DBG("[PresetManager] State data size: " + juce::String(stateData.getSize()) + " bytes");
    
    // Convert MemoryBlock to ValueTree
    auto stateTree = juce::ValueTree::readFromData(stateData.getData(), stateData.getSize());
    
    if (!stateTree.isValid())
        return juce::Result::fail("Failed to create state tree");
    
    DBG("[PresetManager] State tree type: " + stateTree.getType().toString());
    DBG("[PresetManager] State tree has EffectRouter: " + juce::String(stateTree.getChildWithName("EffectRouter").isValid() ? "YES" : "NO"));
    
    // Convert to XML
    auto xml = stateTree.createXml();
    
    if (xml == nullptr)
        return juce::Result::fail("Failed to create XML from state");
    
    // Add metadata attributes
    xml->setAttribute("name", name);
    xml->setAttribute("group", group);
    xml->setAttribute("favorite", 0);
    
    // Write to file
    if (!xml->writeTo(presetFile))
        return juce::Result::fail("Failed to write preset file");
    
    // Refresh and select
    refresh();
    
    DBG("[PresetManager] Saved complete preset: " + name + " in group: " + group);
    return juce::Result::ok();
}

juce::Result PresetManager::loadPreset(const juce::File& file)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    if (!file.existsAsFile())
        return juce::Result::fail("Preset file does not exist");
    
    // Parse XML
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return juce::Result::fail("Failed to parse preset XML");
    
    // Convert to ValueTree
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid())
        return juce::Result::fail("Invalid ValueTree from preset");
    
    DBG("[PresetManager] Loading preset tree type: " + tree.getType().toString());
    DBG("[PresetManager] Tree has EffectRouter: " + juce::String(tree.getChildWithName("EffectRouter").isValid() ? "YES" : "NO"));
    
    // Convert ValueTree to MemoryBlock
    juce::MemoryOutputStream stream;
    tree.writeToStream(stream);
    
    // Load complete plugin state via setStateInformation (includes EffectRouter, sequencers, etc.)
    processor.setStateInformation(stream.getData(), static_cast<int>(stream.getDataSize()));
    
    DBG("[PresetManager] Loaded complete preset: " + file.getFileNameWithoutExtension());
    return juce::Result::ok();
}

void PresetManager::setFavorite(const juce::File& presetFile, bool fav)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    updateFavoriteInFile(presetFile, fav);
    
    // Update in-memory list
    for (auto& preset : allPresets)
    {
        if (preset.file == presetFile)
        {
            preset.favorite = fav;
            break;
        }
    }
}

juce::Result PresetManager::createGroup(const juce::String& groupName)
{
    if (groupName.isEmpty() || groupName == "FAVORITES")
        return juce::Result::fail("Invalid group name");
    
    auto groupFolder = getUserPresetsRoot().getChildFile(groupName);
    if (groupFolder.exists())
        return juce::Result::fail("Group already exists");
    
    if (!groupFolder.createDirectory())
        return juce::Result::fail("Failed to create group folder");
    
    refresh();
    return juce::Result::ok();
}

juce::Result PresetManager::renameGroup(const juce::String& oldName, const juce::String& newName)
{
    if (oldName == "FAVORITES" || newName == "FAVORITES")
        return juce::Result::fail("Cannot rename FAVORITES");
    
    auto oldFolder = getUserPresetsRoot().getChildFile(oldName);
    auto newFolder = getUserPresetsRoot().getChildFile(newName);
    
    if (!oldFolder.exists())
        return juce::Result::fail("Group does not exist");
    
    if (newFolder.exists())
        return juce::Result::fail("Target group already exists");
    
    if (!oldFolder.moveFileTo(newFolder))
        return juce::Result::fail("Failed to rename group folder");
    
    refresh();
    return juce::Result::ok();
}

juce::Result PresetManager::deleteGroup(const juce::String& groupName, bool movePresetsToDefault)
{
    if (groupName == "FAVORITES")
        return juce::Result::fail("Cannot delete FAVORITES");
    
    auto groupFolder = getUserPresetsRoot().getChildFile(groupName);
    if (!groupFolder.exists())
        return juce::Result::fail("Group does not exist");
    
    // Check if empty
    auto presets = groupFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
    if (presets.size() > 0)
    {
        if (movePresetsToDefault)
        {
            // Move presets to "User" group
            auto userFolder = getUserPresetsRoot().getChildFile("User");
            userFolder.createDirectory();
            
            for (auto& preset : presets)
                preset.moveFileTo(userFolder.getChildFile(preset.getFileName()));
        }
        else
        {
            return juce::Result::fail("Group is not empty");
        }
    }
    
    if (!groupFolder.deleteRecursively())
        return juce::Result::fail("Failed to delete group folder");
    
    refresh();
    return juce::Result::ok();
}

juce::File PresetManager::getUserPresetsRoot() const
{
    // Use Application Support directory to avoid permission issues
    // macOS: ~/Library/Application Support/Stepper/Presets
    auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    
    root = root.getChildFile("Stepper").getChildFile("Presets");
    
    // Ensure the full path exists (create all parent directories)
    if (!root.exists())
    {
        auto result = root.createDirectory();
        if (!result.wasOk())
        {
            DBG("[PresetManager] Failed to create presets directory: " + result.getErrorMessage());
        }
    }
    
    return root;
}

PresetInfo PresetManager::parsePresetFile(const juce::File& file) const
{
    PresetInfo info;
    info.file = file;
    info.group = file.getParentDirectory().getFileName();
    
    // Parse XML to extract metadata
    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
    {
        info.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        info.favorite = xml->getIntAttribute("favorite", 0) != 0;
    }
    else
    {
        info.name = file.getFileNameWithoutExtension();
    }
    
    return info;
}

void PresetManager::updateFavoriteInFile(const juce::File& file, bool favorite)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
    {
        xml->setAttribute("favorite", favorite ? 1 : 0);
        xml->writeTo(file);
    }
}
