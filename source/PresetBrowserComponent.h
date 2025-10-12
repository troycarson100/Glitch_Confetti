#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

// Forward declarations
class PresetBrowserOverlay;

// Custom star button component (paints a 5-point star, no Unicode)
class StarButton : public juce::Component
{
public:
    StarButton();
    
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;
    
    void setToggleState(bool shouldBeOn);
    bool getToggleState() const { return isOn; }
    
    std::function<void(bool)> onClick;
    
private:
    bool isOn = false;
    bool isHovered = false;
    
    void mouseEnter(const juce::MouseEvent&) override { isHovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { isHovered = false; repaint(); }
    
    juce::Path createStarPath(float cx, float cy, float outerRadius, float innerRadius) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarButton)
};

// Custom category button that shows a white bar when selected
class CategoryButton : public juce::ImageButton
{
public:
    CategoryButton(const juce::String& name) : juce::ImageButton(name), buttonName(name) {}
    
    void paintButton(juce::Graphics& g, bool over, bool down) override;
    void setSelected(bool shouldBeSelected) { isSelected = shouldBeSelected; repaint(); }
    bool getSelected() const { return isSelected; }
    
private:
    juce::String buttonName;
    bool isSelected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CategoryButton)
};

// Custom preset row component (name + star button)
class PresetRowComponent : public juce::Component
{
public:
    PresetRowComponent();
    
    void updateContent(const juce::String& presetName, bool isFav, bool isSelected);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    
    std::function<void()> onPresetClick;
    std::function<void(bool)> onStarClick;
    std::function<void()> onShowInFinder;
    std::function<void()> onDelete;
    
private:
    juce::String name;
    bool favorite = false;
    bool selected = false;
    StarButton starButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetRowComponent)
};

// Forward declaration
struct UiAssets;

// Preset browser overlay (covers Master area only)
class PresetBrowserOverlay : public juce::Component
{
public:
    PresetBrowserOverlay(PresetManager& manager, const UiAssets& uiAssets);
    ~PresetBrowserOverlay() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    
    // Show the overlay (scans presets and animates in)
    void show();
    
    // Close handler
    std::function<void()> onClose;
    
    // Preset loaded handler (called when a preset is successfully loaded)
    std::function<void(const juce::String&)> onPresetLoaded;
    
private:
    PresetManager& presetManager;
    const UiAssets& assets;
    
    // Custom container that forwards mouse wheel events to parent viewport
    class ScrollableContainer : public juce::Component
    {
    public:
        ScrollableContainer(juce::Viewport* viewport) : parentViewport(viewport) {}
        
        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
        {
            if (parentViewport)
                parentViewport->mouseWheelMove(event, wheel);
        }
        
    private:
        juce::Viewport* parentViewport;
    };
    
    // Category tabs (left side, scrollable)
    juce::Viewport categoryViewport;
    ScrollableContainer categoryContainer;
    std::vector<std::unique_ptr<juce::ImageButton>> categoryButtons;
    juce::String selectedCategory = ""; // Empty = all presets
    
    // Presets list (center, full width)
    juce::ListBox presetsList;
    
    // Data
    juce::Array<PresetInfo> currentPresets;
    
    // List models
    class PresetsListModel;
    std::unique_ptr<PresetsListModel> presetsModel;
    
    // Actions
    void handlePresetSelected(int index);
    void handleStarToggle(int index);
    void handleSavePreset();
    void handleCloseButton();
    void handleCategorySelected(const juce::String& category);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserOverlay)
};

// Presets list model (custom row components with star buttons)
class PresetBrowserOverlay::PresetsListModel : public juce::ListBoxModel
{
public:
    PresetsListModel(PresetBrowserOverlay& owner) : overlay(owner) {}
    
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow(int row, bool isSelected, juce::Component* existingComponent) override;
    
private:
    PresetBrowserOverlay& overlay;
};
