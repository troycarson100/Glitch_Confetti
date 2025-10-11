#include "PresetBrowserComponent.h"

//==============================================================================
// PresetItemComponent Implementation
//==============================================================================

PresetItemComponent::PresetItemComponent()
{
    addAndMakeVisible(nameLabel);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible(starButton);
    starButton.setClickingTogglesState(true);
    starButton.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    starButton.setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    starButton.onClick = [this]() {
        if (onFavoriteToggle)
            onFavoriteToggle(starButton.getToggleState());
    };
}

void PresetItemComponent::updateContent(const juce::String& name, bool isFavorite, bool isSelected)
{
    nameLabel.setText(name, juce::dontSendNotification);
    starButton.setToggleState(isFavorite, juce::dontSendNotification);
    selected = isSelected;
    favorite = isFavorite;
    repaint();
}

void PresetItemComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Star button on the left (20x20)
    starButton.setBounds(bounds.removeFromLeft(25).withSizeKeepingCentre(20, 20));
    
    // Preset name takes the rest
    nameLabel.setBounds(bounds);
}

void PresetItemComponent::paint(juce::Graphics& g)
{
    // Background
    if (selected)
        g.fillAll(juce::Colour(0xff555555));
    else if (isMouseOver())
        g.fillAll(juce::Colour(0xff333333));
    else
        g.fillAll(juce::Colour(0xff2a2a2a));
    
    // Separator line
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawLine(0.0f, (float)getHeight() - 0.5f, (float)getWidth(), (float)getHeight() - 0.5f, 1.0f);
}

void PresetItemComponent::mouseUp(const juce::MouseEvent& event)
{
    if (!starButton.getBounds().contains(event.getPosition()))
    {
        if (onPresetClick)
            onPresetClick();
    }
}

//==============================================================================
// PresetBrowserComponent Implementation
//==============================================================================

PresetBrowserComponent::PresetBrowserComponent(PresetManager& manager)
    : presetManager(manager)
{
    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText("PRESET BROWSER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    
    // Close button (X)
    addAndMakeVisible(closeButton);
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeButton.onClick = [this]() {
        if (onClose)
            onClose();
    };
    
    // Save button
    addAndMakeVisible(saveButton);
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a8f2a));
    saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    saveButton.onClick = [this]() { handleSavePreset(); };
    
    // New category button
    addAndMakeVisible(newCategoryButton);
    newCategoryButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    newCategoryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    newCategoryButton.onClick = [this]() { handleNewCategory(); };
    
    // Category list
    categoryModel = std::make_unique<CategoryListModel>(*this);
    categoryList.setModel(categoryModel.get());
    categoryList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
    categoryList.setRowHeight(30);
    addAndMakeVisible(categoryList);
    
    // Preset list
    presetModel = std::make_unique<PresetListModel>(*this);
    presetList.setModel(presetModel.get());
    presetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
    presetList.setRowHeight(28);
    addAndMakeVisible(presetList);
    
    // Initialize with data
    refreshCategories();
    
    // Block mouse clicks from passing through
    setInterceptsMouseClicks(true, true);
}

PresetBrowserComponent::~PresetBrowserComponent()
{
    categoryList.setModel(nullptr);
    presetList.setModel(nullptr);
}

void PresetBrowserComponent::paint(juce::Graphics& g)
{
    // Semi-transparent dark overlay
    g.fillAll(juce::Colour(0xf0000000));
    
    // Main panel background
    auto bounds = getLocalBounds().reduced(20);
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    
    // Border
    g.setColour(juce::Colour(0xff444444));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 2.0f);
}

void PresetBrowserComponent::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    
    // Title bar at top (40px)
    auto titleArea = bounds.removeFromTop(40);
    titleLabel.setBounds(titleArea.withSizeKeepingCentre(200, 30));
    closeButton.setBounds(titleArea.removeFromRight(40).reduced(5));
    
    // Bottom buttons area (40px)
    auto bottomArea = bounds.removeFromBottom(40);
    saveButton.setBounds(bottomArea.removeFromLeft(120).reduced(5));
    
    // Main content area
    auto contentArea = bounds.reduced(10);
    
    // Left category column (30% width)
    auto leftColumn = contentArea.removeFromLeft(static_cast<int>(contentArea.getWidth() * 0.30f));
    
    // New category button at bottom of left column
    newCategoryButton.setBounds(leftColumn.removeFromBottom(30).reduced(2));
    
    categoryList.setBounds(leftColumn);
    
    // Spacing between columns
    contentArea.removeFromLeft(10);
    
    // Right preset column (remaining width)
    presetList.setBounds(contentArea);
}

void PresetBrowserComponent::refreshCategories()
{
    categories = presetManager.getCategories();
    categoryList.updateContent();
    
    // Select first category (Favorites) by default
    if (categories.size() > 0)
    {
        categoryList.selectRow(0);
        selectCategory(0);
    }
}

void PresetBrowserComponent::refreshPresets()
{
    if (selectedCategory.isEmpty() && categories.size() > 0)
        selectedCategory = categories[selectedCategoryIndex];
    
    currentPresets = presetManager.getPresetsForCategory(selectedCategory);
    presetList.updateContent();
    presetList.deselectAllRows();
}

void PresetBrowserComponent::selectCategory(int index)
{
    if (index < 0 || index >= categories.size())
        return;
    
    selectedCategoryIndex = index;
    selectedCategory = categories[index];
    
    DBG("[PRESET BROWSER] Selected category: " << selectedCategory);
    
    refreshPresets();
}

void PresetBrowserComponent::handleSavePreset()
{
    // Prompt for preset name
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("Save Preset")
            .withMessage("Enter preset name:")
            .withButton("Save")
            .withButton("Cancel"),
        [this](int result) {
            if (result == 1) // Save clicked
            {
                // Get the input text
                juce::String presetName = "New Preset"; // TODO: Get from text input
                
                // Use current category (or default if Favorites)
                juce::String saveCategory = (selectedCategory == "Favorites" || selectedCategory.isEmpty()) 
                    ? "User" : selectedCategory;
                
                // Ensure category exists
                auto categoryFolder = presetManager.getUserPresetsFolder().getChildFile(saveCategory);
                if (!categoryFolder.exists())
                    presetManager.createCategory(saveCategory);
                
                auto result = presetManager.savePreset(presetName, saveCategory);
                
                if (result.wasOk())
                {
                    refreshCategories();
                    refreshPresets();
                }
                else
                {
                    DBG("[PRESET BROWSER] Failed to save: " << result.getErrorMessage());
                }
            }
        });
}

void PresetBrowserComponent::handleNewCategory()
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("New Category")
            .withMessage("Enter category name:")
            .withButton("Create")
            .withButton("Cancel"),
        [this](int result) {
            if (result == 1) // Create clicked
            {
                juce::String categoryName = "New Category"; // TODO: Get from text input
                
                auto createResult = presetManager.createCategory(categoryName);
                
                if (createResult.wasOk())
                {
                    refreshCategories();
                }
                else
                {
                    DBG("[PRESET BROWSER] Failed to create category: " << createResult.getErrorMessage());
                }
            }
        });
}

void PresetBrowserComponent::handleLoadPreset(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= currentPresets.size())
        return;
    
    const auto& preset = currentPresets[presetIndex];
    
    auto result = presetManager.loadPreset(preset.file);
    
    if (result.wasOk())
    {
        DBG("[PRESET BROWSER] Loaded preset: " << preset.name);
        presetList.selectRow(presetIndex);
    }
    else
    {
        DBG("[PRESET BROWSER] Failed to load preset: " << result.getErrorMessage());
    }
}

void PresetBrowserComponent::handleToggleFavorite(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= currentPresets.size())
        return;
    
    auto& preset = currentPresets.getReference(presetIndex);
    bool newState = !preset.isFavorite;
    
    auto result = presetManager.setPresetFavorite(preset.category, preset.name, newState);
    
    if (result.wasOk())
    {
        preset.isFavorite = newState;
        
        // If we're viewing Favorites and user un-starred, remove from list
        if (selectedCategory == "Favorites" && !newState)
        {
            currentPresets.remove(presetIndex);
        }
        
        // Refresh the list
        presetList.updateContent();
        
        DBG("[PRESET BROWSER] Toggled favorite for: " << preset.name << " -> " << (newState ? "ON" : "OFF"));
    }
}

