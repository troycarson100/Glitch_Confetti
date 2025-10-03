#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class BigComboWithSvgLNF : public juce::LookAndFeel_V4
{
public:
    // Tunables
    float popupFontPx   = 18.0f;
    int   rowHeightPx   = 32;
    int   minPopupWidth = 300;
    int   closedHeight  = 36;   // closed ComboBox height for neat visuals
    int   closedPad     = 10;   // left/right padding in closed control

    // Provide your SVGs here
    std::unique_ptr<juce::Drawable> caretSVG;     // right side caret
    std::unique_ptr<juce::Drawable> leadingSVG;   // optional left icon (your "external text" badge)

    // Call these to set the SVGs (take ownership copies)
    void setCaretSVG (std::unique_ptr<juce::Drawable> d)   { caretSVG   = std::move(d); }
    void setLeadingSVG (std::unique_ptr<juce::Drawable> d) { leadingSVG = std::move(d); }

    // 1) Make the POPUP itself bigger (font + row height + width)
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(popupFontPx, juce::Font::plain);
    }

    juce::PopupMenu::Options getComboBoxPopupMenuOptions (juce::ComboBox& box,
                                                          juce::PopupMenu::Options options)
    {
        // Force larger row height and min width, anchored to the box
        return options.withStandardItemHeight(rowHeightPx)
                      .withMinimumWidth(minPopupWidth)
                      .withTargetComponent(&box);
    }

    // Legacy hook some JUCE versions still read:
    int getPopupMenuItemHeight() { return rowHeightPx; }

    // 2) Draw the CLOSED control with your SVGs — DOES NOT affect popup sizing
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);

        // No background - transparent
        g.fillAll(juce::Colours::transparentBlack);

        // Content bounds inside padding
        auto content = juce::Rectangle<int>(0, 0, width, height).reduced(closedPad);

        // Draw optional left icon ("external text" SVG)
        int leftIconW = 0;
        if (leadingSVG != nullptr)
        {
            leftIconW = juce::jmin(height - 8, 24); // keep tasteful size
            auto iconBounds = content.removeFromLeft(leftIconW).toFloat();
            iconBounds = iconBounds.withSizeKeepingCentre((float)leftIconW, (float)leftIconW);
            leadingSVG->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
            content.removeFromLeft(6); // gap after icon
        }

        // Draw right caret SVG (smaller size)
        int caretW = 0;
        if (caretSVG != nullptr)
        {
            caretW = juce::jmin(height - 8, 12); // Reduced from 20 to 12 for smaller carrot
            auto caretArea = content.removeFromRight(caretW).toFloat();
            caretArea = caretArea.withSizeKeepingCentre((float)caretW, (float)caretW);
            caretSVG->drawWithin(g, caretArea, juce::RectanglePlacement::centred, 1.0f);
            content.removeFromRight(6); // gap before caret
        }

        // Don't draw any text - only the carrot SVG
    }

    // Make the text area vertically centered in closed control
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        // Hide the text label completely
        label.setBounds(0, 0, 0, 0);
        label.setVisible(false);
    }

    // Slightly thicker popup border and padded background (optional cosmetics)
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll(juce::Colour(0xFF202225));
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRect(0, 0, w, h, 1);
    }
};
