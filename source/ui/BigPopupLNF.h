#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// A LookAndFeel that makes PopupMenu (and ComboBox popup) larger and readable.
class BigPopupLNF : public juce::LookAndFeel_V4
{
public:
    // Scale factors you can tweak
    float popupFontHeight = 18.0f;     // px
    int   rowHeight       = 30;        // px per item
    int   minWidth        = 260;       // px minimum width of the popup
    
    // Carrot images for the dropdown
    std::unique_ptr<juce::Drawable> inactiveCarrot;
    std::unique_ptr<juce::Drawable> activeCarrot;

    // Make popup menu text bigger
    juce::Font getPopupMenuFont() override
    {
        return juce::Font (popupFontHeight, juce::Font::plain);
    }

    // Tell ComboBox how to build its PopupMenu::Options
    juce::PopupMenu::Options getComboBoxPopupMenuOptions (juce::ComboBox& box, juce::PopupMenu::Options options)
    {
        // Enforce both a bigger row height and a bigger min width
        options = options.withStandardItemHeight (rowHeight)
                         .withMinimumWidth (minWidth)
                         .withTargetComponent (&box);
        return options;
    }
    
    // Override ComboBox drawing to show carrot instead of arrow
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        // Don't draw the default ComboBox appearance
        g.fillAll(juce::Colours::transparentBlack);
    }
    
    // Override arrow drawing to show carrot
    void drawComboBoxText (juce::Graphics& g, juce::ComboBox& box, juce::Label& label)
    {
        // Don't draw text
    }
    
    // Override button drawing to show carrot
    void drawComboBoxButton (juce::Graphics& g, int x, int y, int width, int height,
                             bool isButtonDown, bool isButtonOver, juce::ComboBox& box)
    {
        // Draw carrot instead of default arrow
        auto carrotArea = juce::Rectangle<float>(x, y, width, height).reduced(2.0f);
        
        if (box.isPopupActive() && activeCarrot != nullptr) {
            activeCarrot->drawWithin(g, carrotArea, juce::RectanglePlacement::centred, 1.0f);
        } else if (inactiveCarrot != nullptr) {
            inactiveCarrot->drawWithin(g, carrotArea, juce::RectanglePlacement::centred, 1.0f);
        }
    }

    // (Optional) slightly larger checkmarks/ticks etc. via icon size = rowHeight if you want
    int getPopupMenuBorderSize() override      { return 8; }  // a bit more padding looks nicer
    int getPopupMenuItemHeight()               { return rowHeight; } // some JUCE versions also query this
    
    // Set carrot images
    void setCarrotImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active)
    {
        inactiveCarrot = std::move(inactive);
        activeCarrot = std::move(active);
    }
};
