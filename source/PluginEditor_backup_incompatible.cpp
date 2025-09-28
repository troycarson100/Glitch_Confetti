#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "FontManager.h"

//==============================================================================
// CustomKnob Implementation
//==============================================================================

CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 100.0, 1.0);
    setValue(50.0);
}

// CustomStepButton implementation
CustomStepButton::CustomStepButton(int stepIndex) : juce::Button("Step" + juce::String(stepIndex)), stepIndex(stepIndex)
{
    setClickingTogglesState(false);
}

void CustomStepButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Save the graphics state
    g.saveState();
    
    // Apply 70% opacity if button is inactive
    if (inactive)
    {
        g.setOpacity(0.7f);
    }
    
    // Draw the appropriate SVG based on active state
    if (active && activeImage != nullptr)
    {
        activeImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else if (!active && inactiveImage != nullptr)
    {
        inactiveImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback drawing if SVGs not loaded
        g.setColour(active ? juce::Colours::orange : juce::Colours::darkgrey);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        g.setColour(juce::Colours::white);
        auto stepFont = FontManager::getInstance().getFont("Akira Expanded", 12.0f, juce::Font::bold);
        g.setFont(stepFont);
        g.drawText(juce::String(stepIndex + 1), bounds, juce::Justification::centred);
    }
    
    // Restore the graphics state
    g.restoreState();
}

void CustomStepButton::clicked()
{
    // This will be handled by the parent PluginEditor
}

void CustomStepButton::setActive(bool active)
{
    if (this->active != active)
    {
        this->active = active;
        repaint();
    }
}

void CustomStepButton::setInactive(bool inactive)
{
    if (this->inactive != inactive)
    {
        this->inactive = inactive;
        repaint();
    }
}

void CustomStepButton::setActiveImage(std::unique_ptr<juce::Drawable> activeImage)
{
    this->activeImage = std::move(activeImage);
}

void CustomStepButton::setInactiveImage(std::unique_ptr<juce::Drawable> inactiveImage)
{
    this->inactiveImage = std::move(inactiveImage);
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw the ring (static background) if available
    if (ringImage != nullptr)
    {
        ringImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback ring - simple dark circle
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(bounds);
        g.setColour(juce::Colours::lightgrey);
        g.drawEllipse(bounds, 2.0f);
    }
    
    // Draw the inner part (rotating) if available
    if (innerImage != nullptr)
    {
        // Calculate rotation angle based on slider value (300° total rotation centered on straight up)
        auto value = getValue();
        auto range = getMaximum() - getMinimum();
        auto normalizedValue = (value - getMinimum()) / range;
        // Convert to 300° total rotation centered on 0° (straight up)
        // Range: -150° to +150° (300° total, with 0° at 50% value)
        auto angle = (normalizedValue - 0.5) * 300.0 * juce::MathConstants<double>::pi / 180.0;
        
        // Make inner image 10% smaller than the ring
        auto innerBounds = bounds.reduced(bounds.getWidth() * 0.10f, bounds.getHeight() * 0.10f);
        
        // Position the inner image at the bottom of the knob bounds, moved up 4px
        innerBounds = innerBounds.withY(bounds.getBottom() - innerBounds.getHeight() - 4);
        
        // Calculate the center of the inner knob for rotation pivot
        auto innerCenterX = innerBounds.getCentreX();
        auto innerCenterY = innerBounds.getCentreY();
        
        // Apply rotation transform around the center of the inner knob itself
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(static_cast<float>(angle), 
                                                      innerCenterX, 
                                                      innerCenterY));
        
        // Draw the inner image centered horizontally but bottom-aligned
        innerImage->drawWithin(g, innerBounds, juce::RectanglePlacement::centred, 1.0f);
        g.restoreState();
    }
    else
    {
        // Draw a simple indicator line as fallback
        g.setColour(juce::Colours::white);
        auto value = getValue();
        auto range = getMaximum() - getMinimum();
        auto normalizedValue = (value - getMinimum()) / range;
        auto angle = normalizedValue * 2.0 * juce::MathConstants<double>::pi - juce::MathConstants<double>::pi;
        
        auto centre = bounds.getCentre();
        auto radius = bounds.getWidth() * 0.3f;
        auto endX = centre.x + radius * cos(angle);
        auto endY = centre.y + radius * sin(angle);
        
        g.drawLine(centre.x, centre.y, endX, endY, 3.0f);
    }
}

void CustomKnob::resized()
{
    // The knob will fill the entire bounds
}

void CustomKnob::setRingImage(std::unique_ptr<juce::Drawable> newRingImage)
{
    ringImage = std::move(newRingImage);
}

void CustomKnob::setInnerImage(std::unique_ptr<juce::Drawable> newInnerImage)
{
    innerImage = std::move(newInnerImage);
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Set the size to match our desired dimensions (fixed size)
    setSize (974, 532);
    
    // Lock the size - no resizing
    setResizable (false, false);
    
    // Load SVG assets
    loadSVGAssets();
    
    // Setup UI components
    setupStepButtons();
    setupKnobs();
    setupLabels();
    setupValueLabels();
    setupStepControls();
    
    // Initialize snapshots
    initializeSnapshots();
    
    // Update step button visibility based on initial step count
    updateStepButtonVisibility();
    
    // Force resized to be called to position components
    resized();
    
    // Start timer to update value labels
    startTimer(50); // Update every 50ms
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Draw background maintaining aspect ratio
    if (backgroundImage != nullptr)
    {
        backgroundImage->drawWithin (g, getLocalBounds().toFloat(), 
                                   juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback background
        g.fillAll (juce::Colour (0xff2d2d2d));
    }
    
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
    
    
    // Layout step sequencer (left side, bottom) - moved down 40px, right 10px
    auto stepArea = juce::Rectangle<int>(20, bounds.getHeight() - 160, 420, 150);
    stepArea.reduce (20, 20);
    
    // Position step controls at the top of the step area, moved 140px to the right (40 + 100)
    if (stepCountBox != nullptr)
    {
        stepCountBox->setBounds(stepArea.getX() + 140, stepArea.getY() - 30, 40, 25);
    }
    
    if (rateComboBox != nullptr)
    {
        rateComboBox->setBounds(stepArea.getX() + 190, stepArea.getY() - 30, 80, 25);
    }
    
    if (rateTypeButton != nullptr)
    {
        rateTypeButton->setBounds(stepArea.getX() + 280, stepArea.getY() - 30, 25, 25);
    }
    
    // Arrange step buttons in 2 rows of 8 - 3% more smaller and moved down 5px
    const int buttonSize = 42;  // 3% more smaller (43 * 0.97 = 41.71, rounded to 42)
    const int spacing = 8;      // More spacing (was 5)
    const int buttonsPerRow = 8;
    const int totalWidth = buttonsPerRow * buttonSize + (buttonsPerRow - 1) * spacing;
    const int totalHeight = 2 * buttonSize + spacing;
    
    // Center the grid within the step area and move down 5px
    int startX = stepArea.getX() + (stepArea.getWidth() - totalWidth) / 2;
    int startY = stepArea.getY() + (stepArea.getHeight() - totalHeight) / 2 + 5;
    
    for (int i = 0; i < 16; ++i)
    {
        if (i < static_cast<int>(stepButtons.size()))
        {
            int row = i / buttonsPerRow;
            int col = i % buttonsPerRow;
            
            int x = startX + col * (buttonSize + spacing);
            int y = startY + row * (buttonSize + spacing);
            
            stepButtons[i]->setBounds (x, y, buttonSize, buttonSize);
        }
    }
    
    // Position knobs in the same area as the debug knobs
    const int knobSize = 69; // 15% bigger than 60 (60 * 1.15 = 69)
    const int cols = 4;
    const int rows = 2;
    
    // Use the same positioning as the debug knobs
    int knobStartX = 50;
    int knobStartY = 100;
    
    // Move bottom row down 35px (20px + 15px)
    int bottomRowOffset = 35;
    
        for (int i = 0; i < static_cast<int>(knobs.size()); ++i)
        {
            int row = i / cols;
            int col = i % cols;
            
        int x = knobStartX + col * (knobSize + 30);
        int y = knobStartY + row * (knobSize + 40) + (row > 0 ? bottomRowOffset : 0) + (row == 0 ? 20 : 0);
        
        knobs[i]->setBounds (x, y, knobSize, knobSize);
        
        // Make sure the knob is visible
        knobs[i]->setVisible(true);
        knobs[i]->setEnabled(true);
        
        // Position title above knob
        if (i < static_cast<int>(labels.size()))
        {
            labels[i]->setBounds (x, y - 20, knobSize, 15);
        }
        
            // Position value label below knob, moved up 8px
            if (i < static_cast<int>(valueLabels.size()))
            {
                valueLabels[i]->setBounds (x, y + knobSize + 5 - 8, knobSize, 15);
            }
    }
    
    // Debug: Force repaint to ensure components are visible
    repaint();
}

void PluginEditor::loadSVGAssets()
{
    DBG("=== Loading SVG Assets ===");
    
    // Load background with proper error checking
    juce::String backgroundData(BinaryData::Background_Mustard_svg, BinaryData::Background_Mustard_svgSize);
    auto backgroundXML = juce::XmlDocument::parse(backgroundData);
    if (backgroundXML != nullptr)
    {
        backgroundImage = juce::Drawable::createFromSVG(*backgroundXML);
        if (backgroundImage != nullptr)
            DBG("✓ Background loaded");
        else
            DBG("✗ Background createFromSVG failed");
    }
    else
    {
        DBG("✗ Background XML parse failed");
    }
    
    // Load step button images
    juce::String stepInactiveData(BinaryData::Step_Inactive_svg, BinaryData::Step_Inactive_svgSize);
    auto stepInactiveXML = juce::XmlDocument::parse(stepInactiveData);
    if (stepInactiveXML != nullptr)
    {
        stepInactiveImage = juce::Drawable::createFromSVG(*stepInactiveXML);
        if (stepInactiveImage != nullptr)
            DBG("✓ Step Inactive loaded");
        else
            DBG("✗ Step Inactive createFromSVG failed");
    }
    else
    {
        DBG("✗ Step Inactive XML parse failed");
    }
    
    juce::String stepActiveData(BinaryData::Step_Active_svg, BinaryData::Step_Active_svgSize);
    auto stepActiveXML = juce::XmlDocument::parse(stepActiveData);
    if (stepActiveXML != nullptr)
    {
        stepActiveImage = juce::Drawable::createFromSVG(*stepActiveXML);
        if (stepActiveImage != nullptr)
            DBG("✓ Step Active loaded");
        else
            DBG("✗ Step Active createFromSVG failed");
    }
    else
    {
        DBG("✗ Step Active XML parse failed");
    }
    
    // Load knob ring with proper error checking
    juce::String ringData(BinaryData::Knob_Basic_Ring_svg, BinaryData::Knob_Basic_Ring_svgSize);
    auto ringXML = juce::XmlDocument::parse(ringData);
    if (ringXML != nullptr)
    {
        knobRingImage = juce::Drawable::createFromSVG(*ringXML);
        if (knobRingImage != nullptr)
            DBG("✓ Knob Ring loaded");
        else
            DBG("✗ Knob Ring createFromSVG failed");
    }
    else
    {
        DBG("✗ Knob Ring XML parse failed");
    }
    
    // Load knob inner with proper error checking
    juce::String innerData(BinaryData::Knob_Basic_Inside_svg, BinaryData::Knob_Basic_Inside_svgSize);
    auto innerXML = juce::XmlDocument::parse(innerData);
    if (innerXML != nullptr)
    {
        knobInnerImage = juce::Drawable::createFromSVG(*innerXML);
        if (knobInnerImage != nullptr)
            DBG("✓ Knob Inner loaded");
        else
            DBG("✗ Knob Inner createFromSVG failed");
    }
    else
    {
        DBG("✗ Knob Inner XML parse failed");
    }
    
    DBG("=== SVG Loading Complete ===");
}

void PluginEditor::timerCallback()
{
    // Update value labels with current knob values
    for (int i = 0; i < static_cast<int>(valueLabels.size()) && i < static_cast<int>(knobs.size()); ++i)
    {
        if (valueLabels[i] != nullptr && knobs[i] != nullptr)
        {
            auto value = knobs[i]->getValue();
            valueLabels[i]->setText(juce::String(value, 1), juce::dontSendNotification);
        }
    }
    
    // Update step sequencer automation
    updateStepSequencer();
}

void PluginEditor::setupStepButtons()
{
    stepButtons.clear();
    
    // Create 16 step buttons (2 rows of 8)
    for (int i = 0; i < 16; ++i)
    {
        auto button = std::make_unique<CustomStepButton>(i);
        
        // Set the first button as active by default
        button->setActive(i == currentActiveStep);
        
        // Set up click handler
        button->onClick = [this, i]() {
            // Save current knob values to the previous active step
            if (currentActiveStep < static_cast<int>(stepButtons.size()))
            {
                saveSnapshot(currentActiveStep);
                stepButtons[currentActiveStep]->setActive(false);
            }
            
            // Activate clicked step
            currentActiveStep = i;
            stepButtons[i]->setActive(true);
            
            // Restore snapshot values for the clicked step
            restoreSnapshot(i);
        };
        
        // Set SVG images if available
        if (stepActiveImage != nullptr)
        {
            button->setActiveImage(stepActiveImage->createCopy());
        }
        if (stepInactiveImage != nullptr)
        {
            button->setInactiveImage(stepInactiveImage->createCopy());
        }
        
        addAndMakeVisible (*button);
        stepButtons.push_back (std::move(button));
    }
    
    // Debug output
    DBG("Created " << stepButtons.size() << " step buttons");
}

void PluginEditor::setupKnobs()
{
    knobs.clear();
    knobAttachments.clear();
    
    std::vector<std::string> knobNames = {
        "time", "feedback", "wowDepth", "wowRate", 
        "drive", "hiCut", "lowCut", "mix"
    };
    
    for (const auto& name : knobNames)
    {
        auto knob = std::make_unique<CustomKnob>();
        
        // Set the SVG images for this knob
        if (knobRingImage != nullptr)
        {
            auto ringCopy = knobRingImage->createCopy();
            knob->setRingImage(std::move(ringCopy));
        }
        
        if (knobInnerImage != nullptr)
        {
            auto innerCopy = knobInnerImage->createCopy();
            knob->setInnerImage(std::move(innerCopy));
        }
        
        // Create parameter attachment
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getValueTreeState(), name, *knob);
        
        addAndMakeVisible (*knob);
        knobs.push_back (std::move(knob));
        knobAttachments.push_back (std::move(attachment));
    }
}

void PluginEditor::setupLabels()
{
    labels.clear();
    
    std::vector<std::string> labelNames = {
        "Time", "Feedback", "Wow Depth", "Wow Rate", 
        "Drive", "Hi-Cut", "Low-Cut", "Mix"
    };
    
    for (const auto& name : labelNames)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText (name, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, juce::Colours::white);
        // Use AlteHaasGroteskBold font
        auto customFont = FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold);
        DBG("Knob label font: " + customFont.getTypefaceName());
        label->setFont (customFont);
        
        addAndMakeVisible (*label);
        labels.push_back (std::move(label));
    }
}

void PluginEditor::setupValueLabels()
{
    // Create value display labels for knobs
    for (int i = 0; i < 8; ++i)
    {
        auto valueLabel = std::make_unique<juce::Label>();
        valueLabel->setText ("0", juce::dontSendNotification);
        valueLabel->setJustificationType (juce::Justification::centred);
        valueLabel->setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        // Use AlteHaasGroteskBold font for value labels too
        auto customFont = FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain);
        valueLabel->setFont (customFont);
        addAndMakeVisible (*valueLabel);
        valueLabels.push_back (std::move (valueLabel));
    }
}

// DraggableNumberBox implementation
DraggableNumberBox::DraggableNumberBox()
{
    setSize(40, 25);
    
    // Create text editor for typing
    textEditor = std::make_unique<juce::TextEditor>();
    textEditor->setVisible(false);
    textEditor->setInputRestrictions(2, "0123456789"); // Only allow digits, max 2 characters
    textEditor->setJustification(juce::Justification::centred);
    textEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    textEditor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    textEditor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    textEditor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    textEditor->addListener(this);
    addAndMakeVisible(*textEditor);
}

void DraggableNumberBox::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw white stroke with 4px width and 2px radius
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(bounds, 2.0f, 4.0f);
    
    // Only draw the number if not editing
    if (!isEditing)
    {
        g.setColour(juce::Colours::white);
        // Use Akira Expanded font for step section
        auto customFont = FontManager::getInstance().getFont("Akira Expanded", 12.0f, juce::Font::bold);
        DBG("Step count font: " + customFont.getTypefaceName());
        g.setFont(customFont);
        g.drawText(juce::String(currentValue), bounds, juce::Justification::centred);
    }
}

void DraggableNumberBox::mouseDown(const juce::MouseEvent& event)
{
    if (!isEditing)
    {
        isDragging = true;
        lastMouseX = event.getMouseDownX();
    }
}

void DraggableNumberBox::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDragging || isEditing) return;
    
    int deltaX = event.getMouseDownX() - lastMouseX;
    int sensitivity = 3; // Pixels per step - even more responsive
    
    if (abs(deltaX) >= sensitivity)
    {
        int direction = deltaX > 0 ? 1 : -1; // Drag right = increase, drag left = decrease
        int newValue = juce::jlimit(minValue, maxValue, currentValue + direction);
        
        if (newValue != currentValue)
        {
            currentValue = newValue;
            lastMouseX = event.getMouseDownX();
            repaint();
            
            if (onValueChanged)
                onValueChanged(currentValue);
        }
    }
}

void DraggableNumberBox::mouseUp(const juce::MouseEvent& event)
{
    isDragging = false;
}

void DraggableNumberBox::mouseDoubleClick(const juce::MouseEvent& event)
{
    // Start text editing mode
    isEditing = true;
    textEditor->setVisible(true);
    textEditor->setText(juce::String(currentValue));
    textEditor->setBounds(getLocalBounds());
    textEditor->grabKeyboardFocus();
    textEditor->selectAll();
    repaint();
}

void DraggableNumberBox::textEditorTextChanged(juce::TextEditor& editor)
{
    // Validate input as user types
    auto text = editor.getText();
    int value = text.getIntValue();
    
    if (value < minValue || value > maxValue)
    {
        // If invalid, revert to current value
        editor.setText(juce::String(currentValue));
    }
}

void DraggableNumberBox::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    // Accept the value and exit editing mode
    int newValue = editor.getText().getIntValue();
    newValue = juce::jlimit(minValue, maxValue, newValue);
    
    if (newValue != currentValue)
    {
        currentValue = newValue;
        if (onValueChanged)
            onValueChanged(currentValue);
    }
    
    exitEditingMode();
}

void DraggableNumberBox::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    // Cancel editing and revert to original value
    exitEditingMode();
}

void DraggableNumberBox::textEditorFocusLost(juce::TextEditor& editor)
{
    // Accept the value when focus is lost
    int newValue = editor.getText().getIntValue();
    newValue = juce::jlimit(minValue, maxValue, newValue);
    
    if (newValue != currentValue)
    {
        currentValue = newValue;
        if (onValueChanged)
            onValueChanged(currentValue);
    }
    
    exitEditingMode();
}

void DraggableNumberBox::setValue(int value)
{
    currentValue = juce::jlimit(minValue, maxValue, value);
    repaint();
}

void DraggableNumberBox::exitEditingMode()
{
    isEditing = false;
    textEditor->setVisible(false);
    repaint();
}

//==============================================================================
// CustomComboBox Implementation
//==============================================================================

CustomComboBox::CustomComboBox()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
}

void CustomComboBox::paint(juce::Graphics& g)
{
    // Call the base class paint to draw the combo box
    juce::ComboBox::paint(g);
    
    // Override the text with Akira Expanded font
    auto bounds = getLocalBounds().toFloat();
    auto customFont = FontManager::getInstance().getFont("Akira Expanded", 12.0f, juce::Font::bold);
    g.setFont(customFont);
    g.setColour(juce::Colours::white);
    
    // Draw the selected text
    auto textBounds = bounds.reduced(4.0f);
    g.drawText(getText(), textBounds, juce::Justification::centred);
}

//==============================================================================
// CustomTextButton Implementation
//==============================================================================

CustomTextButton::CustomTextButton()
{
    setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

void CustomTextButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    // Call the base class paint to draw the button
    juce::TextButton::paintButton(g, isMouseOverButton, isButtonDown);
    
    // Override the text with Akira Expanded font
    auto bounds = getLocalBounds().toFloat();
    auto customFont = FontManager::getInstance().getFont("Akira Expanded", 12.0f, juce::Font::bold);
    g.setFont(customFont);
    g.setColour(juce::Colours::white);
    
    // Draw the button text
    g.drawText(getButtonText(), bounds, juce::Justification::centred);
}

void PluginEditor::setupStepControls()
{
    // Create rate combo box with new values
    rateComboBox = std::make_unique<CustomComboBox>();
    rateComboBox->addItem("1", 1);
    rateComboBox->addItem("1/2", 2);
    rateComboBox->addItem("1/4", 3);
    rateComboBox->addItem("1/8", 4);
    rateComboBox->addItem("1/16", 5);
    rateComboBox->addItem("1/32", 6);
    rateComboBox->setSelectedId(1); // Default to 1 step per beat
    rateComboBox->onChange = [this]() {
        updateStepRate();
    };
    addAndMakeVisible(*rateComboBox);
    
    // Create rate type button (S/D/T)
    rateTypeButton = std::make_unique<CustomTextButton>();
    rateTypeButton->setButtonText("S");
    rateTypeButton->onClick = [this]() {
        rateType = (rateType + 1) % 3;
        switch (rateType) {
            case 0: rateTypeButton->setButtonText("S"); break; // Straight
            case 1: rateTypeButton->setButtonText("D"); break; // Triplet
            case 2: rateTypeButton->setButtonText("T"); break; // Dotted
        }
        updateStepRate();
    };
    addAndMakeVisible(*rateTypeButton);
    
    // Create draggable step count box
    stepCountBox = std::make_unique<DraggableNumberBox>();
    stepCountBox->setValue(16);
    stepCountBox->onValueChanged = [this](int value) {
        stepCount = value;
        // Ensure current step is within valid range
        if (currentStep >= stepCount) {
            setActiveStep(0);
        }
        // Update visual state of step buttons
        updateStepButtonVisibility();
    };
    addAndMakeVisible(*stepCountBox);
}

// Snapshot functionality implementation
void PluginEditor::initializeSnapshots()
{
    // Initialize 16 steps with 8 parameters each
    stepSnapshots.resize(16);
    
    for (int step = 0; step < 16; ++step)
    {
        for (int param = 0; param < 8; ++param)
        {
            // Initialize with default values (50% for most parameters)
            stepSnapshots[step][param] = 50.0f;
        }
    }
}

void PluginEditor::saveSnapshot(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < static_cast<int>(stepSnapshots.size()) && 
        stepIndex < static_cast<int>(knobs.size()))
    {
        for (int i = 0; i < 8 && i < static_cast<int>(knobs.size()); ++i)
        {
            stepSnapshots[stepIndex][i] = knobs[i]->getValue();
        }
    }
}

void PluginEditor::restoreSnapshot(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < static_cast<int>(stepSnapshots.size()) && 
        stepIndex < static_cast<int>(knobs.size()))
    {
        for (int i = 0; i < 8 && i < static_cast<int>(knobs.size()); ++i)
        {
            knobs[i]->setValue(stepSnapshots[stepIndex][i], juce::dontSendNotification);
        }
    }
}

// Step sequencer automation implementation
void PluginEditor::updateStepSequencer()
{
    // Get transport info from the processor
    auto transport = processorRef.getPlayHead();
    if (transport == nullptr) return;
    
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;
    transport->getCurrentPosition(positionInfo);
    
    if (positionInfo.isPlaying)
    {
        // Calculate current beat position
        double currentBeatPosition = positionInfo.ppqPosition;
        
        // Calculate how many steps should have passed based on the rate
        double stepsPassed = (currentBeatPosition - lastBeatPosition) * stepRate;
        
        if (stepsPassed >= 1.0)
        {
            // Move to next step
            int newStep = (currentStep + static_cast<int>(stepsPassed)) % stepCount;
            setActiveStep(newStep);
            lastBeatPosition = currentBeatPosition;
        }
    }
    else
    {
        // Reset when not playing
        lastBeatPosition = 0.0;
    }
}

void PluginEditor::setActiveStep(int step)
{
    if (step < 0 || step >= stepCount) return;
    
    // Save current snapshot before switching
    if (currentActiveStep < static_cast<int>(stepButtons.size()))
    {
        saveSnapshot(currentActiveStep);
        stepButtons[currentActiveStep]->setActive(false);
    }
    
    // Set new active step
    currentActiveStep = step;
    currentStep = step;
    
    if (currentActiveStep < static_cast<int>(stepButtons.size()))
    {
        stepButtons[currentActiveStep]->setActive(true);
        restoreSnapshot(currentActiveStep);
    }
}

void PluginEditor::updateStepButtonVisibility()
{
    // Set step buttons beyond the step count to inactive (70% opacity)
    for (int i = 0; i < 16; ++i)
    {
        if (i < static_cast<int>(stepButtons.size()))
        {
            // Make steps beyond the step count inactive
            bool shouldBeInactive = (i >= stepCount);
            stepButtons[i]->setInactive(shouldBeInactive);
        }
    }
}

void PluginEditor::updateStepRate()
{
    if (rateComboBox == nullptr) return;
    
    int selectedId = rateComboBox->getSelectedId();
    double baseRate = 1.0;
    
    // Get base rate from combo box
    switch (selectedId) {
        case 1: baseRate = 1.0; break;    // 1
        case 2: baseRate = 0.5; break;    // 1/2
        case 3: baseRate = 0.25; break;   // 1/4
        case 4: baseRate = 0.125; break;  // 1/8
        case 5: baseRate = 0.0625; break; // 1/16
        case 6: baseRate = 0.03125; break; // 1/32
        default: baseRate = 1.0; break;
    }
    
    // Apply rate type modifier
    switch (rateType) {
        case 0: // Straight
            stepRate = baseRate;
            break;
        case 1: // Triplet (2/3 of straight)
            stepRate = baseRate * 2.0 / 3.0;
            break;
        case 2: // Dotted (1.5x straight)
            stepRate = baseRate * 1.5;
            break;
    }
}