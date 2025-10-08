#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "ui/PanIndicator.h"

//==============================================================================
// CustomEffectDropdown Implementation
//==============================================================================



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
    setupMasterKnobs();
    setupMacroKnobs();
        
        // Setup effects area
        setupEffectsArea();
        setupSpaceDelayUI();
        setupFxPowerButton();
        
        // Setup All Steps toggle
        setupAllStepsToggle();
        
        // Setup sequencer area
        setupSequencerArea();
        
        // Setup step power button
        setupStepPowerButton();
        
        // Setup UI toggle
        setupUIToggle();
        
        // Setup play button
        setupPlayButton();
        
        // Setup AutoPan page components
        setupAutoPanKnobs();
        setupAutoPanEffectsArea();
        setupAutoPanSequencerArea();
        setupAutoPanAllStepsToggle();
        setupAutoPanStepPowerButton();
        
        // Setup tab system
        setupTabSystem();
        
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
    // Draw the appropriate background based on current page
    if (currentPage == FxPageID::SpaceDelay) {
        // Draw the SpaceDelay background SVG
        if (assets.spaceDelayBackgroundTab1 != nullptr)
        {
            assets.spaceDelayBackgroundTab1->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            // Fallback background
            g.fillAll (juce::Colour(0xff2a2a2a));
        }
    } else if (currentPage == FxPageID::Panner) {
        // Draw the Panner background SVG
        if (assets.pannerBackgroundTab2 != nullptr)
        {
            assets.pannerBackgroundTab2->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            // Fallback background
            g.fillAll (juce::Colour(0xff2a2a2a));
        }
    }
    
    // Only draw grid overlay and main areas if UI is visible
    if (uiVisible) {
    // Draw the grid overlay
    drawGridOverlay(g);
    
    // Draw the three main areas
    drawMainAreas(g);
    }

    // Draw knob lock icons on top of UI (only on SpaceDelay page)
    if (currentPage == FxPageID::SpaceDelay)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (knobLockButtons[i] != nullptr)
            {
                auto b = knobLockButtons[i]->getBounds().toFloat();
                if (knobLocked[i]) {
                    if (assets.lockedIcon != nullptr)
                        assets.lockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                } else {
                    if (assets.unlockedIcon != nullptr)
                        assets.unlockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                }
            }
        }
    }
}

void PluginEditor::resized()
{
    // Place the tabs along the top strip - smaller and moved up 10px
    const int tabH = 34; // visual tab height (smaller)
    const int tabW = 120; // width (smaller)

    // Positions moved up 10px and closer together
    if (tabSpaceDelay) tabSpaceDelay->setBounds(24, 0, tabW, tabH);   // orange left tab
    if (tabPanner) tabPanner->setBounds(170, 0, tabW, tabH);      // green tab closer to left tab
}


void PluginEditor::drawGridOverlay(juce::Graphics& g)
{
    // Only draw the grid overlay if UI is visible
    if (!uiVisible) return;
    
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
    // Only draw the colored area boxes if UI is visible
    if (!uiVisible) return;
    
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
                // In sync mode for Time knob, do not overwrite the slider position (we use it to select divisions)
                if (!(i == 0 && timeSyncEnabled))
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
                    if (i == 0)
                    {
                        if (timeSyncEnabled)
                        {
                            // Show division text instead of ms
                            const double bpm = processorRef.getBpmOrDefault(120.0);
                            std::vector<std::pair<juce::String, double>> divisions = {
                                {"2", 2.0}, {"1", 1.0}, {"1/2", 0.5}, {"1/4", 0.25}, {"1/8", 0.125}, {"1/16", 0.0625}, {"1/32", 0.03125}, {"1/64", 0.015625}
                            };
                            double mult = 1.0;
                            if (timeSyncStdMode == 1) mult = 2.0/3.0; else if (timeSyncStdMode == 2) mult = 1.5;
                            // Compute nearest division index from current knob pos (already set above when not dragging)
                            int idx = juce::jlimit(0, 7, (int) std::round(knobs[i]->getValue() * 7.0f));
                            auto label = divisions[idx].first;
                            if (timeSyncStdMode == 1) label << "t"; else if (timeSyncStdMode == 2) label << ".";
                            valueText = label;
                        }
                        else
                        {
                        valueText = juce::String(actualValue, 0) + "ms";
                        }
                    }
                    else if (i == 5 || i == 6) // Hi-Cut, Low-Cut - show in Hz
                        valueText = juce::String((int) std::round(actualValue)) + "Hz";
                    else if (i == 3) // Wow Rate - show in Hz
                        valueText = juce::String((int) std::round(actualValue)) + "Hz";
                    else // Others - show as percentage (whole numbers)
                        valueText = juce::String((int) std::round(actualValue * 100)) + "%";
                }
                else
                {
                    // Fallback for other parameter types
                    valueText = juce::String((int) std::round(paramValue * 100)) + "%";
                }
                
                valueLabels[i]->setText(valueText, juce::dontSendNotification);
            }
            
            // Update indicator bar
            if (indicatorBars[i] != nullptr)
            {
                float indicatorValue = paramValue;

                // If sequencer is enabled and running, show the playing step's snapshot value
                const bool seqEnabled = processorRef.isSequencerEnabled();
                const bool seqActive = processorRef.getSeqActive();
                const int playingStep = processorRef.getCurrentSeqStepAudioThread(); // Read from audio thread
                
                // Only show sequencer snapshot if sequencer is actively playing
                if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16)
                {
                    StepSnapshot s = processorRef.getSafeSnapshot(playingStep);
                    auto* p = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter(i == 0 ? "timeMs"
                                                                                                                    : i == 1 ? "feedback"
                                                                                                                    : i == 2 ? "wowDepth"
                                                                                                                    : i == 3 ? "wowRate"
                                                                                                                    : i == 4 ? "drive"
                                                                                                                    : i == 5 ? "hiCut"
                                                                                                                    : i == 6 ? "lowCut"
                                                                                                                    : "mix"));
                    if (p != nullptr)
                    {
                        float actual = 0.0f;
                        switch (i)
                        {
                            case 0: actual = s.delay.timeMs; break;                                  // ms
                            case 1: actual = (s.delay.feedback / 100.0f) * 0.95f; break;             // % -> 0..0.95
                            case 2: actual = (s.delay.wowDepth / 100.0f); break;                     // % -> 0..1
                            case 3: actual = s.delay.wowRate; break;                                 // Hz 0.1..8
                            case 4: actual = (s.delay.saturation / 100.0f); break;                   // % -> 0..1
                            case 5: actual = s.delay.highCut; break;                                 // Hz 1k..20k
                            case 6: actual = s.delay.lowCut; break;                                  // Hz 20..2000
                            case 7: actual = s.delay.mix; break;                                     // 0..1
                            default: break;
                        }
                        indicatorValue = p->convertTo0to1(actual);
                    }
                }
                // If sequencer is not running or not active, indicator bars show current parameter values (paramValue)

                indicatorBars[i]->setValue(indicatorValue);
            }
        }
    }
    
        // Update master knob value labels (SliderAttachment handles knob values automatically)
        for (int i = 0; i < 3; ++i)
        {
            if (masterKnobs[i] != nullptr && masterValueLabels[i] != nullptr)
            {
                float knobValue = masterKnobs[i]->getValue();
                
                if (i == 1) { // Dry/Wet knob - show as percentage
                    int percentage = (int) std::round(knobValue * 100);
                    masterValueLabels[i]->setText(juce::String(percentage) + "%", juce::dontSendNotification);
                } else { // Input and Output knobs - show as dB
                    masterValueLabels[i]->setText(juce::String(knobValue, 1) + " dB", juce::dontSendNotification);
                }
            }
        }
        
        // Modern dual-bar meters update automatically via their timer
    
    // Update AutoPan knob values and UI
    for (int i = 0; i < 6; ++i)
    {
        if (autopanKnobs[i] != nullptr)
        {
            // Update value labels for AutoPan knobs
            if (autopanValueLabels[i] != nullptr)
            {
                float knobValue = autopanKnobs[i]->getValue();
                juce::String valueText;
                
                switch (i) {
                    case 0: // Rate
                        if (autopanTimeSyncEnabled) {
                            // Show division text - matches allDivisions array from PanSync.h
                            std::vector<juce::String> divisionLabels = {
                                "4 bars", "2 bars", "1 bar", "1/2.", "1/2", 
                                "1/4.", "1/4", "1/4T", 
                                "1/8", "1/8.", "1/8T",
                                "1/16", "1/16.", "1/16T", "1/32", "1/64"
                            };
                            // Map 0.0-1.0 to indices 0-15 (16 divisions)
                            int divIndex = juce::jlimit(0, 15, (int)(knobValue * 15.0f));
                            valueText = divisionLabels[divIndex];
                        } else {
                            valueText = juce::String(knobValue, 2) + "Hz";
                        }
                        break;
                    case 1: // Phase
                        valueText = juce::String(knobValue, 0) + "°";
                        break;
                    case 2: // Wave Type
                        {
                            std::vector<juce::String> waveTypes = {"Sine", "Triangle", "Ramp Dn", "Ramp Up", "Random"};
                            int typeIndex = juce::jlimit(0, 4, (int)knobValue);
                            valueText = waveTypes[typeIndex];
                        }
                        break;
                    case 3: // Wave Shape
                        valueText = juce::String(knobValue, 2);
                        break;
                    case 4: // Normal/Inverted
                        valueText = knobValue > 0.5 ? "Inverted" : "Normal";
                        break;
                    case 5: // Amount
                        valueText = juce::String((int)(knobValue * 100)) + "%";
                        break;
                }
                autopanValueLabels[i]->setText(valueText, juce::dontSendNotification);
            }
            
            // Update indicator bars to show current playing step's snapshot value
            if (autopanIndicatorBars[i] != nullptr)
            {
                float indicatorValue = autopanKnobs[i]->getValue();
                
                // If AutoPan sequencer is enabled and running, show the playing step's snapshot value
                const bool seqEnabled = processorRef.getAutoPanSeqState().enabled.load();
                const bool seqActive = processorRef.getAutoPanSeqState().active.load();
                const int playingStep = processorRef.getAutoPanPlayingStep();
                
                if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16)
                {
                    StepSnapshot s = processorRef.getAutoPanSafeSnapshot(playingStep);
                    
                    switch (i) {
                        case 0: // Rate (0-1)
                            indicatorValue = s.autopan.rate;
                            break;
                        case 1: // Phase (0-360, normalize to 0-1)
                            indicatorValue = s.autopan.phase / 360.0f;
                            break;
                        case 2: // Wave Type (0-4, normalize to 0-1)
                            indicatorValue = (float)s.autopan.waveType / 4.0f;
                            break;
                        case 3: // Wave Shape (0-1)
                            indicatorValue = s.autopan.waveShape;
                            break;
                        case 4: // Inverted (0-1)
                            indicatorValue = s.autopan.inverted ? 1.0f : 0.0f;
                            break;
                        case 5: // Amount (0-1)
                            indicatorValue = s.autopan.amount;
                            break;
                    }
                }
                
                autopanIndicatorBars[i]->setValue(indicatorValue);
            }
        }
    }
    
    
    // Update sequencer UI (Delay)
    updateSequencerUI();
    
    // Update AutoPan sequencer UI
    updateAutoPanSequencerUI();
}

//==============================================================================
// AllStepsToggleButton Implementation
//==============================================================================

AllStepsToggleButton::AllStepsToggleButton() : juce::Button("allStepsToggle")
{
    setClickingTogglesState(true);
}

void AllStepsToggleButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    juce::ignoreUnused(over, down);
    
    auto bounds = getLocalBounds().toFloat();
    
    // Choose image based on toggle state
    juce::Drawable* imageToDraw = nullptr;
    if (getToggleState() && activeImage != nullptr)
    {
        imageToDraw = activeImage.get();
    }
    else if (inactiveImage != nullptr)
    {
        imageToDraw = inactiveImage.get();
    }
    
    if (imageToDraw != nullptr)
    {
        imageToDraw->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback drawing
        g.setColour(juce::Colours::white);
        g.fillEllipse(bounds);
    }
}

void AllStepsToggleButton::setImages(std::unique_ptr<juce::Drawable> inactive, std::unique_ptr<juce::Drawable> active)
{
    inactiveImage = std::move(inactive);
    activeImage = std::move(active);
}

//==============================================================================
// LockButton Implementation
//==============================================================================

LockButton::LockButton() : juce::Button("lockButton")
{
    setClickingTogglesState(true);
}

void LockButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    juce::ignoreUnused(over, down);
    
    auto bounds = getLocalBounds().toFloat();
    
    // Choose image based on toggle state
    juce::Drawable* imageToDraw = nullptr;
    if (getToggleState() && lockedImage != nullptr)
    {
        imageToDraw = lockedImage.get();
    }
    else if (unlockedImage != nullptr)
    {
        imageToDraw = unlockedImage.get();
    }
    
    if (imageToDraw != nullptr)
    {
        // Draw the image (alpha is already applied to the image itself)
        imageToDraw->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        // Fallback drawing
        g.setColour(juce::Colours::white.withAlpha(buttonAlpha));
        g.fillEllipse(bounds);
    }
}

void LockButton::setImages(std::unique_ptr<juce::Drawable> unlocked, std::unique_ptr<juce::Drawable> locked)
{
    unlockedImage = std::move(unlocked);
    lockedImage = std::move(locked);
}

void LockButton::setAlpha(float alpha)
{
    buttonAlpha = alpha;
    
    // Create greyed-out versions of the images when alpha < 1.0
    if (alpha < 1.0f && (unlockedImage != nullptr || lockedImage != nullptr))
    {
        // Store original images if not already stored
        if (originalUnlockedImage == nullptr && unlockedImage != nullptr)
            originalUnlockedImage = unlockedImage->createCopy();
        if (originalLockedImage == nullptr && lockedImage != nullptr)
            originalLockedImage = lockedImage->createCopy();
        
        // Create greyed versions
        if (unlockedImage != nullptr)
        {
            auto greyUnlocked = originalUnlockedImage->createCopy();
            greyUnlocked->setAlpha(alpha);
            unlockedImage = std::move(greyUnlocked);
        }
        
        if (lockedImage != nullptr)
        {
            auto greyLocked = originalLockedImage->createCopy();
            greyLocked->setAlpha(alpha);
            lockedImage = std::move(greyLocked);
        }
    }
    else if (alpha >= 1.0f && (originalUnlockedImage != nullptr || originalLockedImage != nullptr))
    {
        // Restore original images when alpha is full
        if (originalUnlockedImage != nullptr)
            unlockedImage = originalUnlockedImage->createCopy();
        if (originalLockedImage != nullptr)
            lockedImage = originalLockedImage->createCopy();
    }
    
    repaint();
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
        // Calculate rotation: normalize value to 0-1 range for rotation calculation
        // Map 0.0-1.0 to -150 to +150 degrees, then convert to radians
        float normalizedValue = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
        float rotationAngle = (normalizedValue - 0.5f) * 300.0f * juce::MathConstants<float>::pi / 180.0f;
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

PlayButton::PlayButton() : juce::Button("playButton")
{
    setClickingTogglesState(false); // We handle state manually
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

void PlayButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    auto centre = bounds.getCentre();
    
    // Set color based on playing state
    juce::Colour buttonColor = isPlaying ? juce::Colours::white : juce::Colours::grey;
    g.setColour(buttonColor);
    
    // Draw play symbol (triangle pointing right)
    juce::Path playPath;
    float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.6f;
    float x = centre.x - size * 0.3f;
    float y = centre.y - size * 0.5f;
    
    playPath.addTriangle(x, y, x, y + size, x + size, centre.y);
    g.fillPath(playPath);
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
    
    // Opacity for inactive steps (70% dimmed → 30% visible)
    const float stepOpacity = isEnabledStep ? 1.0f : 0.3f;
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(1.0f, 1.0f));

    // Draw grey highlight background when playing
    if (isPlaying) {
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.fillRoundedRectangle(bounds, 5.0f);
    }
    
    // Choose which image to draw based on state
    std::unique_ptr<juce::Drawable>* imageToDraw = nullptr;
    
    if (isSelected) {
        imageToDraw = &activeImage;
    } else {
        imageToDraw = &inactiveImage;
    }
    
    if (imageToDraw && *imageToDraw != nullptr) {
        // Draw the SVG with the desired opacity (do not rely on global g opacity)
        (*imageToDraw)->drawWithin(g, bounds, juce::RectanglePlacement::centred, stepOpacity);
    } else {
        // Fallback drawing
        g.setColour(isSelected ? juce::Colours::white.withAlpha(stepOpacity) : juce::Colours::grey.withAlpha(stepOpacity));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
        
        // Draw step number
        g.setColour(juce::Colours::black.withAlpha(stepOpacity));
        g.setFont(12.0f);
        g.drawText(juce::String(stepIndex + 1), bounds, juce::Justification::centred);
    }
    
    // Add highlight when hovering
    if (over) {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 5.0f);
    }
    g.restoreState();
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
                if (i == 0 && timeSyncEnabled)
                {
                    // Use full throw to select division; update parameter directly without changing slider value
                    float pos = knobs[0]->getValue();
                    int idx = juce::jlimit(0, 7, (int) std::round(pos * 7.0f));
                    // Ascending divisions: smallest first
                    static const double baseBeats[8] = { 1.0/64.0, 1.0/32.0, 1.0/16.0, 1.0/8.0, 1.0/4.0, 1.0/2.0, 1.0, 2.0 };
                    const double bpm = processorRef.getBpmOrDefault(120.0);
                    double mult = 1.0;
                    if (timeSyncStdMode == 1) mult = 2.0/3.0; else if (timeSyncStdMode == 2) mult = 1.5;
                    double beatsSel = baseBeats[idx] * mult;
                    double ms = beatsSel * (60.0 / juce::jmax(1.0, bpm)) * 1000.0;
                    // Set parameter (normalized) to reflect ms
                    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter("timeMs")))
                    {
                        float norm = p->convertTo0to1((float) ms);
                        p->setValueNotifyingHost(norm);
                        // Update snapshot with real value and keep UI labels consistent
                        processorRef.updateCurrentStepSnapshot(0, (float) ms);
                    }
                }
                else
                {
                updateParameterFromKnob(i);
                }
                
                // If All Steps toggle is active, update all step snapshots
                if (allStepsEnabled)
                {
                    updateAllStepSnapshots(i);
                }
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
        valueLabels[i]->setText("0", juce::dontSendNotification);
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
            
            // Create individual dice button for this knob (kept in code but hidden)
            knobDiceButtons[i] = std::make_unique<CustomDiceButton>();
            // Explicitly keep hidden
            knobDiceButtons[i]->setVisible(false);
            
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
            
            // Remember dice button bounds to reuse for lock button sizing/placement
            const int lockX = diceX;
            const int lockY = diceY;
            
            // Set the dice image
            // Set up lock button replacing dice
            knobLockButtons[i] = std::make_unique<LockButton>();
            addAndMakeVisible(knobLockButtons[i].get());
            knobLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);
            
            // Configure images for on/off states
            std::unique_ptr<juce::Drawable> imgUnlocked, imgLocked;
            if (assets.unlockedIcon) imgUnlocked = assets.unlockedIcon->createCopy();
            if (assets.lockedIcon)   imgLocked   = assets.lockedIcon->createCopy();
            
            knobLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
            knobLockButtons[i]->setToggleState(knobLocked[i], juce::dontSendNotification);
            knobLockButtons[i]->onClick = [this, i]() {
                knobLocked[i] = knobLockButtons[i]->getToggleState();
                repaint();
            };
            
            // No dice click; replaced by lock
    }
    
        DBG("[UI] Created " << 8 << " knobs with labels and indicator bars");
    }

    //==============================================================================
    // Master Knobs Setup
    //==============================================================================
    
    void PluginEditor::setupMasterKnobs()
    {
        DBG("[UI] Setting up master knobs...");
        
        // Master knob parameters
        std::vector<juce::String> masterKnobNames = {
            "Input", "Dry/Wet", "Output"
        };
        
        // Master knob parameters from APVTS (indices 8, 9, 10)
        std::vector<juce::String> masterParamNames = {
            "masterInput", "masterDryWet", "masterOutput"
        };
        
        // Master area bounds (positioned in master area)
        auto masterArea = juce::Rectangle<int>(453, 54, 413, 296);
        
        // Position master knobs horizontally in master area (centered and moved down 20px more)
        const int knobSize = 109; // 30% bigger: 84 * 1.3 = 109
        const int spacing = 129; // knobSize + 20px padding: 109 + 20 = 129
        const int totalKnobWidth = 3 * knobSize + 2 * 20; // 3 knobs + 2 gaps of 20px each
        const int startX = masterArea.getX() + (masterArea.getWidth() - totalKnobWidth) / 2 + 48; // Center the group + 48px right
        const int y = masterArea.getY() + 330; // Moved down 20px more: 310 + 20 = 330
        
        for (int i = 0; i < 3; ++i) {
            // Create master knob
            masterKnobs[i] = std::make_unique<CustomKnob>();
            addAndMakeVisible(masterKnobs[i].get());
            
            // Set master knob images
            if (assets.knobMasterRing != nullptr && assets.knobMasterInside != nullptr) {
                masterKnobs[i]->setRingImage(assets.knobMasterRing->createCopy());
                masterKnobs[i]->setInnerImage(assets.knobMasterInside->createCopy());
            }
            
            // Set knob range to match parameter range
            if (i == 0 || i == 2) { // Input and Output knobs - dB range
                masterKnobs[i]->setRange(-60.0, 6.0, 0.01);
            } else { // Dry/Wet knob - percentage range
                masterKnobs[i]->setRange(0.0, 1.0, 0.001);
            }
            
            // Set initial values
            float defaultValue = (i == 1) ? 1.0f : 0.0f; // Dry/Wet = 100% (1.0), Input/Output = 0.0 dB
            masterKnobs[i]->setValue(defaultValue, juce::dontSendNotification);
            
            // Create SliderAttachment for proper parameter binding
            const char* paramIds[] = {"masterInput", "masterDryWet", "masterOutput"};
            masterAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.getAPVTS(), paramIds[i], *masterKnobs[i]);
            
            masterKnobs[i]->setBounds(startX + i * spacing, y, knobSize, knobSize);
            
            // Create master label (title above knob)
            masterLabels[i] = std::make_unique<juce::Label>();
            masterLabels[i]->setText(masterKnobNames[i], juce::dontSendNotification);
            masterLabels[i]->setFont(juce::Font(12.0f, juce::Font::bold));
            masterLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
            masterLabels[i]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(masterLabels[i].get());
            masterLabels[i]->setBounds(startX + i * spacing, y - 25, knobSize, 20);
            
            // Create master value label (value below knob)
            masterValueLabels[i] = std::make_unique<juce::Label>();
            // Set initial value: Input and Output at 0.0 dB, Dry/Wet at 100%
            if (i == 1) { // Dry/Wet knob
                masterValueLabels[i]->setText("100%", juce::dontSendNotification);
            } else { // Input and Output knobs
                masterValueLabels[i]->setText("0.0 dB", juce::dontSendNotification);
            }
            masterValueLabels[i]->setFont(juce::Font(10.0f, juce::Font::plain));
            masterValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
            masterValueLabels[i]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(masterValueLabels[i].get());
            masterValueLabels[i]->setBounds(startX + i * spacing, y + knobSize - 10, knobSize, 15);
        }
        
        // Setup stereo meters (pre-fx and post-fx)
        const int meterWidth = 20;
        const int meterHeight = 120; // 20% shorter: 150 * 0.8 = 120
        const int meterSpacing = 30; // Space between meter and knob group
        
        // Create modern dual-bar meters
        auto& p = processorRef;
        DualBarMeter::Source inSrc {
            // lambdas read atomics; capture &p by reference OK
            [&]{ return p.getInputMeter().rmsDbL.load(); },
            [&]{ return p.getInputMeter().rmsDbR.load(); },
            [&]{ return p.getInputMeter().peakDbL.load(); },
            [&]{ return p.getInputMeter().peakDbR.load(); }
        };
        DualBarMeter::Source outSrc {
            [&]{ return p.getOutputMeter().rmsDbL.load(); },
            [&]{ return p.getOutputMeter().rmsDbR.load(); },
            [&]{ return p.getOutputMeter().peakDbL.load(); },
            [&]{ return p.getOutputMeter().peakDbR.load(); }
        };

        inMeter  = std::make_unique<DualBarMeter>(inSrc);
        outMeter = std::make_unique<DualBarMeter>(outSrc);
        addAndMakeVisible(*inMeter);
        addAndMakeVisible(*outMeter);
        
        // Create PanManBar visualizer
        PanManBar::Reader r;
        r.getPhase01 = [this] { return processorRef.panClock.phase01.load(std::memory_order_acquire); };
        r.getIncPerSample = [this] { return processorRef.panClock.incPerSample.load(std::memory_order_acquire); };
        r.getSampleRate = [this] { return processorRef.panClock.sampleRate.load(std::memory_order_acquire); };
        r.getDepth01 = [this] { 
            auto* param = processorRef.getAPVTS().getRawParameterValue("autopanAmount");
            return param ? param->load() : 0.5f;
        };
        r.getPhaseOffset01 = [this] { 
            auto* param = processorRef.getAPVTS().getRawParameterValue("autopanPhase");
            return param ? param->load() / 360.0f : 0.5f;  // Default 180° = 0.5
        };
        r.getShape01 = [this] { 
            auto* param = processorRef.getAPVTS().getRawParameterValue("autopanWaveShape");
            return param ? param->load() : 0.0f;
        };
        r.getWaveType = [this] { 
            auto* param = processorRef.getAPVTS().getRawParameterValue("autopanWaveType");
            return param ? (int)param->load() : 0;
        };
        r.getInverted = [this] { 
            auto* param = processorRef.getAPVTS().getRawParameterValue("autopanInverted");
            return param ? param->load() > 0.5f : false;
        };
        r.getIsPlaying = [this] { 
            // Check if sync is enabled and if so, check transport state
            auto* syncParam = processorRef.getAPVTS().getRawParameterValue("autopanTimeSync");
            if (syncParam && syncParam->load() > 0.5f) {
                return processorRef.isTransportPlaying();
            }
            return true; // Free-running mode always plays
        };
        
        panBar = std::make_unique<PanManBar>(r, 72); // 72 bins looks nice
        panBar->setColours(juce::Colour(0xFF2A2C30), juce::Colours::white); // dark track, white bins
        addAndMakeVisible(*panBar);
        
        // Position meters
        inMeter->setBounds(startX - meterSpacing - meterWidth + 10, y + (knobSize - meterHeight) / 2, meterWidth, meterHeight);  // Move right 10px
        outMeter->setBounds(startX + totalKnobWidth + meterSpacing - 10, y + (knobSize - meterHeight) / 2, meterWidth, meterHeight);  // Move left 10px
        
        // Position PanManBar above the master knobs
        panBar->setBounds(startX - meterSpacing - meterWidth + 10, y - 285, (meterWidth * 2 + meterSpacing + totalKnobWidth - 20) - 90, 24);
        
        DBG("[UI] Master knobs and stereo meters setup complete");
    }

    // Macro Knobs Setup
    //==============================================================================
    void PluginEditor::setupMacroKnobs()
    {
        DBG("[UI] Setting up macro knobs...");
        
        // Macro knob parameters
        std::vector<juce::String> macroKnobNames = {
            "Macro 1", "Macro 2"
        };
        
        // Master area bounds (positioned in master area)
        auto masterArea = juce::Rectangle<int>(453, 54, 413, 296);
        
        // Position macro knobs vertically on the right side of master area
        const int knobSize = 92; // 15% bigger than regular knobs: 80 * 1.15 = 92
        const int spacing = 120; // knobSize + 28px padding for vertical stacking
        const int startX = masterArea.getX() + masterArea.getWidth() - knobSize - 20 + 85; // Right side with 20px margin + 85px right
        const int startY = masterArea.getY() + 50 + 20; // Start 50px from top of master area + 20px down
        
        for (int i = 0; i < 2; ++i) {
            // Create macro knob
            macroKnobs[i] = std::make_unique<CustomKnob>();
            addAndMakeVisible(macroKnobs[i].get());
            
            // Set macro knob images (same as effect knobs)
            if (assets.knobRing != nullptr && assets.knobInside != nullptr) {
                macroKnobs[i]->setRingImage(assets.knobRing->createCopy());
                macroKnobs[i]->setInnerImage(assets.knobInside->createCopy());
            }
            
            // Set knob range (0-1 for macro control)
            macroKnobs[i]->setRange(0.0, 1.0, 0.001);
            macroKnobs[i]->setValue(0.0, juce::dontSendNotification);
            
            // Position the knob (move Macro 2 down 5px)
            int yOffset = (i == 1) ? 5 : 0; // Macro 2 (index 1) gets 5px down offset
            macroKnobs[i]->setBounds(startX, startY + i * spacing + yOffset, knobSize, knobSize);
            
            // Create macro label (title above knob)
            macroLabels[i] = std::make_unique<juce::Label>();
            macroLabels[i]->setText(macroKnobNames[i], juce::dontSendNotification);
            macroLabels[i]->setFont(juce::Font(12.0f, juce::Font::bold));
            macroLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
            macroLabels[i]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(macroLabels[i].get());
            macroLabels[i]->setBounds(startX, startY + i * spacing - 25 + yOffset, knobSize, 20);
            
            // Create macro assign button (15px wide, centered to the right of title)
            macroAssignButtons[i] = std::make_unique<juce::DrawableButton>("MacroAssign" + juce::String(i + 1), juce::DrawableButton::ButtonStyle::ImageStretched);
            addAndMakeVisible(macroAssignButtons[i].get());
            
            // Load the appropriate SVG for the assign button
            if (i == 0 && assets.macro1AssignButton != nullptr) {
                macroAssignButtons[i]->setImages(assets.macro1AssignButton.get());
            } else if (i == 1 && assets.macro2AssignButton != nullptr) {
                macroAssignButtons[i]->setImages(assets.macro2AssignButton.get());
            }
            
            // Position the assign button (15px wide, centered to the right of title)
            const int buttonWidth = 15;
            const int buttonHeight = 15;
            const int buttonX = startX + knobSize - buttonWidth - 5; // 5px from right edge of knob
            const int buttonY = startY + i * spacing - 25 + (20 - buttonHeight) / 2 + yOffset; // Centered vertically with title + yOffset
            macroAssignButtons[i]->setBounds(buttonX, buttonY, buttonWidth, buttonHeight);
        }
        
        DBG("[UI] Macro knobs setup complete");
    }

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

    // Time Sync small S toggle to the left of Time label
    struct SSyncButton : public juce::Button {
        SSyncButton() : juce::Button("SSync") {}
        void paintButton(juce::Graphics& g, bool over, bool down) override {
            juce::ignoreUnused(over, down);
            auto r = getLocalBounds().toFloat();
            const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
            auto centre = r.getCentre();
            g.setColour(juce::Colours::white);
            if (getToggleState()) {
                g.fillEllipse(centre.x - radius, centre.y - radius, radius*2, radius*2);
                g.setColour(juce::Colours::black);
            } else {
                g.drawEllipse(centre.x - radius, centre.y - radius, radius*2, radius*2, 2.0f);
            }
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("S", r, juce::Justification::centred);
        }
    };
    timeSyncToggle = std::make_unique<SSyncButton>();
    addAndMakeVisible(timeSyncToggle.get());
    if (knobLabels[0] != nullptr) {
        auto lb = knobLabels[0]->getBounds();
        timeSyncToggle->setBounds(lb.getX() + 10, lb.getY() + 4, 12, 12); // moved left 4px and up 2px
    } else {
        timeSyncToggle->setBounds(effectArea.getX() + 10, effectArea.getY() + 10, 12, 12);
    }
    timeSyncToggle->setClickingTogglesState(true);
    timeSyncToggle->onClick = [this]() {
        timeSyncEnabled = timeSyncToggle->getToggleState();
        if (timeSyncEnabled && knobs[0] != nullptr) {
            const double bpm = processorRef.getBpmOrDefault(120.0);
            std::vector<double> beats = {2.0,1.0,0.5,0.25,0.125,0.0625,0.03125,0.015625};
            double mult = 1.0;
            if (timeSyncStdMode == 1) mult = 2.0/3.0; // triplet
            else if (timeSyncStdMode == 2) mult = 1.5; // dotted
            for (auto& b : beats) b *= mult;
            // Convert to ms
            for (auto& b : beats) b = b * (60.0 / juce::jmax(1.0, bpm)) * 1000.0;
            auto* p = processorRef.getAPVTS().getParameter("timeMs");
            double ms = (double) static_cast<juce::AudioParameterFloat*>(p)->convertFrom0to1(knobs[0]->getValue());
            ms = juce::jlimit(10.0, 2000.0, ms);
            double best = beats.front(); double bd = std::abs(best - ms);
            for (auto v : beats) { double d = std::abs(v - ms); if (d < bd) { best = v; bd = d; } }
            knobs[0]->setValue(p->convertTo0to1((float) best));
            updateParameterFromKnob(0);
        }
        repaint();
        };
        
        DBG("[UI] Effects area setup complete");
    }

void PluginEditor::setupSpaceDelayUI()
{
    DBG("[UI] Setting up space delay UI...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "Space Delay" title using SVG
    spaceDelayTitle = std::make_unique<juce::DrawableButton>("spaceDelayTitle", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(spaceDelayTitle.get());
    spaceDelayTitle->setBounds(effectArea.getX() - 40, effectArea.getY() - 45, 138, 29); // Moved up 5px more and made 15% larger (120*1.15=138, 25*1.15=29)
    
    // Load the Space Delay SVG
    if (assets.tabTitleSpaceDelay != nullptr) {
        spaceDelayTitle->setImages(assets.tabTitleSpaceDelay->createCopy().get());
    }
    
    // Make it non-interactive but enabled for display
    spaceDelayTitle->setClickingTogglesState(false);
    spaceDelayTitle->setEnabled(true); // Enable it so it's not greyed out
    
    // Create effect type dropdown
    effectTypeDropdown = std::make_unique<juce::ComboBox>();
    addAndMakeVisible(effectTypeDropdown.get());
    
    // Create and configure BigComboWithSvgLNF for larger popup menus with SVG caret
    fxComboLNF = std::make_unique<BigComboWithSvgLNF>();
    fxComboLNF->popupFontPx   = 18.0f;  // Larger font
    fxComboLNF->rowHeightPx   = 32;     // Taller rows
    fxComboLNF->minPopupWidth = 300;    // Wider minimum width
    fxComboLNF->closedHeight  = 36;     // Closed control height
    
    // Load carrot SVGs into the LookAndFeel
    if (assets.fxTypeCarrotInactive != nullptr) {
        fxComboLNF->setCaretSVG(assets.fxTypeCarrotInactive->createCopy());
    }
    
    // Apply the custom LookAndFeel to the FX ComboBox only
    effectTypeDropdown->setLookAndFeel(fxComboLNF.get());
    
    // Configure ComboBox colors for dark theme
    effectTypeDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2B2D31));
    effectTypeDropdown->setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xFF101113));
    effectTypeDropdown->setColour(juce::ComboBox::textColourId,       juce::Colours::white);
    
    // Add effect types
    effectTypeDropdown->addItem("Space Delay", 1);
    effectTypeDropdown->addItem("Chorus", 2);
    effectTypeDropdown->addItem("Flanger", 3);
    effectTypeDropdown->addItem("Phaser", 4);
    effectTypeDropdown->setSelectedId(1, juce::dontSendNotification);
    
    // Position dropdown with proper height for closed control
    effectTypeDropdown->setBounds(effectArea.getX() + 80 - 85 - 10, effectArea.getY() - 37 - 5 - 5, 120, fxComboLNF->closedHeight);
    effectTypeDropdown->setJustificationType(juce::Justification::centredLeft);
    
    // Set up dropdown change handler
    effectTypeDropdown->onChange = [this]() {
        int selectedId = effectTypeDropdown->getSelectedId();
        DBG("[UI] Effect type changed to: " << selectedId);
        // TODO: Implement effect type switching logic
    };
    
    DBG("[UI] Space delay UI setup complete");
}

void PluginEditor::setupFxPowerButton()
{
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);

    fxPowerButton = std::make_unique<juce::DrawableButton>("fxPowerButton", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(fxPowerButton.get());

    // Slightly bigger than step power (which is 40). Use 46.
    const int buttonSize = 46;
    fxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);

    fxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    fxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);

    if (assets.fxPowerOn != nullptr)
    {
        fxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }

    fxPowerButton->setClickingTogglesState(true);
    fxPowerButton->setToggleState(fxAreaEnabled, juce::dontSendNotification);
    fxPowerButton->onClick = [this]() {
        fxAreaEnabled = fxPowerButton->getToggleState();
        // Bypass FX when off
        processorRef.setFxEnabled(fxAreaEnabled);
        updateFxAreaVisibility();
        repaint();
    };
}

void PluginEditor::updateFxAreaVisibility()
{
    float alpha = fxAreaEnabled ? 1.0f : 0.3f;

    // Grey title, space delay title, effect dropdown and dice
    if (effectsTitle) effectsTitle->setAlpha(alpha);
    if (spaceDelayTitle) spaceDelayTitle->setAlpha(alpha); // Don't disable, just change alpha
    if (effectTypeDropdown) { effectTypeDropdown->setAlpha(alpha); effectTypeDropdown->setEnabled(fxAreaEnabled); }
    if (diceButton) { diceButton->setAlpha(alpha); diceButton->setEnabled(fxAreaEnabled); }

    // Grey knobs, labels, values, indicators, locks
    for (int i = 0; i < 8; ++i) {
        if (knobs[i]) { knobs[i]->setAlpha(alpha); knobs[i]->setEnabled(fxAreaEnabled); }
        if (knobLabels[i]) knobLabels[i]->setAlpha(alpha);
        if (valueLabels[i]) valueLabels[i]->setAlpha(alpha);
        if (indicatorBars[i]) indicatorBars[i]->setAlpha(alpha);
        if (knobLockButtons[i]) { 
            knobLockButtons[i]->setEnabled(fxAreaEnabled);
            knobLockButtons[i]->setAlpha(alpha);
        }
    }

    // Grey the time sync button
    if (timeSyncToggle) { timeSyncToggle->setAlpha(alpha); timeSyncToggle->setEnabled(fxAreaEnabled); }

    // Grey the power button itself when off
    if (fxPowerButton) fxPowerButton->setAlpha(fxAreaEnabled ? 1.0f : 0.3f);
}

//==============================================================================
// All Steps Toggle Setup
//==============================================================================

void PluginEditor::setupAllStepsToggle()
{
    DBG("[UI] Setting up All Steps toggle...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create All Steps toggle button
    allStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(allStepsToggle.get());
    
    // Position at top middle of effect area, moved 30px right, increased 20% and moved up 6px total
    const int buttonSize = 29; // 24 * 1.2 = 28.8, rounded to 29
    allStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, effectArea.getY() - 1, buttonSize, buttonSize); // Moved up 2px more from 1 to -1
    
    // Set up images
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr)
    {
        static_cast<AllStepsToggleButton*>(allStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    // Set up click handler
    allStepsToggle->setToggleState(false, juce::dontSendNotification);
    allStepsToggle->onClick = [this]() {
        allStepsEnabled = allStepsToggle->getToggleState();
        DBG("[UI] All Steps toggle: " << (allStepsEnabled ? "ON" : "OFF") << " toggleState=" << allStepsToggle->getToggleState());
    };
    
    // Create "All Steps" label
    allStepsLabel = std::make_unique<juce::Label>();
    allStepsLabel->setText("All Steps", juce::dontSendNotification);
    allStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold)); // 12.0f * 1.2 = 14.4f (20% bigger)
    allStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    allStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(allStepsLabel.get());
    
    // Position label to the right of the button, moved 30px right, moved up 4px
    allStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, effectArea.getY() + 1, 80, 24); // Moved up 4px from 5 to 1
    
    DBG("[UI] All Steps toggle setup complete");
}

//==============================================================================
// Sequencer Area Setup
//==============================================================================

void PluginEditor::setupSequencerArea()
{
    DBG("[UI] Setting up sequencer area...");
    
    // Step area bounds
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create STEP title (top left, moved down 10px total and 20% smaller)
    stepTitle = std::make_unique<juce::Label>();
    stepTitle->setText("STEP", juce::dontSendNotification);
    stepTitle->setFont(juce::Font(22.118f, juce::Font::bold)); // 20% smaller: 27.648f * 0.8 = 22.118f
    stepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    stepTitle->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    stepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stepTitle.get());
    stepTitle->setBounds(stepArea.getX() + 10, stepArea.getY(), 80, 30); // Moved down 2px more (total 10px down from original -10)
    
    // Create step dice button (next to STEP title, moved down 15px total and left 15px total)
    stepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(stepDiceButton.get());
    int stepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    stepDiceButton->setBounds(stepArea.getX() + 75, stepArea.getY() + 5, stepDiceSize, stepDiceSize); // Moved down another 10px and left another 10px
    
    // Set up step dice button callback to randomize all step snapshots
    stepDiceButton->onClick = [this]() {
        DBG("[UI] Step dice button clicked - randomizing all step snapshots");
        
        // Randomize all 16 step snapshots, but only for unlocked parameters
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Only randomize unlocked parameters
            if (!knobLocked[0]) snapshot.delay.timeMs = 10.0f + juce::Random::getSystemRandom().nextFloat() * (2000.0f - 10.0f);
            if (!knobLocked[1]) snapshot.delay.feedback = juce::Random::getSystemRandom().nextFloat() * 0.95f;
            if (!knobLocked[2]) snapshot.delay.wowDepth = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[3]) snapshot.delay.wowRate = 0.1f + juce::Random::getSystemRandom().nextFloat() * (8.0f - 0.1f);
            if (!knobLocked[4]) snapshot.delay.saturation = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[5]) snapshot.delay.highCut = 1000.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 1000.0f);
            if (!knobLocked[6]) snapshot.delay.lowCut = 20.0f + juce::Random::getSystemRandom().nextFloat() * (2000.0f - 20.0f);
            if (!knobLocked[7]) snapshot.delay.mix = juce::Random::getSystemRandom().nextFloat();
            
            // Update the snapshot in the processor
            processorRef.setStepSnapshot(step, snapshot);
        }
        
        // Update current step if one is selected to show the new values
        int selectedStep = processorRef.getSelectedStep();
        if (selectedStep >= 0 && selectedStep < 16) {
            // Load the randomized snapshot for the selected step
            auto snapshot = processorRef.getSafeSnapshot(selectedStep);
            
            // Update knobs with the new snapshot values using correct parameter ranges, respecting locks
            // timeMs: 10.0f to 2000.0f
            if (!knobLocked[0]) knobs[0]->setValue((snapshot.delay.timeMs - 10.0f) / (2000.0f - 10.0f), juce::dontSendNotification);
            // feedback: 0.0f to 0.95f (already normalized)
            if (!knobLocked[1]) knobs[1]->setValue(snapshot.delay.feedback, juce::dontSendNotification);
            // wowDepth: 0.0f to 1.0f (already normalized)
            if (!knobLocked[2]) knobs[2]->setValue(snapshot.delay.wowDepth, juce::dontSendNotification);
            // wowRate: 0.1f to 8.0f
            if (!knobLocked[3]) knobs[3]->setValue((snapshot.delay.wowRate - 0.1f) / (8.0f - 0.1f), juce::dontSendNotification);
            // drive/saturation: 0.0f to 1.0f (already normalized)
            if (!knobLocked[4]) knobs[4]->setValue(snapshot.delay.saturation, juce::dontSendNotification);
            // hiCut: 1000.0f to 20000.0f
            if (!knobLocked[5]) knobs[5]->setValue((snapshot.delay.highCut - 1000.0f) / (20000.0f - 1000.0f), juce::dontSendNotification);
            // lowCut: 20.0f to 2000.0f
            if (!knobLocked[6]) knobs[6]->setValue((snapshot.delay.lowCut - 20.0f) / (2000.0f - 20.0f), juce::dontSendNotification);
            // mix: 0.0f to 1.0f (already normalized)
            if (!knobLocked[7]) knobs[7]->setValue(snapshot.delay.mix, juce::dontSendNotification);
            
            // Update value labels and indicator bars to reflect the new values
            for (int i = 0; i < 8; ++i) {
                if (valueLabels[i] != nullptr && !knobLocked[i]) {
                    float knobValue = knobs[i]->getValue();
                    juce::String valueText;
                    if (i == 0 && timeSyncEnabled) {
                        // Show musical division label instead of ms
                        std::vector<juce::String> labels = {"1/64","1/32","1/16","1/8","1/4","1/2","1","2"};
                        int idx = juce::jlimit(0, 7, (int) std::round(knobValue * 7.0f));
                        auto label = labels[idx];
                        if (timeSyncStdMode == 1) label << "t"; else if (timeSyncStdMode == 2) label << ".";
                        valueText = label;
                    } else {
                        if (i == 0) {
                            // Time, sync OFF: show whole ms
                            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter("timeMs"))) {
                                float ms = p->convertFrom0to1(knobs[0]->getValue());
                                valueText = juce::String((int) std::round(ms)) + "ms";
                            } else {
                                valueText = juce::String((int) std::round(knobValue * 100));
                            }
                        } else {
                            valueText = juce::String((int) std::round(knobValue * 100));
                        }
                    }
                    valueLabels[i]->setText(valueText, juce::dontSendNotification);
                }
                if (indicatorBars[i] != nullptr && !knobLocked[i]) {
                    indicatorBars[i]->setValue(knobs[i]->getValue());
                }
                
                // Update the step snapshot in the processor to match the knob values
                if (!knobLocked[i]) updateParameterFromKnob(i);
            }
        }
        
        // Update UI
        updateSequencerUI();
    };
    
    // Set dice image for step dice button
    if (assets.diceLarge != nullptr) {
        stepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
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
    // Allow direct editing for step count (1..16)
    stepAmountLabel->setEditable(true, true, false);
    stepAmountLabel->onTextChange = [this]() {
        if (stepAmountLabel != nullptr)
        {
            int value = stepAmountLabel->getText().getIntValue();
            value = juce::jlimit(1, 16, value);
            processorRef.setStepsUsed(value);
            stepAmountLabel->setText(juce::String(value), juce::dontSendNotification);
            updateSequencerUI();
        }
    };
    addAndMakeVisible(stepAmountLabel.get());
    // Move step amount left by 80px
    stepAmountLabel->setBounds(stepArea.getX() + 180, stepArea.getY() - 10, 30, 25);
    
    // Create rate dropdown (top right)
    rateDropdown = std::make_unique<juce::ComboBox>();
    // Slower divisions added: 4 and 2 bars; and rename 1/1 to 1
    rateDropdown->addItem("4", 1);      // 4 bars (16 beats)
    rateDropdown->addItem("2", 2);      // 2 bars (8 beats)
    rateDropdown->addItem("1", 3);      // 1 bar  (4 beats)
    rateDropdown->addItem("1/2", 4);
    rateDropdown->addItem("1/4", 5);
    rateDropdown->addItem("1/8", 6);
    rateDropdown->addItem("1/16", 7);
    rateDropdown->addItem("1/32", 8);
    rateDropdown->setSelectedId(6); // Default to 1/8
    // Make dropdown transparent (no background or border)
    rateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    rateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    rateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    rateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    rateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible(rateDropdown.get());
    // Move rate left by 80px and widen to avoid arrow overlapping long items
    rateDropdown->setBounds(stepArea.getX() + 220, stepArea.getY() - 10, 74, 25);
    // Wire dropdown -> processor division index (0..7)
    rateDropdown->onChange = [this]() {
        if (rateDropdown != nullptr)
        {
            const int selected = rateDropdown->getSelectedId(); // 1..8
            const int newDivisionIndex = juce::jlimit(0, 7, selected - 1);
            DBG("[UI] Rate dropdown changed: ID=" << selected << " -> divisionIndex=" << newDivisionIndex);
            processorRef.setDivisionIndex(newDivisionIndex);
            updateSequencerUI();
        }
    };
    
    // Create S/T/D circular toggle (top right)
    stdToggle = std::make_unique<CircularToggleButton>();
    stdToggle->setButtonText("-");
    addAndMakeVisible(stdToggle.get());
    // Move toggle up 4px and left 12px, then move all left by 80px (total left 92px)
    // Adjusted +10px to the right per request
    stdToggle->setBounds(stepArea.getX() + 288, stepArea.getY() - 14, 30, 30);
    
    // Set up toggle handler
    stdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        const char* labels[] = {"-", "t", "."};
        stdToggle->setButtonText(labels[stdState]);
        // Inform processor of new STD mode for timing
        processorRef.setStdMode(stdState);
        DBG("[UI] STD toggle clicked: state=" << stdState << " label=" << labels[stdState]);
    };
    
    DBG("[UI] Sequencer area setup complete");
}

void PluginEditor::setupStepPowerButton()
{
    DBG("[UI] Setting up step power button...");
    
    // Step area bounds
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step power button (top right corner of step area)
    stepPowerButton = std::make_unique<juce::DrawableButton>("stepPowerButton", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(stepPowerButton.get());
    
    // Position at top right corner of step area, 20% smaller than 50px and adjusted position
    const int buttonSize = 40; // 50 * 0.8 = 40 (20% smaller)
    stepPowerButton->setBounds(stepArea.getX() + stepArea.getWidth() - buttonSize - 5 + 15 - 5 - 1, stepArea.getY() - 5 - 40 + 25 + 5, buttonSize, buttonSize); // 1px right, 5px up
    
    // Remove background colors
    stepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    stepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    // Set up image
    if (assets.stepPowerOn != nullptr)
    {
        stepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    // Set up click handler
    stepPowerButton->setClickingTogglesState(true);
    stepPowerButton->setToggleState(stepAreaEnabled, juce::dontSendNotification);
    stepPowerButton->onClick = [this]() {
        stepAreaEnabled = stepPowerButton->getToggleState();
        DBG("[UI] Step area power: " << (stepAreaEnabled ? "ON" : "OFF"));
        
        if (!stepAreaEnabled) {
            // Disable sequencer and stop it immediately when turning OFF
            processorRef.setSequencerEnabled(false);
            processorRef.setSequencerActive(false); // Force stop the sequencer
            processorRef.resetSequencerState();
        } else {
            // Enable sequencer when turning ON
            processorRef.setSequencerEnabled(true);
            // If followHost is enabled and DAW is playing, realign immediately
            if (auto* ph = processorRef.getPlayHead()) {
                auto pos = ph->getPosition();
                if (pos.hasValue() && pos->getIsPlaying()) {
                    // Sequencer will be armed by the transport watcher
                }
            }
        }
        
        // Update UI visibility and repaint
        updateStepAreaVisibility();
        repaint();
    };
    
    DBG("[UI] Step power button setup complete");
}

void PluginEditor::updateStepAreaVisibility()
{
    // Grey out all step area components when disabled
    float alpha = stepAreaEnabled ? 1.0f : 0.3f;
    
    // Update step buttons
    for (auto& button : stepButtons) {
        if (button != nullptr) {
            button->setAlpha(alpha);
            button->setEnabled(stepAreaEnabled);
        }
    }
    
    // Update other step area components
    if (stepAmountLabel != nullptr) {
        stepAmountLabel->setAlpha(alpha);
        stepAmountLabel->setEnabled(stepAreaEnabled);
    }
    
    if (rateDropdown != nullptr) {
        rateDropdown->setAlpha(alpha);
        rateDropdown->setEnabled(stepAreaEnabled);
    }
    
    if (stdToggle != nullptr) {
        stdToggle->setAlpha(alpha);
        stdToggle->setEnabled(stepAreaEnabled);
    }
    
    if (stepTitle != nullptr) {
        stepTitle->setAlpha(alpha);
    }
    
    if (stepDiceButton != nullptr) {
        stepDiceButton->setAlpha(alpha);
        stepDiceButton->setEnabled(stepAreaEnabled);
    }
    
    // Update power button itself - grey out when disabled
    if (stepPowerButton != nullptr) {
        stepPowerButton->setAlpha(stepAreaEnabled ? 1.0f : 0.3f);
    }
    }

void PluginEditor::randomizeKnobValues()
{
    DBG("[UI] Randomizing knob values...");
    
    // Randomize each knob to a random value between 0.0 and 1.0
    for (int i = 0; i < 8; ++i)
    {
        if (knobLocked[i]) continue; // respect lock
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        knobs[i]->setValue(randomValue);
        
        // Update the value label
        valueLabels[i]->setText(juce::String((int) std::round(randomValue * 100)), juce::dontSendNotification);
        
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
        if (knobLocked[knobIndex]) return; // respect lock
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        knobs[knobIndex]->setValue(randomValue);
        
        // Update the value label
        valueLabels[knobIndex]->setText(juce::String((int) std::round(randomValue * 100)), juce::dontSendNotification);
        
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

void PluginEditor::updateAllStepSnapshots(int knobIndex)
{
    if (knobIndex >= 0 && knobIndex < 8 && knobs[knobIndex] != nullptr && knobIndex < processorRef.getParameters().size())
    {
        auto* param = processorRef.getParameters().getUnchecked(knobIndex);
        float knobValue = knobs[knobIndex]->getValue();
        
        // Convert normalized value back to actual parameter value
        float actualValue = 0.0f;
        if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
        {
            actualValue = floatParam->convertFrom0to1(knobValue);
        }
        
        DBG("[UI] All Steps: Updating knob " << knobIndex << " with knobValue=" << knobValue << " actualValue=" << actualValue);
        
        // Update all 16 step snapshots with the new value
        for (int step = 0; step < 16; ++step)
        {
            // Get current snapshot for this step
            StepSnapshot snapshot = processorRef.getSafeSnapshot(step);
            
            // Update the specific parameter in the snapshot with proper conversion
            switch (knobIndex)
            {
                case 0: snapshot.delay.timeMs = actualValue; break;
                case 1: snapshot.delay.feedback = actualValue * 100.0f; break; // Convert to percentage
                case 2: snapshot.delay.wowDepth = actualValue * 100.0f; break; // Convert to percentage
                case 3: snapshot.delay.wowRate = actualValue; break;
                case 4: snapshot.delay.saturation = actualValue * 100.0f; break; // Convert to percentage
                case 5: snapshot.delay.highCut = actualValue; break;
                case 6: snapshot.delay.lowCut = actualValue; break;
                case 7: snapshot.delay.mix = actualValue * 100.0f; break; // Convert to percentage
            }
            
            // Set the updated snapshot back
            processorRef.setStepSnapshot(step, snapshot);
        }
        
        DBG("[UI] Updated all 16 step snapshots for knob " << knobIndex << " with value " << actualValue);
    }
}

void PluginEditor::onStepButtonClicked(int stepIndex)
{
    DBG("[UI] Step button " << stepIndex << " clicked");
    
    // Save current step's snapshot before switching
    int currentStep = processorRef.getSelectedStep();
    if (currentStep >= 0 && currentStep < 16) {
        StepSnapshot currentSnapshot;
        currentSnapshot.delay.timeMs = processorRef.getAPVTS().getParameter("timeMs")->convertFrom0to1(processorRef.getAPVTS().getParameter("timeMs")->getValue());
        currentSnapshot.delay.feedback = processorRef.getAPVTS().getParameter("feedback")->convertFrom0to1(processorRef.getAPVTS().getParameter("feedback")->getValue()) * 100.0f;
        currentSnapshot.delay.wowDepth = processorRef.getAPVTS().getParameter("wowDepth")->convertFrom0to1(processorRef.getAPVTS().getParameter("wowDepth")->getValue()) * 100.0f;
        currentSnapshot.delay.wowRate = processorRef.getAPVTS().getParameter("wowRate")->convertFrom0to1(processorRef.getAPVTS().getParameter("wowRate")->getValue());
        currentSnapshot.delay.saturation = processorRef.getAPVTS().getParameter("drive")->convertFrom0to1(processorRef.getAPVTS().getParameter("drive")->getValue()) * 100.0f;
        currentSnapshot.delay.highCut = processorRef.getAPVTS().getParameter("hiCut")->convertFrom0to1(processorRef.getAPVTS().getParameter("hiCut")->getValue());
        currentSnapshot.delay.lowCut = processorRef.getAPVTS().getParameter("lowCut")->convertFrom0to1(processorRef.getAPVTS().getParameter("lowCut")->getValue());
        currentSnapshot.delay.mix = processorRef.getAPVTS().getParameter("mix")->convertFrom0to1(processorRef.getAPVTS().getParameter("mix")->getValue()) * 100.0f;
        processorRef.setStepSnapshot(currentStep, currentSnapshot);
        DBG("[UI] Saved current step " << currentStep << " snapshot before switching");
    }
    
    // Update selected step in processor
    processorRef.setSelectedStep(stepIndex);
    
    // Update UI to show which step is selected
    updateSequencerUI();
    
    // Load the snapshot for this step into the knobs
    auto snapshot = processorRef.getSafeSnapshot(stepIndex);
    
    // Update knobs with snapshot values (convert from actual values to normalized 0-1)
    if (timeSyncEnabled) {
        // Derive index from snapshot time to position knob at correct division location
        const double bpm = processorRef.getBpmOrDefault(120.0);
        std::vector<double> beats = {2.0,1.0,0.5,0.25,0.125,0.0625,0.03125,0.015625};
        double mult = 1.0;
        if (timeSyncStdMode == 1) mult = 2.0/3.0; else if (timeSyncStdMode == 2) mult = 1.5;
        for (auto& b : beats) b *= mult;
        // Convert beats to ms
        for (auto& b : beats) b = b * (60.0 / juce::jmax(1.0, bpm)) * 1000.0;
        double ms = snapshot.delay.timeMs;
        int bestIdx = 0; double bd = std::abs(beats[0] - ms);
        for (int i = 1; i < (int)beats.size(); ++i) { double d = std::abs(beats[i] - ms); if (d < bd) { bd = d; bestIdx = i; } }
        // Map index to 0..1 evenly
        float pos = (float)bestIdx / 7.0f;
        knobs[0]->setValue(pos, juce::dontSendNotification);
    } else {
        knobs[0]->setValue(processorRef.getAPVTS().getParameter("timeMs")->convertTo0to1(snapshot.delay.timeMs), juce::dontSendNotification);
    }
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
    int playingStep = processorRef.getCurrentSeqStepAudioThread(); // Read from audio thread
    const int stepsUsed = processorRef.getSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (stepButtons[i] != nullptr) {
            // Only the selected step should show as selected
            stepButtons[i]->setSelected(i == selectedStep);
            // Show playing highlight only if sequencer is enabled
            bool sequencerEnabled = processorRef.isSequencerEnabled();
            stepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            // Grey out inactive steps beyond stepsUsed
            bool shouldBeEnabled = i < stepsUsed;
            stepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't steal focus if editing)
    if (stepAmountLabel != nullptr && ! stepAmountLabel->isBeingEdited()) {
        int stepsUsed = processorRef.getSeqState().stepsUsed.load();
        stepAmountLabel->setText(juce::String(stepsUsed), juce::dontSendNotification);
    }
    
    // Update rate dropdown
    if (rateDropdown != nullptr) {
        int divisionIndex = processorRef.getSeqState().divisionIndex.load();
        rateDropdown->setSelectedId(divisionIndex + 1);
    }
}

void PluginEditor::setupUIToggle()
{
    DBG("[UI] Setting up UI toggle button...");
    
    // Create UI toggle button (tiny button in top right corner)
    uiToggleButton = std::make_unique<juce::ToggleButton>();
    uiToggleButton->setButtonText("UI");
    uiToggleButton->setSize(30, 20); // Tiny button
    uiToggleButton->setTopLeftPosition(getWidth() - 35, 5); // Top right corner with 5px margin
    
    // Style the button
    uiToggleButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    uiToggleButton->setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    uiToggleButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    
    // Set initial state (hidden by default)
    uiToggleButton->setToggleState(false, juce::dontSendNotification);
    uiVisible = false;
    
    // Set up callback
    uiToggleButton->onClick = [this]() {
        toggleUIVisibility();
    };
    
    addAndMakeVisible(uiToggleButton.get());
    
    // Initially hide all UI areas
    toggleUIVisibility();
    
    DBG("[UI] UI toggle button setup complete");
}

void PluginEditor::toggleUIVisibility()
{
    uiVisible = uiToggleButton->getToggleState();
    
    DBG("[UI] Toggling visual elements visibility: " << (uiVisible ? "SHOW" : "HIDE"));
    
    // Only toggle the visual elements (grid overlay and colored area boxes)
    // All functional UI elements (knobs, buttons, etc.) remain visible
    
    // Trigger repaint to update grid overlay and area box visibility
    repaint();
    
    DBG("[UI] Visual elements visibility toggled: " << (uiVisible ? "VISIBLE" : "HIDDEN"));
}

void PluginEditor::setupPlayButton()
{
    DBG("[UI] Setting up play button...");
    
    // Create play button (next to UI toggle button)
    playButton = std::make_unique<PlayButton>();
    playButton->setSize(30, 20); // Same size as UI toggle
    playButton->setTopLeftPosition(getWidth() - 70, 5); // Next to UI toggle with 5px spacing
    
    // Set initial state (stopped - grey)
    playButton->setPlaying(false);
    
    // Set up callback
    playButton->onClick = [this]() {
        togglePlayback();
    };
    
    addAndMakeVisible(playButton.get());
    
    DBG("[UI] Play button setup complete");
}

void PluginEditor::togglePlayback()
{
    // Toggle the playing state
    bool newPlayingState = !playButton->isPlayingState();
    playButton->setPlaying(newPlayingState);
    
    DBG("[UI] Toggling playback: " << (newPlayingState ? "PLAY" : "STOP"));
    
    if (newPlayingState) {
        // Starting playback - enable sequencer and set origin for standalone mode
        processorRef.setSequencerEnabled(true);
        processorRef.startStandalonePlayback();
    } else {
        // Stopping playback - disable sequencer
        processorRef.setSequencerEnabled(false);
    }
    
    DBG("[UI] Playback toggled: " << (newPlayingState ? "PLAYING" : "STOPPED"));
}

void PluginEditor::setupTabSystem()
{
    DBG("[UI] Setting up tab system...");
    
    // Create tab buttons
    tabSpaceDelay = std::make_unique<juce::DrawableButton>("SpaceDelayTab", juce::DrawableButton::ButtonStyle::ImageFitted);
    tabPanner = std::make_unique<juce::DrawableButton>("PannerTab", juce::DrawableButton::ButtonStyle::ImageFitted);
    
    DBG("[UI] Tab buttons created successfully");
    DBG("[UI] tabSpaceDelay pointer: " << tabSpaceDelay.get());
    DBG("[UI] tabPanner pointer: " << tabPanner.get());
    
    // Set tab images (using existing assets)
    if (assets.tabTitleSpaceDelay) {
        tabSpaceDelay->setImages(assets.tabTitleSpaceDelay.get());
    }
    if (assets.tabTitleSpaceDelay) { // Using same asset for now
        tabPanner->setImages(assets.tabTitleSpaceDelay.get());
    }
    
    // --- Tabs (SVGs you mentioned are already loaded in your Assets) ---
    tabSpaceDelay = std::make_unique<juce::DrawableButton>("tabSpace", juce::DrawableButton::ImageOnButtonBackground);
    tabPanner = std::make_unique<juce::DrawableButton>("tabPanner", juce::DrawableButton::ImageOnButtonBackground);
    
    // Use the tab SVGs you provided:
    if (assets.tabTitleSpaceDelay) {
        tabSpaceDelay->setImages(assets.tabTitleSpaceDelay->createCopy().release(),
                                 nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    if (assets.tabTitleAutoPan) { // Using AutoPan SVG for the second tab
        tabPanner->setImages(assets.tabTitleAutoPan->createCopy().release(),
                             nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    
    // Add bright background colors to make tabs visible for testing
    tabSpaceDelay->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xFFFF6600)); // Bright orange
    tabPanner->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xFF00FF00)); // Bright green
    
    tabSpaceDelay->setTriggeredOnMouseDown(true);
    tabPanner->setTriggeredOnMouseDown(true);
    
    // Click handlers
    tabSpaceDelay->onClick = [this]{ 
        DBG("[UI] SpaceDelay tab clicked!");
        showPage(FxPageID::SpaceDelay); 
    };
    tabPanner->onClick = [this]{ 
        DBG("[UI] Panner tab clicked!");
        showPage(FxPageID::Panner); 
    };
    
    // Ensure tabs never obstruct the master area clicks; they only sit over the header strip
    tabSpaceDelay->setAlwaysOnTop(true);
    tabPanner->setAlwaysOnTop(true);
    
    addAndMakeVisible(*tabSpaceDelay);
    addAndMakeVisible(*tabPanner);
    
    // Position tabs immediately after creation - smaller and moved up 10px
    tabSpaceDelay->setBounds(24, 0, 120, 34);
    tabPanner->setBounds(170, 0, 120, 34);
    
    DBG("[UI] Tab buttons created and added to editor");
    DBG("[UI] tabSpaceDelay bounds: " << tabSpaceDelay->getBounds().toString());
    DBG("[UI] tabPanner bounds: " << tabPanner->getBounds().toString());
    
    // Collect pointers to existing SpaceDelay UI components
    spaceDelayGroup.clear();
    
    // Add all knobs and related components
    for (int i = 0; i < 8; ++i) {
        if (knobs[i]) spaceDelayGroup.push_back(knobs[i].get());
        if (knobLabels[i]) spaceDelayGroup.push_back(knobLabels[i].get());
        if (valueLabels[i]) spaceDelayGroup.push_back(valueLabels[i].get());
        if (indicatorBars[i]) spaceDelayGroup.push_back(indicatorBars[i].get());
        if (knobDiceButtons[i]) spaceDelayGroup.push_back(knobDiceButtons[i].get());
        if (knobLockButtons[i]) spaceDelayGroup.push_back(knobLockButtons[i].get());
    }
    
    // Master knobs are shared between pages - do NOT add to spaceDelayGroup
    // They should remain visible on both SpaceDelay and AutoPan pages
    
    // Macro knobs are shared between pages - do NOT add to spaceDelayGroup
    // They should remain visible on both SpaceDelay and AutoPan pages
    
    // Meters are shared between pages - do NOT add to spaceDelayGroup  
    // They should remain visible on both SpaceDelay and AutoPan pages
    
    // Add effects area components
    if (effectsTitle) spaceDelayGroup.push_back(effectsTitle.get());
    if (spaceDelayTitle) spaceDelayGroup.push_back(spaceDelayTitle.get());
    if (effectTypeDropdown) spaceDelayGroup.push_back(effectTypeDropdown.get());
    if (diceButton) spaceDelayGroup.push_back(diceButton.get());
    if (timeSyncToggle) spaceDelayGroup.push_back(timeSyncToggle.get());
    if (fxPowerButton) spaceDelayGroup.push_back(fxPowerButton.get());
    
    // Add sequencer components
    if (allStepsToggle) spaceDelayGroup.push_back(allStepsToggle.get());
    if (allStepsLabel) spaceDelayGroup.push_back(allStepsLabel.get());
    for (int i = 0; i < 16; ++i) {
        if (stepButtons[i]) spaceDelayGroup.push_back(stepButtons[i].get());
    }
    if (stepAmountLabel) spaceDelayGroup.push_back(stepAmountLabel.get());
    if (rateDropdown) spaceDelayGroup.push_back(rateDropdown.get());
    if (stdToggle) spaceDelayGroup.push_back(stdToggle.get());
    if (stepTitle) spaceDelayGroup.push_back(stepTitle.get());
    if (stepDiceButton) spaceDelayGroup.push_back(stepDiceButton.get());
    if (stepPowerButton) spaceDelayGroup.push_back(stepPowerButton.get());
    if (uiToggleButton) spaceDelayGroup.push_back(uiToggleButton.get());
    if (playButton) spaceDelayGroup.push_back(playButton.get());
    
    // Collect pointers to AutoPan UI components
    pannerGroup.clear();
    
    // Add AutoPan knobs and related components
    for (int i = 0; i < 6; ++i) {
        if (autopanKnobs[i]) pannerGroup.push_back(autopanKnobs[i].get());
        if (autopanKnobLabels[i]) pannerGroup.push_back(autopanKnobLabels[i].get());
        if (autopanValueLabels[i]) pannerGroup.push_back(autopanValueLabels[i].get());
        if (autopanIndicatorBars[i]) pannerGroup.push_back(autopanIndicatorBars[i].get());
        // Note: autopanDiceButtons are NOT added to pannerGroup since they're not in component tree
        if (autopanLockButtons[i]) pannerGroup.push_back(autopanLockButtons[i].get());
    }
    
    // Add AutoPan effects area components
    if (autopanEffectsTitle) pannerGroup.push_back(autopanEffectsTitle.get());
    if (autopanDiceButton) pannerGroup.push_back(autopanDiceButton.get());
    if (autopanTimeSyncToggle) pannerGroup.push_back(autopanTimeSyncToggle.get());
    if (autopanFxPowerButton) pannerGroup.push_back(autopanFxPowerButton.get());
    
    // Add AutoPan sequencer components
    if (autopanAllStepsToggle) pannerGroup.push_back(autopanAllStepsToggle.get());
    if (autopanAllStepsLabel) pannerGroup.push_back(autopanAllStepsLabel.get());
    for (int i = 0; i < 16; ++i) {
        if (autopanStepButtons[i]) pannerGroup.push_back(autopanStepButtons[i].get());
    }
    if (autopanStepAmountLabel) pannerGroup.push_back(autopanStepAmountLabel.get());
    if (autopanRateDropdown) pannerGroup.push_back(autopanRateDropdown.get());
    if (autopanStdToggle) pannerGroup.push_back(autopanStdToggle.get());
    if (autopanStepTitle) pannerGroup.push_back(autopanStepTitle.get());
    if (autopanStepDiceButton) pannerGroup.push_back(autopanStepDiceButton.get());
    if (autopanStepPowerButton) pannerGroup.push_back(autopanStepPowerButton.get());
    
    // Initialize with SpaceDelay page visible
    showPage(FxPageID::SpaceDelay);
    
    DBG("[UI] Tab system setup complete. SpaceDelay components: " << spaceDelayGroup.size());
    DBG("[UI] AutoPan components: " << pannerGroup.size());
}

void PluginEditor::showPage(FxPageID id)
{
    if (currentPage == id) return;
    currentPage = id;

    const bool wantSpace = (id == FxPageID::SpaceDelay);

    // Update processor parameters
    auto* currentPageParam = processorRef.getAPVTS().getParameter("currentPage");
    if (currentPageParam) {
        currentPageParam->setValueNotifyingHost(wantSpace ? 0.0f : 1.0f);
    }
    
    // Enable AutoPan effect when switching to AutoPan page
    if (!wantSpace) {
        autopanFxAreaEnabled = true;
        auto* autopanEnabledParam = processorRef.getAPVTS().getParameter("autopanEnabled");
        if (autopanEnabledParam) {
            autopanEnabledParam->setValueNotifyingHost(1.0f);
        }
        updateAutoPanFxAreaVisibility();
    }

    // Show/Hide without touching parents or bounds
    auto setVisibleVec = [](const std::vector<juce::Component*>& v, bool vis)
    {
        for (auto* c : v) if (c) c->setVisible(vis);
    };

    setVisibleVec(spaceDelayGroup, wantSpace);
    setVisibleVec(pannerGroup, !wantSpace);

    // Optional: a simple visual hint — raise the active tab
    if (wantSpace) { 
        if (tabSpaceDelay) tabSpaceDelay->toFront(false); 
    } else { 
        if (tabPanner) tabPanner->toFront(false); 
    }

    repaint();
}

//==============================================================================
// AutoPan Page Setup Methods
//==============================================================================

void PluginEditor::setupAutoPanKnobs()
{
    DBG("[UI] Setting up AutoPan knobs...");

    // AutoPan knob names (6 knobs instead of 8)
    std::vector<juce::String> autopanKnobNames = {
        "Rate", "Phase", "Wave Type", "Wave Shape", "Normal/Inverted", "Amount"
    };

    // Effect area bounds (EXACT same as delay page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80; // EXACT same as delay page
    const int knobSpacing = 20; // EXACT same as delay page
    const int startX = effectArea.getX() + 15; // EXACT same as delay page
    const int startY = effectArea.getY() + effectArea.getHeight() - 210; // EXACT same as delay page
    
    for (int i = 0; i < 6; ++i) {
        // Create knob
        autopanKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(autopanKnobs[i].get());
        autopanKnobs[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        
        // Set knob properties with specific ranges for each knob
        autopanKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        autopanKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        // Set specific ranges and values for each AutoPan knob
        switch (i) {
            case 0: { // Rate - initialize with sync range since sync is ON by default
                autopanKnobs[i]->setRange(0.0, 1.0, 0.001);  // 0-1 for divisions
                // Get the current parameter value (should be in 0-1 range for sync)
                auto* rateParam = processorRef.getAPVTS().getRawParameterValue("autopanRate");
                autopanKnobs[i]->setValue(rateParam ? rateParam->load() : 0.5, juce::dontSendNotification);
                break;
            }
            case 1: { // Phase (0-360 degrees, default 180 degrees)
                autopanKnobs[i]->setRange(0.0, 360.0, 1.0);
                autopanKnobs[i]->setValue(180.0, juce::dontSendNotification);
                break;
            }
            case 2: { // Wave Type (0-4, default 0 = Sine)
                autopanKnobs[i]->setRange(0, 4, 1);
                autopanKnobs[i]->setValue(0, juce::dontSendNotification);
                break;
            }
            case 3: { // Wave Shape (0-1, default 0.5)
                autopanKnobs[i]->setRange(0.0, 1.0, 0.01);
                autopanKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            }
            case 4: { // Normal/Inverted (0-1, default 0 = Normal)
                autopanKnobs[i]->setRange(0, 1, 1);
                autopanKnobs[i]->setValue(0, juce::dontSendNotification);
                break;
            }
            case 5: { // Amount (0-1, default 0.5)
                autopanKnobs[i]->setRange(0.0, 1.0, 0.01);
                autopanKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            }
        }
        
        // Connect to parameters
        std::vector<juce::String> paramIds = {
            "autopanRate", "autopanPhase", "autopanWaveType", "autopanWaveShape", "autopanInverted", "autopanAmount"
        };
        
        autopanAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), paramIds[i], *autopanKnobs[i]);
        
        // Add listener to save snapshots when knob changes
        autopanKnobs[i]->onValueChange = [this, i]() {
            // Update current step snapshot with new value
            float value = autopanKnobs[i]->getValue();
            processorRef.updateAutoPanCurrentStepSnapshot(i, value);
        };
        
        // autopanKnobs[i]->setLookAndFeel(&customLookAndFeel); // TODO: Add custom look and feel
        
        // Set knob images
        if (assets.knobRing) {
            autopanKnobs[i]->setRingImage(assets.knobRing->createCopy());
        }
        if (assets.knobInside) {
            autopanKnobs[i]->setInnerImage(assets.knobInside->createCopy());
        }
        
        // Position knob (EXACT same logic as delay page)
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px (EXACT same as delay page)
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1; // Moved up 6px from +5 to -1
            
        autopanKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label (EXACT same positioning as delay page)
        autopanKnobLabels[i] = std::make_unique<juce::Label>();
        autopanKnobLabels[i]->setText(autopanKnobNames[i], juce::dontSendNotification);
        autopanKnobLabels[i]->setFont(juce::Font(12.0f, juce::Font::bold));
        autopanKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        autopanKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(autopanKnobLabels[i].get());
        autopanKnobLabels[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        autopanKnobLabels[i]->setBounds(x, y - 15, knobSize, 20); // EXACT same as delay page
        
        // Create value label (EXACT same positioning as delay page)
        autopanValueLabels[i] = std::make_unique<juce::Label>();
        autopanValueLabels[i]->setText("0", juce::dontSendNotification);
        autopanValueLabels[i]->setFont(juce::Font(10.0f, juce::Font::plain));
        autopanValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        autopanValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(autopanValueLabels[i].get());
        autopanValueLabels[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        autopanValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15); // EXACT same as delay page
        
        // Create indicator bar (EXACT same positioning as delay page)
        autopanIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(autopanIndicatorBars[i].get());
        autopanIndicatorBars[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        autopanIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13); // EXACT same as delay page
        autopanIndicatorBars[i]->setValue(0.5f); // Set initial value
        
        // Create dice button (hidden like delay page - NOT added to component tree)
        autopanDiceButtons[i] = std::make_unique<CustomDiceButton>();
        // Do NOT call addAndMakeVisible - keep it hidden like delay page
        autopanDiceButtons[i]->onClick = [this, i]() { randomizeIndividualAutoPanKnob(i); };

        // Create lock button (EXACT same positioning logic as delay page)
        autopanLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(autopanLockButtons[i].get());
        autopanLockButtons[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        
        // Calculate text width and position lock button accordingly (EXACT same logic as delay page)
        const int diceSize = 10; // 20% bigger: 8 * 1.2 = 9.6, rounded to 10
        const int diceSpacing = 5; // Fixed distance from end of title text
        
        // Get the font used for knob labels
        juce::Font labelFont(12.0f, juce::Font::bold);
        
        // Calculate the width of the knob title text
        int textWidth = labelFont.getStringWidth(autopanKnobNames[i]);
        
        // Position lock button at the end of the title text + fixed spacing
        int lockX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
        int lockY = y - 10; // Moved up 3px from -7 to -10
        
        autopanLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);
        
        // Configure images for on/off states (EXACT same as delay page)
        std::unique_ptr<juce::Drawable> imgUnlocked, imgLocked;
        if (assets.unlockedIcon) imgUnlocked = assets.unlockedIcon->createCopy();
        if (assets.lockedIcon)   imgLocked   = assets.lockedIcon->createCopy();
        
        autopanLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        autopanLockButtons[i]->setToggleState(autopanKnobLocked[i], juce::dontSendNotification);
        autopanLockButtons[i]->onClick = [this, i]() {
            autopanKnobLocked[i] = autopanLockButtons[i]->getToggleState();
            repaint();
        };
    }
    
    DBG("[UI] AutoPan knobs setup complete");
}

void PluginEditor::setupAutoPanEffectsArea()
{
    DBG("[UI] Setting up AutoPan effects area...");
    
    // Effect area bounds (EXACT same as delay page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title (EXACT same as delay page)
    autopanEffectsTitle = std::make_unique<juce::Label>();
    autopanEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    autopanEffectsTitle->setFont(juce::Font(27.648f, juce::Font::bold)); // EXACT same as delay page
    autopanEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    autopanEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(autopanEffectsTitle.get());
    autopanEffectsTitle->setVisible(false); // Initially hidden until AutoPan page is selected
    autopanEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30); // EXACT same as delay page
    
    // Wave Type dropdown removed - using knob only
    
    // Create dice button (EXACT same positioning as delay page)
    autopanDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(autopanDiceButton.get());
    autopanDiceButton->setVisible(false); // Initially hidden until AutoPan page is selected
    
    // Position dice button to the right of the title (EXACT same as delay page)
    const int diceSize = 32; // 20% smaller: 40 * 0.8 = 32
    autopanDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize); // EXACT same as delay page
    
    // Set the dice image (EXACT same as delay page)
    if (assets.diceLarge != nullptr)
    {
        autopanDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    autopanDiceButton->onClick = [this]() { randomizeAutoPanKnobValues(); };
    
    // Create time sync toggle (for Rate knob) - using SSyncButton like delay page
    struct AutoPanSSyncButton : public juce::Button {
        AutoPanSSyncButton() : juce::Button("AutoPanSSync") {}
        void paintButton(juce::Graphics& g, bool over, bool down) override {
            juce::ignoreUnused(over, down);
            auto r = getLocalBounds().toFloat();
            const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
            auto centre = r.getCentre();
            g.setColour(juce::Colours::white);
            if (getToggleState()) {
                g.fillEllipse(centre.x - radius, centre.y - radius, radius*2, radius*2);
                g.setColour(juce::Colours::black);
            } else {
                g.drawEllipse(centre.x - radius, centre.y - radius, radius*2, radius*2, 2.0f);
            }
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("S", r, juce::Justification::centred);
        }
    };
    autopanTimeSyncToggle = std::make_unique<AutoPanSSyncButton>();
    addAndMakeVisible(autopanTimeSyncToggle.get());
    autopanTimeSyncToggle->setVisible(false); // Initially hidden until AutoPan page is selected
    
    // Position relative to first knob label (Rate knob) - EXACT same as delay page
    if (autopanKnobLabels[0] != nullptr) {
        auto lb = autopanKnobLabels[0]->getBounds();
        autopanTimeSyncToggle->setBounds(lb.getX() + 10, lb.getY() + 4, 12, 12); // moved left 4px and up 2px
    } else {
        autopanTimeSyncToggle->setBounds(effectArea.getX() + 10, effectArea.getY() + 10, 12, 12);
    }
    
    autopanTimeSyncToggle->setClickingTogglesState(true);
    
    // Initialize toggle state from parameter
    auto* syncParam = processorRef.getAPVTS().getRawParameterValue("autopanTimeSync");
    if (syncParam) {
        autopanTimeSyncEnabled = syncParam->load() > 0.5f;
        autopanTimeSyncToggle->setToggleState(autopanTimeSyncEnabled, juce::dontSendNotification);
    }
    
    autopanTimeSyncToggle->onClick = [this]() {
        autopanTimeSyncEnabled = autopanTimeSyncToggle->getToggleState();
        
        // Update the parameter
        auto* syncParam = processorRef.getAPVTS().getParameter("autopanTimeSync");
        if (syncParam) {
            syncParam->setValueNotifyingHost(autopanTimeSyncEnabled ? 1.0f : 0.0f);
        }
        
        if (autopanTimeSyncEnabled) {
            // Switch to time sync mode - show divisions instead of Hz
            // Update knob range to divisions (0.0-1.0 for smooth control)
            autopanKnobs[0]->setRange(0.0, 1.0, 0.001);
            
            // Keep the current parameter value - don't reset it
            auto* rateParam = processorRef.getAPVTS().getRawParameterValue("autopanRate");
            if (rateParam) {
                autopanKnobs[0]->setValue(rateParam->load(), juce::dontSendNotification);
            }
        } else {
            // Switch to free rate mode - show Hz
            autopanKnobs[0]->setRange(0.05, 90.0, 0.01);
            
            // Keep the current parameter value - don't reset it
            auto* rateParam = processorRef.getAPVTS().getRawParameterValue("autopanRate");
            if (rateParam) {
                autopanKnobs[0]->setValue(rateParam->load(), juce::dontSendNotification);
            }
        }
    };
    
    // Create FX power button (EXACT same positioning as delay page)
    autopanFxPowerButton = std::make_unique<juce::DrawableButton>("autopanFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(autopanFxPowerButton.get());
    autopanFxPowerButton->setVisible(false); // Initially hidden until AutoPan page is selected

    // Slightly bigger than step power (which is 40). Use 46. (EXACT same as delay page)
    const int buttonSize = 46;
    autopanFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);

    autopanFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    autopanFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);

    if (assets.fxPowerOn != nullptr)
    {
        autopanFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }
    
    autopanFxPowerButton->onClick = [this]() { 
        autopanFxAreaEnabled = !autopanFxAreaEnabled;
        updateAutoPanFxAreaVisibility();
        
        // Update processor parameter
        auto* autopanEnabledParam = processorRef.getAPVTS().getParameter("autopanEnabled");
        if (autopanEnabledParam) {
            autopanEnabledParam->setValueNotifyingHost(autopanFxAreaEnabled ? 1.0f : 0.0f);
        }
    };
    
    DBG("[UI] AutoPan effects area setup complete");
}

void PluginEditor::setupAutoPanSequencerArea()
{
    DBG("[UI] Setting up AutoPan sequencer area...");
    
    // Sequencer area bounds (EXACT same as delay page)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    autopanStepTitle = std::make_unique<juce::Label>();
    autopanStepTitle->setText("STEP", juce::dontSendNotification);
    autopanStepTitle->setFont(juce::Font(22.118f, juce::Font::bold)); // 20% smaller: 27.648f * 0.8 = 22.118f
    autopanStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    autopanStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(autopanStepTitle.get());
    autopanStepTitle->setVisible(false); // Initially hidden until AutoPan page is selected
    autopanStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30); // EXACT same as delay page
    
    // Create step buttons (2 rows of 8, EXACT same layout as delay page)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35; // Same as delay page
    
    for (int i = 0; i < 16; ++i) {
        autopanStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(autopanStepButtons[i].get());
        autopanStepButtons[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        
        // Position buttons in 2 rows of 8 (EXACT same as delay page)
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        autopanStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set step button images
        if (assets.stepActive) {
            autopanStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive) {
            autopanStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        // Set click handler
        autopanStepButtons[i]->onClick = [this, i]() { onAutoPanStepButtonClicked(i); };
    }
    
    // Create step amount label (EXACT same positioning as delay page)
    autopanStepAmountLabel = std::make_unique<juce::Label>();
    autopanStepAmountLabel->setText("16", juce::dontSendNotification);
    autopanStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    autopanStepAmountLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    autopanStepAmountLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    autopanStepAmountLabel->setColour(juce::Label::outlineColourId, juce::Colours::white);
    autopanStepAmountLabel->setJustificationType(juce::Justification::centred);
    autopanStepAmountLabel->setBorderSize(juce::BorderSize<int>(2));
    autopanStepAmountLabel->setEditable(true, true, false);
    autopanStepAmountLabel->onTextChange = [this]() {
        int value = autopanStepAmountLabel->getText().getIntValue();
        if (value >= 1 && value <= 16) {
            value = juce::jlimit(1, 16, value);
            processorRef.setAutoPanStepsUsed(value);
            autopanStepAmountLabel->setText(juce::String(value), juce::dontSendNotification);
            updateAutoPanSequencerUI();
        }
    };
    addAndMakeVisible(autopanStepAmountLabel.get());
    autopanStepAmountLabel->setVisible(false); // Initially hidden until AutoPan page is selected
    autopanStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    
    // Create rate dropdown (EXACT same positioning as delay page)
    autopanRateDropdown = std::make_unique<juce::ComboBox>();
    autopanRateDropdown->addItem("4", 1);      // 4 bars (16 beats)
    autopanRateDropdown->addItem("2", 2);      // 2 bars (8 beats)
    autopanRateDropdown->addItem("1", 3);      // 1 bar  (4 beats)
    autopanRateDropdown->addItem("1/2", 4);
    autopanRateDropdown->addItem("1/4", 5);
    autopanRateDropdown->addItem("1/8", 6);
    autopanRateDropdown->addItem("1/16", 7);
    autopanRateDropdown->addItem("1/32", 8);
    autopanRateDropdown->setSelectedId(6); // Default to 1/8
    autopanRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    autopanRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    autopanRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    autopanRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    autopanRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    autopanRateDropdown->onChange = [this]() {
        if (autopanRateDropdown != nullptr) {
            const int selected = autopanRateDropdown->getSelectedId();
            if (selected >= 1 && selected <= 8) {
                const int newDivisionIndex = juce::jlimit(0, 7, selected - 1);
                DBG("[UI] AutoPan rate dropdown changed: ID=" << selected << " -> divisionIndex=" << newDivisionIndex);
                processorRef.setAutoPanDivisionIndex(newDivisionIndex);
                updateAutoPanSequencerUI();
            }
        }
    };
    addAndMakeVisible(autopanRateDropdown.get());
    autopanRateDropdown->setVisible(false); // Initially hidden until AutoPan page is selected
    autopanRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    // Create STD toggle (EXACT same positioning as delay page)
    autopanStdToggle = std::make_unique<CircularToggleButton>();
    autopanStdToggle->setButtonText("-");
    addAndMakeVisible(autopanStdToggle.get());
    autopanStdToggle->setVisible(false); // Initially hidden until AutoPan page is selected
    autopanStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    // Set up toggle handler (EXACT same as delay page)
    autopanStdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        
        switch (stdState) {
            case 0: autopanStdToggle->setButtonText("-"); break;
            case 1: autopanStdToggle->setButtonText("t"); break;
            case 2: autopanStdToggle->setButtonText("."); break;
        }
    };
    
    // Create step dice button (EXACT same positioning as delay page)
    autopanStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(autopanStepDiceButton.get());
    autopanStepDiceButton->setVisible(false); // Initially hidden until AutoPan page is selected
    int autopanStepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    autopanStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, autopanStepDiceSize, autopanStepDiceSize);
    
    // Set up step dice button SVG (EXACT same as delay page)
    if (assets.diceLarge != nullptr)
    {
        autopanStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    // Set up step dice button callback to randomize all AutoPan step snapshots
    autopanStepDiceButton->onClick = [this]() {
        DBG("[UI] AutoPan step dice button clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getAutoPanSafeSnapshot(step);
            
            // Randomize all AutoPan parameters for this step
            snapshot.autopan.rate = juce::Random::getSystemRandom().nextFloat(); // 0-1
            snapshot.autopan.phase = juce::Random::getSystemRandom().nextFloat() * 360.0f; // 0-360
            snapshot.autopan.waveType = juce::Random::getSystemRandom().nextInt(5); // 0-4
            snapshot.autopan.waveShape = juce::Random::getSystemRandom().nextFloat(); // 0-1
            snapshot.autopan.inverted = juce::Random::getSystemRandom().nextBool();
            snapshot.autopan.amount = juce::Random::getSystemRandom().nextFloat(); // 0-1
            
            processorRef.setAutoPanStepSnapshot(step, snapshot);
        }
        
        // Reload current step's values into knobs
        int currentStep = processorRef.getAutoPanCurrentStep();
        StepSnapshot currentSnapshot = processorRef.getAutoPanSafeSnapshot(currentStep);
        if (autopanKnobs[0]) autopanKnobs[0]->setValue(currentSnapshot.autopan.rate, juce::sendNotification);
        if (autopanKnobs[1]) autopanKnobs[1]->setValue(currentSnapshot.autopan.phase, juce::sendNotification);
        if (autopanKnobs[2]) autopanKnobs[2]->setValue((float)currentSnapshot.autopan.waveType, juce::sendNotification);
        if (autopanKnobs[3]) autopanKnobs[3]->setValue(currentSnapshot.autopan.waveShape, juce::sendNotification);
        if (autopanKnobs[4]) autopanKnobs[4]->setValue(currentSnapshot.autopan.inverted ? 1.0f : 0.0f, juce::sendNotification);
        if (autopanKnobs[5]) autopanKnobs[5]->setValue(currentSnapshot.autopan.amount, juce::sendNotification);
        
        DBG("[UI] All AutoPan step snapshots randomized");
    };
    
    // Create step power button (EXACT same positioning as delay page)
    autopanStepPowerButton = std::make_unique<juce::DrawableButton>("autopanStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(autopanStepPowerButton.get());
    autopanStepPowerButton->setVisible(false); // Initially hidden until AutoPan page is selected
    
    // Position at top right corner of step area, 20% smaller than 50px and adjusted position (EXACT same as delay page)
    const int powerButtonSize = 40; // 50 * 0.8 = 40 (20% smaller)
    autopanStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - powerButtonSize - 5 + 15 - 5 - 1, sequencerArea.getY() - 5 - 40 + 25 + 5, powerButtonSize, powerButtonSize); // 1px right, 5px up
    
    // Remove background colors (EXACT same as delay page)
    autopanStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    autopanStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn != nullptr)
    {
        autopanStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    autopanStepPowerButton->onClick = [this]() { 
        autopanStepAreaEnabled = !autopanStepAreaEnabled;
        // Enable/disable the AutoPan sequencer in processor
        processorRef.setAutoPanSequencerEnabled(autopanStepAreaEnabled);
        updateAutoPanStepAreaVisibility();
        DBG("[UI] AutoPan sequencer " << (autopanStepAreaEnabled ? "enabled" : "disabled"));
    };
    
    DBG("[UI] AutoPan sequencer area setup complete");
}

void PluginEditor::setupAutoPanAllStepsToggle()
{
    DBG("[UI] Setting up AutoPan All Steps toggle...");
    
    // Effect area bounds (EXACT same as delay page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "All Steps" toggle button - using AllStepsToggleButton like delay page
    autopanAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(autopanAllStepsToggle.get());
    autopanAllStepsToggle->setVisible(false); // Initially hidden until AutoPan page is selected
    
    // Position button in EXACT same location as delay page
    const int buttonSize = 29; // 24 * 1.2 = 28.8, rounded to 29
    autopanAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, effectArea.getY() - 1, buttonSize, buttonSize);
    
    // Set up images (EXACT same as delay page)
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr)
    {
        static_cast<AllStepsToggleButton*>(autopanAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    // Create "All Steps" label
    autopanAllStepsLabel = std::make_unique<juce::Label>();
    autopanAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    autopanAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold)); // 12.0f * 1.2 = 14.4f (20% bigger)
    autopanAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    autopanAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(autopanAllStepsLabel.get());
    autopanAllStepsLabel->setVisible(false); // Initially hidden until AutoPan page is selected
    
    // Position label to the right of the button, moved 30px right, moved up 4px
    autopanAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, effectArea.getY() + 1, 80, 24); // Moved up 4px from 5 to 1
    
    DBG("[UI] AutoPan All Steps toggle setup complete");
}

void PluginEditor::setupAutoPanStepPowerButton()
{
    DBG("[UI] AutoPan step power button already created in setupAutoPanSequencerArea");
}


// AutoPan helper methods
void PluginEditor::randomizeAutoPanKnobValues()
{
    DBG("[UI] Randomizing AutoPan knob values...");
    
    for (int i = 0; i < 6; ++i)
    {
        if (autopanLockButtons[i] && autopanLockButtons[i]->getToggleState()) {
            continue; // Skip locked knobs
        }
        
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        
        // Special handling for specific knobs
        switch (i) {
            case 2: // Wave Type (0-4)
                randomValue = (float)(juce::Random::getSystemRandom().nextInt(5)); // 0, 1, 2, 3, or 4
                break;
            case 4: // Inverted (0 or 1)
                randomValue = (float)(juce::Random::getSystemRandom().nextInt(2)); // 0 or 1
                break;
            default:
                // Other knobs use full 0-1 range
                break;
        }
        
        if (autopanKnobs[i]) {
            autopanKnobs[i]->setValue(randomValue, juce::sendNotification);
        }
    }
    
    DBG("[UI] All AutoPan knob values randomized");
}

void PluginEditor::randomizeIndividualAutoPanKnob(int knobIndex)
{
    DBG("[UI] Randomizing individual AutoPan knob " << knobIndex << "...");
    
    if (knobIndex >= 0 && knobIndex < 6)
    {
        if (autopanLockButtons[knobIndex] && autopanLockButtons[knobIndex]->getToggleState()) {
            return; // Skip if locked
        }
        
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        
        // Special handling for specific knobs
        switch (knobIndex) {
            case 2: // Wave Type (0-4)
                randomValue = (float)(juce::Random::getSystemRandom().nextInt(5));
                break;
            case 4: // Inverted (0 or 1)
                randomValue = (float)(juce::Random::getSystemRandom().nextInt(2));
                break;
        }
        
        if (autopanKnobs[knobIndex]) {
            autopanKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
        }
    }
}

void PluginEditor::updateAutoPanParameterFromKnob(int knobIndex)
{
    // This is called when knob values change to update the APVTS
    // The SliderAttachment already handles this, so this can be empty
    // or used for additional UI updates if needed
}

void PluginEditor::onAutoPanStepButtonClicked(int stepIndex)
{
    DBG("[UI] AutoPan step button " << stepIndex << " clicked");
    
    // Save current step's snapshot before switching
    int currentStep = autopanUiSelectedStep;  // Use UI selected step, not audio thread step
    if (currentStep >= 0 && currentStep < 16) {
        StepSnapshot currentSnapshot;
        // Read current AutoPan knob values and save to snapshot
        if (autopanKnobs[0]) currentSnapshot.autopan.rate = autopanKnobs[0]->getValue();
        if (autopanKnobs[1]) currentSnapshot.autopan.phase = autopanKnobs[1]->getValue();
        if (autopanKnobs[2]) currentSnapshot.autopan.waveType = (int)autopanKnobs[2]->getValue();
        if (autopanKnobs[3]) currentSnapshot.autopan.waveShape = autopanKnobs[3]->getValue();
        if (autopanKnobs[4]) currentSnapshot.autopan.inverted = autopanKnobs[4]->getValue() > 0.5f;
        if (autopanKnobs[5]) currentSnapshot.autopan.amount = autopanKnobs[5]->getValue();
        
        processorRef.setAutoPanStepSnapshot(currentStep, currentSnapshot);
        DBG("[UI] Saved AutoPan snapshot for step " << currentStep);
    }
    
    // Switch to new step (both UI and processor tracking)
    autopanUiSelectedStep = stepIndex;
    processorRef.setAutoPanSelectedStep(stepIndex);
    
    // Load new step's snapshot into knobs
    StepSnapshot newSnapshot = processorRef.getAutoPanSafeSnapshot(stepIndex);
    if (autopanKnobs[0]) autopanKnobs[0]->setValue(newSnapshot.autopan.rate, juce::sendNotification);
    if (autopanKnobs[1]) autopanKnobs[1]->setValue(newSnapshot.autopan.phase, juce::sendNotification);
    if (autopanKnobs[2]) autopanKnobs[2]->setValue((float)newSnapshot.autopan.waveType, juce::sendNotification);
    if (autopanKnobs[3]) autopanKnobs[3]->setValue(newSnapshot.autopan.waveShape, juce::sendNotification);
    if (autopanKnobs[4]) autopanKnobs[4]->setValue(newSnapshot.autopan.inverted ? 1.0f : 0.0f, juce::sendNotification);
    if (autopanKnobs[5]) autopanKnobs[5]->setValue(newSnapshot.autopan.amount, juce::sendNotification);
    
    // Update UI will be called from timer
    updateAutoPanSequencerUI();
    
    DBG("[UI] Switched to AutoPan step " << stepIndex);
}

void PluginEditor::updateAutoPanSequencerUI()
{
    // Update AutoPan step button selection and playing states
    int selectedStep = autopanUiSelectedStep;  // UI selected step for editing
    int playingStep = processorRef.getAutoPanPlayingStep();  // Audio thread playing step
    const int stepsUsed = processorRef.getAutoPanSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (autopanStepButtons[i] != nullptr) {
            // Selected step (clicked for editing) shows SVG
            autopanStepButtons[i]->setSelected(i == selectedStep);
            // Playing step (during sequencer playback) shows grey highlight
            bool sequencerEnabled = processorRef.getAutoPanSeqState().enabled.load();
            autopanStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
            // Grey out inactive steps beyond stepsUsed
            bool shouldBeEnabled = i < stepsUsed;
            autopanStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display
    if (autopanStepAmountLabel != nullptr) {
        autopanStepAmountLabel->setText(juce::String(stepsUsed), juce::dontSendNotification);
    }
    
    // Update rate dropdown
    if (autopanRateDropdown != nullptr) {
        int divisionIndex = processorRef.getAutoPanSeqState().divisionIndex.load();
        autopanRateDropdown->setSelectedId(divisionIndex + 1);
    }
}

void PluginEditor::updateAutoPanFxAreaVisibility()
{
    // Toggle visibility of AutoPan effect area components
    bool visible = autopanFxAreaEnabled;
    
    for (int i = 0; i < 6; ++i)
    {
        if (autopanKnobs[i]) autopanKnobs[i]->setVisible(visible);
        if (autopanKnobLabels[i]) autopanKnobLabels[i]->setVisible(visible);
        if (autopanValueLabels[i]) autopanValueLabels[i]->setVisible(visible);
        if (autopanIndicatorBars[i]) autopanIndicatorBars[i]->setVisible(visible);
        if (autopanLockButtons[i]) autopanLockButtons[i]->setVisible(visible);
    }
    
    if (autopanEffectsTitle) autopanEffectsTitle->setVisible(visible);
    if (autopanDiceButton) autopanDiceButton->setVisible(visible);
    if (autopanTimeSyncToggle) autopanTimeSyncToggle->setVisible(visible);
}

void PluginEditor::updateAutoPanStepAreaVisibility()
{
    // Toggle visibility of AutoPan step area components
    bool visible = autopanStepAreaEnabled;
    
    for (int i = 0; i < 16; ++i)
    {
        if (autopanStepButtons[i]) autopanStepButtons[i]->setVisible(visible);
    }
    
    if (autopanStepTitle) autopanStepTitle->setVisible(visible);
    if (autopanStepAmountLabel) autopanStepAmountLabel->setVisible(visible);
    if (autopanRateDropdown) autopanRateDropdown->setVisible(visible);
    if (autopanStdToggle) autopanStdToggle->setVisible(visible);
    if (autopanStepDiceButton) autopanStepDiceButton->setVisible(visible);
    if (autopanAllStepsToggle) autopanAllStepsToggle->setVisible(visible);
    if (autopanAllStepsLabel) autopanAllStepsLabel->setVisible(visible);
}