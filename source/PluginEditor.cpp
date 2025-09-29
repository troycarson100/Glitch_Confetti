#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

//==============================================================================
// PluginEditor Implementation
//==============================================================================

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    DBG("[UI] PluginEditor constructor starting...");
    
    // Set the size to match our desired dimensions
    setSize (974, 532);
    
    // Lock the size - no resizing
    setResizable (false, false);
    
    // Load all UI assets
    if (!assets.loadAll()) {
        DBG("[UI] Failed to load UI assets!");
    } else {
        DBG("[UI] UI assets loaded successfully");
    }
    
        // Setup knobs
    setupKnobs();
        
        // Setup effects area
        setupEffectsArea();
        
        // Setup sequencer area
        setupSequencerArea();
        
        // Start timer for UI updates
        startTimer(100); // Update every 100ms for smoother knob interaction
        
        DBG("[UI] PluginEditor initialized with fresh start");
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Draw the mustard background SVG
    if (assets.backgroundMustard != nullptr)
    {
        assets.backgroundMustard->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback background
        g.fillAll (juce::Colour(0xff2a2a2a));
    }
    
    // Draw the grid overlay
    drawGridOverlay(g);
    
    // Draw the three main areas
    drawMainAreas(g);
}

void PluginEditor::resized()
{
    // Empty for now
}


void PluginEditor::drawGridOverlay(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const int gridSize = 50; // 50 pixel grid
    const int fontSize = 10;
    
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(fontSize);
    
    // Draw vertical grid lines
    for (int x = 0; x <= bounds.getWidth(); x += gridSize)
    {
        g.drawVerticalLine(x, 0, bounds.getHeight());
        
        // Add x-coordinate labels
        if (x > 0 && x < bounds.getWidth())
        {
            g.drawText(juce::String(x), x - 20, 5, 40, 15, juce::Justification::centred);
        }
    }
    
    // Draw horizontal grid lines
    for (int y = 0; y <= bounds.getHeight(); y += gridSize)
    {
        g.drawHorizontalLine(y, 0, bounds.getWidth());
        
        // Add y-coordinate labels
        if (y > 0 && y < bounds.getHeight())
        {
            g.drawText(juce::String(y), 5, y - 7, 30, 15, juce::Justification::centred);
        }
    }
    
    // Add section letters (A, B, C, D, etc.)
    g.setColour(juce::Colours::yellow.withAlpha(0.8f));
    g.setFont(fontSize + 2);
    
    int sectionIndex = 0;
    for (int y = gridSize; y < bounds.getHeight(); y += gridSize * 2)
    {
        for (int x = gridSize; x < bounds.getWidth(); x += gridSize * 2)
        {
            char sectionLetter = 'A' + (sectionIndex % 26);
            g.drawText(juce::String(sectionLetter), x - 10, y - 10, 20, 20, juce::Justification::centred);
            sectionIndex++;
        }
    }
}

void PluginEditor::drawMainAreas(juce::Graphics& g)
{
    // Define the three main areas based on your grid coordinates
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);    // 88px wider, 90px shorter
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);     // 10px shorter, moved down 4px
    auto masterArea = juce::Rectangle<int>(460, 54, 495, 460);   // 5px narrower
    
    // Draw effect area (top-left, red border)
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(effectArea.toFloat(), 10.0f);
    g.setColour(juce::Colours::red);
    g.drawRoundedRectangle(effectArea.toFloat(), 10.0f, 3.0f);
    g.setColour(juce::Colours::white);
    g.drawText("EFFECT AREA", effectArea, juce::Justification::centred);
    
    // Draw step area (bottom-left, blue border)
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(stepArea.toFloat(), 10.0f);
    g.setColour(juce::Colours::blue);
    g.drawRoundedRectangle(stepArea.toFloat(), 10.0f, 3.0f);
    g.setColour(juce::Colours::white);
    g.drawText("STEP AREA", stepArea, juce::Justification::centred);
    
    // Draw master area (right side, magenta border)
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(masterArea.toFloat(), 10.0f);
    g.setColour(juce::Colours::magenta);
    g.drawRoundedRectangle(masterArea.toFloat(), 10.0f, 3.0f);
    g.setColour(juce::Colours::white);
    g.drawText("MASTER AREA", masterArea, juce::Justification::centred);
}

void PluginEditor::timerCallback()
{
    // Update knob values and UI based on parameter values
    for (int i = 0; i < 8; ++i)
    {
        if (knobs[i] != nullptr && i < processorRef.getParameters().size())
        {
            auto* param = processorRef.getParameters().getUnchecked(i);
            float paramValue = param->getValue();
            
            // Only update knob value if it's not currently being dragged
            if (!knobs[i]->isMouseButtonDown())
            {
                knobs[i]->setValue(paramValue, juce::dontSendNotification);
            }
            
            // Update value label
            if (valueLabels[i] != nullptr)
            {
                // Format value based on parameter type
                juce::String valueText;
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
                {
                    float actualValue = floatParam->convertFrom0to1(paramValue);
                    if (i == 0) // Time - show in ms
                        valueText = juce::String(actualValue, 0) + "ms";
                    else if (i == 5 || i == 6) // Hi-Cut, Low-Cut - show in Hz
                        valueText = juce::String(actualValue, 0) + "Hz";
                    else if (i == 3) // Wow Rate - show in Hz
                        valueText = juce::String(actualValue, 1) + "Hz";
                    else // Others - show as percentage
                        valueText = juce::String(actualValue * 100, 1) + "%";
                }
                else
                {
                    // Fallback for other parameter types
                    valueText = juce::String(paramValue * 100, 1) + "%";
                }
                
                valueLabels[i]->setText(valueText, juce::dontSendNotification);
            }
            
            // Update indicator bar
            if (indicatorBars[i] != nullptr)
            {
                indicatorBars[i]->setValue(paramValue);
            }
        }
    }
    
    // Update sequencer UI
    updateSequencerUI();
}

    //==============================================================================
    // CustomDiceButton Implementation
    //==============================================================================

    CustomDiceButton::CustomDiceButton() : juce::Button("diceButton")
    {
        setClickingTogglesState(false);
    }

    void CustomDiceButton::paintButton(juce::Graphics& g, bool over, bool down)
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw the dice SVG
        if (diceImage != nullptr)
        {
            // Add a subtle highlight when hovering
            if (over)
            {
                g.setColour(juce::Colours::white.withAlpha(0.1f));
                g.fillRoundedRectangle(bounds, 5.0f);
            }
            
            // Draw the dice image
            diceImage->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            // Fallback: draw a simple dice shape
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(bounds, 5.0f);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
            
            // Draw dots
            g.setColour(juce::Colours::black);
            float dotSize = bounds.getWidth() * 0.15f;
            float centerX = bounds.getCentreX();
            float centerY = bounds.getCentreY();
            
            // Draw 5 dots in dice pattern
            g.fillEllipse(centerX - dotSize, centerY - dotSize, dotSize, dotSize);
            g.fillEllipse(centerX + dotSize, centerY - dotSize, dotSize, dotSize);
            g.fillEllipse(centerX, centerY, dotSize, dotSize);
            g.fillEllipse(centerX - dotSize, centerY + dotSize, dotSize, dotSize);
            g.fillEllipse(centerX + dotSize, centerY + dotSize, dotSize, dotSize);
        }
    }

    void CustomDiceButton::setDiceImage(std::unique_ptr<juce::Drawable> dice)
    {
        diceImage = std::move(dice);
        repaint();
    }

    //==============================================================================
    // IndicatorBar Implementation
    //==============================================================================

IndicatorBar::IndicatorBar()
{
}

void IndicatorBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw background stroke with 4px border radius
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    
    // Draw fill based on current value - no gap, no border radius on right side
    auto fillBounds = bounds.reduced(1.0f); // Reduced gap from 2.0f to 1.0f
    fillBounds.setWidth(fillBounds.getWidth() * currentValue);
    
    g.setColour(juce::Colours::white);
    // Create a path with specific corner radius: top-left=4px, top-right=0px, bottom-left=1px, bottom-right=0px
    juce::Path fillPath;
    fillPath.addRoundedRectangle(fillBounds.getX(), fillBounds.getY(), fillBounds.getWidth(), fillBounds.getHeight(),
                                 4.0f, 0.0f, true, false, true, false);
    g.fillPath(fillPath);
}

void IndicatorBar::setValue(float value)
{
    currentValue = juce::jlimit(0.0f, 1.0f, value);
    repaint();
}

//==============================================================================
// CustomKnob Implementation
//==============================================================================

CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 1.0, 0.001); // Finer resolution for smoother movement
    setValue(0.5);
    // Set rotation to start at -150 degrees and go to +150 degrees (300 degree total range)
    setRotaryParameters(-150.0f * juce::MathConstants<float>::pi / 180.0f, 150.0f * juce::MathConstants<float>::pi / 180.0f, true);
    
    // Set mouse drag sensitivity for smoother interaction
    setMouseDragSensitivity(200); // Lower = more sensitive, higher = less sensitive
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto centre = bounds.getCentre();
    
    // Paint knob without debug spam
    
    // Draw inner image with rotation first (bottom layer) - 4.368% smaller
    if (innerImage != nullptr)
    {
        g.saveState();
        // Calculate rotation: 0.5 at value 0.5 should be straight up (0 degrees)
        // Map 0.0-1.0 to -150 to +150 degrees, then convert to radians
        float rotationAngle = (getValue() - 0.5f) * 300.0f * juce::MathConstants<float>::pi / 180.0f;
        g.addTransform(juce::AffineTransform::rotation(rotationAngle, centre.x, centre.y));
        // Make inner image 15% smaller from current size and bottom align
        auto innerBounds = bounds.reduced(bounds.getWidth() * 0.15f, bounds.getHeight() * 0.15f);
        innerImage->drawWithin(g, innerBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::yTop, 1.0f);
        g.restoreState();
    }
    
        // Draw ring image on top (3.03% smaller) - simple approach
        if (ringImage != nullptr)
        {
            g.saveState();
            // No rotation - keep normal orientation
            // Make ring 5% smaller
            auto ringBounds = bounds.reduced(bounds.getWidth() * 0.05f, bounds.getHeight() * 0.05f);
            // Draw the ring on top with bottom alignment
            ringImage->drawWithin(g, ringBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::yTop, 1.0f);
            g.restoreState();
        }
}

void CustomKnob::resized() {}

void CustomKnob::setRingImage(std::unique_ptr<juce::Drawable> ring)
{
    ringImage = std::move(ring);
    repaint();
}

void CustomKnob::setInnerImage(std::unique_ptr<juce::Drawable> inner)
{
    innerImage = std::move(inner);
    repaint();
}

//==============================================================================
// CircularToggleButton Implementation
//==============================================================================

CircularToggleButton::CircularToggleButton() : juce::Button("circularToggle")
{
    setClickingTogglesState(false);
}

void CircularToggleButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    auto centre = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
    
    // Draw circle background (white fill, no stroke)
    g.setColour(juce::Colours::white);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);
    
    // Draw text (black)
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(getButtonText(), bounds, juce::Justification::centred);
}

//==============================================================================
// StepButton Implementation
//==============================================================================

StepButton::StepButton(int stepIndex) : juce::Button("stepButton"), stepIndex(stepIndex)
{
    setClickingTogglesState(false);
}

void StepButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Choose which image to draw based on state
    std::unique_ptr<juce::Drawable>* imageToDraw = nullptr;
    
    if (isSelected || isPlaying) {
        imageToDraw = &activeImage;
    } else {
        imageToDraw = &inactiveImage;
    }
    
    if (imageToDraw && *imageToDraw != nullptr) {
        (*imageToDraw)->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    } else {
        // Fallback drawing
        g.setColour(isSelected || isPlaying ? juce::Colours::white : juce::Colours::grey);
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
        
        // Draw step number
        g.setColour(juce::Colours::black);
        g.setFont(12.0f);
        g.drawText(juce::String(stepIndex + 1), bounds, juce::Justification::centred);
    }
    
    // Add highlight when hovering
    if (over) {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 5.0f);
    }
}

void StepButton::setActiveImage(std::unique_ptr<juce::Drawable> active)
{
    activeImage = std::move(active);
    repaint();
}

void StepButton::setInactiveImage(std::unique_ptr<juce::Drawable> inactive)
{
    inactiveImage = std::move(inactive);
    repaint();
}

void StepButton::setSelected(bool selected)
{
    if (isSelected != selected) {
        isSelected = selected;
        repaint();
    }
}

void StepButton::setPlaying(bool playing)
{
    if (isPlaying != playing) {
        isPlaying = playing;
        repaint();
    }
}

//==============================================================================
// PluginEditor Helper Methods
//==============================================================================


void PluginEditor::setupKnobs()
{
    DBG("[UI] Setting up knobs...");
    
        // Knob parameters
        std::vector<juce::String> knobNames = {
            "Time", "Feedback", "Wow Depth", "Wow Rate",
            "Drive", "Hi-Cut", "Low-Cut", "Mix"
        };
        
        // Effect area bounds
        auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
        const int knobSize = 80; // Increased from 60 to 80 (20px larger)
        const int knobSpacing = 20; // Reduced from 25 to 20 (5px less padding)
        const int startX = effectArea.getX() + 15; // Moved 5px left from +20 to +15
        const int startY = effectArea.getY() + effectArea.getHeight() - 210; // Adjusted for larger knobs
    
    for (int i = 0; i < 8; ++i)
    {
        // Create knob
        knobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(knobs[i].get());
        
        // Connect to DSP parameters based on knob order
        std::vector<juce::String> parameterIds = {
            "timeMs", "feedback", "wowDepth", "wowRate",
            "saturation", "highCut", "lowCut", "mix"
        };
        
        // Set parameter ranges and initial values
        if (i < processorRef.getParameters().size())
        {
            auto* param = processorRef.getParameters().getUnchecked(i);
            
            // All knobs use normalized 0-1 range for consistent UI behavior
            knobs[i]->setRange(0.0, 1.0, 0.001);
            
            // Set initial value from parameter (already normalized)
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
            {
                knobs[i]->setValue(param->getValue(), juce::dontSendNotification);
            }
            else
            {
                knobs[i]->setValue(0.5, juce::dontSendNotification);
            }
            
            // Add listener to update snapshots when knob changes
            knobs[i]->onValueChange = [this, i]() {
                updateParameterFromKnob(i);
            };
        }
        
            // Set images
            if (assets.knobRing != nullptr)
                knobs[i]->setRingImage(assets.knobRing->createCopy());
            if (assets.knobInside != nullptr)
                knobs[i]->setInnerImage(assets.knobInside->createCopy());
        
        // Position knob with special handling for top and bottom rows
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1; // Moved up 6px from +5 to -1
            
        knobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create label
        knobLabels[i] = std::make_unique<juce::Label>();
        knobLabels[i]->setText(knobNames[i], juce::dontSendNotification);
        knobLabels[i]->setFont(juce::Font(12.0f, juce::Font::bold));
        knobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        knobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobLabels[i].get());
        knobLabels[i]->setBounds(x, y - 15, knobSize, 20); // Moved down 5px from -20 to -15
        
        // Create value label
        valueLabels[i] = std::make_unique<juce::Label>();
        valueLabels[i]->setText("0.50", juce::dontSendNotification);
        valueLabels[i]->setFont(juce::Font(10.0f, juce::Font::plain));
        valueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        valueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabels[i].get());
        valueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15); // Moved up 12px from +2 to -10
        
            // Create indicator bar
            indicatorBars[i] = std::make_unique<IndicatorBar>();
            addAndMakeVisible(indicatorBars[i].get());
            indicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13); // Made 5px taller: 8 + 5 = 13
            indicatorBars[i]->setValue(0.5f); // Set initial value
            
            // Create individual dice button for this knob
            knobDiceButtons[i] = std::make_unique<CustomDiceButton>();
            addAndMakeVisible(knobDiceButtons[i].get());
            
            // Calculate text width and position dice button accordingly
            const int diceSize = 10; // 20% bigger: 8 * 1.2 = 9.6, rounded to 10
            const int diceSpacing = 5; // Fixed distance from end of title text
            
            // Get the font used for knob labels
            juce::Font labelFont(12.0f, juce::Font::bold);
            
            // Calculate the width of the knob title text
            int textWidth = labelFont.getStringWidth(knobNames[i]);
            
            // Position dice button at the end of the title text + fixed spacing
            int diceX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
            int diceY = y - 10; // Moved up 3px from -7 to -10
            
            knobDiceButtons[i]->setBounds(diceX, diceY, diceSize, diceSize);
            
            // Set the dice image
            if (assets.knobDice != nullptr)
            {
                knobDiceButtons[i]->setDiceImage(assets.knobDice->createCopy());
            }
            
            // Set up click handler for individual knob randomization
            knobDiceButtons[i]->onClick = [this, i]() {
                randomizeIndividualKnob(i);
            };
    }
    
        DBG("[UI] Created " << 8 << " knobs with labels and indicator bars");
    }

    //==============================================================================
    // Effects Area Setup
    //==============================================================================

    void PluginEditor::setupEffectsArea()
    {
        DBG("[UI] Setting up effects area...");
        
        // Effect area bounds
        auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
        
        // Create "EFFECT" title
        effectsTitle = std::make_unique<juce::Label>();
        effectsTitle->setText("EFFECT", juce::dontSendNotification);
        effectsTitle->setFont(juce::Font(27.648f, juce::Font::bold)); // 20% bigger: 23.04f * 1.2 = 27.648f
        effectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
        effectsTitle->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(effectsTitle.get());
        effectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30); // Moved left 10px and down 5px
        
        // Create dice button
        diceButton = std::make_unique<CustomDiceButton>();
        addAndMakeVisible(diceButton.get());
        
        // Position dice button to the right of the title (20% smaller and moved down 5px)
        const int diceSize = 32; // 20% smaller: 40 * 0.8 = 32
        diceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize); // Moved down 5px from +0 to +5
        
        // Set the dice image
        if (assets.diceLarge != nullptr)
        {
            diceButton->setDiceImage(assets.diceLarge->createCopy());
        }
        
        // Set up dice button click handler
        diceButton->onClick = [this]() {
            randomizeKnobValues();
        };
        
        DBG("[UI] Effects area setup complete");
    }

//==============================================================================
// Sequencer Area Setup
//==============================================================================

void PluginEditor::setupSequencerArea()
{
    DBG("[UI] Setting up sequencer area...");
    
    // Step area bounds
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step buttons (2 rows of 8)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = stepArea.getX() + 20;
    const int startY = stepArea.getY() + 35; // Moved up 5px from +40 to +35
    
    for (int i = 0; i < 16; ++i) {
        stepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(stepButtons[i].get());
        
        // Position buttons in 2 rows of 8
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        stepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set images
        if (assets.stepActive != nullptr)
            stepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        if (assets.stepInactive != nullptr)
            stepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        
        // Set up click handler
        stepButtons[i]->onClick = [this, i]() {
            onStepButtonClicked(i);
        };
    }
    
    // Create step amount label with white border (top right)
    stepAmountLabel = std::make_unique<juce::Label>();
    stepAmountLabel->setText("16", juce::dontSendNotification);
    stepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    stepAmountLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    stepAmountLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    stepAmountLabel->setColour(juce::Label::outlineColourId, juce::Colours::white);
    stepAmountLabel->setJustificationType(juce::Justification::centred);
    stepAmountLabel->setBorderSize(juce::BorderSize<int>(2));
    addAndMakeVisible(stepAmountLabel.get());
    stepAmountLabel->setBounds(stepArea.getX() + 260, stepArea.getY() - 10, 30, 25); // Moved left 40px from +300 to +260
    
    // Create rate dropdown (top right)
    rateDropdown = std::make_unique<juce::ComboBox>();
    rateDropdown->addItem("1/1", 1);
    rateDropdown->addItem("1/2", 2);
    rateDropdown->addItem("1/4", 3);
    rateDropdown->addItem("1/8", 4);
    rateDropdown->addItem("1/16", 5);
    rateDropdown->addItem("1/32", 6);
    rateDropdown->setSelectedId(4); // Default to 1/8
    rateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    rateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    rateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible(rateDropdown.get());
    rateDropdown->setBounds(stepArea.getX() + 300, stepArea.getY() - 10, 60, 25); // Moved left 40px from +340 to +300
    
    // Create S/T/D circular toggle (top right)
    stdToggle = std::make_unique<CircularToggleButton>();
    stdToggle->setButtonText("-");
    addAndMakeVisible(stdToggle.get());
    stdToggle->setBounds(stepArea.getX() + 370, stepArea.getY() - 10, 30, 30); // Made circular (30x30)
    
    // Set up toggle handler
    stdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        const char* labels[] = {"-", "t", "."};
        stdToggle->setButtonText(labels[stdState]);
    };
    
    DBG("[UI] Sequencer area setup complete");
}

void PluginEditor::randomizeKnobValues()
{
    DBG("[UI] Randomizing knob values...");
    
    // Randomize each knob to a random value between 0.0 and 1.0
    for (int i = 0; i < 8; ++i)
    {
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        knobs[i]->setValue(randomValue);
        
        // Update the value label
        valueLabels[i]->setText(juce::String(randomValue, 2), juce::dontSendNotification);
        
        // Update the indicator bar
        indicatorBars[i]->setValue(randomValue);
    }
    
    DBG("[UI] All knob values randomized");
}

void PluginEditor::randomizeIndividualKnob(int knobIndex)
{
    DBG("[UI] Randomizing individual knob " << knobIndex << "...");
    
    if (knobIndex >= 0 && knobIndex < 8)
    {
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        knobs[knobIndex]->setValue(randomValue);
        
        // Update the value label
        valueLabels[knobIndex]->setText(juce::String(randomValue, 2), juce::dontSendNotification);
        
        // Update the indicator bar
        indicatorBars[knobIndex]->setValue(randomValue);
        
        DBG("[UI] Knob " << knobIndex << " randomized to " << randomValue);
    }
}

void PluginEditor::updateParameterFromKnob(int knobIndex)
{
    if (knobIndex >= 0 && knobIndex < 8 && knobs[knobIndex] != nullptr && knobIndex < processorRef.getParameters().size())
    {
        auto* param = processorRef.getParameters().getUnchecked(knobIndex);
        float knobValue = knobs[knobIndex]->getValue();
        
        // Knob value is already normalized (0-1), so directly set parameter
        param->setValueNotifyingHost(knobValue);
        
        // Update the current step snapshot with the new value
        // Convert normalized value back to actual parameter value
        float actualValue = 0.0f;
        if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
        {
            actualValue = floatParam->convertFrom0to1(knobValue);
        }
        
        // Update the snapshot for the currently selected step
        processorRef.updateCurrentStepSnapshot(knobIndex, actualValue);
    }
}

void PluginEditor::onStepButtonClicked(int stepIndex)
{
    DBG("[UI] Step button " << stepIndex << " clicked");
    
    // Update selected step in processor
    processorRef.setSelectedStep(stepIndex);
    
    // Update UI to show which step is selected
    updateSequencerUI();
    
    // Load the snapshot for this step into the knobs
    auto snapshot = processorRef.getSafeSnapshot(stepIndex);
    
    // Update knobs with snapshot values (convert from actual values to normalized 0-1)
    knobs[0]->setValue(processorRef.getAPVTS().getParameter("timeMs")->convertTo0to1(snapshot.delay.timeMs), juce::dontSendNotification);
    knobs[1]->setValue(processorRef.getAPVTS().getParameter("feedback")->convertTo0to1(snapshot.delay.feedback / 100.0f), juce::dontSendNotification);
    knobs[2]->setValue(processorRef.getAPVTS().getParameter("wowDepth")->convertTo0to1(snapshot.delay.wowDepth / 100.0f), juce::dontSendNotification);
    knobs[3]->setValue(processorRef.getAPVTS().getParameter("wowRate")->convertTo0to1(snapshot.delay.wowRate), juce::dontSendNotification);
    knobs[4]->setValue(processorRef.getAPVTS().getParameter("drive")->convertTo0to1(snapshot.delay.saturation / 100.0f), juce::dontSendNotification);
    knobs[5]->setValue(processorRef.getAPVTS().getParameter("hiCut")->convertTo0to1(snapshot.delay.highCut), juce::dontSendNotification);
    knobs[6]->setValue(processorRef.getAPVTS().getParameter("lowCut")->convertTo0to1(snapshot.delay.lowCut), juce::dontSendNotification);
    knobs[7]->setValue(processorRef.getAPVTS().getParameter("mix")->convertTo0to1(snapshot.delay.mix / 100.0f), juce::dontSendNotification);
    
    // Also update the APVTS parameters to match the snapshot
    processorRef.getAPVTS().getParameter("timeMs")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("timeMs")->convertTo0to1(snapshot.delay.timeMs));
    processorRef.getAPVTS().getParameter("feedback")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("feedback")->convertTo0to1(snapshot.delay.feedback / 100.0f));
    processorRef.getAPVTS().getParameter("wowDepth")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("wowDepth")->convertTo0to1(snapshot.delay.wowDepth / 100.0f));
    processorRef.getAPVTS().getParameter("wowRate")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("wowRate")->convertTo0to1(snapshot.delay.wowRate));
    processorRef.getAPVTS().getParameter("drive")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("drive")->convertTo0to1(snapshot.delay.saturation / 100.0f));
    processorRef.getAPVTS().getParameter("hiCut")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("hiCut")->convertTo0to1(snapshot.delay.highCut));
    processorRef.getAPVTS().getParameter("lowCut")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("lowCut")->convertTo0to1(snapshot.delay.lowCut));
    processorRef.getAPVTS().getParameter("mix")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("mix")->convertTo0to1(snapshot.delay.mix / 100.0f));
}

void PluginEditor::updateSequencerUI()
{
    // Update step button selection states
    int selectedStep = processorRef.getSelectedStep();
    int playingStep = processorRef.getPlayingStep();
    
    for (int i = 0; i < 16; ++i) {
        if (stepButtons[i] != nullptr) {
            // Only the selected step should show as selected
            stepButtons[i]->setSelected(i == selectedStep);
            // Only the playing step should show as playing (if sequencer is enabled)
            stepButtons[i]->setPlaying(i == playingStep && processorRef.isSequencerEnabled());
        }
    }
    
    // Update step amount display
    if (stepAmountLabel != nullptr) {
        int stepsUsed = processorRef.getSeqState().stepsUsed.load();
        stepAmountLabel->setText(juce::String(stepsUsed), juce::dontSendNotification);
    }
    
    // Update rate dropdown
    if (rateDropdown != nullptr) {
        int divisionIndex = processorRef.getSeqState().divisionIndex.load();
        rateDropdown->setSelectedId(divisionIndex + 1);
    }
}