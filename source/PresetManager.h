#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Metadata for a single preset
struct PresetInfo {
    juce::String name;
    juce::String category;
    juce::File file;
    bool isFavorite = false;
    
    PresetInfo() = default;
    PresetInfo(const juce::String& n, const juce::String& cat, const juce::File& f, bool fav = false)
        : name(n), category(cat), file(f), isFavorite(fav) {}
};

// Preset management system (file-based with categories)
class PresetManager
{
public:
    PresetManager(juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& state);
    
    // Initialize and scan presets from disk
    void initialize();
    
    // Get list of all categories (including "Favorites")
    juce::StringArray getCategories() const;
    
    // Get presets for a specific category
    juce::Array<PresetInfo> getPresetsForCategory(const juce::String& category) const;
    
    // Save current state as a new preset
    juce::Result savePreset(const juce::String& name, const juce::String& category);
    
    // Load a preset from file
    juce::Result loadPreset(const juce::File& presetFile);
    juce::Result loadPreset(const juce::String& category, const juce::String& name);
    
    // Toggle favorite status of a preset
    juce::Result setPresetFavorite(const juce::String& category, const juce::String& name, bool isFavorite);
    
    // Delete a preset file
    juce::Result deletePreset(const juce::String& category, const juce::String& name);
    
    // Create a new category (folder)
    juce::Result createCategory(const juce::String& categoryName);
    
    // Delete a category (only if empty)
    juce::Result deleteCategory(const juce::String& categoryName);
    
    // Get current preset name (if any)
    juce::String getCurrentPresetName() const { return currentPresetName; }
    
    // Check if current state has been modified since last preset load
    bool hasUnsavedChanges() const { return stateModified; }
    
    // Get user presets folder (public for UI access)
    juce::File getUserPresetsFolder() const;
    
private:
    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    
    juce::String currentPresetName;
    bool stateModified = false;
    
    // All scanned presets
    juce::Array<PresetInfo> allPresets;
    
    // Scan all presets from disk
    void scanPresets();
    
    // Find preset by category and name
    PresetInfo* findPreset(const juce::String& category, const juce::String& name);
    
    // Update favorite flag in preset file
    juce::Result updatePresetFavoriteInFile(const juce::File& file, bool isFavorite);
};

