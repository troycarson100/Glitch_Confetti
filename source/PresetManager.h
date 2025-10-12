#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Forward declaration
class PluginProcessor;

// Metadata for a single preset
struct PresetInfo {
    juce::String name;
    juce::String group;      // Folder name (e.g., "Bass", "Leads")
    juce::File file;
    bool favorite = false;
    
    PresetInfo() = default;
    PresetInfo(const juce::String& n, const juce::String& g, const juce::File& f, bool fav = false)
        : name(n), group(g), file(f), favorite(fav) {}
};

// Preset management system (thread-safe, runs on message thread)
class PresetManager
{
public:
    PresetManager(juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& state);
    
    // Scan presets from disk (call on message thread)
    void refresh();
    
    // Get list of all groups (includes "FAVORITES" first)
    juce::StringArray getGroups() const;
    
    // Get presets for a specific group ("FAVORITES" returns all favorite presets)
    juce::Array<PresetInfo> getPresetsInGroup(const juce::String& group) const;
    
    // Get all presets from all groups
    juce::Array<PresetInfo> getAllPresets() const { return allPresets; }
    
    // Save current APVTS state as a preset
    juce::Result saveCurrentStateAsPreset(const juce::String& name, const juce::String& group);
    
    // Load a preset from file (updates APVTS on message thread)
    juce::Result loadPreset(const juce::File& file);
    
    // Toggle favorite status (updates XML attribute in file)
    void setFavorite(const juce::File& presetFile, bool fav);
    
    // Create a new group (makes subfolder)
    juce::Result createGroup(const juce::String& groupName);
    
    // Rename a group (renames folder)
    juce::Result renameGroup(const juce::String& oldName, const juce::String& newName);
    
    // Delete a group (prompts if non-empty)
    juce::Result deleteGroup(const juce::String& groupName, bool movePresetsToDefault = false);
    
    // Get user presets root folder
    juce::File getUserPresetsRoot() const;
    
private:
    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    
    // All scanned presets
    juce::Array<PresetInfo> allPresets;
    
    // Parse a preset file and extract metadata
    PresetInfo parsePresetFile(const juce::File& file) const;
    
    // Update the "favorite" attribute in an XML preset file
    void updateFavoriteInFile(const juce::File& file, bool favorite);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
