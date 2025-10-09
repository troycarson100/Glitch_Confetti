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
    
    // Allow children to receive mouse clicks and keyboard focus
    setInterceptsMouseClicks(true, true); // (for this component, for children)
    
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
        
        // Sync AutoPan sequencer state with processor on startup
        processorRef.setAutoPanSequencerEnabled(autopanStepAreaEnabled);
        DBG("[UI] Initial AutoPan sequencer state synced: enabled=" << autopanStepAreaEnabled);
        
        // Setup Dirt page components
        setupDirtKnobs();
        setupDirtEffectsArea();
        setupDirtSequencerArea();
        setupDirtAllStepsToggle();
        
        // Sync Dirt sequencer state with processor on startup
        processorRef.setDirtSequencerEnabled(dirtStepAreaEnabled);
        DBG("[UI] Initial Dirt sequencer state synced: enabled=" << dirtStepAreaEnabled);
        
        // Initialize FX power button states from parameters
        auto* autopanEnabledParam = processorRef.getAPVTS().getRawParameterValue("autopanEnabled");
        if (autopanEnabledParam) {
            autopanFxAreaEnabled = autopanEnabledParam->load() > 0.5f;
            if (autopanFxPowerButton) {
                autopanFxPowerButton->setToggleState(autopanFxAreaEnabled, juce::dontSendNotification);
            }
            updateAutoPanFxAreaVisibility(); // Update UI to reflect initial state
            DBG("[UI] AutoPan FX power initialized: " << (autopanFxAreaEnabled ? "ON" : "OFF"));
        }
        
        auto* dirtEnabledParam = processorRef.getAPVTS().getRawParameterValue("dirtEnabled");
        if (dirtEnabledParam) {
            dirtFxAreaEnabled = dirtEnabledParam->load() > 0.5f;
            if (dirtFxPowerButton) {
                dirtFxPowerButton->setToggleState(dirtFxAreaEnabled, juce::dontSendNotification);
            }
            updateDirtFxAreaVisibility(); // Update UI to reflect initial state
            DBG("[UI] Dirt FX power initialized: " << (dirtFxAreaEnabled ? "ON" : "OFF"));
        }
        
        // Setup Chorus page components
        setupChorusKnobs();
        setupChorusEffectsArea();
        setupChorusSequencerArea();
        setupChorusAllStepsToggle();
        
        // Sync Chorus sequencer state with processor on startup
        processorRef.setChorusSequencerEnabled(chorusStepAreaEnabled);
        DBG("[UI] Initial Chorus sequencer state synced: enabled=" << chorusStepAreaEnabled);
        
        // Initialize Chorus FX power button state from parameter
        auto* chorusEnabledParam = processorRef.getAPVTS().getRawParameterValue("chorusEnabled");
        if (chorusEnabledParam) {
            chorusFxAreaEnabled = chorusEnabledParam->load() > 0.5f;
            if (chorusFxPowerButton) {
                chorusFxPowerButton->setToggleState(chorusFxAreaEnabled, juce::dontSendNotification);
            }
            updateChorusFxAreaVisibility(); // Update UI to reflect initial state
            DBG("[UI] Chorus FX power initialized: " << (chorusFxAreaEnabled ? "ON" : "OFF"));
        }
        
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
    } else if (currentPage == FxPageID::Dirt) {
        // Draw the Dirt background SVG
        if (assets.dirtBackgroundTab3 != nullptr)
        {
            assets.dirtBackgroundTab3->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            // Fallback background
            g.fillAll (juce::Colour(0xff2a2a2a));
        }
    } else if (currentPage == FxPageID::Chorus) {
        // Draw the Chorus background SVG
        if (assets.chorusBackgroundTab4 != nullptr)
        {
            assets.chorusBackgroundTab4->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
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

    // Draw knob lock icons on top of UI for all pages
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
    else if (currentPage == FxPageID::Panner)
    {
        for (int i = 0; i < 6; ++i) // AutoPan has 6 knobs
        {
            if (autopanLockButtons[i] != nullptr)
            {
                auto b = autopanLockButtons[i]->getBounds().toFloat();
                if (autopanKnobLocked[i]) {
                    if (assets.lockedIcon != nullptr)
                        assets.lockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                } else {
                    if (assets.unlockedIcon != nullptr)
                        assets.unlockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                }
            }
        }
    }
    else if (currentPage == FxPageID::Dirt)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (dirtLockButtons[i] != nullptr)
            {
                auto b = dirtLockButtons[i]->getBounds().toFloat();
                if (dirtKnobLocked[i]) {
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
                float knobValue = autopanKnobs[i]->getValue();
                float indicatorValue = 0.0f;
                
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
                else
                {
                    // Sequencer is off - normalize knob value to 0-1 for indicator
                    switch (i) {
                        case 0: // Rate
                            if (autopanTimeSyncEnabled) {
                                indicatorValue = knobValue; // Already 0-1 in sync mode
                            } else {
                                indicatorValue = (knobValue - 0.05f) / (90.0f - 0.05f); // Normalize 0.05-90 to 0-1
                            }
                            break;
                        case 1: // Phase (0-360)
                            indicatorValue = knobValue / 360.0f;
                            break;
                        case 2: // Wave Type (0-4)
                            indicatorValue = knobValue / 4.0f;
                            break;
                        case 3: // Wave Shape (0-1)
                            indicatorValue = knobValue;
                            break;
                        case 4: // Inverted (0-1)
                            indicatorValue = knobValue;
                            break;
                        case 5: // Amount (0-1)
                            indicatorValue = knobValue;
                            break;
                    }
                }
                
                autopanIndicatorBars[i]->setValue(indicatorValue);
            }
        }
    }
    
    // Update Dirt knob value labels
    for (int i = 0; i < 8; ++i)
    {
        if (dirtKnobs[i] != nullptr && dirtValueLabels[i] != nullptr)
        {
            float knobValue = dirtKnobs[i]->getValue();
            juce::String valueText;
            
            switch (i) {
                case 0: valueText = juce::String(knobValue, 1) + " dB"; break; // Drive
                case 1: valueText = juce::String(knobValue, 2); break; // Color
                case 2: valueText = juce::String(knobValue, 2); break; // Asym
                case 3: valueText = juce::String(knobValue, 2); break; // Texture
                case 4: valueText = juce::String((int)knobValue) + " Hz"; break; // Low-Cut
                case 5: valueText = juce::String((int)knobValue) + " Hz"; break; // High-Cut
                case 6: valueText = juce::String(knobValue, 2); break; // Tone
                case 7: valueText = juce::String((int)(knobValue * 100)) + "%"; break; // Mix
            }
            
            dirtValueLabels[i]->setText(valueText, juce::dontSendNotification);
        }
    }
    
    // Update Dirt knob indicators (same pattern as AutoPan)
    for (int i = 0; i < 8; ++i)
    {
        if (dirtIndicatorBars[i] != nullptr && dirtKnobs[i] != nullptr)
        {
            float knobValue = dirtKnobs[i]->getValue();
            float indicatorValue = 0.0f;
            
            // If Dirt sequencer is enabled and running, show the playing step's snapshot value
            const bool seqEnabled = processorRef.getDirtSeqState().enabled.load();
            const bool seqActive = processorRef.getDirtSeqState().active.load();
            const int playingStep = processorRef.getDirtPlayingStep();
            
            if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16)
            {
                StepSnapshot s = processorRef.getDirtSafeSnapshot(playingStep);
                
                switch (i) {
                    case 0: indicatorValue = s.dirt.drive / 36.0f; break; // Normalize 0-36 to 0-1
                    case 1: indicatorValue = (s.dirt.color + 1.0f) / 2.0f; break; // Normalize -1..1 to 0-1
                    case 2: indicatorValue = (s.dirt.asym + 1.0f) / 2.0f; break; // Normalize -1..1 to 0-1
                    case 3: indicatorValue = s.dirt.texture; break;
                    case 4: indicatorValue = (s.dirt.lowCut - 20.0f) / 280.0f; break; // Normalize 20-300 to 0-1
                    case 5: indicatorValue = (s.dirt.highCut - 3000.0f) / 19000.0f; break; // Normalize 3k-22k to 0-1
                    case 6: indicatorValue = (s.dirt.tone + 1.0f) / 2.0f; break; // Normalize -1..1 to 0-1
                    case 7: indicatorValue = s.dirt.mix; break;
                }
            }
            else
            {
                // Sequencer is off - normalize knob value to 0-1 for indicator
                switch (i) {
                    case 0: // Drive (0-36 dB)
                        indicatorValue = knobValue / 36.0f;
                        break;
                    case 1: // Color (-1 to +1)
                        indicatorValue = (knobValue + 1.0f) / 2.0f;
                        break;
                    case 2: // Asym (-1 to +1)
                        indicatorValue = (knobValue + 1.0f) / 2.0f;
                        break;
                    case 3: // Texture (0-1)
                        indicatorValue = knobValue;
                        break;
                    case 4: // Low-Cut (20-300 Hz)
                        indicatorValue = (knobValue - 20.0f) / 280.0f;
                        break;
                    case 5: // High-Cut (3000-22000 Hz)
                        indicatorValue = (knobValue - 3000.0f) / 19000.0f;
                        break;
                    case 6: // Tone (-1 to +1)
                        indicatorValue = (knobValue + 1.0f) / 2.0f;
                        break;
                    case 7: // Mix (0-1)
                        indicatorValue = knobValue;
                        break;
                }
            }
            
            dirtIndicatorBars[i]->setValue(indicatorValue);
        }
    }
    
    
    // Update Chorus knob value labels
    for (int i = 0; i < 8; ++i)
    {
        if (chorusKnobs[i] != nullptr && chorusValueLabels[i] != nullptr)
        {
            float knobValue = chorusKnobs[i]->getValue();
            juce::String valueText;
            
            switch (i) {
                case 0: valueText = juce::String(knobValue, 1) + " ms"; break; // Delay (5-50ms)
                case 1: valueText = juce::String(knobValue, 2) + " Hz"; break; // Rate (0.02-8Hz)
                case 2: valueText = juce::String(knobValue, 1) + " ms"; break; // Depth (0-12ms)
                case 3: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Feedback (0-0.9 → 0-90%)
                case 4: valueText = juce::String((int)(knobValue)); break; // Voices (2-8)
                case 5: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Width (0-1 → 0-100%)
                case 6: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Shape (0-1 → 0-100%)
                case 7: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Mix (0-1 → 0-100%)
            }
            
            chorusValueLabels[i]->setText(valueText, juce::dontSendNotification);
        }
    }
    
    // Update Chorus knob indicators
    for (int i = 0; i < 8; ++i)
    {
        if (chorusIndicatorBars[i] != nullptr && chorusKnobs[i] != nullptr)
        {
            float knobValue = chorusKnobs[i]->getValue();
            float indicatorValue = 0.0f;
            
            const bool seqEnabled = processorRef.getChorusSeqState().enabled.load();
            const bool seqActive = processorRef.getChorusSeqState().active.load();
            const int playingStep = processorRef.getChorusPlayingStep();
            
            if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16)
            {
                StepSnapshot s = processorRef.getChorusSafeSnapshot(playingStep);
                
                switch (i) {
                    case 0: indicatorValue = (s.chorus.delayTime - 5.0f) / 45.0f; break; // Delay (5-50ms)
                    case 1: indicatorValue = (s.chorus.rate - 0.02f) / 7.98f; break; // Rate (0.02-8Hz)
                    case 2: indicatorValue = s.chorus.depth / 12.0f; break; // Depth (0-12ms)
                    case 3: indicatorValue = s.chorus.feedback / 0.9f; break; // Feedback (0-0.9)
                    case 4: indicatorValue = (s.chorus.voices - 2.0f) / 6.0f; break; // Voices (2-8)
                    case 5: indicatorValue = s.chorus.width; break; // Width (0-1)
                    case 6: indicatorValue = s.chorus.tone; break; // Shape (0-1)
                    case 7: indicatorValue = s.chorus.mix; break; // Mix (0-1)
                }
            }
            else
            {
                switch (i) {
                    case 0: indicatorValue = (knobValue - 5.0f) / 45.0f; break; // Delay (5-50ms)
                    case 1: indicatorValue = (knobValue - 0.02f) / 7.98f; break; // Rate (0.02-8Hz)
                    case 2: indicatorValue = knobValue / 12.0f; break; // Depth (0-12ms)
                    case 3: indicatorValue = knobValue / 0.9f; break; // Feedback (0-0.9)
                    case 4: indicatorValue = (knobValue - 2.0f) / 6.0f; break; // Voices (2-8)
                    case 5: indicatorValue = knobValue; break; // Width (0-1)
                    case 6: indicatorValue = knobValue; break; // Shape (0-1)
                    case 7: indicatorValue = knobValue; break; // Mix (0-1)
                }
            }
            
            chorusIndicatorBars[i]->setValue(indicatorValue);
        }
    }
    
    // Update sequencer UI (Delay)
    updateSequencerUI();
    
    // Update AutoPan sequencer UI
    updateAutoPanSequencerUI();
    
    // Update Dirt sequencer UI
    updateDirtSequencerUI();
    
    // Update Chorus sequencer UI
    updateChorusSequencerUI();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    // If any step amount TextEditor has focus, let it handle the keypress
    if (autopanStepAmountLabel && autopanStepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it
    }
    if (dirtStepAmountLabel && dirtStepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it
    }
    if (chorusStepAmountLabel && chorusStepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it
    }
    if (stepAmountLabel && stepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it (if we convert Delay page too)
    }
    
    // Otherwise, let the parent class handle it
    return false; // Don't consume the key, pass it up the chain
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
        const int panBarY = y - 285;
        panBar->setBounds(startX - meterSpacing - meterWidth + 10, panBarY, (meterWidth * 2 + meterSpacing + totalKnobWidth - 20) - 90, 24);
        
        // Create and position Output Visualizer (glowing waveform display)
        outputVisualizer = std::make_unique<OutputVisualizer>(processorRef.outputVisualizerBuffer);
        addAndMakeVisible(*outputVisualizer);
        
        // Position visualizer below PanManBar with 10px gap
        const int vizX = startX - meterSpacing - meterWidth + 10;
        const int vizY = panBarY + 24 + 10; // Below PanManBar + 10px gap
        const int vizWidth = (meterWidth * 2 + meterSpacing + totalKnobWidth - 20) - 90;
        const int vizHeight = 120; // 25% reduction from 160px for more compact appearance
        outputVisualizer->setBounds(vizX, vizY, vizWidth, vizHeight);
        
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

void PluginEditor::updateAutoPanFxAreaVisibility()
{
    float alpha = autopanFxAreaEnabled ? 1.0f : 0.3f;

    // Grey title, dice
    if (autopanEffectsTitle) autopanEffectsTitle->setAlpha(alpha);
    if (autopanDiceButton) { autopanDiceButton->setAlpha(alpha); autopanDiceButton->setEnabled(autopanFxAreaEnabled); }

    // Grey knobs, labels, values, indicators, locks
    for (int i = 0; i < 6; ++i) {
        if (autopanKnobs[i]) { autopanKnobs[i]->setAlpha(alpha); autopanKnobs[i]->setEnabled(autopanFxAreaEnabled); }
        if (autopanKnobLabels[i]) autopanKnobLabels[i]->setAlpha(alpha);
        if (autopanValueLabels[i]) autopanValueLabels[i]->setAlpha(alpha);
        if (autopanIndicatorBars[i]) autopanIndicatorBars[i]->setAlpha(alpha);
        if (autopanLockButtons[i]) { 
            autopanLockButtons[i]->setEnabled(autopanFxAreaEnabled);
            autopanLockButtons[i]->setAlpha(alpha);
        }
    }

    // Grey the time sync toggle
    if (autopanTimeSyncToggle) { autopanTimeSyncToggle->setAlpha(alpha); autopanTimeSyncToggle->setEnabled(autopanFxAreaEnabled); }

    // Grey the All Steps toggle and label (these are in effects area)
    if (autopanAllStepsToggle) { autopanAllStepsToggle->setAlpha(alpha); autopanAllStepsToggle->setEnabled(autopanFxAreaEnabled); }
    if (autopanAllStepsLabel) autopanAllStepsLabel->setAlpha(alpha);

    // Grey the power button itself when off
    if (autopanFxPowerButton) autopanFxPowerButton->setAlpha(autopanFxAreaEnabled ? 1.0f : 0.3f);
}

void PluginEditor::updateDirtFxAreaVisibility()
{
    float alpha = dirtFxAreaEnabled ? 1.0f : 0.3f;

    // Grey title, dice
    if (dirtEffectsTitle) dirtEffectsTitle->setAlpha(alpha);
    if (dirtDiceButton) { dirtDiceButton->setAlpha(alpha); dirtDiceButton->setEnabled(dirtFxAreaEnabled); }

    // Grey knobs, labels, values, indicators, locks
    for (int i = 0; i < 8; ++i) {
        if (dirtKnobs[i]) { dirtKnobs[i]->setAlpha(alpha); dirtKnobs[i]->setEnabled(dirtFxAreaEnabled); }
        if (dirtKnobLabels[i]) dirtKnobLabels[i]->setAlpha(alpha);
        if (dirtValueLabels[i]) dirtValueLabels[i]->setAlpha(alpha);
        if (dirtIndicatorBars[i]) dirtIndicatorBars[i]->setAlpha(alpha);
        if (dirtLockButtons[i]) { 
            dirtLockButtons[i]->setEnabled(dirtFxAreaEnabled);
            dirtLockButtons[i]->setAlpha(alpha);
        }
    }

    // Grey the All Steps toggle and label (these are in effects area)
    if (dirtAllStepsToggle) { dirtAllStepsToggle->setAlpha(alpha); dirtAllStepsToggle->setEnabled(dirtFxAreaEnabled); }
    if (dirtAllStepsLabel) dirtAllStepsLabel->setAlpha(alpha);

    // Grey the power button itself when off
    if (dirtFxPowerButton) dirtFxPowerButton->setAlpha(dirtFxAreaEnabled ? 1.0f : 0.3f);
}

void PluginEditor::updateDirtStepAreaVisibility()
{
    // Grey out Dirt step area components (not including All Steps toggle/label - those are in effects area)
    float alpha = dirtStepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i)
    {
        if (dirtStepButtons[i]) { 
            dirtStepButtons[i]->setAlpha(alpha); 
            dirtStepButtons[i]->setEnabled(dirtStepAreaEnabled);
        }
    }
    
    if (dirtStepTitle) dirtStepTitle->setAlpha(alpha);
    if (dirtStepAmountLabel) dirtStepAmountLabel->setAlpha(alpha); // Keep enabled for editing even when sequencer is off
    if (dirtRateDropdown) { dirtRateDropdown->setAlpha(alpha); dirtRateDropdown->setEnabled(dirtStepAreaEnabled); }
    if (dirtStdToggle) { dirtStdToggle->setAlpha(alpha); dirtStdToggle->setEnabled(dirtStepAreaEnabled); }
    if (dirtStepDiceButton) { dirtStepDiceButton->setAlpha(alpha); dirtStepDiceButton->setEnabled(dirtStepAreaEnabled); }
    if (dirtStepPowerButton) dirtStepPowerButton->setAlpha(dirtStepAreaEnabled ? 1.0f : 0.3f);
    // Note: All Steps toggle/label are NOT controlled here - they're in the effects area
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
    stepAmountLabel->onEditorHide = [this]() {
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
    tabDirt = std::make_unique<juce::DrawableButton>("tabDirt", juce::DrawableButton::ImageOnButtonBackground);
    tabChorus = std::make_unique<juce::DrawableButton>("tabChorus", juce::DrawableButton::ImageOnButtonBackground);
    
    // Use the tab SVGs you provided:
    if (assets.tabTitleSpaceDelay) {
        tabSpaceDelay->setImages(assets.tabTitleSpaceDelay->createCopy().release(),
                                 nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    if (assets.tabTitleAutoPan) { // Using AutoPan SVG for the second tab
        tabPanner->setImages(assets.tabTitleAutoPan->createCopy().release(),
                             nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    if (assets.tabDirtIcon) { // Using Dirt icon SVG for the third tab
        tabDirt->setImages(assets.tabDirtIcon->createCopy().release(),
                             nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    if (assets.tabChorusIcon) { // Using Chorus icon SVG for the fourth tab
        tabChorus->setImages(assets.tabChorusIcon->createCopy().release(),
                             nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    
    // Add bright background colors to make tabs visible for testing
    tabSpaceDelay->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xFFFF6600)); // Bright orange
    tabPanner->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xFF00FF00)); // Bright green
    tabDirt->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xFFFF00FF)); // Bright magenta
    tabChorus->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xFF00FFFF)); // Bright cyan
    
    tabSpaceDelay->setTriggeredOnMouseDown(true);
    tabPanner->setTriggeredOnMouseDown(true);
    tabDirt->setTriggeredOnMouseDown(true);
    tabChorus->setTriggeredOnMouseDown(true);
    
    // Click handlers
    tabSpaceDelay->onClick = [this]{ 
        DBG("[UI] SpaceDelay tab clicked!");
        showPage(FxPageID::SpaceDelay); 
    };
    tabPanner->onClick = [this]{ 
        DBG("[UI] Panner tab clicked!");
        showPage(FxPageID::Panner); 
    };
    tabDirt->onClick = [this]{ 
        DBG("[UI] Dirt tab clicked!");
        showPage(FxPageID::Dirt); 
    };
    tabChorus->onClick = [this]{ 
        DBG("[UI] Chorus tab clicked!");
        showPage(FxPageID::Chorus); 
    };
    
    // Ensure tabs never obstruct the master area clicks; they only sit over the header strip
    tabSpaceDelay->setAlwaysOnTop(true);
    tabPanner->setAlwaysOnTop(true);
    tabDirt->setAlwaysOnTop(true);
    tabChorus->setAlwaysOnTop(true);
    
    addAndMakeVisible(*tabSpaceDelay);
    addAndMakeVisible(*tabPanner);
    addAndMakeVisible(*tabDirt);
    addAndMakeVisible(*tabChorus);
    
    // Position tabs immediately after creation - smaller and moved up 10px
    tabSpaceDelay->setBounds(24, 0, 120, 34);
    tabPanner->setBounds(170, 0, 120, 34);
    tabDirt->setBounds(316, 0, 120, 34);
    tabChorus->setBounds(462, 0, 120, 34);
    
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
    
    // Collect pointers to Dirt UI components
    dirtGroup.clear();
    
    // Add Dirt knobs and related components
    for (int i = 0; i < 8; ++i) {
        if (dirtKnobs[i]) dirtGroup.push_back(dirtKnobs[i].get());
        if (dirtKnobLabels[i]) dirtGroup.push_back(dirtKnobLabels[i].get());
        if (dirtValueLabels[i]) dirtGroup.push_back(dirtValueLabels[i].get());
        if (dirtIndicatorBars[i]) dirtGroup.push_back(dirtIndicatorBars[i].get());
        if (dirtLockButtons[i]) dirtGroup.push_back(dirtLockButtons[i].get());
    }
    
    // Add Dirt effects area components
    if (dirtEffectsTitle) dirtGroup.push_back(dirtEffectsTitle.get());
    if (dirtDiceButton) dirtGroup.push_back(dirtDiceButton.get());
    if (dirtFxPowerButton) dirtGroup.push_back(dirtFxPowerButton.get());
    
    // Add Dirt sequencer components
    if (dirtAllStepsToggle) dirtGroup.push_back(dirtAllStepsToggle.get());
    if (dirtAllStepsLabel) dirtGroup.push_back(dirtAllStepsLabel.get());
    for (int i = 0; i < 16; ++i) {
        if (dirtStepButtons[i]) dirtGroup.push_back(dirtStepButtons[i].get());
    }
    if (dirtStepAmountLabel) dirtGroup.push_back(dirtStepAmountLabel.get());
    if (dirtRateDropdown) dirtGroup.push_back(dirtRateDropdown.get());
    if (dirtStdToggle) dirtGroup.push_back(dirtStdToggle.get());
    if (dirtStepTitle) dirtGroup.push_back(dirtStepTitle.get());
    if (dirtStepDiceButton) dirtGroup.push_back(dirtStepDiceButton.get());
    if (dirtStepPowerButton) dirtGroup.push_back(dirtStepPowerButton.get());
    
    // Populate Chorus group
    chorusGroup.clear();
    
    // Add Chorus knobs and related components
    for (int i = 0; i < 8; ++i) {
        if (chorusKnobs[i]) chorusGroup.push_back(chorusKnobs[i].get());
        if (chorusKnobLabels[i]) chorusGroup.push_back(chorusKnobLabels[i].get());
        if (chorusValueLabels[i]) chorusGroup.push_back(chorusValueLabels[i].get());
        if (chorusIndicatorBars[i]) chorusGroup.push_back(chorusIndicatorBars[i].get());
        if (chorusLockButtons[i]) chorusGroup.push_back(chorusLockButtons[i].get());
    }
    
    // Add Chorus effects area components
    if (chorusEffectsTitle) chorusGroup.push_back(chorusEffectsTitle.get());
    if (chorusDiceButton) chorusGroup.push_back(chorusDiceButton.get());
    if (chorusFxPowerButton) chorusGroup.push_back(chorusFxPowerButton.get());
    
    // Add Chorus sequencer components
    if (chorusAllStepsToggle) chorusGroup.push_back(chorusAllStepsToggle.get());
    if (chorusAllStepsLabel) chorusGroup.push_back(chorusAllStepsLabel.get());
    for (int i = 0; i < 16; ++i) {
        if (chorusStepButtons[i]) chorusGroup.push_back(chorusStepButtons[i].get());
    }
    if (chorusStepAmountLabel) chorusGroup.push_back(chorusStepAmountLabel.get());
    if (chorusRateDropdown) chorusGroup.push_back(chorusRateDropdown.get());
    if (chorusStdToggle) chorusGroup.push_back(chorusStdToggle.get());
    if (chorusStepTitle) chorusGroup.push_back(chorusStepTitle.get());
    if (chorusStepDiceButton) chorusGroup.push_back(chorusStepDiceButton.get());
    if (chorusStepPowerButton) chorusGroup.push_back(chorusStepPowerButton.get());
    
    // Initialize with SpaceDelay page visible
    showPage(FxPageID::SpaceDelay);
    
    DBG("[UI] Tab system setup complete. SpaceDelay components: " << spaceDelayGroup.size());
    DBG("[UI] AutoPan components: " << pannerGroup.size());
    DBG("[UI] Dirt components: " << dirtGroup.size());
    DBG("[UI] Chorus components: " << chorusGroup.size());
}

void PluginEditor::showPage(FxPageID id)
{
    if (currentPage == id) return;
    currentPage = id;

    // Update processor parameters (0 = SpaceDelay, 1 = AutoPan, 2 = Dirt, 3 = Chorus)
    auto* currentPageParam = processorRef.getAPVTS().getParameter("currentPage");
    if (currentPageParam) {
        float pageValue = 0.0f;
        if (id == FxPageID::Panner) pageValue = 1.0f;
        else if (id == FxPageID::Dirt) pageValue = 2.0f;
        else if (id == FxPageID::Chorus) pageValue = 3.0f;
        currentPageParam->setValueNotifyingHost(pageValue);
    }
    
    // Update AutoPan UI to reflect current parameter state (don't force it on)
    if (id == FxPageID::Panner) {
        auto* autopanEnabledParam = processorRef.getAPVTS().getRawParameterValue("autopanEnabled");
        if (autopanEnabledParam) {
            autopanFxAreaEnabled = autopanEnabledParam->load() > 0.5f;
            if (autopanFxPowerButton) {
                autopanFxPowerButton->setToggleState(autopanFxAreaEnabled, juce::dontSendNotification);
            }
        }
        updateAutoPanFxAreaVisibility();
    }
    
    // Update Dirt UI to reflect current parameter state (don't force it on)
    if (id == FxPageID::Dirt) {
        auto* dirtEnabledParam = processorRef.getAPVTS().getRawParameterValue("dirtEnabled");
        if (dirtEnabledParam) {
            dirtFxAreaEnabled = dirtEnabledParam->load() > 0.5f;
            if (dirtFxPowerButton) {
                dirtFxPowerButton->setToggleState(dirtFxAreaEnabled, juce::dontSendNotification);
            }
        }
        updateDirtFxAreaVisibility();
    }
    
    // Update Chorus UI to reflect current parameter state (don't force it on)
    if (id == FxPageID::Chorus) {
        auto* chorusEnabledParam = processorRef.getAPVTS().getRawParameterValue("chorusEnabled");
        if (chorusEnabledParam) {
            chorusFxAreaEnabled = chorusEnabledParam->load() > 0.5f;
            if (chorusFxPowerButton) {
                chorusFxPowerButton->setToggleState(chorusFxAreaEnabled, juce::dontSendNotification);
            }
        }
        updateChorusFxAreaVisibility();
    }

    // Show/Hide without touching parents or bounds
    auto setVisibleVec = [](const std::vector<juce::Component*>& v, bool vis)
    {
        for (auto* c : v) if (c) c->setVisible(vis);
    };

    setVisibleVec(spaceDelayGroup, id == FxPageID::SpaceDelay);
    setVisibleVec(pannerGroup, id == FxPageID::Panner);
    setVisibleVec(dirtGroup, id == FxPageID::Dirt);
    setVisibleVec(chorusGroup, id == FxPageID::Chorus);

    // Raise the active tab to front
    if (id == FxPageID::SpaceDelay && tabSpaceDelay) tabSpaceDelay->toFront(false);
    else if (id == FxPageID::Panner && tabPanner) tabPanner->toFront(false);
    else if (id == FxPageID::Dirt && tabDirt) tabDirt->toFront(false);
    else if (id == FxPageID::Chorus && tabChorus) tabChorus->toFront(false);
    
    // Bring step amount editors to front when page is shown
    if (id == FxPageID::Panner && autopanStepAmountLabel) {
        autopanStepAmountLabel->toFront(true);
        autopanStepAmountLabel->setWantsKeyboardFocus(true);
    }
    else if (id == FxPageID::Dirt && dirtStepAmountLabel) {
        dirtStepAmountLabel->toFront(true);
        dirtStepAmountLabel->setWantsKeyboardFocus(true);
    }
    else if (id == FxPageID::Chorus && chorusStepAmountLabel) {
        chorusStepAmountLabel->toFront(true);
        chorusStepAmountLabel->setWantsKeyboardFocus(true);
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
            
            // If All Steps toggle is active, update all step snapshots
            if (autopanAllStepsEnabled) {
                for (int step = 0; step < 16; ++step) {
                    auto snapshot = processorRef.getAutoPanSafeSnapshot(step);
                    switch (i) {
                        case 0: snapshot.autopan.rate = value; break;
                        case 1: snapshot.autopan.phase = value; break;
                        case 2: snapshot.autopan.waveType = (int)value; break;
                        case 3: snapshot.autopan.waveShape = value; break;
                        case 4: snapshot.autopan.inverted = value > 0.5f; break;
                        case 5: snapshot.autopan.amount = value; break;
                    }
                    processorRef.setAutoPanStepSnapshot(step, snapshot);
                }
            }
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
    
    autopanFxPowerButton->setClickingTogglesState(true);
    autopanFxPowerButton->setToggleState(autopanFxAreaEnabled, juce::dontSendNotification);
    autopanFxPowerButton->onClick = [this]() { 
        autopanFxAreaEnabled = autopanFxPowerButton->getToggleState();
        DBG("[UI] AutoPan FX power: " << (autopanFxAreaEnabled ? "ON" : "OFF"));
        updateAutoPanFxAreaVisibility();
        repaint();
        
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
    
    // Create step amount editor (using TextEditor for proper keyboard input)
    struct DebugTextEditor : public juce::TextEditor {
        void mouseDown(const juce::MouseEvent& e) override {
            DBG("[UI] AutoPan step amount mouseDown detected!");
            juce::TextEditor::mouseDown(e);
        }
        void focusGained(FocusChangeType cause) override {
            DBG("[UI] AutoPan step amount focusGained!");
            juce::TextEditor::focusGained(cause);
        }
    };
    autopanStepAmountLabel = std::make_unique<DebugTextEditor>();
    autopanStepAmountLabel->setText("16");
    autopanStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    autopanStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    autopanStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    autopanStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    autopanStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    autopanStepAmountLabel->setJustification(juce::Justification::centred); // Use centred text
    autopanStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    autopanStepAmountLabel->setIndents(0, 0); // No left/top padding for better centering
    autopanStepAmountLabel->setInputRestrictions(2, "0123456789"); // Only allow 1-2 digit numbers
    autopanStepAmountLabel->setWantsKeyboardFocus(true);
    autopanStepAmountLabel->setMouseClickGrabsKeyboardFocus(true);
    autopanStepAmountLabel->setCaretVisible(true);
    autopanStepAmountLabel->setPopupMenuEnabled(false);
    autopanStepAmountLabel->setScrollbarsShown(false);
    autopanStepAmountLabel->setMultiLine(false); // Single line only
    autopanStepAmountLabel->setReturnKeyStartsNewLine(false); // Return commits instead of new line
    autopanStepAmountLabel->setInterceptsMouseClicks(true, false);
    autopanStepAmountLabel->onTextChange = [this]() {
        DBG("[UI] AutoPan step amount text changed to: " << autopanStepAmountLabel->getText());
    };
    autopanStepAmountLabel->onReturnKey = [this]() {
        DBG("[UI] AutoPan step amount Return key pressed");
        if (autopanStepAmountLabel != nullptr) {
            int value = autopanStepAmountLabel->getText().getIntValue();
            if (value < 1 || value > 16) value = juce::jlimit(1, 16, value);
            processorRef.setAutoPanStepsUsed(value);
            autopanStepAmountLabel->setText(juce::String(value), false);
            updateAutoPanSequencerUI();
            autopanStepAmountLabel->giveAwayKeyboardFocus(); // Exit editing mode
        }
    };
    autopanStepAmountLabel->onFocusLost = [this]() {
        if (autopanStepAmountLabel != nullptr) {
            int value = autopanStepAmountLabel->getText().getIntValue();
            if (value < 1) value = 1;
            if (value > 16) value = 16;
            processorRef.setAutoPanStepsUsed(value);
            autopanStepAmountLabel->setText(juce::String(value), false);
            updateAutoPanSequencerUI();
        }
    };
    addAndMakeVisible(autopanStepAmountLabel.get());
    autopanStepAmountLabel->setVisible(false); // Initially hidden until AutoPan page is selected
    autopanStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    autopanStepAmountLabel->setAlwaysOnTop(true);
    
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
    autopanRateDropdown->setSelectedId(6); // Default to 1/8 (ID 6 = divisionIndex 5)
    
    // Sync with processor's autopanSeq.divisionIndex on startup
    const int processorDivIdx = processorRef.getAutoPanSeqState().divisionIndex.load();
    autopanRateDropdown->setSelectedId(processorDivIdx + 1); // divisionIndex 0-7 -> ID 1-8
    DBG("[UI] AutoPan rate dropdown initialized: divIdx=" << processorDivIdx << " -> ID=" << (processorDivIdx + 1));
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
            
            // Randomize all AutoPan parameters for this step (respecting lock states)
            // Only randomize if lock button is NOT toggled (locked)
            if (!autopanKnobLocked[0]) {
                snapshot.autopan.rate = juce::Random::getSystemRandom().nextFloat(); // 0-1
            }
            if (!autopanKnobLocked[1]) {
                snapshot.autopan.phase = juce::Random::getSystemRandom().nextFloat() * 360.0f; // 0-360
            }
            if (!autopanKnobLocked[2]) {
                snapshot.autopan.waveType = juce::Random::getSystemRandom().nextInt(5); // 0-4
            }
            if (!autopanKnobLocked[3]) {
                snapshot.autopan.waveShape = juce::Random::getSystemRandom().nextFloat(); // 0-1
            }
            if (!autopanKnobLocked[4]) {
                snapshot.autopan.inverted = juce::Random::getSystemRandom().nextBool();
            }
            if (!autopanKnobLocked[5]) {
                snapshot.autopan.amount = juce::Random::getSystemRandom().nextFloat(); // 0-1
            }
            
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
    
    // Set up click handler (EXACT same as delay page)
    autopanStepPowerButton->setClickingTogglesState(true);
    autopanStepPowerButton->setToggleState(autopanStepAreaEnabled, juce::dontSendNotification);
    autopanStepPowerButton->onClick = [this]() { 
        autopanStepAreaEnabled = autopanStepPowerButton->getToggleState();
        DBG("[UI] AutoPan step area power: " << (autopanStepAreaEnabled ? "ON" : "OFF"));
        
        if (!autopanStepAreaEnabled) {
            // Disable sequencer when turning OFF
            processorRef.setAutoPanSequencerEnabled(false);
        } else {
            // Enable sequencer when turning ON
            processorRef.setAutoPanSequencerEnabled(true);
        }
        
        updateAutoPanStepAreaVisibility();
        repaint();
        DBG("[UI] AutoPan seq.enabled=" << processorRef.getAutoPanSeqState().enabled.load() 
            << " active=" << processorRef.getAutoPanSeqState().active.load());
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
    
    // Set up click handler (EXACT same as delay page)
    autopanAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    autopanAllStepsToggle->onClick = [this]() {
        autopanAllStepsEnabled = autopanAllStepsToggle->getToggleState();
        DBG("[UI] AutoPan All Steps toggle: " << (autopanAllStepsEnabled ? "ON" : "OFF") << " toggleState=" << autopanAllStepsToggle->getToggleState());
    };
    
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

//==============================================================================
// Dirt Page Setup Methods
//==============================================================================

void PluginEditor::setupDirtKnobs()
{
    DBG("[UI] Setting up Dirt knobs...");

    // Dirt knob names (8 knobs - exact Delay page layout)
    std::vector<juce::String> dirtKnobNames = {
        "Drive", "Color", "Asym", "Texture", "Low-Cut", "High-Cut", "Tone", "Mix"
    };

    // Effect area bounds (EXACT same as delay page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80; // EXACT same as delay page
    const int knobSpacing = 20; // EXACT same as delay page
    const int startX = effectArea.getX() + 15; // EXACT same as delay page
    const int startY = effectArea.getY() + effectArea.getHeight() - 210; // EXACT same as delay page
    
    for (int i = 0; i < 8; ++i) {
        // Create knob
        dirtKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(dirtKnobs[i].get());
        dirtKnobs[i]->setVisible(false); // Initially hidden until Dirt page is selected
        
        // Set knob properties
        dirtKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dirtKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        // Set specific ranges for each Dirt knob
        switch (i) {
            case 0: // Drive (0-36 dB)
                dirtKnobs[i]->setRange(0.0, 36.0, 0.1);
                dirtKnobs[i]->setValue(12.0, juce::dontSendNotification);
                break;
            case 1: // Color (-1 to +1)
                dirtKnobs[i]->setRange(-1.0, 1.0, 0.01);
                dirtKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 2: // Asym (-1 to +1)
                dirtKnobs[i]->setRange(-1.0, 1.0, 0.01);
                dirtKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 3: // Texture (0-1)
                dirtKnobs[i]->setRange(0.0, 1.0, 0.01);
                dirtKnobs[i]->setValue(0.35, juce::dontSendNotification);
                break;
            case 4: // Low-Cut (20-300 Hz)
                dirtKnobs[i]->setRange(20.0, 300.0, 1.0);
                dirtKnobs[i]->setValue(60.0, juce::dontSendNotification);
                break;
            case 5: // High-Cut (3000-22000 Hz)
                dirtKnobs[i]->setRange(3000.0, 22000.0, 100.0);
                dirtKnobs[i]->setValue(12000.0, juce::dontSendNotification);
                break;
            case 6: // Tone (-1 to +1)
                dirtKnobs[i]->setRange(-1.0, 1.0, 0.01);
                dirtKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                dirtKnobs[i]->setRange(0.0, 1.0, 0.01);
                dirtKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
        }
        
        // Connect to parameters
        std::vector<juce::String> paramIds = {
            "dirtDrive", "dirtColor", "dirtAsym", "dirtTexture", 
            "dirtLowCut", "dirtHighCut", "dirtTone", "dirtMix"
        };
        
        dirtAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), paramIds[i], *dirtKnobs[i]);
        
        // Add listener to save snapshots when knob changes
        dirtKnobs[i]->onValueChange = [this, i]() {
            float value = dirtKnobs[i]->getValue();
            processorRef.updateDirtCurrentStepSnapshot(i, value);
            
            // If All Steps toggle is active, update all step snapshots
            if (dirtAllStepsEnabled) {
                for (int step = 0; step < 16; ++step) {
                    auto snapshot = processorRef.getDirtSafeSnapshot(step);
                    switch (i) {
                        case 0: snapshot.dirt.drive = value; break;
                        case 1: snapshot.dirt.color = value; break;
                        case 2: snapshot.dirt.asym = value; break;
                        case 3: snapshot.dirt.texture = value; break;
                        case 4: snapshot.dirt.lowCut = value; break;
                        case 5: snapshot.dirt.highCut = value; break;
                        case 6: snapshot.dirt.tone = value; break;
                        case 7: snapshot.dirt.mix = value; break;
                    }
                    processorRef.setDirtStepSnapshot(step, snapshot);
                }
            }
        };
        
        // Set knob images
        if (assets.knobRing) {
            dirtKnobs[i]->setRingImage(assets.knobRing->createCopy());
        }
        if (assets.knobInside) {
            dirtKnobs[i]->setInnerImage(assets.knobInside->createCopy());
        }
        
        // Position knob (EXACT same logic as AutoPan/Delay page)
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px (EXACT same as delay page)
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1; // Moved up 6px from +5 to -1
            
        dirtKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label (EXACT same positioning as AutoPan page)
        dirtKnobLabels[i] = std::make_unique<juce::Label>();
        dirtKnobLabels[i]->setText(dirtKnobNames[i], juce::dontSendNotification);
        dirtKnobLabels[i]->setFont(juce::Font(12.0f, juce::Font::bold));
        dirtKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        dirtKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(dirtKnobLabels[i].get());
        dirtKnobLabels[i]->setVisible(false);
        dirtKnobLabels[i]->setBounds(x, y - 15, knobSize, 20); // EXACT same as AutoPan page
        
        // Create value label (EXACT same positioning as AutoPan page)
        dirtValueLabels[i] = std::make_unique<juce::Label>();
        dirtValueLabels[i]->setText("0", juce::dontSendNotification);
        dirtValueLabels[i]->setFont(juce::Font(10.0f, juce::Font::plain));
        dirtValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        dirtValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(dirtValueLabels[i].get());
        dirtValueLabels[i]->setVisible(false);
        dirtValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15); // EXACT same as AutoPan page
        
        // Create indicator bar (EXACT same positioning as AutoPan page)
        dirtIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(dirtIndicatorBars[i].get());
        dirtIndicatorBars[i]->setVisible(false);
        dirtIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13); // EXACT same as AutoPan page
        dirtIndicatorBars[i]->setValue(0.5f);
        
        // Create dice button (hidden like AutoPan page)
        dirtDiceButtons[i] = std::make_unique<CustomDiceButton>();
        dirtDiceButtons[i]->onClick = [this, i]() { randomizeIndividualDirtKnob(i); };
        
        // Create lock button (EXACT same positioning logic as AutoPan page)
        dirtLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(dirtLockButtons[i].get());
        dirtLockButtons[i]->setVisible(false);
        
        const int diceSize = 10;
        const int diceSpacing = 5;
        
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(dirtKnobNames[i]);
        int lockX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
        int lockY = y - 10;
        
        dirtLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);
        
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            dirtLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        dirtLockButtons[i]->setToggleState(dirtKnobLocked[i], juce::dontSendNotification);
        dirtLockButtons[i]->onClick = [this, i]() {
            dirtKnobLocked[i] = dirtLockButtons[i]->getToggleState();
            repaint();
        };
    }

    DBG("[UI] Dirt knobs setup complete");
}

void PluginEditor::setupDirtEffectsArea()
{
    DBG("[UI] Setting up Dirt effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title
    dirtEffectsTitle = std::make_unique<juce::Label>();
    dirtEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    dirtEffectsTitle->setFont(juce::Font(27.648f, juce::Font::bold));
    dirtEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    dirtEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dirtEffectsTitle.get());
    dirtEffectsTitle->setVisible(false);
    dirtEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Create dice button
    dirtDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(dirtDiceButton.get());
    dirtDiceButton->setVisible(false);
    
    const int diceSize = 32;
    dirtDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    
    if (assets.diceLarge != nullptr) {
        dirtDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    dirtDiceButton->onClick = [this]() { randomizeDirtKnobValues(); };
    
    // Create FX power button (EXACT same positioning as AutoPan)
    dirtFxPowerButton = std::make_unique<juce::DrawableButton>("dirtFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(dirtFxPowerButton.get());
    dirtFxPowerButton->setVisible(false);

    const int buttonSize = 46;
    dirtFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                 effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);

    dirtFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    dirtFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);

    if (assets.fxPowerOn != nullptr)
    {
        dirtFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }

    dirtFxPowerButton->setClickingTogglesState(true);
    dirtFxPowerButton->setToggleState(dirtFxAreaEnabled, juce::dontSendNotification);
    dirtFxPowerButton->onClick = [this]() {
        dirtFxAreaEnabled = dirtFxPowerButton->getToggleState();
        DBG("[UI] Dirt FX power: " << (dirtFxAreaEnabled ? "ON" : "OFF"));
        
        // Update processor parameter
        auto* dirtEnabledParam = processorRef.getAPVTS().getParameter("dirtEnabled");
        if (dirtEnabledParam) {
            dirtEnabledParam->setValueNotifyingHost(dirtFxAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateDirtFxAreaVisibility();
        repaint();
    };
    
    DBG("[UI] Dirt effects area setup complete");
}

void PluginEditor::setupDirtSequencerArea()
{
    DBG("[UI] Setting up Dirt sequencer area...");
    
    // Sequencer area bounds (EXACT same as AutoPan page)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title (EXACT same as AutoPan)
    dirtStepTitle = std::make_unique<juce::Label>();
    dirtStepTitle->setText("STEP", juce::dontSendNotification);
    dirtStepTitle->setFont(juce::Font(22.118f, juce::Font::bold)); // 20% smaller: 27.648f * 0.8 = 22.118f
    dirtStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    dirtStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dirtStepTitle.get());
    dirtStepTitle->setVisible(false);
    dirtStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step buttons (2 rows of 8, EXACT same layout as AutoPan page)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        dirtStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(dirtStepButtons[i].get());
        dirtStepButtons[i]->setVisible(false);
        
        // Position buttons in 2 rows of 8 (EXACT same as AutoPan page)
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        dirtStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set step button images
        if (assets.stepActive) {
            dirtStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive) {
            dirtStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        dirtStepButtons[i]->onClick = [this, i]() { onDirtStepButtonClicked(i); };
    }
    
    // Create step amount editor (using TextEditor for proper keyboard input)
    struct DirtDebugTextEditor : public juce::TextEditor {
        void mouseDown(const juce::MouseEvent& e) override {
            DBG("[UI] Dirt step amount mouseDown detected!");
            juce::TextEditor::mouseDown(e);
        }
        void focusGained(FocusChangeType cause) override {
            DBG("[UI] Dirt step amount focusGained!");
            juce::TextEditor::focusGained(cause);
        }
    };
    dirtStepAmountLabel = std::make_unique<DirtDebugTextEditor>();
    dirtStepAmountLabel->setText("16");
    dirtStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    dirtStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    dirtStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    dirtStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    dirtStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    dirtStepAmountLabel->setJustification(juce::Justification::centred); // Use centred text
    dirtStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    dirtStepAmountLabel->setIndents(0, 0); // No left/top padding for better centering
    dirtStepAmountLabel->setInputRestrictions(2, "0123456789"); // Only allow 1-2 digit numbers
    dirtStepAmountLabel->setWantsKeyboardFocus(true);
    dirtStepAmountLabel->setMouseClickGrabsKeyboardFocus(true);
    dirtStepAmountLabel->setCaretVisible(true);
    dirtStepAmountLabel->setPopupMenuEnabled(false);
    dirtStepAmountLabel->setScrollbarsShown(false);
    dirtStepAmountLabel->setMultiLine(false); // Single line only
    dirtStepAmountLabel->setReturnKeyStartsNewLine(false); // Return commits instead of new line
    dirtStepAmountLabel->setInterceptsMouseClicks(true, false);
    dirtStepAmountLabel->onTextChange = [this]() {
        DBG("[UI] Dirt step amount text changed to: " << dirtStepAmountLabel->getText());
    };
    dirtStepAmountLabel->onReturnKey = [this]() {
        DBG("[UI] Dirt step amount Return key pressed");
        if (dirtStepAmountLabel != nullptr) {
            int value = dirtStepAmountLabel->getText().getIntValue();
            if (value < 1 || value > 16) value = juce::jlimit(1, 16, value);
            processorRef.setDirtStepsUsed(value);
            dirtStepAmountLabel->setText(juce::String(value), false);
            updateDirtSequencerUI();
            dirtStepAmountLabel->giveAwayKeyboardFocus(); // Exit editing mode
        }
    };
    dirtStepAmountLabel->onFocusLost = [this]() {
        if (dirtStepAmountLabel != nullptr) {
            int value = dirtStepAmountLabel->getText().getIntValue();
            if (value < 1) value = 1;
            if (value > 16) value = 16;
            processorRef.setDirtStepsUsed(value);
            dirtStepAmountLabel->setText(juce::String(value), false);
            updateDirtSequencerUI();
        }
    };
    addAndMakeVisible(dirtStepAmountLabel.get());
    dirtStepAmountLabel->setVisible(false);
    dirtStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    dirtStepAmountLabel->setAlwaysOnTop(true);
    
    // Create rate dropdown (EXACT same as AutoPan)
    dirtRateDropdown = std::make_unique<juce::ComboBox>();
    dirtRateDropdown->addItem("4", 1);
    dirtRateDropdown->addItem("2", 2);
    dirtRateDropdown->addItem("1", 3);
    dirtRateDropdown->addItem("1/2", 4);
    dirtRateDropdown->addItem("1/4", 5);
    dirtRateDropdown->addItem("1/8", 6);
    dirtRateDropdown->addItem("1/16", 7);
    dirtRateDropdown->addItem("1/32", 8);
    dirtRateDropdown->setSelectedId(6);
    
    const int processorDivIdx = processorRef.getDirtSeqState().divisionIndex.load();
    dirtRateDropdown->setSelectedId(processorDivIdx + 1);
    
    dirtRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    dirtRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    dirtRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    dirtRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    dirtRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    dirtRateDropdown->onChange = [this]() {
        if (dirtRateDropdown != nullptr) {
            const int selected = dirtRateDropdown->getSelectedId();
            if (selected >= 1 && selected <= 8) {
                const int newDivisionIndex = juce::jlimit(0, 7, selected - 1);
                processorRef.setDirtDivisionIndex(newDivisionIndex);
                updateDirtSequencerUI();
            }
        }
    };
    addAndMakeVisible(dirtRateDropdown.get());
    dirtRateDropdown->setVisible(false);
    dirtRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25); // EXACT same as AutoPan
    
    // Create STD toggle (EXACT same positioning as AutoPan)
    dirtStdToggle = std::make_unique<CircularToggleButton>();
    dirtStdToggle->setButtonText("-");
    addAndMakeVisible(dirtStdToggle.get());
    dirtStdToggle->setVisible(false);
    dirtStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    dirtStdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        
        switch (stdState) {
            case 0: dirtStdToggle->setButtonText("-"); break;
            case 1: dirtStdToggle->setButtonText("t"); break;
            case 2: dirtStdToggle->setButtonText("."); break;
        }
    };
    
    // Create step dice button (EXACT same positioning as AutoPan)
    dirtStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(dirtStepDiceButton.get());
    dirtStepDiceButton->setVisible(false);
    int dirtStepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    dirtStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, dirtStepDiceSize, dirtStepDiceSize);
    
    if (assets.diceLarge != nullptr) {
        dirtStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    dirtStepDiceButton->onClick = [this]() {
        DBG("[UI] Dirt step dice button clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getDirtSafeSnapshot(step);
            
            if (!dirtKnobLocked[0]) snapshot.dirt.drive = juce::Random::getSystemRandom().nextFloat() * 36.0f;
            if (!dirtKnobLocked[1]) snapshot.dirt.color = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            if (!dirtKnobLocked[2]) snapshot.dirt.asym = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            if (!dirtKnobLocked[3]) snapshot.dirt.texture = juce::Random::getSystemRandom().nextFloat();
            if (!dirtKnobLocked[4]) snapshot.dirt.lowCut = 20.0f + juce::Random::getSystemRandom().nextFloat() * 280.0f;
            if (!dirtKnobLocked[5]) snapshot.dirt.highCut = 3000.0f + juce::Random::getSystemRandom().nextFloat() * 19000.0f;
            if (!dirtKnobLocked[6]) snapshot.dirt.tone = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            if (!dirtKnobLocked[7]) snapshot.dirt.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setDirtStepSnapshot(step, snapshot);
        }
        
        int currentStep = processorRef.getDirtCurrentStep();
        StepSnapshot currentSnapshot = processorRef.getDirtSafeSnapshot(currentStep);
        if (dirtKnobs[0]) dirtKnobs[0]->setValue(currentSnapshot.dirt.drive, juce::sendNotification);
        if (dirtKnobs[1]) dirtKnobs[1]->setValue(currentSnapshot.dirt.color, juce::sendNotification);
        if (dirtKnobs[2]) dirtKnobs[2]->setValue(currentSnapshot.dirt.asym, juce::sendNotification);
        if (dirtKnobs[3]) dirtKnobs[3]->setValue(currentSnapshot.dirt.texture, juce::sendNotification);
        if (dirtKnobs[4]) dirtKnobs[4]->setValue(currentSnapshot.dirt.lowCut, juce::sendNotification);
        if (dirtKnobs[5]) dirtKnobs[5]->setValue(currentSnapshot.dirt.highCut, juce::sendNotification);
        if (dirtKnobs[6]) dirtKnobs[6]->setValue(currentSnapshot.dirt.tone, juce::sendNotification);
        if (dirtKnobs[7]) dirtKnobs[7]->setValue(currentSnapshot.dirt.mix, juce::sendNotification);
    };
    
    // Create step power button (EXACT same positioning as AutoPan)
    dirtStepPowerButton = std::make_unique<juce::DrawableButton>("dirtStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(dirtStepPowerButton.get());
    dirtStepPowerButton->setVisible(false);
    
    const int powerButtonSize = 40;
    dirtStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - powerButtonSize - 5 + 15 - 5 - 1, 
                                   sequencerArea.getY() - 5 - 40 + 25 + 5, powerButtonSize, powerButtonSize);
    
    dirtStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    dirtStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn != nullptr) {
        dirtStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    dirtStepPowerButton->setClickingTogglesState(true);
    dirtStepPowerButton->setToggleState(dirtStepAreaEnabled, juce::dontSendNotification);
    dirtStepPowerButton->onClick = [this]() {
        dirtStepAreaEnabled = dirtStepPowerButton->getToggleState();
        DBG("[UI] Dirt step area power: " << (dirtStepAreaEnabled ? "ON" : "OFF"));
        
        if (!dirtStepAreaEnabled) {
            processorRef.setDirtSequencerEnabled(false);
        } else {
            processorRef.setDirtSequencerEnabled(true);
        }
        
        updateDirtStepAreaVisibility();
        repaint();
    };
    
    DBG("[UI] Dirt sequencer area setup complete");
}

void PluginEditor::setupDirtAllStepsToggle()
{
    DBG("[UI] Setting up Dirt All Steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    dirtAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(dirtAllStepsToggle.get());
    dirtAllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    dirtAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                  effectArea.getY() - 1, buttonSize, buttonSize);
    
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr) {
        static_cast<AllStepsToggleButton*>(dirtAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    dirtAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    dirtAllStepsToggle->onClick = [this]() {
        dirtAllStepsEnabled = dirtAllStepsToggle->getToggleState();
        DBG("[UI] Dirt All Steps toggle: " << (dirtAllStepsEnabled ? "ON" : "OFF"));
    };
    
    dirtAllStepsLabel = std::make_unique<juce::Label>();
    dirtAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    dirtAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    dirtAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    dirtAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dirtAllStepsLabel.get());
    dirtAllStepsLabel->setVisible(false);
    dirtAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                effectArea.getY() + 1, 80, 24);
    
    DBG("[UI] Dirt All Steps toggle setup complete");
}


// AutoPan helper methods
void PluginEditor::randomizeAutoPanKnobValues()
{
    DBG("[UI] Randomizing AutoPan knob values...");
    
    for (int i = 0; i < 6; ++i)
    {
        if (autopanKnobLocked[i]) {
            continue; // Skip locked knobs
        }
        
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        
        // Special handling for specific knobs
        switch (i) {
            case 1: // Phase (0-360 degrees)
                randomValue = juce::Random::getSystemRandom().nextFloat() * 360.0f;
                break;
            case 2: // Wave Type (0-4) - normalize to knob range
                randomValue = (float)(juce::Random::getSystemRandom().nextInt(5));
                break;
            case 4: // Inverted (0 or 1)
                randomValue = (float)(juce::Random::getSystemRandom().nextInt(2));
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

// Dirt page helper methods
void PluginEditor::randomizeDirtKnobValues()
{
    DBG("[UI] Randomizing Dirt knob values...");
    
    for (int i = 0; i < 8; ++i)
    {
        if (dirtKnobLocked[i]) {
            continue; // Skip locked knobs
        }
        
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        
        switch (i) {
            case 0: // Drive (0-36 dB)
                randomValue = juce::Random::getSystemRandom().nextFloat() * 36.0f;
                break;
            case 1: // Color (-1 to +1)
                randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                break;
            case 2: // Asym (-1 to +1)
                randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                break;
            case 3: // Texture (0-1)
                randomValue = juce::Random::getSystemRandom().nextFloat();
                break;
            case 4: // Low-Cut (20-300 Hz)
                randomValue = 20.0f + juce::Random::getSystemRandom().nextFloat() * 280.0f;
                break;
            case 5: // High-Cut (3000-22000 Hz)
                randomValue = 3000.0f + juce::Random::getSystemRandom().nextFloat() * 19000.0f;
                break;
            case 6: // Tone (-1 to +1)
                randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                break;
            case 7: // Mix (0-1)
                randomValue = juce::Random::getSystemRandom().nextFloat();
                break;
        }
        
        if (dirtKnobs[i]) {
            dirtKnobs[i]->setValue(randomValue, juce::sendNotification);
        }
    }
    
    DBG("[UI] All Dirt knob values randomized");
}

void PluginEditor::randomizeIndividualDirtKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || dirtKnobLocked[knobIndex])
        return;
    
    float randomValue = juce::Random::getSystemRandom().nextFloat();
    
    switch (knobIndex) {
        case 0: randomValue = juce::Random::getSystemRandom().nextFloat() * 36.0f; break;
        case 1: randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; break;
        case 2: randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; break;
        case 3: randomValue = juce::Random::getSystemRandom().nextFloat(); break;
        case 4: randomValue = 20.0f + juce::Random::getSystemRandom().nextFloat() * 280.0f; break;
        case 5: randomValue = 3000.0f + juce::Random::getSystemRandom().nextFloat() * 19000.0f; break;
        case 6: randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; break;
        case 7: randomValue = juce::Random::getSystemRandom().nextFloat(); break;
    }
    
    if (dirtKnobs[knobIndex]) {
        dirtKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
    }
}

void PluginEditor::onDirtStepButtonClicked(int stepIndex)
{
    DBG("[UI] Dirt step button " << stepIndex << " clicked");
    
    // Save current step's snapshot before switching
    int currentStep = dirtUiSelectedStep;
    if (currentStep >= 0 && currentStep < 16) {
        StepSnapshot currentSnapshot;
        if (dirtKnobs[0]) currentSnapshot.dirt.drive = dirtKnobs[0]->getValue();
        if (dirtKnobs[1]) currentSnapshot.dirt.color = dirtKnobs[1]->getValue();
        if (dirtKnobs[2]) currentSnapshot.dirt.asym = dirtKnobs[2]->getValue();
        if (dirtKnobs[3]) currentSnapshot.dirt.texture = dirtKnobs[3]->getValue();
        if (dirtKnobs[4]) currentSnapshot.dirt.lowCut = dirtKnobs[4]->getValue();
        if (dirtKnobs[5]) currentSnapshot.dirt.highCut = dirtKnobs[5]->getValue();
        if (dirtKnobs[6]) currentSnapshot.dirt.tone = dirtKnobs[6]->getValue();
        if (dirtKnobs[7]) currentSnapshot.dirt.mix = dirtKnobs[7]->getValue();
        
        processorRef.setDirtStepSnapshot(currentStep, currentSnapshot);
    }
    
    // Switch to new step
    dirtUiSelectedStep = stepIndex;
    processorRef.setDirtSelectedStep(stepIndex);
    
    // Load new step's snapshot into knobs
    StepSnapshot newSnapshot = processorRef.getDirtSafeSnapshot(stepIndex);
    if (dirtKnobs[0]) dirtKnobs[0]->setValue(newSnapshot.dirt.drive, juce::sendNotification);
    if (dirtKnobs[1]) dirtKnobs[1]->setValue(newSnapshot.dirt.color, juce::sendNotification);
    if (dirtKnobs[2]) dirtKnobs[2]->setValue(newSnapshot.dirt.asym, juce::sendNotification);
    if (dirtKnobs[3]) dirtKnobs[3]->setValue(newSnapshot.dirt.texture, juce::sendNotification);
    if (dirtKnobs[4]) dirtKnobs[4]->setValue(newSnapshot.dirt.lowCut, juce::sendNotification);
    if (dirtKnobs[5]) dirtKnobs[5]->setValue(newSnapshot.dirt.highCut, juce::sendNotification);
    if (dirtKnobs[6]) dirtKnobs[6]->setValue(newSnapshot.dirt.tone, juce::sendNotification);
    if (dirtKnobs[7]) dirtKnobs[7]->setValue(newSnapshot.dirt.mix, juce::sendNotification);
    
    updateDirtSequencerUI();
    
    DBG("[UI] Switched to Dirt step " << stepIndex);
}

void PluginEditor::updateDirtSequencerUI()
{
    int selectedStep = dirtUiSelectedStep;
    int playingStep = processorRef.getDirtCurrentStep();
    const int stepsUsed = processorRef.getDirtSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (dirtStepButtons[i] != nullptr) {
            dirtStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getDirtSeqState().enabled.load();
            dirtStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            bool shouldBeEnabled = i < stepsUsed;
            dirtStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't overwrite if user is editing)
    if (dirtStepAmountLabel != nullptr && !dirtStepAmountLabel->hasKeyboardFocus(true)) {
        dirtStepAmountLabel->setText(juce::String(stepsUsed), false); // TextEditor uses bool, not notification enum
    }
    
    if (dirtRateDropdown != nullptr) {
        int divisionIndex = processorRef.getDirtSeqState().divisionIndex.load();
        dirtRateDropdown->setSelectedId(divisionIndex + 1);
    }
}

void PluginEditor::updateAutoPanSequencerUI()
{
    // Update AutoPan step button selection and playing states (EXACT same logic as Delay sequencer)
    int selectedStep = autopanUiSelectedStep;  // UI selected step for editing
    int playingStep = processorRef.getAutoPanCurrentStep();  // Read from audio thread (same as Delay sequencer)
    const int stepsUsed = processorRef.getAutoPanSeqState().stepsUsed.load();
    
    static int lastPlayingStep = -1;
    if (playingStep != lastPlayingStep) {
        DBG("[UI] AutoPan playingStep changed: " << lastPlayingStep << " -> " << playingStep);
        lastPlayingStep = playingStep;
    }
    
    for (int i = 0; i < 16; ++i) {
        if (autopanStepButtons[i] != nullptr) {
            // Selected step (clicked for editing) shows SVG
            autopanStepButtons[i]->setSelected(i == selectedStep);
            // Show playing highlight only if sequencer is enabled (EXACT same as Delay sequencer)
            bool sequencerEnabled = processorRef.getAutoPanSeqState().enabled.load();
            autopanStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            // Grey out inactive steps beyond stepsUsed
            bool shouldBeEnabled = i < stepsUsed;
            autopanStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't overwrite if user is editing)
    if (autopanStepAmountLabel != nullptr && !autopanStepAmountLabel->hasKeyboardFocus(true)) {
        autopanStepAmountLabel->setText(juce::String(stepsUsed), false); // TextEditor uses bool, not notification enum
    }
    
    // Update rate dropdown
    if (autopanRateDropdown != nullptr) {
        int divisionIndex = processorRef.getAutoPanSeqState().divisionIndex.load();
        autopanRateDropdown->setSelectedId(divisionIndex + 1);
    }
}

void PluginEditor::updateAutoPanStepAreaVisibility()
{
    // Grey out AutoPan step area components (not including All Steps toggle/label - those are in effects area)
    float alpha = autopanStepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i)
    {
        if (autopanStepButtons[i]) { 
            autopanStepButtons[i]->setAlpha(alpha); 
            autopanStepButtons[i]->setEnabled(autopanStepAreaEnabled);
        }
    }
    
    if (autopanStepTitle) autopanStepTitle->setAlpha(alpha);
    if (autopanStepAmountLabel) autopanStepAmountLabel->setAlpha(alpha); // Keep enabled for editing even when sequencer is off
    if (autopanRateDropdown) { autopanRateDropdown->setAlpha(alpha); autopanRateDropdown->setEnabled(autopanStepAreaEnabled); }
    if (autopanStdToggle) { autopanStdToggle->setAlpha(alpha); autopanStdToggle->setEnabled(autopanStepAreaEnabled); }
    if (autopanStepDiceButton) { autopanStepDiceButton->setAlpha(alpha); autopanStepDiceButton->setEnabled(autopanStepAreaEnabled); }
    if (autopanStepPowerButton) autopanStepPowerButton->setAlpha(autopanStepAreaEnabled ? 1.0f : 0.3f);
    // Note: All Steps toggle/label are NOT controlled here - they're in the effects area
}

//==============================================================================
// Chorus Page Setup Methods (TODO: Full implementation coming)
//==============================================================================

void PluginEditor::setupChorusKnobs()
{
    DBG("[UI] Setting up Chorus knobs...");

    // Chorus knob names (8 knobs) - new order to match ChorusEngine
    std::vector<juce::String> chorusKnobNames = {
        "Delay", "Rate", "Depth", "Feedback", "Voices", "Width", "Shape", "Mix"
    };

    // Effect area bounds (EXACT same as Dirt page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    for (int i = 0; i < 8; ++i)
    {
        chorusKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(chorusKnobs[i].get());
        chorusKnobs[i]->setVisible(false);

        // Set parameter ranges based on knob index (new ChorusEngine parameters)
        switch (i) {
            case 0: // Delay (ms) - base delay time
                chorusKnobs[i]->setRange(5.0, 50.0, 0.01);
                chorusKnobs[i]->setValue(18.0);
                break;
            case 1: // Rate (Hz) - LFO rate
                chorusKnobs[i]->setRange(0.02, 8.0, 0.01);
                chorusKnobs[i]->setValue(0.8);
                break;
            case 2: // Depth (ms) - modulation amplitude
                chorusKnobs[i]->setRange(0.0, 12.0, 0.01);
                chorusKnobs[i]->setValue(5.0);
                break;
            case 3: // Feedback (0-1) - feedback amount
                chorusKnobs[i]->setRange(0.0, 0.9, 0.01);
                chorusKnobs[i]->setValue(0.15);
                break;
            case 4: // Voices (2-8) - number of voices
                chorusKnobs[i]->setRange(2.0, 8.0, 1.0);
                chorusKnobs[i]->setValue(4.0);
                break;
            case 5: // Width (0-1) - stereo width
                chorusKnobs[i]->setRange(0.0, 1.0, 0.01);
                chorusKnobs[i]->setValue(0.85);
                break;
            case 6: // Shape (0-1) - wave shape: 0=sin, 0.5=tri, 1=soft square
                chorusKnobs[i]->setRange(0.0, 1.0, 0.01);
                chorusKnobs[i]->setValue(0.25);
                break;
            case 7: // Mix (0-1) - dry/wet mix
                chorusKnobs[i]->setRange(0.0, 1.0, 0.01);
                chorusKnobs[i]->setValue(0.5);
                break;
        }

        chorusKnobs[i]->onValueChange = [this, i]() {
            if (chorusKnobs[i] != nullptr) {
                updateChorusParameterFromKnob(i);

                if (chorusAllStepsEnabled) {
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getChorusSafeSnapshot(step);
                        float value = chorusKnobs[i]->getValue();
                        switch (i) {
                            case 0: snapshot.chorus.delayTime = value; break;  // Delay
                            case 1: snapshot.chorus.rate = value; break;        // Rate
                            case 2: snapshot.chorus.depth = value; break;       // Depth
                            case 3: snapshot.chorus.feedback = value; break;    // Feedback
                            case 4: snapshot.chorus.voices = value; break;      // Voices
                            case 5: snapshot.chorus.width = value; break;       // Width
                            case 6: snapshot.chorus.tone = value; break;        // Shape
                            case 7: snapshot.chorus.mix = value; break;         // Mix
                        }
                        processorRef.setChorusStepSnapshot(step, snapshot);
                    }
                }
            }
        };

        if (assets.knobRing != nullptr)
            chorusKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            chorusKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);

        if (i < 4)
            y -= 23;
        else
            y -= 1;

        chorusKnobs[i]->setBounds(x, y, knobSize, knobSize);

        // Create label
        chorusKnobLabels[i] = std::make_unique<juce::Label>();
        chorusKnobLabels[i]->setText(chorusKnobNames[i], juce::dontSendNotification);
        chorusKnobLabels[i]->setFont(juce::Font(12.0f, juce::Font::bold));
        chorusKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        chorusKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(chorusKnobLabels[i].get());
        chorusKnobLabels[i]->setVisible(false);
        chorusKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);

        // Create value label
        chorusValueLabels[i] = std::make_unique<juce::Label>();
        chorusValueLabels[i]->setText("0", juce::dontSendNotification);
        chorusValueLabels[i]->setFont(juce::Font(10.0f, juce::Font::plain));
        chorusValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        chorusValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(chorusValueLabels[i].get());
        chorusValueLabels[i]->setVisible(false);
        chorusValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);

        // Create indicator bar
        chorusIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(chorusIndicatorBars[i].get());
        chorusIndicatorBars[i]->setVisible(false);
        chorusIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        chorusIndicatorBars[i]->setValue(0.5f);

        // Create dice button (hidden like Dirt page)
        chorusDiceButtons[i] = std::make_unique<CustomDiceButton>();
        chorusDiceButtons[i]->onClick = [this, i]() { randomizeIndividualChorusKnob(i); };

        // Create lock button (EXACT same as Dirt page)
        chorusLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(chorusLockButtons[i].get());
        chorusLockButtons[i]->setVisible(false);
        
        const int diceSize = 10;
        const int diceSpacing = 5;
        
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(chorusKnobNames[i]);
        
        int lockX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
        int lockY = y - 10;
        
        chorusLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);
        
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            chorusLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        
        chorusLockButtons[i]->setToggleState(chorusKnobLocked[i], juce::dontSendNotification);
        chorusLockButtons[i]->onClick = [this, i]() {
            chorusKnobLocked[i] = chorusLockButtons[i]->getToggleState();
            DBG("[UI] Chorus knob " << i << " lock: " << (chorusKnobLocked[i] ? "LOCKED" : "UNLOCKED"));
        };
    }

    // Create parameter attachments to connect knobs to APVTS
    std::vector<juce::String> chorusParamIds = {
        "chorusDelayMs", "chorusRateHz", "chorusDepthMs", "chorusFeedback", 
        "chorusVoices", "chorusWidth", "chorusShape", "chorusMix"
    };
    
    for (int i = 0; i < 8; ++i) {
        chorusAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), chorusParamIds[i], *chorusKnobs[i]);
    }

    DBG("[UI] Chorus knobs setup complete");
}

void PluginEditor::setupChorusEffectsArea()
{
    DBG("[UI] Setting up Chorus effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title
    chorusEffectsTitle = std::make_unique<juce::Label>();
    chorusEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    chorusEffectsTitle->setFont(juce::Font(27.648f, juce::Font::bold));
    chorusEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    chorusEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(chorusEffectsTitle.get());
    chorusEffectsTitle->setVisible(false);
    chorusEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Create dice button
    chorusDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(chorusDiceButton.get());
    chorusDiceButton->setVisible(false);
    
    const int diceSize = 32;
    chorusDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    
    if (assets.diceLarge != nullptr)
    {
        chorusDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    chorusDiceButton->onClick = [this]() { randomizeChorusKnobValues(); };
    
    // Create FX power button
    chorusFxPowerButton = std::make_unique<juce::DrawableButton>("chorusFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(chorusFxPowerButton.get());
    chorusFxPowerButton->setVisible(false);

    const int buttonSize = 46;
    chorusFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                 effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);

    chorusFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    chorusFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);

    if (assets.fxPowerOn != nullptr)
    {
        chorusFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }

    chorusFxPowerButton->setClickingTogglesState(true);
    chorusFxPowerButton->setToggleState(chorusFxAreaEnabled, juce::dontSendNotification);
    chorusFxPowerButton->onClick = [this]() {
        chorusFxAreaEnabled = chorusFxPowerButton->getToggleState();
        DBG("[UI] Chorus FX power: " << (chorusFxAreaEnabled ? "ON" : "OFF"));
        
        auto* chorusEnabledParam = processorRef.getAPVTS().getParameter("chorusEnabled");
        if (chorusEnabledParam) {
            chorusEnabledParam->setValueNotifyingHost(chorusFxAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateChorusFxAreaVisibility();
        repaint();
    };
    
    DBG("[UI] Chorus effects area setup complete");
}

void PluginEditor::setupChorusSequencerArea()
{
    DBG("[UI] Setting up Chorus sequencer area...");
    
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    chorusStepTitle = std::make_unique<juce::Label>();
    chorusStepTitle->setText("STEP", juce::dontSendNotification);
    chorusStepTitle->setFont(juce::Font(22.118f, juce::Font::bold));
    chorusStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    chorusStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(chorusStepTitle.get());
    chorusStepTitle->setVisible(false);
    chorusStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step buttons (2 rows of 8)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        chorusStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(chorusStepButtons[i].get());
        chorusStepButtons[i]->setVisible(false);
        
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        chorusStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        if (assets.stepActive) {
            chorusStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive) {
            chorusStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        chorusStepButtons[i]->onClick = [this, i]() { onChorusStepButtonClicked(i); };
    }
    
    // Create step amount editor
    struct ChorusDebugTextEditor : public juce::TextEditor {
        void mouseDown(const juce::MouseEvent& e) override {
            DBG("[UI] Chorus step amount mouseDown detected!");
            juce::TextEditor::mouseDown(e);
        }
        void focusGained(FocusChangeType cause) override {
            DBG("[UI] Chorus step amount focusGained!");
            juce::TextEditor::focusGained(cause);
        }
    };
    chorusStepAmountLabel = std::make_unique<ChorusDebugTextEditor>();
    chorusStepAmountLabel->setText("16");
    chorusStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    chorusStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    chorusStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    chorusStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    chorusStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    chorusStepAmountLabel->setJustification(juce::Justification::centred);
    chorusStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    chorusStepAmountLabel->setIndents(0, 0);
    chorusStepAmountLabel->setInputRestrictions(2, "0123456789");
    chorusStepAmountLabel->setWantsKeyboardFocus(true);
    chorusStepAmountLabel->setMouseClickGrabsKeyboardFocus(true);
    chorusStepAmountLabel->setCaretVisible(true);
    chorusStepAmountLabel->setPopupMenuEnabled(false);
    chorusStepAmountLabel->setScrollbarsShown(false);
    chorusStepAmountLabel->setMultiLine(false);
    chorusStepAmountLabel->setReturnKeyStartsNewLine(false);
    chorusStepAmountLabel->setInterceptsMouseClicks(true, false);
    chorusStepAmountLabel->setAlwaysOnTop(true);
    chorusStepAmountLabel->onTextChange = [this]() {
        DBG("[UI] Chorus step amount text changed to: " << chorusStepAmountLabel->getText());
    };
    chorusStepAmountLabel->onReturnKey = [this]() {
        DBG("[UI] Chorus step amount Return key pressed");
        if (chorusStepAmountLabel != nullptr) {
            int value = juce::jlimit(1, 16, chorusStepAmountLabel->getText().getIntValue());
            processorRef.setChorusStepsUsed(value);
            chorusStepAmountLabel->setText(juce::String(value), false);
            updateChorusSequencerUI();
            chorusStepAmountLabel->giveAwayKeyboardFocus();
        }
    };
    chorusStepAmountLabel->onFocusLost = [this]() {
        if (chorusStepAmountLabel != nullptr) {
            int value = juce::jlimit(1, 16, chorusStepAmountLabel->getText().getIntValue());
            processorRef.setChorusStepsUsed(value);
            chorusStepAmountLabel->setText(juce::String(value), false);
            updateChorusSequencerUI();
        }
    };
    addAndMakeVisible(chorusStepAmountLabel.get());
    chorusStepAmountLabel->setVisible(false);
    chorusStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    chorusStepAmountLabel->setAlwaysOnTop(true);
    
    // Create rate dropdown
    chorusRateDropdown = std::make_unique<juce::ComboBox>();
    chorusRateDropdown->addItem("4", 1);
    chorusRateDropdown->addItem("2", 2);
    chorusRateDropdown->addItem("1", 3);
    chorusRateDropdown->addItem("1/2", 4);
    chorusRateDropdown->addItem("1/4", 5);
    chorusRateDropdown->addItem("1/8", 6);
    chorusRateDropdown->addItem("1/16", 7);
    chorusRateDropdown->addItem("1/32", 8);
    chorusRateDropdown->setSelectedId(6);
    
    const int processorDivIdx = processorRef.getChorusSeqState().divisionIndex.load();
    chorusRateDropdown->setSelectedId(processorDivIdx + 1);
    
    chorusRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    chorusRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    chorusRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    chorusRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    chorusRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    chorusRateDropdown->onChange = [this]() {
        if (chorusRateDropdown != nullptr) {
            const int selected = chorusRateDropdown->getSelectedId();
            if (selected >= 1 && selected <= 8) {
                const int newDivisionIndex = juce::jlimit(0, 7, selected - 1);
                processorRef.setChorusDivisionIndex(newDivisionIndex);
                updateChorusSequencerUI();
            }
        }
    };
    addAndMakeVisible(chorusRateDropdown.get());
    chorusRateDropdown->setVisible(false);
    chorusRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    // Create STD toggle
    chorusStdToggle = std::make_unique<CircularToggleButton>();
    chorusStdToggle->setButtonText("-");
    addAndMakeVisible(chorusStdToggle.get());
    chorusStdToggle->setVisible(false);
    chorusStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    chorusStdToggle->onClick = [this]() {
        static int stdState = 0;
        stdState = (stdState + 1) % 3;
        
        switch (stdState) {
            case 0: chorusStdToggle->setButtonText("-"); break;
            case 1: chorusStdToggle->setButtonText("t"); break;
            case 2: chorusStdToggle->setButtonText("."); break;
        }
    };
    
    // Create step dice button
    chorusStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(chorusStepDiceButton.get());
    chorusStepDiceButton->setVisible(false);
    int chorusStepDiceSize = static_cast<int>(35 * 0.7);
    chorusStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, chorusStepDiceSize, chorusStepDiceSize);
    
    if (assets.diceLarge != nullptr) {
        chorusStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    chorusStepDiceButton->onClick = [this]() {
        DBG("[UI] Chorus step dice button clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getChorusSafeSnapshot(step);
            
            if (!chorusKnobLocked[0]) snapshot.chorus.delayTime = 5.0f + juce::Random::getSystemRandom().nextFloat() * 45.0f;  // Delay 5-50ms
            if (!chorusKnobLocked[1]) snapshot.chorus.rate = 0.02f + juce::Random::getSystemRandom().nextFloat() * 7.98f;        // Rate 0.02-8Hz
            if (!chorusKnobLocked[2]) snapshot.chorus.depth = juce::Random::getSystemRandom().nextFloat() * 12.0f;               // Depth 0-12ms
            if (!chorusKnobLocked[3]) snapshot.chorus.feedback = juce::Random::getSystemRandom().nextFloat() * 0.9f;             // Feedback 0-0.9
            if (!chorusKnobLocked[4]) snapshot.chorus.voices = 2.0f + juce::Random::getSystemRandom().nextFloat() * 6.0f;        // Voices 2-8
            if (!chorusKnobLocked[5]) snapshot.chorus.width = juce::Random::getSystemRandom().nextFloat();                       // Width 0-1
            if (!chorusKnobLocked[6]) snapshot.chorus.tone = juce::Random::getSystemRandom().nextFloat();                        // Shape 0-1
            if (!chorusKnobLocked[7]) snapshot.chorus.mix = juce::Random::getSystemRandom().nextFloat();                         // Mix 0-1
            
            processorRef.setChorusStepSnapshot(step, snapshot);
        }
        
        int currentStep = processorRef.getChorusCurrentStep();
        StepSnapshot currentSnapshot = processorRef.getChorusSafeSnapshot(currentStep);
        if (chorusKnobs[0]) chorusKnobs[0]->setValue(currentSnapshot.chorus.rate, juce::sendNotification);
        if (chorusKnobs[1]) chorusKnobs[1]->setValue(currentSnapshot.chorus.depth, juce::sendNotification);
        if (chorusKnobs[2]) chorusKnobs[2]->setValue(currentSnapshot.chorus.voices, juce::sendNotification);
        if (chorusKnobs[3]) chorusKnobs[3]->setValue(currentSnapshot.chorus.delayTime, juce::sendNotification);
        if (chorusKnobs[4]) chorusKnobs[4]->setValue(currentSnapshot.chorus.feedback, juce::sendNotification);
        if (chorusKnobs[5]) chorusKnobs[5]->setValue(currentSnapshot.chorus.width, juce::sendNotification);
        if (chorusKnobs[6]) chorusKnobs[6]->setValue(currentSnapshot.chorus.tone, juce::sendNotification);
        if (chorusKnobs[7]) chorusKnobs[7]->setValue(currentSnapshot.chorus.mix, juce::sendNotification);
    };
    
    // Create step power button
    chorusStepPowerButton = std::make_unique<juce::DrawableButton>("chorusStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(chorusStepPowerButton.get());
    chorusStepPowerButton->setVisible(false);
    
    const int powerButtonSize = 40;
    chorusStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - powerButtonSize - 5 + 15 - 5 - 1, 
                                   sequencerArea.getY() - 5 - 40 + 25 + 5, powerButtonSize, powerButtonSize);
    
    chorusStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    chorusStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn != nullptr) {
        chorusStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    chorusStepPowerButton->setClickingTogglesState(true);
    chorusStepPowerButton->setToggleState(chorusStepAreaEnabled, juce::dontSendNotification);
    chorusStepPowerButton->onClick = [this]() {
        chorusStepAreaEnabled = chorusStepPowerButton->getToggleState();
        DBG("[UI] Chorus step area power: " << (chorusStepAreaEnabled ? "ON" : "OFF"));
        
        if (!chorusStepAreaEnabled) {
            processorRef.setChorusSequencerEnabled(false);
        } else {
            processorRef.setChorusSequencerEnabled(true);
        }
        
        updateChorusStepAreaVisibility();
        repaint();
    };
    
    DBG("[UI] Chorus sequencer area setup complete");
}

void PluginEditor::setupChorusAllStepsToggle()
{
    DBG("[UI] Setting up Chorus All Steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    chorusAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(chorusAllStepsToggle.get());
    chorusAllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    chorusAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                    effectArea.getY() - 1, buttonSize, buttonSize);
    
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr) {
        static_cast<AllStepsToggleButton*>(chorusAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    chorusAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    chorusAllStepsToggle->onClick = [this]() {
        chorusAllStepsEnabled = chorusAllStepsToggle->getToggleState();
        DBG("[UI] Chorus All Steps toggle: " << (chorusAllStepsEnabled ? "ON" : "OFF"));
    };
    
    chorusAllStepsLabel = std::make_unique<juce::Label>();
    chorusAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    chorusAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    chorusAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    chorusAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(chorusAllStepsLabel.get());
    chorusAllStepsLabel->setVisible(false);
    chorusAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                    effectArea.getY() + 1, 80, 24);
    
    DBG("[UI] Chorus All Steps toggle setup complete");
}

void PluginEditor::updateChorusFxAreaVisibility()
{
    float alpha = chorusFxAreaEnabled ? 1.0f : 0.3f;

    if (chorusEffectsTitle) chorusEffectsTitle->setAlpha(alpha);
    if (chorusDiceButton) { chorusDiceButton->setAlpha(alpha); chorusDiceButton->setEnabled(chorusFxAreaEnabled); }

    for (int i = 0; i < 8; ++i) {
        if (chorusKnobs[i]) { chorusKnobs[i]->setAlpha(alpha); chorusKnobs[i]->setEnabled(chorusFxAreaEnabled); }
        if (chorusKnobLabels[i]) chorusKnobLabels[i]->setAlpha(alpha);
        if (chorusValueLabels[i]) chorusValueLabels[i]->setAlpha(alpha);
        if (chorusIndicatorBars[i]) chorusIndicatorBars[i]->setAlpha(alpha);
        if (chorusLockButtons[i]) { 
            chorusLockButtons[i]->setEnabled(chorusFxAreaEnabled);
            chorusLockButtons[i]->setAlpha(alpha);
        }
    }

    if (chorusAllStepsToggle) { chorusAllStepsToggle->setAlpha(alpha); chorusAllStepsToggle->setEnabled(chorusFxAreaEnabled); }
    if (chorusAllStepsLabel) chorusAllStepsLabel->setAlpha(alpha);

    if (chorusFxPowerButton) chorusFxPowerButton->setAlpha(chorusFxAreaEnabled ? 1.0f : 0.3f);
}

void PluginEditor::updateChorusStepAreaVisibility()
{
    float alpha = chorusStepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i)
    {
        if (chorusStepButtons[i]) { 
            chorusStepButtons[i]->setAlpha(alpha); 
            chorusStepButtons[i]->setEnabled(chorusStepAreaEnabled);
        }
    }
    
    if (chorusStepTitle) chorusStepTitle->setAlpha(alpha);
    if (chorusStepAmountLabel) chorusStepAmountLabel->setAlpha(alpha);
    if (chorusRateDropdown) { chorusRateDropdown->setAlpha(alpha); chorusRateDropdown->setEnabled(chorusStepAreaEnabled); }
    if (chorusStdToggle) { chorusStdToggle->setAlpha(alpha); chorusStdToggle->setEnabled(chorusStepAreaEnabled); }
    if (chorusStepDiceButton) { chorusStepDiceButton->setAlpha(alpha); chorusStepDiceButton->setEnabled(chorusStepAreaEnabled); }
    if (chorusStepPowerButton) chorusStepPowerButton->setAlpha(chorusStepAreaEnabled ? 1.0f : 0.3f);
}

void PluginEditor::randomizeChorusKnobValues()
{
    for (int i = 0; i < 8; ++i) {
        if (!chorusKnobLocked[i] && chorusKnobs[i] != nullptr) {
            randomizeIndividualChorusKnob(i);
        }
    }
}

void PluginEditor::randomizeIndividualChorusKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || chorusKnobs[knobIndex] == nullptr) return;
    if (chorusKnobLocked[knobIndex]) return;
    
    float randomValue = 0.0f;
    switch (knobIndex) {
        case 0: randomValue = 0.1f + juce::Random::getSystemRandom().nextFloat() * 9.9f; break;
        case 1: randomValue = juce::Random::getSystemRandom().nextFloat() * 100.0f; break;
        case 2: randomValue = 1.0f + juce::Random::getSystemRandom().nextFloat() * 3.0f; break;
        case 3: randomValue = 5.0f + juce::Random::getSystemRandom().nextFloat() * 45.0f; break;
        case 4: randomValue = juce::Random::getSystemRandom().nextFloat() * 80.0f; break;
        case 5: randomValue = juce::Random::getSystemRandom().nextFloat() * 200.0f; break;
        case 6: randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; break;
        case 7: randomValue = juce::Random::getSystemRandom().nextFloat() * 100.0f; break;
    }
    
    chorusKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
}

void PluginEditor::updateChorusParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || chorusKnobs[knobIndex] == nullptr) return;
    
    float value = chorusKnobs[knobIndex]->getValue();
    processorRef.updateChorusCurrentStepSnapshot(knobIndex, value);
}

void PluginEditor::onChorusStepButtonClicked(int stepIndex)
{
    DBG("[UI] Chorus step button " << stepIndex << " clicked");
    
    int currentStep = chorusUiSelectedStep;
    if (currentStep >= 0 && currentStep < 16) {
        StepSnapshot currentSnapshot;
        if (chorusKnobs[0]) currentSnapshot.chorus.delayTime = chorusKnobs[0]->getValue();  // Delay
        if (chorusKnobs[1]) currentSnapshot.chorus.rate = chorusKnobs[1]->getValue();        // Rate
        if (chorusKnobs[2]) currentSnapshot.chorus.depth = chorusKnobs[2]->getValue();       // Depth
        if (chorusKnobs[3]) currentSnapshot.chorus.feedback = chorusKnobs[3]->getValue();    // Feedback
        if (chorusKnobs[4]) currentSnapshot.chorus.voices = chorusKnobs[4]->getValue();      // Voices
        if (chorusKnobs[5]) currentSnapshot.chorus.width = chorusKnobs[5]->getValue();       // Width
        if (chorusKnobs[6]) currentSnapshot.chorus.tone = chorusKnobs[6]->getValue();
        if (chorusKnobs[7]) currentSnapshot.chorus.mix = chorusKnobs[7]->getValue();
        
        processorRef.setChorusStepSnapshot(currentStep, currentSnapshot);
    }
    
    chorusUiSelectedStep = stepIndex;
    processorRef.setChorusSelectedStep(stepIndex);
    
    StepSnapshot newSnapshot = processorRef.getChorusSafeSnapshot(stepIndex);
    if (chorusKnobs[0]) chorusKnobs[0]->setValue(newSnapshot.chorus.delayTime, juce::sendNotification);  // Delay
    if (chorusKnobs[1]) chorusKnobs[1]->setValue(newSnapshot.chorus.rate, juce::sendNotification);        // Rate
    if (chorusKnobs[2]) chorusKnobs[2]->setValue(newSnapshot.chorus.depth, juce::sendNotification);       // Depth
    if (chorusKnobs[3]) chorusKnobs[3]->setValue(newSnapshot.chorus.feedback, juce::sendNotification);    // Feedback
    if (chorusKnobs[4]) chorusKnobs[4]->setValue(newSnapshot.chorus.voices, juce::sendNotification);      // Voices
    if (chorusKnobs[5]) chorusKnobs[5]->setValue(newSnapshot.chorus.width, juce::sendNotification);       // Width
    if (chorusKnobs[6]) chorusKnobs[6]->setValue(newSnapshot.chorus.tone, juce::sendNotification);        // Shape
    if (chorusKnobs[7]) chorusKnobs[7]->setValue(newSnapshot.chorus.mix, juce::sendNotification);         // Mix
    
    updateChorusSequencerUI();
    
    DBG("[UI] Switched to Chorus step " << stepIndex);
}

void PluginEditor::updateChorusSequencerUI()
{
    int selectedStep = chorusUiSelectedStep;
    int playingStep = processorRef.getChorusPlayingStep();
    const int stepsUsed = processorRef.getChorusSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (chorusStepButtons[i] != nullptr) {
            chorusStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getChorusSeqState().enabled.load();
            chorusStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
            bool shouldBeEnabled = i < stepsUsed;
            chorusStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    if (chorusStepAmountLabel != nullptr && !chorusStepAmountLabel->hasKeyboardFocus(true)) {
        chorusStepAmountLabel->setText(juce::String(stepsUsed), false);
    }
    
    if (chorusRateDropdown != nullptr) {
        int divisionIndex = processorRef.getChorusSeqState().divisionIndex.load();
        chorusRateDropdown->setSelectedId(divisionIndex + 1);
    }
}