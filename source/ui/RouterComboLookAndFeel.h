#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Custom LookAndFeel for router effect selector dropdowns
 * - Small button (12x12 for carrot icon)
 * - Large popup menu (independent of button size)
 */
class RouterComboLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        // Don't draw anything - completely transparent
        // The carrot SVG will be painted separately in PluginEditor::paint()
        juce::ignoreUnused(g, width, height, box);
    }
    
    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(16.0f); // Larger font for popup menu
    }
    
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        // Hide text completely when closed
        label.setBounds(0, 0, 0, 0);
        label.setFont(getComboBoxFont(box));
    }
    
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label&) override
    {
        // Create larger popup menu independent of button size
        auto options = juce::PopupMenu::Options()
            .withTargetComponent(&box)
            .withMinimumWidth(150)        // Much wider than the 12px button
            .withMaximumNumColumns(1)
            .withStandardItemHeight(28);  // Comfortable row height
        
        return options;
    }
};

