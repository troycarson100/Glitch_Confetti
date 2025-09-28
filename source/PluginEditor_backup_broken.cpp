#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "FontManager.h"

//==============================================================================
// Division mapping helper: step period in beats (quarters)
//==============================================================================
static inline double stepPeriodBeatsFromDivision(int divisionIndex, int rateType = 0)
{
    // divisionIndex: 0:1/1, 1:1/2, 2:1/4, 3:1/8, 4:1/16, 5:1/32
    // rateType: 0=Straight, 1=Dotted, 2=Triplet
    double basePeriod;
    switch (divisionIndex)
    {
        case 0: basePeriod = 4.0;   // 1/1 = 4 quarter notes in 4/4
                break;
        case 1: basePeriod = 2.0;   // 1/2
                break;
        case 2: basePeriod = 1.0;   // 1/4
                break;
        case 3: basePeriod = 0.5;   // 1/8
                break;
        case 4: basePeriod = 0.25;  // 1/16
                break;
        case 5: basePeriod = 0.125; // 1/32
                break;
        default: basePeriod = 0.25; // safe default 1/16
                break;
    }
    
    // Apply rate type modifications - these should preserve the relative differences between rates
    switch (rateType)
    {
        case 0: return basePeriod;                    // Straight - normal timing
        case 1: return basePeriod * 1.5;              // Dotted - 1.5x slower for ALL rates
        case 2: return basePeriod * (2.0/3.0);        // Triplet - 2/3 faster for ALL rates
        default: return basePeriod;
    }
}

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

    // Draw highlight BACKGROUND if this step is currently being sequenced
    if (sequencerActive)
    {
        g.setColour(juce::Colour(0xFF5B5B5B)); // #5B5B5B color
        g.fillRoundedRectangle(bounds, 10.0f);

        // Add a subtle border to make it more visible
        g.setColour(juce::Colour(0xFF7A7A7A).withAlpha(0.8f)); // Slightly lighter border
        g.drawRoundedRectangle(bounds, 10.0f, 3.0f);
    }

    // Draw the appropriate SVG based on clicked active state
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
        g.fillRoundedRectangle(bounds, 10.0f);

        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
    }

    // Apply 70% opacity overlay if button is inactive
    if (inactive)
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 10.0f);
    }
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

void CustomStepButton::setSequencerActive(bool sequencerActive)
{
    if (this->sequencerActive != sequencerActive)
    {
        this->sequencerActive = sequencerActive;
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

void CustomStepButton::setImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active)
{
    inactiveImage = std::move(inactive);
    activeImage = std::move(active);
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
    // Plugin loading - no debug output to prevent crashes
    
    // Initialize sequencer state
    lastDivisionIndex = divisionIndex;  // Initialize to current division
    divisionChangeCounter = 0;          // Initialize counter
    
    // Set the size to match our desired dimensions (fixed size)
    // Add parameter listener
    processorRef.getValueTreeState().addParameterListener("timeMs", this);
    processorRef.getValueTreeState().addParameterListener("feedback", this);
    processorRef.getValueTreeState().addParameterListener("wowDepth", this);
    processorRef.getValueTreeState().addParameterListener("wowRate", this);
    processorRef.getValueTreeState().addParameterListener("saturation", this);
    processorRef.getValueTreeState().addParameterListener("highCut", this);
    processorRef.getValueTreeState().addParameterListener("lowCut", this);
    processorRef.getValueTreeState().addParameterListener("mix", this);
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
    
    // Initialize step rate
    updateStepRate();
    
    // Initialize step indicator bars (one for each knob)
    stepIndicatorAreas.resize(8);
    currentStepProgress.resize(8, 0.0f);
    for (int i = 0; i < 8; ++i) {
        stepIndicatorAreas[i] = juce::Rectangle<int>(0, 0, 63, 10); // 25% smaller: 84*0.75=63, 14*0.75=10.5, rounded to 10
    }
    
    
    // Force resized to be called to position components
    resized();
    
    // Start timer for PPQ-driven sequencer
    startTimerHz(30); // ~33 ms; smooth enough for the highlight
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
    
    // Draw step indicator bars (one for each knob)
    if (sequencerPowerOn)
    {
        for (int i = 0; i < 8; ++i) {
            if (i < static_cast<int>(stepIndicatorAreas.size())) {
                auto barArea = stepIndicatorAreas[i].toFloat();
                
                // Draw the border (2px stroke, 4px border radius)
                g.setColour(juce::Colour(0xFFFFFFFF)); // White border
                g.drawRoundedRectangle(barArea, 4.0f, 2.0f);
                
                // Draw the white fill based on current step progress
                if (i < static_cast<int>(currentStepProgress.size()) && currentStepProgress[i] > 0.0f)
                {
                    // Create a clipped area for the fill to stay within the rounded rectangle
                    g.saveState();
                    
                    // Clip to the rounded rectangle bounds
                    g.reduceClipRegion(stepIndicatorAreas[i]);
                    
                    // Calculate fill width within the clipped area
                    auto fillArea = barArea;
                    float fillWidth = fillArea.getWidth() * currentStepProgress[i];
                    fillArea.setWidth(fillWidth);
                    
                    
                    // Ensure fill doesn't exceed the bar area
                    fillArea = fillArea.getIntersection(barArea);
                    
                    g.setColour(juce::Colour(0xFFFFFFFF)); // White fill
                    // Draw fill as regular rectangle - the clipping will handle the rounded corners
                    g.fillRect(fillArea);
                    
                    g.restoreState();
                }
            }
        }
    }
    
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
    
    
    // Layout step sequencer (left side, bottom) - moved down 40px, right 10px
    auto stepArea = juce::Rectangle<int>(20, bounds.getHeight() - 160, 420, 150);
    stepArea.reduce (20, 20);
    
    // Position step indicator bars below each knob - moved to end of resized()
    // This will be called after knobs are positioned
    
    // Position step controls at the top of the step area, moved 140px to the right (40 + 100)
    if (stepCountBox != nullptr)
    {
        stepCountBox->setBounds(stepArea.getX() + 140, stepArea.getY() - 30, 40, 25);
    }
    
    if (rateComboBox != nullptr)
    {
        rateComboBox->setBounds(stepArea.getX() + 190, stepArea.getY() - 30, 60, 25);
    }
    
    if (rateTypeButton != nullptr)
    {
        rateTypeButton->setBounds(stepArea.getX() + 280, stepArea.getY() - 30, 25, 25);
    }
    
    // Position power button in top right corner of step area - 25% bigger, 19.25px right, 5px down
    if (powerButton != nullptr)
    {
        int buttonSize = 31; // 25 * 1.25 = 31.25, rounded to 31
        powerButton->setBounds(stepArea.getRight() - buttonSize + 19.25f, stepArea.getY() - 27, buttonSize, buttonSize);
    }
    
    // Play button removed - now syncs with DAW transport
    
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
    
    // Use the same positioning as the debug knobs (moved up 4px and left 5px)
    int knobStartX = 45;  // 50 - 5 = 45
    int knobStartY = 96;  // 100 - 4 = 96
    
    // Move bottom row down 35px (20px + 15px)
    int bottomRowOffset = 35;
    
        for (int i = 0; i < static_cast<int>(knobs.size()); ++i)
        {
            int row = i / cols;
            int col = i % cols;
            
        int x = knobStartX + col * (knobSize + 30);
        int y = knobStartY + row * (knobSize + 40) + (row > 0 ? bottomRowOffset : 0) + (row == 0 ? 15 : 0); // Moved top row up 5px (20 -> 15)
        
        knobs[i]->setBounds (x, y, knobSize, knobSize);
        
        // Make sure the knob is visible
        knobs[i]->setVisible(true);
        knobs[i]->setEnabled(true);
        
        // Position title above knob (moved down 2px: 20 -> 18)
        if (i < static_cast<int>(labels.size()))
        {
            labels[i]->setBounds (x, y - 18, knobSize, 15);
        }
        
            // Position value label below knob, moved up 8px
            if (i < static_cast<int>(valueLabels.size()))
            {
                valueLabels[i]->setBounds (x, y + knobSize + 5 - 8, knobSize, 15);
            }
    }
    
    // Position step indicator bars below each knob (after knobs are positioned)
    for (int i = 0; i < 8; ++i) {
        if (i < static_cast<int>(knobs.size()) && knobs[i] != nullptr) {
            auto knobBounds = knobs[i]->getBounds();
            stepIndicatorAreas[i] = juce::Rectangle<int>(
                knobBounds.getX() + (knobBounds.getWidth() - 63) / 2,  // Center under knob (25% smaller: 84*0.75=63)
                knobBounds.getBottom() + 15,  // 15px below knob (5px + 10px)
                63, 10  // 25% smaller: 84*0.75=63, 14*0.75=10.5, rounded to 10
            );
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
    
    // Load carrot images for combo box
    juce::String carrotInactiveData(BinaryData::FX_Type_Carrot_Inactive_svg, BinaryData::FX_Type_Carrot_Inactive_svgSize);
    auto carrotInactiveXML = juce::XmlDocument::parse(carrotInactiveData);
    if (carrotInactiveXML != nullptr)
    {
        carrotInactiveImage = juce::Drawable::createFromSVG(*carrotInactiveXML);
        if (carrotInactiveImage != nullptr)
            DBG("✓ Carrot Inactive loaded");
        else
            DBG("✗ Carrot Inactive createFromSVG failed");
    }
    else
    {
        DBG("✗ Carrot Inactive XML parse failed");
    }
    
    juce::String carrotActiveData(BinaryData::FX_Type_Carrot_Active_svg, BinaryData::FX_Type_Carrot_Active_svgSize);
    auto carrotActiveXML = juce::XmlDocument::parse(carrotActiveData);
    if (carrotActiveXML != nullptr)
    {
        carrotActiveImage = juce::Drawable::createFromSVG(*carrotActiveXML);
        if (carrotActiveImage != nullptr)
            DBG("✓ Carrot Active loaded");
        else
            DBG("✗ Carrot Active createFromSVG failed");
    }
    else
    {
        DBG("✗ Carrot Active XML parse failed");
    }
    
    // Load power button image
    juce::String powerButtonData(BinaryData::Step_Power_On_svg, BinaryData::Step_Power_On_svgSize);
    auto powerButtonXML = juce::XmlDocument::parse(powerButtonData);
    if (powerButtonXML != nullptr)
    {
        powerButtonImage = juce::Drawable::createFromSVG(*powerButtonXML);
        if (powerButtonImage != nullptr)
            DBG("✓ Power Button loaded");
        else
            DBG("✗ Power Button createFromSVG failed");
    }
    else
    {
        DBG("✗ Power Button XML parse failed");
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
    
    // Handle division change transition
    if (divisionChangeCounter > 0) {
        divisionChangeCounter--;
        if (divisionChangeCounter == 0) {
            lastDivisionIndex = divisionIndex;  // Reset to current division
        }
    }
    
    // Only run sequencer if power is on
    if (!sequencerPowerOn) {
        clearSequencerUI();
        return;
    }
    
    // Get transport and sequencer state
    TransportCache t;
    processorRef.getTransportSnapshot(t);
    const bool playing = (t.valid && t.playing);
    const bool seqOn = processorRef.isSequencerEnabled();
    
    const int stepToShow = (seqOn && playing)
                         ? processorRef.getPlayingStep()
                         : currentActiveStep; // UI selection
    
    if (seqOn && playing && stepToShow >= 0) {
        applyStepHighlight(stepToShow);
    } else {
        clearSequencerUI();
    }
    
    // Update step progress for all indicator bars based on SAVED step values
    refreshBarsFromStep(stepToShow);
    
    // Trigger repaint to update the indicator bar
    repaint();
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
            if (i < static_cast<int>(stepButtons.size()))
            {
                stepButtons[i]->setActive(true);
                
                // Restore snapshot values for the clicked step
                restoreSnapshot(i);
            }
        };
        
        // Set SVG images if available
        if (stepActiveImage != nullptr && stepInactiveImage != nullptr)
        {
            button->setImages(stepInactiveImage->createCopy(), stepActiveImage->createCopy());
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
        // Use custom font
        label->setFont (juce::Font(12.0f, juce::Font::bold));
        
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
        // Use custom font for value labels
        valueLabel->setFont (juce::Font(10.0f, juce::Font::plain));
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
        // Use custom font for step section
        g.setFont(juce::Font(12.0f, juce::Font::bold));
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
    // Set all colors to transparent to prevent base class drawing
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::focusedOutlineColourId, juce::Colours::transparentBlack);
    
    // Make sure this combo box doesn't interfere with other components
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    
    // Prevent this combo box from capturing mouse events meant for other components
    setInterceptsMouseClicks(true, true);
    
    // Use custom look and feel that disables text drawing
    setLookAndFeel(&customLookAndFeel);
}

void CustomComboBox::paint(juce::Graphics& g)
{
    // Clear the entire area first to remove any base class drawing
    auto bounds = getLocalBounds().toFloat();
    
    // No background fill - transparent
    // g.setColour(juce::Colours::darkgrey);
    // g.fillRoundedRectangle(bounds, 4.0f);
    
    // No border for rate dropdown
    // g.setColour(juce::Colours::white);
    // g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    
    // Draw dropdown arrow using carrot SVG - make it 30% smaller and closer to text
    auto arrowBounds = bounds.removeFromRight(10.0f).reduced(0.5f); // Even smaller width and minimal padding
    
    // Use carrot SVG instead of triangle - show active when dropdown is open
    // Scale the carrot to 70% of original size (30% smaller)
    const float carrotScale = 0.7f;
    g.setOpacity(1.0f); // Ensure 100% opacity
    if (isDropdownOpen && carrotActiveImage)
    {
        carrotActiveImage->drawWithin(g, arrowBounds, juce::RectanglePlacement::centred, carrotScale);
    }
    else if (carrotInactiveImage)
    {
        carrotInactiveImage->drawWithin(g, arrowBounds, juce::RectanglePlacement::centred, carrotScale);
    }
    else
    {
        // Fallback to triangle if SVG not loaded
        g.setColour(juce::Colours::white);
        juce::Path arrow;
        arrow.addTriangle(arrowBounds.getCentreX(), arrowBounds.getY() + 3,
                         arrowBounds.getX() + 3, arrowBounds.getBottom() - 3,
                         arrowBounds.getRight() - 3, arrowBounds.getBottom() - 3);
        g.fillPath(arrow);
    }
    
    // Draw the selected text with custom font
    g.setColour(juce::Colours::white);
    auto textBounds = bounds.reduced(4.0f);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(getText(), textBounds, juce::Justification::centred);
}

void CustomComboBox::paintOverChildren(juce::Graphics& g)
{
    // Override to prevent any base class drawing
}

void CustomComboBox::lookAndFeelChanged()
{
    // Override to prevent base class from setting up default look and feel
}

void CustomComboBox::enablementChanged()
{
    // Override to prevent base class from changing appearance
}

void CustomComboBox::setCarrotImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active)
{
    carrotInactiveImage = std::move(inactive);
    carrotActiveImage = std::move(active);
}

void CustomComboBox::mouseDown(const juce::MouseEvent& event)
{
    isDropdownOpen = true;
    repaint(); // Repaint to show active carrot
    juce::ComboBox::mouseDown(event);
    isDropdownOpen = false;
    repaint(); // Repaint to show inactive carrot
}

void CustomComboBox::resized()
{
    // Override to prevent base class resizing that might cause issues
    juce::ComboBox::resized();
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
    auto bounds = getLocalBounds().toFloat();
    
    // No background fill - transparent
    // g.setColour(juce::Colours::darkgrey);
    // g.fillRoundedRectangle(bounds, 4.0f);
    
    // Draw border with 2px stroke
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    
    // Draw text with system font (temporarily disabled custom fonts due to memory leaks)
    g.setFont(juce::Font(12.0f, juce::Font::bold));
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
    divisionIndex = 0; // Set divisionIndex to match the combo box selection (1 = 1/1)
    rateComboBox->onChange = [this]() {
        updateStepRate();
    };
    
    // Set carrot images for the combo box
    if (carrotInactiveImage && carrotActiveImage)
    {
        rateComboBox->setCarrotImages(carrotInactiveImage->createCopy(), 
                                     carrotActiveImage->createCopy());
    }
    
    addAndMakeVisible(*rateComboBox);
    
    // Create rate type button (S/D/T)
    rateTypeButton = std::make_unique<CustomTextButton>();
    rateTypeButton->setButtonText("S");
    rateTypeButton->onClick = [this]() {
        rateType = (rateType + 1) % 3;
        switch (rateType) {
            case 0: rateTypeButton->setButtonText("S"); break; // Straight
            case 1: rateTypeButton->setButtonText("D"); break; // Dotted
            case 2: rateTypeButton->setButtonText("T"); break; // Triplet
        }
        updateStepRate();
    };
    addAndMakeVisible(*rateTypeButton);
    
    // Play/stop button removed - now syncs with DAW transport
    
    // Create draggable step count box
    stepCountBox = std::make_unique<DraggableNumberBox>();
    stepCountBox->setValue(16);
    stepCountBox->onValueChanged = [this](int value) {
        stepsUsed = juce::jlimit(1, 16, value);
        processorRef.setStepsUsed(stepsUsed); // Sync with processor
        // Ensure current step is within valid range
        if (currentStep >= stepsUsed) {
            setActiveStep(0);
        }
        // Update visual state of step buttons
        updateStepButtonVisibility();
    };
    
    // Ensure step count box doesn't interfere with other components
    stepCountBox->setWantsKeyboardFocus(true);
    stepCountBox->setMouseClickGrabsKeyboardFocus(true);
    stepCountBox->setInterceptsMouseClicks(true, true);
    
    addAndMakeVisible(*stepCountBox);
    
    // Create power button
    powerButton = std::make_unique<CustomStepButton>(-1); // Use -1 as special index for power button
    powerButton->setButtonText(""); // No text, just SVG
    powerButton->onClick = [this]() {
        sequencerPowerOn = !sequencerPowerOn;
        processorRef.setSequencerEnabled(sequencerPowerOn);
        updatePowerState();
    };
    
    // Set power button image
    if (powerButtonImage)
    {
        powerButton->setImages(powerButtonImage->createCopy(), powerButtonImage->createCopy());
    }
    
    addAndMakeVisible(*powerButton);
}

// Snapshot functionality implementation
void PluginEditor::initializeSnapshots()
{
    // Initialize 16 steps with default StepSnapshot values
    for (int step = 0; step < 16; ++step)
    {
        stepSnapshots[step] = StepSnapshot(); // Uses default constructor values
    }
}

void PluginEditor::saveSnapshot(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < 16 && stepIndex < static_cast<int>(knobs.size()))
    {
        auto& snap = stepSnapshots[stepIndex];
        // Map knob values to delay parameters
        snap.delay.timeMs = knobs[0] ? knobs[0]->getValue() * 2000.0f : 250.0f;
        snap.delay.feedback = knobs[1] ? knobs[1]->getValue() * 100.0f : 20.0f;
        snap.delay.wowDepth = knobs[2] ? knobs[2]->getValue() * 100.0f : 0.0f;
        snap.delay.wowRate = knobs[3] ? knobs[3]->getValue() * 8.0f : 1.0f;
        snap.delay.saturation = knobs[4] ? knobs[4]->getValue() * 100.0f : 0.0f;
        snap.delay.highCut = knobs[5] ? 1000.0f + knobs[5]->getValue() * 19000.0f : 20000.0f;
        snap.delay.lowCut = knobs[6] ? 20.0f + knobs[6]->getValue() * 1980.0f : 20.0f;
        snap.delay.mix = knobs[7] ? knobs[7]->getValue() * 100.0f : 50.0f;
    }
}

void PluginEditor::restoreSnapshot(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < 16 && stepIndex < static_cast<int>(knobs.size()))
    {
        const auto& snap = stepSnapshots[stepIndex];
        // Map delay parameters to knob values
        if (knobs[0]) knobs[0]->setValue(snap.delay.timeMs / 2000.0f, juce::dontSendNotification);
        if (knobs[1]) knobs[1]->setValue(snap.delay.feedback / 100.0f, juce::dontSendNotification);
        if (knobs[2]) knobs[2]->setValue(snap.delay.wowDepth / 100.0f, juce::dontSendNotification);
        if (knobs[3]) knobs[3]->setValue(snap.delay.wowRate / 8.0f, juce::dontSendNotification);
        if (knobs[4]) knobs[4]->setValue(snap.delay.saturation / 100.0f, juce::dontSendNotification);
        if (knobs[5]) knobs[5]->setValue((snap.delay.highCut - 1000.0f) / 19000.0f, juce::dontSendNotification);
        if (knobs[6]) knobs[6]->setValue((snap.delay.lowCut - 20.0f) / 1980.0f, juce::dontSendNotification);
        if (knobs[7]) knobs[7]->setValue(snap.delay.mix / 100.0f, juce::dontSendNotification);
    }
}

// PPQ-driven sequencer helpers
int PluginEditor::computeStepFromTransport(const TransportCache& t) const
{
    if (!t.valid || !t.playing) return -1;

    const int activeSteps = juce::jlimit(1, stepCount, stepsUsed);   // e.g., 16
    const double periodBeats = stepPeriodBeatsFromDivision(divisionIndex, rateType);
    if (periodBeats <= 0.0) return -1;

    // Absolute quarters since session start:
    const double ppq = std::max(0.0, t.ppq.load());

    // Check if division has changed - if so, maintain current step position
    if (divisionIndex != lastDivisionIndex) {
        // Division changed - keep current step to avoid jumping
        const int currentStepIndex = (currentStep >= 0) ? currentStep : 0;
        return currentStepIndex % activeSteps;
    }

    // How many steps have elapsed total:
    const double stepsElapsed = ppq / periodBeats;

    // Wrap through the user's step count:
    const int idx = (int) (std::fmod(stepsElapsed, (double) activeSteps) + activeSteps) % activeSteps;
    return idx;   // 0..activeSteps-1, independent of division
}

void PluginEditor::applyStepHighlight(int newStep)
{
    if (newStep == currentStep)
        return;

    // Clear previous
    if (lastPaintedStep >= 0 && lastPaintedStep < (int)stepButtons.size())
        if (auto* b = stepButtons[lastPaintedStep].get()) { 
            b->setSequencerActive(false); 
        }

    // Set new
    currentStep = newStep;
    lastPaintedStep = newStep;

    if (newStep >= 0 && newStep < (int)stepButtons.size())
        if (auto* b = stepButtons[newStep].get()) { 
            b->setSequencerActive(true); 
        }
}

void PluginEditor::clearSequencerUI()
{
    if (currentStep == -1 && lastPaintedStep == -1) return;
    for (auto& b : stepButtons)
        if (b) b->setSequencerActive(false);
    currentStep = -1;
    lastPaintedStep = -1;

    // Clear step progress for all knobs
    for (int i = 0; i < 8; ++i) {
        if (i < static_cast<int>(currentStepProgress.size())) {
            currentStepProgress[i] = 0.0f;
        }
    }
}

void PluginEditor::refreshBarsFromStep(int stepToShow)
{
    // Clear all bars first
    for (int i = 0; i < 8; ++i)
    {
        if (i < static_cast<int>(currentStepProgress.size()))
        {
            currentStepProgress[i] = 0.0f;
        }
    }
    
    // If step is valid, update bars with snapshot values
    if (stepToShow >= 0 && stepToShow < 16)
    {
        const auto& snap = stepSnapshots[stepToShow];
        
        // Map delay snapshot fields to bars (0..1)
        if (static_cast<int>(currentStepProgress.size()) >= 8)
        {
            currentStepProgress[0] = snap.delay.timeMs / 2000.0f;      // Time (0-2000ms)
            currentStepProgress[1] = snap.delay.feedback / 100.0f;     // Feedback (0-100%)
            currentStepProgress[2] = snap.delay.wowDepth / 100.0f;     // Wow Depth (0-100%)
            currentStepProgress[3] = snap.delay.wowRate / 8.0f;        // Wow Rate (0-8Hz)
            currentStepProgress[4] = snap.delay.saturation / 100.0f;   // Drive (0-100%)
            currentStepProgress[5] = (snap.delay.highCut - 1000.0f) / 19000.0f;  // Hi Cut (1k-20k Hz)
            currentStepProgress[6] = (snap.delay.lowCut - 20.0f) / 1980.0f;      // Low Cut (20-2k Hz)
            currentStepProgress[7] = snap.delay.mix / 100.0f;          // Mix (0-100%)
        }
    }
}


void PluginEditor::updatePowerState()
{
    if (powerButton != nullptr)
    {
        powerButton->setActive(sequencerPowerOn);
        // Use setAlpha instead of setOpacity
        powerButton->setAlpha(sequencerPowerOn ? 1.0f : 0.7f);
    }
    
    // Update step area controls opacity
    float controlOpacity = sequencerPowerOn ? 1.0f : 0.7f;
    
    if (rateComboBox != nullptr)
        rateComboBox->setAlpha(controlOpacity);
    if (rateTypeButton != nullptr)
        rateTypeButton->setAlpha(controlOpacity);
    if (stepCountBox != nullptr)
        stepCountBox->setAlpha(controlOpacity);
    
    // Update step buttons opacity
    for (auto& button : stepButtons)
    {
        if (button != nullptr)
        {
            button->setAlpha(controlOpacity);
        }
    }
    
    // Clear sequencer if power is off
    if (!sequencerPowerOn)
    {
        clearSequencerUI();
    }
}

void PluginEditor::setSequencerPower(bool powerOn)
{
    sequencerPowerOn = powerOn;
    updatePowerState();
}


void PluginEditor::setActiveStep(int step)
{
    if (step < 0 || step >= stepCount) 
    {
        return;
    }
    
    // Save current snapshot before switching
    if (currentActiveStep >= 0 && currentActiveStep < static_cast<int>(stepButtons.size()))
    {
        saveSnapshot(currentActiveStep);
        stepButtons[currentActiveStep]->setActive(false);
    }
    
    // Set new active step
    currentActiveStep = step;
    currentStep = step;
    processorRef.setSelectedStep(step); // Sync with processor
    
    if (currentActiveStep >= 0 && currentActiveStep < static_cast<int>(stepButtons.size()))
    {
        stepButtons[currentActiveStep]->setActive(true);
        restoreSnapshot(currentActiveStep);
    }
}

void PluginEditor::setSequencerStep(int step)
{
    if (step < 0 || step >= stepCount)
    {
        return;
    }

    // Clear previous sequencer active state
    for (int i = 0; i < static_cast<int>(stepButtons.size()); ++i)
    {
        if (stepButtons[i] != nullptr)
        {
            stepButtons[i]->setSequencerActive(false);
        }
    }

    // Set new sequencer active step
    if (step < static_cast<int>(stepButtons.size()) && stepButtons[step] != nullptr)
    {
        stepButtons[step]->setSequencerActive(true);
        
        // Force repaint of the entire editor to ensure visual update
        repaint();
    }
}

void PluginEditor::updateStepButtonVisibility()
{
    // Set step buttons beyond the step count to inactive (70% opacity)
    for (int i = 0; i < 16; ++i)
    {
        if (i < static_cast<int>(stepButtons.size()))
        {
            // Make steps beyond the stepsUsed count inactive
            bool shouldBeInactive = (i >= stepsUsed);
            stepButtons[i]->setInactive(shouldBeInactive);
        }
    }
}

void PluginEditor::resetStepSequencer()
{
    // Reset to step 0 when playback starts
    setActiveStep(0);
}

void PluginEditor::clearSequencer()
{
    // Clear all sequencer active states when playback stops
    for (int i = 0; i < static_cast<int>(stepButtons.size()); ++i)
    {
        if (stepButtons[i] != nullptr)
        {
            stepButtons[i]->setSequencerActive(false);
        }
    }
}

void PluginEditor::updateStepRate()
{
    if (rateComboBox == nullptr) return;
    
    int selectedId = rateComboBox->getSelectedId();
    int newDivisionIndex = divisionIndex;
    
    // Map combo box selection to division index
    switch (selectedId) {
        case 1: newDivisionIndex = 0; break;  // 1/1
        case 2: newDivisionIndex = 1; break;  // 1/2
        case 3: newDivisionIndex = 2; break;  // 1/4
        case 4: newDivisionIndex = 3; break;  // 1/8
        case 5: newDivisionIndex = 4; break;  // 1/16
        case 6: newDivisionIndex = 5; break;  // 1/32
        default: newDivisionIndex = 4; break; // default 1/16
    }
    
    // Only update if division actually changed
    if (newDivisionIndex != divisionIndex) {
        lastDivisionIndex = divisionIndex;  // Store previous division
        divisionIndex = newDivisionIndex;   // Update to new division
        divisionChangeCounter = 3;          // Set counter for smooth transition (3 timer cycles)
        processorRef.setDivisionIndex(newDivisionIndex); // Sync with processor
        // Don't clear sequencer - let it maintain current position
    }
}
void PluginEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Save the parameter change to the current step's snapshot
    saveSnapshot(currentActiveStep);
}

// Remove parameter listeners in destructor
PluginEditor::~PluginEditor()
{
    // Remove parameter listeners
    processorRef.getValueTreeState().removeParameterListener("timeMs", this);
    processorRef.getValueTreeState().removeParameterListener("feedback", this);
    processorRef.getValueTreeState().removeParameterListener("wowDepth", this);
    processorRef.getValueTreeState().removeParameterListener("wowRate", this);
    processorRef.getValueTreeState().removeParameterListener("saturation", this);
    processorRef.getValueTreeState().removeParameterListener("highCut", this);
    processorRef.getValueTreeState().removeParameterListener("lowCut", this);
    processorRef.getValueTreeState().removeParameterListener("mix", this);
}
