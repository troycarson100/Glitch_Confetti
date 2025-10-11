#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

// Custom component for a preset item row (with star button)
class PresetItemComponent : public juce::Component
{
public:
    PresetItemComponent();
    
    void updateContent(const juce::String& name, bool isFavorite, bool isSelected);
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;
    
    std::function<void()> onPresetClick;
    std::function<void(bool)> onFavoriteToggle;
    
private:
    juce::Label nameLabel;
    juce::DrawableButton starButton { "star", juce::DrawableButton::ImageFitted };
    bool selected = false;
    bool favorite = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetItemComponent)
};

// Preset browser overlay component
class PresetBrowserComponent : public juce::Component
{
public:
    PresetBrowserComponent(PresetManager& manager);
    ~PresetBrowserComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refreshCategories();
    void refreshPresets();
    void selectCategory(int index);
    
    std::function<void()> onClose;
    
private:
    PresetManager& presetManager;
    
    // UI Components
    juce::ListBox categoryList;
    juce::ListBox presetList;
    juce::TextButton closeButton { "×" };
    juce::TextButton saveButton { "Save Preset" };
    juce::TextButton newCategoryButton { "+" };
    juce::Label titleLabel;
    
    // Data
    juce::StringArray categories;
    juce::Array<PresetInfo> currentPresets;
    juce::String selectedCategory;
    int selectedCategoryIndex = 0;
    
    // List models
    class CategoryListModel;
    class PresetListModel;
    std::unique_ptr<CategoryListModel> categoryModel;
    std::unique_ptr<PresetListModel> presetModel;
    
    // Actions
    void handleSavePreset();
    void handleNewCategory();
    void handleLoadPreset(int presetIndex);
    void handleToggleFavorite(int presetIndex);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserComponent)
};

// Category list model (simple text list)
class PresetBrowserComponent::CategoryListModel : public juce::ListBoxModel
{
public:
    CategoryListModel(PresetBrowserComponent& owner) : browser(owner) {}
    
    int getNumRows() override { return browser.categories.size(); }
    
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (row >= browser.categories.size()) return;
        
        // Background
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xff444444));
        else
            g.fillAll(juce::Colour(0xff2a2a2a));
        
        // Category name
        g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffaaaaaa));
        g.setFont(14.0f);
        
        juce::String categoryName = browser.categories[row];
        if (categoryName == "Favorites")
            categoryName = "★ " + categoryName; // Add star icon
        
        g.drawText(categoryName, 10, 0, width - 20, height, juce::Justification::centredLeft);
        
        // Separator line
        g.setColour(juce::Colour(0xff1a1a1a));
        g.drawLine(0.0f, (float)height - 0.5f, (float)width, (float)height - 0.5f, 1.0f);
    }
    
    void selectedRowsChanged(int lastRowSelected) override
    {
        if (lastRowSelected >= 0 && lastRowSelected < browser.categories.size())
            browser.selectCategory(lastRowSelected);
    }
    
private:
    PresetBrowserComponent& browser;
};

// Preset list model (uses custom row components for star buttons)
class PresetBrowserComponent::PresetListModel : public juce::ListBoxModel
{
public:
    PresetListModel(PresetBrowserComponent& owner) : browser(owner) {}
    
    int getNumRows() override { return browser.currentPresets.size(); }
    
    juce::Component* refreshComponentForRow(int row, bool isSelected, juce::Component* existingComponent) override
    {
        if (row >= browser.currentPresets.size())
        {
            delete existingComponent;
            return nullptr;
        }
        
        PresetItemComponent* item = dynamic_cast<PresetItemComponent*>(existingComponent);
        if (item == nullptr)
            item = new PresetItemComponent();
        
        const auto& preset = browser.currentPresets[row];
        item->updateContent(preset.name, preset.isFavorite, isSelected);
        
        item->onPresetClick = [this, row]() {
            browser.handleLoadPreset(row);
        };
        
        item->onFavoriteToggle = [this, row](bool newState) {
            browser.handleToggleFavorite(row);
        };
        
        return item;
    }
    
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override
    {
        // Component handles its own painting
    }
    
private:
    PresetBrowserComponent& browser;
};

