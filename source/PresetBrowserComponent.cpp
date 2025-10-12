#include "PresetBrowserComponent.h"
#include "ui/Assets.h"

// ============================================================================================
// StarButton Implementation
// ============================================================================================

StarButton::StarButton()
{
    setSize(18, 18);
}

void StarButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    
    auto starPath = createStarPath(cx, cy, radius, radius * 0.4f);
    
    if (isOn)
    {
        // Filled star (favorite) - white
        g.setColour(juce::Colours::white);
        g.fillPath(starPath);
    }
    else
    {
        // Outline star (not favorite) - white
        g.setColour(juce::Colours::white);
        g.strokePath(starPath, juce::PathStrokeType(1.5f));
    }
}

void StarButton::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && contains(event.position.toInt()))
    {
        isOn = !isOn;
        repaint();
        
        if (onClick)
            onClick(isOn);
    }
}

void StarButton::setToggleState(bool shouldBeOn)
{
    isOn = shouldBeOn;
    repaint();
}

juce::Path StarButton::createStarPath(float cx, float cy, float outerRadius, float innerRadius) const
{
    juce::Path star;
    const int numPoints = 5;
    const float angleStep = juce::MathConstants<float>::twoPi / numPoints;
    
    for (int i = 0; i < numPoints * 2; ++i)
    {
        float angle = i * angleStep * 0.5f - juce::MathConstants<float>::halfPi;
        float radius = (i % 2 == 0) ? outerRadius : innerRadius;
        float x = cx + radius * std::cos(angle);
        float y = cy + radius * std::sin(angle);
        
        if (i == 0)
            star.startNewSubPath(x, y);
        else
            star.lineTo(x, y);
    }
    
    star.closeSubPath();
    return star;
}

// ============================================================================================
// CategoryButton Implementation
// ============================================================================================

void CategoryButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    // Draw the normal image button
    juce::ImageButton::paintButton(g, over, down);
    
    // Draw white bar on right edge if selected
    if (isSelected)
    {
        auto bounds = getLocalBounds();
        int barX = bounds.getWidth() - 5;
        int barY = 1;
        int barHeight = bounds.getHeight() - 2;
        
        g.setColour(juce::Colours::white);
        g.fillRect(barX, barY, 5, barHeight);
        
        DBG("[CategoryButton] Drawing white bar for: " + buttonName + 
            " at x=" + juce::String(barX) + 
            " y=" + juce::String(barY) + 
            " w=5 h=" + juce::String(barHeight));
    }
}

// ============================================================================================
// PresetRowComponent Implementation
// ============================================================================================

PresetRowComponent::PresetRowComponent()
{
    addAndMakeVisible(starButton);
    
    starButton.onClick = [this](bool newState) {
        if (onStarClick)
            onStarClick(newState);
    };
}

void PresetRowComponent::updateContent(const juce::String& presetName, bool isFav, bool isSelected)
{
    name = presetName;
    favorite = isFav;
    selected = isSelected;
    starButton.setToggleState(isFav);
    repaint();
}

void PresetRowComponent::paint(juce::Graphics& g)
{
    // No background - transparent
    
    // Preset name text (ASCII only, explicit font) - moved right 5px more: 28 + 5 = 33px
    g.setColour(selected ? juce::Colours::white : juce::Colour(0xFFCCCCCC));
    g.setFont(juce::FontOptions("Arial", 14.0f, juce::Font::plain));
    
    // Text starts at 33px
    auto textBounds = juce::Rectangle<int>(33, 0, getWidth() - 33 - 16, getHeight());
    g.drawText(name, textBounds, juce::Justification::centredLeft, true);
    
    // Bottom separator
    g.setColour(juce::Colour(0xFF3A3A3A));
    g.drawLine(0.0f, (float)getHeight() - 0.5f, (float)getWidth(), (float)getHeight() - 0.5f, 1.0f);
}

void PresetRowComponent::resized()
{
    // Star button moved right 5px more: 2 + 5 = 7px
    starButton.setBounds(7, (getHeight() - 18) / 2, 18, 18);
}

void PresetRowComponent::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && !starButton.getBounds().contains(event.position.toInt()))
    {
        if (onPresetClick)
            onPresetClick();
    }
}

void PresetRowComponent::mouseDown(const juce::MouseEvent& event)
{
    // Right-click shows context menu
    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem("Show in Finder", [this]() {
            if (onShowInFinder)
                onShowInFinder();
        });
        menu.addSeparator();
        menu.addItem("Delete", [this]() {
            if (onDelete)
                onDelete();
        });
        
        menu.showMenuAsync(juce::PopupMenu::Options());
    }
}

// ============================================================================================
// PresetBrowserOverlay Implementation
// ============================================================================================

PresetBrowserOverlay::PresetBrowserOverlay(PresetManager& manager, const UiAssets& uiAssets)
    : presetManager(manager), assets(uiAssets), categoryContainer(&categoryViewport)
{
    setOpaque(true);
    setAlwaysOnTop(true);
    
    // Setup category tabs viewport (scrollable)
    categoryViewport.setScrollBarsShown(false, false); // No visible scrollbars, but still scrollable
    categoryViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::nonHover); // Enable drag scrolling
    categoryViewport.setViewedComponent(&categoryContainer, false);
    categoryViewport.setInterceptsMouseClicks(true, true); // Intercept mouse events for scrolling
    categoryContainer.setInterceptsMouseClicks(true, true); // Allow buttons to receive clicks but also allow scrolling
    addAndMakeVisible(categoryViewport);
    
    // Create category buttons (scale down large PNGs to fit sidebar) - add them in order
    auto addCategoryButton = [this](const juce::String& name, const juce::Image& image, int& yPos) {
        if (image.isValid())
        {
            auto button = std::make_unique<CategoryButton>(name);
            
            // Scale down to fit in sidebar, 10% smaller (1427px / 8.06 = ~177px)
            int width = static_cast<int>(image.getWidth() / 8.06f);
            int height = static_cast<int>(image.getHeight() / 8.06f);
            
            // Use the original high-quality image and let JUCE scale it smoothly
            // No hover/click highlighting - all states use the same image
            button->setImages(false, true, true,
                            image, 1.0f, juce::Colours::transparentBlack,
                            image, 1.0f, juce::Colours::transparentBlack,
                            image, 1.0f, juce::Colours::transparentBlack);
            
            button->setBounds(0, yPos, width, height);
            button->setMouseClickGrabsKeyboardFocus(false); // Don't grab focus
            button->setWantsKeyboardFocus(false); // Don't want keyboard focus
            button->setInterceptsMouseClicks(true, false); // Intercept own clicks but not children
            
            juce::String categoryName = name; // Capture by value
            button->onClick = [this, categoryName]() {
                handleCategorySelected(categoryName);
            };
            
            categoryContainer.addAndMakeVisible(button.get());
            categoryButtons.push_back(std::move(button));
            
            yPos += height + 1; // 1px margin between buttons
        }
    };
    
    int yPos = 15; // Start 15px down
    addCategoryButton("FAVORITES", assets.favoritesMenuTab, yPos);
    addCategoryButton("Rhythmic", assets.rhythmicMenuTab, yPos);
    addCategoryButton("Distort", assets.distortMenuTab, yPos);
    addCategoryButton("Lofi", assets.lofiMenuTab, yPos);
    addCategoryButton("Bass", assets.bassMenuTab, yPos);
    addCategoryButton("GuitarSynth", assets.guitarSynthMenuTab, yPos);
    addCategoryButton("User", assets.userMenuTab, yPos);
    
    // Set container size based on total height (width adjusted for smaller tabs)
    categoryContainer.setSize(270, yPos > 0 ? yPos - 1 : 0); // Wide enough for 177px tabs with padding
    
    // Presets list (center of screen, no groups)
    presetsModel = std::make_unique<PresetsListModel>(*this);
    presetsList.setModel(presetsModel.get());
    presetsList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF131313));
    presetsList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    presetsList.setRowHeight(40);
    addAndMakeVisible(presetsList);
}

PresetBrowserOverlay::~PresetBrowserOverlay()
{
    presetsList.setModel(nullptr);
}

void PresetBrowserOverlay::paint(juce::Graphics& g)
{
    // Black background (#131313)
    g.fillAll(juce::Colour(0xFF131313));
}

void PresetBrowserOverlay::resized()
{
    auto bounds = getLocalBounds();
    
    // Left side: Category viewport (adjusted for smaller tabs)
    auto categoryArea = juce::Rectangle<int>(10, 0, 270, bounds.getHeight());
    categoryViewport.setBounds(categoryArea);
    
    // Right side: Presets list - starts at 220px to allow room for stars/text at negative coords
    // Star is at -50, so we need list to start at least 50px before the visible area (280-60=220)
    auto presetsArea = juce::Rectangle<int>(220, 10, bounds.getWidth() - 220 - 20, bounds.getHeight() - 20);
    presetsList.setBounds(presetsArea);
}

void PresetBrowserOverlay::mouseDown(const juce::MouseEvent& event)
{
    // If click is outside the preset browser bounds, close it
    if (!getLocalBounds().contains(event.getPosition()))
    {
        if (onClose)
            onClose();
    }
}

void PresetBrowserOverlay::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Forward mouse wheel events to the category viewport when over the category area
    if (categoryViewport.getBounds().contains(event.getPosition()))
    {
        categoryViewport.mouseWheelMove(event.withNewPosition(event.position - categoryViewport.getPosition().toFloat()), wheel);
    }
    else
    {
        // Let the component handle it normally (for presets list scrolling)
        juce::Component::mouseWheelMove(event, wheel);
    }
}

void PresetBrowserOverlay::show()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    // Scan presets
    presetManager.refresh();
    
    // Default to Rhythmic category
    handleCategorySelected("Rhythmic");
    
    setVisible(true);
}


void PresetBrowserOverlay::handlePresetSelected(int index)
{
    if (index >= 0 && index < currentPresets.size())
    {
        const auto& preset = currentPresets[index];
        auto result = presetManager.loadPreset(preset.file);
        
        if (result.wasOk() && onPresetLoaded)
        {
            onPresetLoaded(preset.name);
        }
        
        if (result.failed())
        {
            DBG("[PresetBrowser] Load failed: " + result.getErrorMessage());
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Load Error",
                result.getErrorMessage());
        }
        else
        {
            DBG("[PresetBrowser] Loaded preset: " + preset.name);
        }
    }
}

void PresetBrowserOverlay::handleStarToggle(int index)
{
    if (index >= 0 && index < currentPresets.size())
    {
        auto preset = currentPresets[index];
        bool newState = !preset.favorite;
        
        presetManager.setFavorite(preset.file, newState);
        
        // Update local copy
        currentPresets.getReference(index).favorite = newState;
        
        // Just repaint the row
        presetsList.repaintRow(index);
    }
}

void PresetBrowserOverlay::handleSavePreset()
{
    // Always save to "User" group (no category selection)
    juce::String targetGroup = "User";
    
    // Generate a unique name
    juce::String name = "Preset_" + juce::String(juce::Time::currentTimeMillis() % 10000);
    
    auto result = presetManager.saveCurrentStateAsPreset(name, targetGroup);
    
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Save Error",
            result.getErrorMessage());
    }
    else
    {
        DBG("[PresetBrowser] Saved preset: " + name);
        // Refresh to show new preset
        show();
    }
}


void PresetBrowserOverlay::handleCloseButton()
{
    setVisible(false);
    if (onClose)
        onClose();
}

void PresetBrowserOverlay::handleCategorySelected(const juce::String& category)
{
    DBG("[PresetBrowser] Category selected: " + category);
    selectedCategory = category;
    
    // Reload presets filtered by category
    currentPresets.clear();
    
    if (category == "FAVORITES")
    {
        currentPresets = presetManager.getPresetsInGroup("FAVORITES");
    }
    else if (category.isEmpty())
    {
        // Show all presets
        auto groups = presetManager.getGroups();
        for (const auto& group : groups)
        {
            if (group != "FAVORITES")
            {
                auto presetsInGroup = presetManager.getPresetsInGroup(group);
                currentPresets.addArray(presetsInGroup);
            }
        }
    }
    else
    {
        // Show presets from specific category/group
        currentPresets = presetManager.getPresetsInGroup(category);
    }
    
    presetsList.updateContent();
    presetsList.repaint();
    
    // Update button selection states
    for (size_t i = 0; i < categoryButtons.size(); ++i)
    {
        auto* button = categoryButtons[i].get();
        if (button)
        {
            bool shouldBeSelected = (button->getName() == category);
            if (auto* categoryBtn = dynamic_cast<CategoryButton*>(button))
            {
                categoryBtn->setSelected(shouldBeSelected);
            }
        }
    }
}

// ============================================================================================
// PresetsListModel Implementation
// ============================================================================================

int PresetBrowserOverlay::PresetsListModel::getNumRows()
{
    return overlay.currentPresets.size();
}

juce::Component* PresetBrowserOverlay::PresetsListModel::refreshComponentForRow(int row, bool isSelected, juce::Component* existingComponent)
{
    if (row < 0 || row >= overlay.currentPresets.size())
    {
        delete existingComponent;
        return nullptr;
    }
    
    PresetRowComponent* rowComp = dynamic_cast<PresetRowComponent*>(existingComponent);
    if (rowComp == nullptr)
        rowComp = new PresetRowComponent();
    
    const auto& preset = overlay.currentPresets[row];
    rowComp->updateContent(preset.name, preset.favorite, isSelected);
    
    rowComp->onPresetClick = [this, row]() {
        overlay.handlePresetSelected(row);
    };
    
    rowComp->onStarClick = [this, row](bool) {
        overlay.handleStarToggle(row);
    };
    
    rowComp->onShowInFinder = [this, row]() {
        if (row >= 0 && row < overlay.currentPresets.size())
        {
            overlay.currentPresets[row].file.revealToUser();
        }
    };
    
    rowComp->onDelete = [this, row]() {
        if (row >= 0 && row < overlay.currentPresets.size())
        {
            const auto& preset = overlay.currentPresets[row];
            
            // Confirm deletion
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Delete Preset")
                    .withMessage("Are you sure you want to delete \"" + preset.name + "\"?")
                    .withButton("Delete")
                    .withButton("Cancel"),
                [this, preset](int result) {
                    if (result == 1) // Delete confirmed
                    {
                        if (preset.file.deleteFile())
                        {
                            DBG("[PresetBrowser] Deleted preset: " + preset.name);
                            overlay.show(); // Refresh the list
                        }
                        else
                        {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::AlertWindow::WarningIcon,
                                "Delete Error",
                                "Failed to delete preset file"
                            );
                        }
                    }
                }
            );
        }
    };
    
    return rowComp;
}
