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
    // Empty for now
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
    setRange(0.0, 1.0, 0.01);
    setValue(0.5);
    // Set rotation to start at -150 degrees and go to +150 degrees (300 degree total range)
    setRotaryParameters(-150.0f * juce::MathConstants<float>::pi / 180.0f, 150.0f * juce::MathConstants<float>::pi / 180.0f, true);
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto centre = bounds.getCentre();
    
    DBG("[Knob] Painting knob - bounds: " << bounds.toString() << " centre: " << centre.toString());
    DBG("[Knob] Inner image: " << (innerImage != nullptr ? "OK" : "NULL"));
    DBG("[Knob] Ring image: " << (ringImage != nullptr ? "OK" : "NULL"));
    DBG("[Knob] Value: " << getValue() << " rotation: " << (getValue() * 300.0f));
    DBG("[Knob] Inner bounds reduction: " << (bounds.getWidth() * 0.04368f) << " x " << (bounds.getHeight() * 0.04368f));
    
    // Draw inner image with rotation first (bottom layer) - 4.368% smaller
    if (innerImage != nullptr)
    {
        g.saveState();
        // Calculate rotation: 0.5 at value 0.5 should be straight up (0 degrees)
        // Map 0.0-1.0 to -150 to +150 degrees, then convert to radians
        float rotationAngle = (getValue() - 0.5f) * 300.0f * juce::MathConstants<float>::pi / 180.0f;
        DBG("[Knob] Rotation angle: " << rotationAngle << " radians (" << (rotationAngle * 180.0f / juce::MathConstants<float>::pi) << " degrees)");
        g.addTransform(juce::AffineTransform::rotation(rotationAngle, centre.x, centre.y));
        // Make inner image 15% smaller from current size and bottom align
        auto innerBounds = bounds.reduced(bounds.getWidth() * 0.15f, bounds.getHeight() * 0.15f);
        DBG("[Knob] Inner bounds after expansion: " << innerBounds.toString());
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
            DBG("[Knob] Ring bounds: " << ringBounds.toString());
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
// PluginEditor Helper Methods
//==============================================================================


void PluginEditor::setupKnobs()
{
    DBG("[UI] Setting up knobs...");
    
        // Knob parameters
        std::vector<juce::String> knobNames = {
            "Time", "Feedback", "Mix", "Drive",
            "Low Cut", "High Cut", "Wow Rate", "Wow Depth"
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