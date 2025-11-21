#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "ui/PanIndicator.h"
#include "ui/RouterComboLookAndFeel.h"
#include "RandomizationManager.h"
#include "PresetManager.h"
#include "PresetBrowserComponent.h"
#include "GumroadLicenseManager.h"
#include "ui/GumroadLicenseDialog.h"

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
    
    // Initialize randomization manager (thread-safe)
    randomizationManager = std::make_unique<RandomizationManager>(processorRef, processorRef.getAPVTS(), this);
    
    // Initialize preset management system
    presetManager = std::make_unique<PresetManager>(processorRef, processorRef.getAPVTS());
    
    // Initialize license manager with Gumroad product ID
    licenseManager = std::make_unique<GumroadLicenseManager>("YJv8qP_umZv8fuNaDD5dQg==");
    licenseManager->loadLicenseState();
    
    // Check license on startup (after a short delay to allow UI to initialize)
    juce::Timer::callAfterDelay(1000, [this]() {
        checkLicenseOnStartup();
    });
    
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
        setupSpaceDelayAllStepsToggle();
        
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
        DBG("[UI] Initial AutoPan sequencer state synced: enabled=" + juce::String(autopanStepAreaEnabled ? 1 : 0));
        
        // Setup Dirt page components
        setupDirtKnobs();
        setupDirtEffectsArea();
        setupDirtSequencerArea();
        setupDirtAllStepsToggle();
        
        // Sync Dirt sequencer state with processor on startup
        processorRef.setDirtSequencerEnabled(dirtStepAreaEnabled);
        DBG("[UI] Initial Dirt sequencer state synced: enabled=" + juce::String(dirtStepAreaEnabled ? 1 : 0));
        
        // Initialize FX power button states from parameters
        auto* autopanEnabledParam = processorRef.getAPVTS().getRawParameterValue("autopanEnabled");
        if (autopanEnabledParam) {
            autopanFxAreaEnabled = autopanEnabledParam->load() > 0.5f;
            if (autopanFxPowerButton) {
                autopanFxPowerButton->setToggleState(autopanFxAreaEnabled, juce::dontSendNotification);
            }
            updateAutoPanFxAreaVisibility(); // Update UI to reflect initial state
            DBG("[UI] AutoPan FX power initialized: " + juce::String(autopanFxAreaEnabled ? "ON" : "OFF"));
        }
        
        auto* dirtEnabledParam = processorRef.getAPVTS().getRawParameterValue("dirtEnabled");
        if (dirtEnabledParam) {
            dirtFxAreaEnabled = dirtEnabledParam->load() > 0.5f;
            if (dirtFxPowerButton) {
                dirtFxPowerButton->setToggleState(dirtFxAreaEnabled, juce::dontSendNotification);
            }
            updateDirtFxAreaVisibility(); // Update UI to reflect initial state
            DBG("[UI] Dirt FX power initialized: " + juce::String(dirtFxAreaEnabled ? "ON" : "OFF"));
        }
        
        // Setup Chorus page components
        setupChorusKnobs();
        setupChorusEffectsArea();
        setupChorusSequencerArea();
        setupChorusAllStepsToggle();
        
        // Sync Chorus sequencer state with processor on startup
        processorRef.setChorusSequencerEnabled(chorusStepAreaEnabled);
        DBG("[UI] Initial Chorus sequencer state synced: enabled=" + juce::String(chorusStepAreaEnabled ? 1 : 0));
        
        // Setup Reverb page
        DBG("[UI] Setting up Reverb page...");
        setupReverbKnobs();
        setupReverbEffectsArea();
        setupReverbSequencerArea();
        setupReverbAllStepsToggle();
        
        // Sync Reverb sequencer state with processor on startup
        processorRef.setReverbSequencerEnabled(reverbStepAreaEnabled);
        DBG("[UI] Initial Reverb sequencer state synced: enabled=" + juce::String(reverbStepAreaEnabled ? 1 : 0));
        
        // Setup Granular page (with sequencer)
        DBG("[UI] Setting up Granular page...");
        setupGranularKnobs();
        setupGranularEffectsArea();
        setupGranularSequencerArea();
        setupGranularAllStepsToggle();
        
        // Sync Granular sequencer state with processor on startup
        processorRef.setGranularSequencerEnabled(granularStepAreaEnabled);
        DBG("[UI] Initial Granular sequencer state synced: enabled=" + juce::String(granularStepAreaEnabled ? 1 : 0));
        
        // Initialize Granular FX power button state from parameter
        auto* granEnabledParam = processorRef.getAPVTS().getRawParameterValue("granEnabled");
        if (granEnabledParam) {
            granularFxAreaEnabled = granEnabledParam->load() > 0.5f;
            if (granularFxPowerButton) {
                granularFxPowerButton->setToggleState(granularFxAreaEnabled, juce::dontSendNotification);
            }
            updateGranularFxAreaVisibility();
            DBG("[UI] Granular FX power initialized: " + juce::String(granularFxAreaEnabled ? "ON" : "OFF"));
        }
        
        // Setup Rhythm Gate page
        DBG("[UI] Setting up Slicer page...");
        setupSlicerKnobs();
        setupSlicerEffectsArea();
        setupSlicerSequencerArea();
        setupSlicerAllStepsToggle();
        
        // Sync Slicer sequencer state with processor on startup
        processorRef.setSlicerSequencerEnabled(slicerStepAreaEnabled);
        
        // Setup Dub Delay page - DEBUGGING UI CRASH
        DBG("[UI] Setting up Dub Delay page...");
        setupDubDelayKnobs();
        setupDubDelayEffectsArea();
        setupDubDelaySequencerArea();
        setupDubDelayAllStepsToggle();
        
        // Sync Dub Delay sequencer state with processor on startup
        // processorRef.setDubDelaySequencerEnabled(dubdelayStepAreaEnabled);
        DBG("[UI] Initial Slicer sequencer state synced: enabled=" + juce::String(slicerStepAreaEnabled ? 1 : 0));
        
        // Setup Redux page
        DBG("[UI] Setting up Redux page...");
        setupReduxKnobs();
        setupReduxEffectsArea();
        setupReduxSequencerArea();
        setupReduxAllStepsToggle();
        
        // Setup PhaseBloom page
        DBG("[UI] Setting up PhaseBloom page...");
        setupPhaseBloomKnobs();
        setupPhaseBloomEffectsArea();
        setupPhaseBloomSequencerArea();
        setupPhaseBloomAllStepsToggle();
        
        // Setup Formant page
        DBG("[UI] Setting up Formant page...");
        setupFormantKnobs();
        setupFormantEffectsArea();
        setupFormantSequencerArea();
        setupFormantAllStepsToggle();
        
        // Setup Filter page
        DBG("[UI] Setting up Filter page...");
        setupFilterKnobs();
        setupFilterEffectsArea();
        setupFilterSequencerArea();
        setupFilterAllStepsToggle();
        populateFilterGroup();
        
        // Setup Saturate page
        DBG("[UI] Setting up Saturate page...");
        setupSaturateKnobs();
        setupSaturateEffectsArea();
        setupSaturateSequencerArea();
        setupSaturateAllStepsToggle();
        
        // Setup Form 2 page
        DBG("[UI] Setting up Form 2 page...");
        // Form2 page removed - using Formant instead
        // setupForm2Knobs();
        // setupForm2EffectsArea();
        // setupForm2SequencerArea();
        // setupForm2AllStepsToggle();
        
        // Ensure form2Group is always hidden on startup
        for (auto* c : form2Group) {
            if (c) c->setVisible(false);
        }
        
        // Setup COMPRESS+ sliders
        DBG("[UI] Setting up COMPRESS+ page...");
        setupCompressSliders();
        
        // Populate Redux group for visibility management (same pattern as other effects)
        reduxGroup.clear();
        
        // Add all Redux components to the group
        for (int i = 0; i < 8; ++i) {
            if (reduxKnobs[i]) reduxGroup.push_back(reduxKnobs[i].get());
            if (reduxKnobLabels[i]) reduxGroup.push_back(reduxKnobLabels[i].get());
            if (reduxValueLabels[i]) reduxGroup.push_back(reduxValueLabels[i].get());
            if (reduxIndicatorBars[i]) reduxGroup.push_back(reduxIndicatorBars[i].get());
            if (reduxDiceButtons[i]) reduxGroup.push_back(reduxDiceButtons[i].get());
            if (reduxLockButtons[i]) reduxGroup.push_back(reduxLockButtons[i].get());
        }
        
        // Add other Redux components to group
        if (reduxEffectsTitle) reduxGroup.push_back(reduxEffectsTitle.get());
        if (reduxDiceButton) reduxGroup.push_back(reduxDiceButton.get());
        if (reduxFxPowerButton) reduxGroup.push_back(reduxFxPowerButton.get());
        if (reduxStepTitle) reduxGroup.push_back(reduxStepTitle.get());
        if (reduxStepPowerButton) reduxGroup.push_back(reduxStepPowerButton.get());
        if (reduxStepAmountLabel) reduxGroup.push_back(reduxStepAmountLabel.get());
        if (reduxRateDropdown) reduxGroup.push_back(reduxRateDropdown.get());
        if (reduxStdToggle) reduxGroup.push_back(reduxStdToggle.get());
        if (reduxStepDiceButton) reduxGroup.push_back(reduxStepDiceButton.get());
        if (reduxAllStepsToggle) reduxGroup.push_back(reduxAllStepsToggle.get());
        if (reduxAllStepsLabel) reduxGroup.push_back(reduxAllStepsLabel.get());
        
        // Add step buttons to group
        for (int i = 0; i < 16; ++i) {
            if (reduxStepButtons[i]) reduxGroup.push_back(reduxStepButtons[i].get());
        }
        
        DBG("[UI] Redux page setup complete");
        
        // Populate PhaseBloom group for visibility management (same pattern as other effects)
        phaseBloomGroup.clear();
        
        // Add all PhaseBloom components to the group
        for (int i = 0; i < 8; ++i) {
            if (phaseBloomKnobs[i]) phaseBloomGroup.push_back(phaseBloomKnobs[i].get());
            if (phaseBloomKnobLabels[i]) phaseBloomGroup.push_back(phaseBloomKnobLabels[i].get());
            if (phaseBloomValueLabels[i]) phaseBloomGroup.push_back(phaseBloomValueLabels[i].get());
            if (phaseBloomIndicatorBars[i]) phaseBloomGroup.push_back(phaseBloomIndicatorBars[i].get());
            if (phaseBloomDiceButtons[i]) phaseBloomGroup.push_back(phaseBloomDiceButtons[i].get());
            if (phaseBloomLockButtons[i]) phaseBloomGroup.push_back(phaseBloomLockButtons[i].get());
        }
        
        // Add other PhaseBloom components to group
        if (phaseBloomEffectsTitle) phaseBloomGroup.push_back(phaseBloomEffectsTitle.get());
        if (phaseBloomDiceButton) phaseBloomGroup.push_back(phaseBloomDiceButton.get());
        if (phaseBloomFxPowerButton) phaseBloomGroup.push_back(phaseBloomFxPowerButton.get());
        if (phaseBloomStepTitle) phaseBloomGroup.push_back(phaseBloomStepTitle.get());
        if (phaseBloomStepPowerButton) phaseBloomGroup.push_back(phaseBloomStepPowerButton.get());
        if (phaseBloomStepAmountLabel) phaseBloomGroup.push_back(phaseBloomStepAmountLabel.get());
        if (phaseBloomRateDropdown) phaseBloomGroup.push_back(phaseBloomRateDropdown.get());
        if (phaseBloomStdToggle) phaseBloomGroup.push_back(phaseBloomStdToggle.get());
        if (phaseBloomStepDiceButton) phaseBloomGroup.push_back(phaseBloomStepDiceButton.get());
        if (phaseBloomAllStepsToggle) phaseBloomGroup.push_back(phaseBloomAllStepsToggle.get());
        if (phaseBloomAllStepsLabel) phaseBloomGroup.push_back(phaseBloomAllStepsLabel.get());
        
        // Add step buttons to group
        for (int i = 0; i < 16; ++i) {
            if (phaseBloomStepButtons[i]) phaseBloomGroup.push_back(phaseBloomStepButtons[i].get());
        }
        
        DBG("[UI] PhaseBloom page setup complete");
        
        // Initialize Slicer step power button state from processor
        {
            const bool enabled = processorRef.getSlicerSeqState().enabled.load();
            slicerStepAreaEnabled = enabled;
            if (slicerStepPowerButton) {
                slicerStepPowerButton->setToggleState(enabled, juce::dontSendNotification);
            }
        }
        
        // Initialize Slicer FX power button state - start enabled
        slicerFxAreaEnabled = true;
        if (slicerFxPowerButton) {
            slicerFxPowerButton->setToggleState(true, juce::dontSendNotification);
        }
        updateSlicerFxAreaVisibility();
        updateSlicerStepAreaVisibility();
        DBG("[UI] Slicer FX power initialized: ON");
        
        // Initialize Chorus FX power button state from parameter
        auto* chorusEnabledParam = processorRef.getAPVTS().getRawParameterValue("chorusEnabled");
        if (chorusEnabledParam) {
            chorusFxAreaEnabled = chorusEnabledParam->load() > 0.5f;
            if (chorusFxPowerButton) {
                chorusFxPowerButton->setToggleState(chorusFxAreaEnabled, juce::dontSendNotification);
            }
            updateChorusFxAreaVisibility(); // Update UI to reflect initial state
            DBG("[UI] Chorus FX power initialized: " + juce::String(chorusFxAreaEnabled ? "ON" : "OFF"));
        }
        
        // Setup tab system
        setupTabSystem();
        
        // Start timer for UI updates
        startTimer(16); // ~60Hz (16ms) for ultra-smooth UI updates
        
        DBG("[UI] PluginEditor initialized with fresh start");
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    // === LAYERED TAB BACKGROUNDS ===
    // Draw ALL tab backgrounds in order (Tab1, Tab2, Tab3, Tab4)
    // This creates a layered effect where inactive tabs peek out behind the active one
    auto& router = processorRef.getEffectRouter();
    int currentSlotIndex = static_cast<int>(currentPage);
    
    // Helper lambda to get background for a given slot
    auto getBackgroundForSlot = [&](int slotIndex) -> juce::Drawable* {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slotIndex));
        int tabNumber = slotIndex + 1;
        
        switch (effect)
        {
            case EffectID::SpaceDelay:
                if (tabNumber == 1) return assets.spaceDelayBackgroundTab1.get();
                else if (tabNumber == 2) return assets.spaceDelayBackgroundTab2.get();
                else if (tabNumber == 3) return assets.spaceDelayBackgroundTab3.get();
                else if (tabNumber == 4) return assets.spaceDelayBackgroundTab4.get();
                break;
                
            case EffectID::AutoPan:
                if (tabNumber == 1) return assets.pannerBackgroundTab1.get();
                else if (tabNumber == 2) return assets.pannerBackgroundTab2.get();
                else if (tabNumber == 3) return assets.pannerBackgroundTab3.get();
                else if (tabNumber == 4) return assets.pannerBackgroundTab4.get();
                break;
                
            case EffectID::Dirt:
                if (tabNumber == 1) return assets.dirtBackgroundTab1.get();
                else if (tabNumber == 2) return assets.dirtBackgroundTab2.get();
                else if (tabNumber == 3) return assets.dirtBackgroundTab3.get();
                else if (tabNumber == 4) return assets.dirtBackgroundTab4.get();
                break;
                
            case EffectID::Chorus:
                if (tabNumber == 1) return assets.chorusBackgroundTab1.get();
                else if (tabNumber == 2) return assets.chorusBackgroundTab2.get();
                else if (tabNumber == 3) return assets.chorusBackgroundTab3.get();
                else if (tabNumber == 4) return assets.chorusBackgroundTab4.get();
                break;
                
            case EffectID::Reverb:
                if (tabNumber == 1 && assets.reverbBackgroundTab1) return assets.reverbBackgroundTab1.get();
                else if (tabNumber == 2 && assets.reverbBackgroundTab2) return assets.reverbBackgroundTab2.get();
                else if (tabNumber == 3 && assets.reverbBackgroundTab3) return assets.reverbBackgroundTab3.get();
                else if (tabNumber == 4 && assets.reverbBackgroundTab4) return assets.reverbBackgroundTab4.get();
                break;
                
            case EffectID::Granular:
                if (tabNumber == 1 && assets.granularBackgroundTab1) return assets.granularBackgroundTab1.get();
                else if (tabNumber == 2 && assets.granularBackgroundTab2) return assets.granularBackgroundTab2.get();
                else if (tabNumber == 3 && assets.granularBackgroundTab3) return assets.granularBackgroundTab3.get();
                else if (tabNumber == 4 && assets.granularBackgroundTab4) return assets.granularBackgroundTab4.get();
                break;
                
            case EffectID::Slicer:
                if (tabNumber == 1 && assets.slicerBackgroundTab1) return assets.slicerBackgroundTab1.get();
                else if (tabNumber == 2 && assets.slicerBackgroundTab2) return assets.slicerBackgroundTab2.get();
                else if (tabNumber == 3 && assets.slicerBackgroundTab3) return assets.slicerBackgroundTab3.get();
                else if (tabNumber == 4 && assets.slicerBackgroundTab4) return assets.slicerBackgroundTab4.get();
                break;
                
            case EffectID::DubDelay:
                if (tabNumber == 1 && assets.dubdelayBackgroundTab1) {
                    DBG("[PAINT] DubDelay background tab 1 loaded");
                    return assets.dubdelayBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.dubdelayBackgroundTab2) {
                    DBG("[PAINT] DubDelay background tab 2 loaded");
                    return assets.dubdelayBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.dubdelayBackgroundTab3) {
                    DBG("[PAINT] DubDelay background tab 3 loaded");
                    return assets.dubdelayBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.dubdelayBackgroundTab4) {
                    DBG("[PAINT] DubDelay background tab 4 loaded");
                    return assets.dubdelayBackgroundTab4.get();
                }
                // DBG("[PAINT] DubDelay background for tab " << tabNumber << " is null");
                break;
            case EffectID::Redux:
                if (tabNumber == 1 && assets.reduxBackgroundTab1) {
                    return assets.reduxBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.reduxBackgroundTab2) {
                    return assets.reduxBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.reduxBackgroundTab3) {
                    return assets.reduxBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.reduxBackgroundTab4) {
                    return assets.reduxBackgroundTab4.get();
                }
                break;
                
            case EffectID::PhaseBloom:
                if (tabNumber == 1 && assets.phasebloomBackgroundTab1) {
                    return assets.phasebloomBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.phasebloomBackgroundTab2) {
                    return assets.phasebloomBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.phasebloomBackgroundTab3) {
                    return assets.phasebloomBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.phasebloomBackgroundTab4) {
                    return assets.phasebloomBackgroundTab4.get();
                }
                break;
                
            case EffectID::Formant:
                if (tabNumber == 1 && assets.formBackgroundTab1) {
                    return assets.formBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.formBackgroundTab2) {
                    return assets.formBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.formBackgroundTab3) {
                    return assets.formBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.formBackgroundTab4) {
                    return assets.formBackgroundTab4.get();
                }
                break;
                
            case EffectID::Saturate:
                if (tabNumber == 1 && assets.saturateBackgroundTab1) {
                    return assets.saturateBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.saturateBackgroundTab2) {
                    return assets.saturateBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.saturateBackgroundTab3) {
                    return assets.saturateBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.saturateBackgroundTab4) {
                    return assets.saturateBackgroundTab4.get();
                }
                break;
                
            // Form2 page removed - using Formant instead (fallback to Formant backgrounds)
            case EffectID::Form2:
                // Convert Form2 to Formant backgrounds
                if (tabNumber == 1 && assets.formBackgroundTab1) {
                    return assets.formBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.formBackgroundTab2) {
                    return assets.formBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.formBackgroundTab3) {
                    return assets.formBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.formBackgroundTab4) {
                    return assets.formBackgroundTab4.get();
                }
                break;
                
            case EffectID::Filter:
                if (tabNumber == 1 && assets.filterBackgroundTab1) {
                    return assets.filterBackgroundTab1.get();
                }
                else if (tabNumber == 2 && assets.filterBackgroundTab2) {
                    return assets.filterBackgroundTab2.get();
                }
                else if (tabNumber == 3 && assets.filterBackgroundTab3) {
                    return assets.filterBackgroundTab3.get();
                }
                else if (tabNumber == 4 && assets.filterBackgroundTab4) {
                    return assets.filterBackgroundTab4.get();
                }
                break;
        }
        return nullptr;
    };
    
    // Draw all tab backgrounds cascading outward from the selected tab
    // The selected tab is drawn LAST (on top), with others cascading behind it
    // This creates a visual effect where the selected tab is most visible
    auto bounds = getLocalBounds().toFloat();
    bool hasBackground = false;
    
    // Build the drawing order based on selected tab
    // Last drawn = on top, so selected tab must be drawn LAST
    std::vector<int> drawOrder;
    
    if (currentSlotIndex == 0) // Tab 1 selected
    {
        drawOrder = {3, 2, 1, 0}; // Cascade from right to left, ending with selected tab 1 on top
    }
    else if (currentSlotIndex == 1) // Tab 2 selected
    {
        drawOrder = {3, 0, 2, 1}; // Cascade outward from tab 2, ending with selected tab 2 on top
    }
    else if (currentSlotIndex == 2) // Tab 3 selected
    {
        drawOrder = {0, 3, 1, 2}; // Cascade outward from tab 3, ending with selected tab 3 on top
    }
    else // Tab 4 selected (currentSlotIndex == 3)
    {
        drawOrder = {0, 1, 2, 3}; // Cascade from left to right, ending with selected tab 4 on top
    }
    
    // Helper lambda to get effect icon for a given slot (using new consistent icons)
    auto getEffectIconForSlot = [&](int slotIndex) -> juce::Drawable* {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slotIndex));
        switch (effect)
        {
            case EffectID::SpaceDelay: return assets.tabSpaceIcon.get();      // Space_Icon
            case EffectID::AutoPan:    return assets.tabAutoPanIcon.get();    // AutoPan_Icon
            case EffectID::Dirt:       return assets.tabDirtIconNew.get();    // Dirt_Icon
            case EffectID::Chorus:     return assets.tabChorusIconNew.get();  // Chorus_Icon
            case EffectID::Reverb:     return assets.tabHallIcon.get();       // Hall_Icon (was reverb)
            case EffectID::Granular:   return assets.tabGrainIcon.get();      // Grain_Icon
            case EffectID::Slicer:     return assets.tabSlicerIcon.get();      // Slicer_Icon
            case EffectID::DubDelay:   
                DBG("[PAINT] DubDelay icon: " << (assets.tabDubDelayIcon ? "loaded" : "null"));
                return assets.tabDubDelayIcon.get();    // DubDelay_Icon
            case EffectID::Redux:       return assets.tabReduxIcon.get();       // Redux_Icon
            case EffectID::PhaseBloom:  return assets.tabPhaseBloomIcon.get();  // PhaseBloom_Icon
            case EffectID::Formant:     return assets.tabFormantIcon.get();     // Form_Icon
            case EffectID::Saturate:    return assets.tabSaturateIcon.get();
            // Form2 page removed - using Formant icon instead
            case EffectID::Form2:       return assets.tabFormantIcon.get();      // Use Formant icon for Form2
            case EffectID::Filter:      return assets.tabFilterIcon.get();
        }
        return nullptr;
    };
    
    // Draw backgrounds and icons in the specified order
    for (int slot : drawOrder)
    {
        juce::Drawable* bg = getBackgroundForSlot(slot);
        if (bg != nullptr)
        {
            bg->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
            hasBackground = true;
        }

        // Draw the effect icon on top of its background
        juce::Drawable* icon = getEffectIconForSlot(slot);
        if (icon != nullptr)
        {
            // Position icon to align with carrot SVGs horizontally
            float tabIconX, tabIconY, tabW, tabH;
            switch (slot)
            {
                case 0: // Tab 1 (SpaceDelay)
                    tabIconX = 12.0f;
                    tabIconY = 5.0f;
                    tabW = 120.0f;
                    tabH = 44.0f;
                    break;
                case 1: // Tab 2 (Panner)
                    tabIconX = 148.0f;
                    tabIconY = 5.0f;
                    tabW = 120.0f;
                    tabH = 44.0f;
                    break;
                case 2: // Tab 3 (Dirt)
                    tabIconX = 268.0f;
                    tabIconY = 5.0f;
                    tabW = 120.0f;
                    tabH = 44.0f;
                    break;
                case 3: // Tab 4 (Chorus)
                    tabIconX = 396.0f;
                    tabIconY = 5.0f;
                    tabW = 120.0f;
                    tabH = 44.0f;
                    break;
                default:
                    continue; // Skip invalid slots
            }
            
            // Use natural icon size (250x70 from SVG viewBox) reduced by 60%, then 22% smaller, then 10% smaller
            float iconWidth = 250.0f * 0.4f * 0.78f * 0.9f;  // 60% reduction, then 22% smaller, then 10% smaller = 70.2px
            float iconHeight = 70.0f * 0.4f * 0.78f * 0.9f;  // 60% reduction, then 22% smaller, then 10% smaller = 19.66px
            float iconX = tabIconX + (tabW - iconWidth) / 2.0f - 16.0f; // Center horizontally, then move left 16px
            float iconY = 12.0f - 32.0f - 10.0f + 20.0f - 5.0f + 25.0f - 4.0f + 3.0f; // Move up 32px + 10px more, then down 20px, then up 5px, then down 25px, then up 4px, then down 3px
            
            // Create bounds at reduced size
            auto iconBounds = juce::Rectangle<float>(iconX, iconY, iconWidth, iconHeight);
            icon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
    // Fallback background if no backgrounds loaded
    if (!hasBackground)
    {
        g.fillAll(juce::Colour(0xff2a2a2a));
        DBG("[ROUTER] No backgrounds found");
    }
    
    // Only draw grid overlay and main areas if UI is visible
    if (uiVisible) {
    // Draw the grid overlay
    drawGridOverlay(g);
    
    // Draw the three main areas
    drawMainAreas(g);
    }

    // Draw knob lock icons on top of UI - ROUTER-AWARE
    // Draw icons for the effect currently assigned to the current slot
    EffectID assignedEffect = router.getEffectInSlot(static_cast<SlotID>(currentSlotIndex));
    switch (assignedEffect)
    {
        case EffectID::SpaceDelay:
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
            break;
            
        case EffectID::AutoPan:
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
            break;
            
        case EffectID::Dirt:
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
            break;
            
        case EffectID::Chorus:
            for (int i = 0; i < 8; ++i)
            {
                if (chorusLockButtons[i] != nullptr)
                {
                    auto b = chorusLockButtons[i]->getBounds().toFloat();
                    if (chorusKnobLocked[i]) {
                        if (assets.lockedIcon != nullptr)
                            assets.lockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                    } else {
                        if (assets.unlockedIcon != nullptr)
                            assets.unlockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                    }
                }
            }
            break;
            
        case EffectID::Reverb:
            for (int i = 0; i < 8; ++i)
            {
                if (reverbLockButtons[i] != nullptr)
                {
                    auto b = reverbLockButtons[i]->getBounds().toFloat();
                    if (reverbKnobLocked[i]) {
                        if (assets.lockedIcon != nullptr)
                            assets.lockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                    } else {
                        if (assets.unlockedIcon != nullptr)
                            assets.unlockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                    }
                }
            }
            break;
            
        case EffectID::Slicer:
            // Draw lock buttons for slicer knobs (6 knobs)
            for (int i = 0; i < 6; ++i)
            {
                if (slicerLockButtons[i] != nullptr)
                {
                    auto b = slicerLockButtons[i]->getBounds().toFloat();
                    if (slicerKnobLocked[i]) {
                        if (assets.lockedIcon != nullptr)
                            assets.lockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                    } else {
                        if (assets.unlockedIcon != nullptr)
                            assets.unlockedIcon->drawWithin(g, b, juce::RectanglePlacement::centred, 1.0f);
                    }
                }
            }
            
            // Draw LED strip visualization
            {
                auto* patternParam = processorRef.getAPVTS().getRawParameterValue("slicerPattern");
                int patternIdx = patternParam ? static_cast<int>(patternParam->load()) : 0;
                
                // Draw LEDs based on current pattern
                for (int i = 0; i < 16; ++i)
                {
                    if (slicerLEDStrip[i] != nullptr && slicerLEDStrip[i]->isVisible())
                    {
                        auto ledBounds = slicerLEDStrip[i]->getBounds().toFloat();
                        
                        // Get pattern value (0 or 1) for this step
                        float patternValue = 0.0f;
                        // This will be implemented based on the pattern in RhythmGateEngine
                        // For now, draw a simple on/off indicator
                        bool isOn = (i % 2 == 0); // Placeholder
                        bool isCurrent = (i == slicerCurrentStep);
                        
                        // LED color: green if on, dim gray if off, bright white if current step
                        juce::Colour ledColor = isCurrent ? juce::Colours::white : 
                                               (isOn ? juce::Colour(0xff00ff00) : juce::Colour(0xff404040));
                        
                        g.setColour(ledColor);
                        g.fillRoundedRectangle(ledBounds, 2.0f);
                        
                        // Border
                        g.setColour(juce::Colours::white.withAlpha(0.3f));
                        g.drawRoundedRectangle(ledBounds, 2.0f, 1.0f);
                    }
                }
            }
            break;
    }
    
    // Draw carrot icons on dropdowns (FX_Type_Carrot_Inactive.svg)
    if (assets.fxTypeCarrotInactive)
    {
        for (auto* selector : {effectSelector1.get(), effectSelector2.get(), 
                                effectSelector3.get(), effectSelector4.get()})
        {
            if (selector && selector->isVisible())
            {
                auto bounds = selector->getBounds().toFloat();
                assets.fxTypeCarrotInactive->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
            }
        }
    }
    
    // Draw license overlay if license is invalid
    if (licenseManager && !licenseManager->isLicenseValid())
    {
        // Draw semi-transparent overlay (no text, just blocking overlay)
        g.fillAll(juce::Colour(0xCC000000)); // Dark overlay with alpha
        
        return; // Don't draw the rest of the UI
    }
}

void PluginEditor::resized()
{
    // Tabs are positioned in setupTabSystem() - no need to reposition here
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
    
    // Draw license overlay if license is invalid
    if (licenseManager && !licenseManager->isLicenseValid())
    {
        // Draw semi-transparent overlay (no text, just blocking overlay)
        g.fillAll(juce::Colour(0xCC000000)); // Dark overlay with alpha
        
        return; // Don't draw the rest of the UI
    }
}

void PluginEditor::timerCallback()
{
    // Update knob values and UI based on parameter values
    for (int i = 0; i < 8; ++i)
    {
        if (knobs[i] != nullptr && i < processorRef.getParameters().size())
        {
            // Special handling for Time knob in sync mode
            if (i == 0 && timeSyncEnabled) {
                // In sync mode, skip timer-based updates entirely
                // The knob is controlled by delayTimeDiv parameter, not timeMs
                // Label is updated by updateSpaceDelayTimeLabel()
                continue;
            }
            
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
                    if (i == 0) {
                        // Time knob in non-sync mode
                        valueText = juce::String(actualValue, 0) + "ms";
                    } else if (i == 5 || i == 6) {
                        // Hi-Cut, Low-Cut - show in Hz
                        valueText = juce::String((int) std::round(actualValue)) + "Hz";
                    } else if (i == 3) {
                        // Wow Rate - show in Hz
                        valueText = juce::String((int) std::round(actualValue)) + "Hz";
                    } else {
                        // Other knobs show percentage
                        valueText = juce::String((int) std::round(actualValue * 100)) + "%";
                    }
                } else {
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
                    // Special handling for 0.0 to avoid "-0.0"
                    juce::String valueText;
                    if (std::abs(knobValue) < 0.05f) // Close enough to 0.0
                        valueText = "0.0 dB";
                    else
                        valueText = juce::String(knobValue, 1) + " dB";
                    masterValueLabels[i]->setText(valueText, juce::dontSendNotification);
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
    
    // Update Redux sequencer UI
    updateReduxSequencerUI();
    
    // Update PhaseBloom sequencer UI
    updatePhaseBloomSequencerUI();
    
    // Update Chorus sequencer UI
    updateChorusSequencerUI();
    
    // Update Reverb knob value labels
    for (int i = 0; i < 8; ++i)
    {
        if (reverbKnobs[i] != nullptr && reverbValueLabels[i] != nullptr)
        {
            float knobValue = reverbKnobs[i]->getValue();
            juce::String valueText;
            
            switch (i) {
                case 0: // Width (0-1: mono to wide)
                    valueText = juce::String((int)(knobValue * 100.0f)) + "%";
                    break;
                case 1: valueText = juce::String(knobValue, 2); break; // Size (0.1-1.5)
                case 2: valueText = juce::String(knobValue, 0) + " ms"; break; // Predelay (0-200ms)
                case 3: valueText = juce::String(knobValue, 0) + " Hz"; break; // Damping (1k-20k Hz)
                case 4: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Diffusion (0-1)
                case 5: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Early (0-1)
                case 6: valueText = juce::String(knobValue, 1) + "s"; break; // Decay (0.2-20s)
                case 7: valueText = juce::String(knobValue * 100.0f, 0) + "%"; break; // Mix (0-1)
            }
            
            reverbValueLabels[i]->setText(valueText, juce::dontSendNotification);
        }
    }
    
    // Update Reverb knob indicators
    for (int i = 0; i < 8; ++i)
    {
        if (reverbIndicatorBars[i] != nullptr && reverbKnobs[i] != nullptr)
        {
            float knobValue = reverbKnobs[i]->getValue();
            float indicatorValue = 0.0f;
            
            const bool seqEnabled = processorRef.getReverbSeqState().enabled.load();
            const bool seqActive = processorRef.getReverbSeqState().active.load();
            const int playingStep = processorRef.getReverbPlayingStep();
            
            if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16)
            {
                StepSnapshot s = processorRef.getReverbSafeSnapshot(playingStep);
                
                switch (i) {
                    case 0: indicatorValue = s.reverb.type; break; // Width (0-1, repurposed from type)
                    case 1: indicatorValue = (s.reverb.size - 0.1f) / 1.4f; break; // Size (0.1-1.5)
                    case 2: indicatorValue = s.reverb.predelayMs / 200.0f; break; // Predelay (0-200ms)
                    case 3: indicatorValue = (s.reverb.dampHz - 1000.0f) / 19000.0f; break; // Damping (1k-20k)
                    case 4: indicatorValue = s.reverb.diffusion; break; // Diffusion (0-1)
                    case 5: indicatorValue = s.reverb.early; break; // Early (0-1)
                    case 6: indicatorValue = (s.reverb.decaySec - 0.2f) / 19.8f; break; // Decay (0.2-20s)
                    case 7: indicatorValue = s.reverb.mix; break; // Mix (0-1)
                }
            }
            else
            {
                // Show current knob position normalized to 0-1
                switch (i) {
                    case 0: indicatorValue = knobValue / 2.0f; break; // Type (0-2)
                    case 1: indicatorValue = (knobValue - 0.1f) / 1.4f; break; // Size (0.1-1.5)
                    case 2: indicatorValue = knobValue / 200.0f; break; // Predelay (0-200ms)
                    case 3: indicatorValue = (knobValue - 1000.0f) / 19000.0f; break; // Damping (1k-20k)
                    case 4: indicatorValue = knobValue; break; // Diffusion (0-1)
                    case 5: indicatorValue = knobValue; break; // Early (0-1)
                    case 6: indicatorValue = (knobValue - 0.2f) / 19.8f; break; // Decay (0.2-20s)
                    case 7: indicatorValue = knobValue; break; // Mix (0-1)
                }
            }
            
            reverbIndicatorBars[i]->setValue(indicatorValue);
        }
    }
    
    // Update Reverb sequencer UI
    updateReverbSequencerUI();
    
    // Update Granular value labels
    for (int i = 0; i < 8; ++i)
    {
        if (granularValueLabels[i] != nullptr && granularKnobs[i] != nullptr)
        {
            float knobValue = granularKnobs[i]->getValue();
            juce::String valueText;
            
            if (i == 1 && granularDensitySyncEnabled) {
                // Density in sync mode: show division
                std::vector<juce::String> divisions = {"2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
                int divIdx = juce::jlimit(0, 7, (int)std::round(knobValue));
                valueText = divisions[divIdx];
                
                // Add std mode suffix
                if (granularDensitySyncStdMode == 1) valueText << "t";
                else if (granularDensitySyncStdMode == 2) valueText << ".";
            } else {
                switch (i) {
                    case 0: valueText = juce::String(knobValue, 1) + "ms"; break; // Size
                    case 1: valueText = juce::String(knobValue, 1) + "Hz"; break; // Density (Hz mode)
                    case 2: valueText = juce::String((int)(knobValue * 100)) + "%"; break; // Position
                    case 3: valueText = juce::String(knobValue, 1) + "ms"; break; // Spray
                    case 4: valueText = juce::String(knobValue, 1) + "st"; break; // Pitch
                    case 5: valueText = juce::String((int)(knobValue * 100)) + "%"; break; // Random
                    case 6: valueText = juce::String((int)(knobValue * 100)) + "%"; break; // Texture
                    case 7: valueText = juce::String((int)(knobValue * 100)) + "%"; break; // Mix
                }
            }
            
            granularValueLabels[i]->setText(valueText, juce::dontSendNotification);
        }
    }
    
    // Update Granular knob indicators
    for (int i = 0; i < 8; ++i)
    {
        if (granularIndicatorBars[i] != nullptr && granularKnobs[i] != nullptr)
        {
            float knobValue = granularKnobs[i]->getValue();
            float indicatorValue = 0.0f;
            
            const bool seqEnabled = processorRef.getGranularSeqState().enabled.load();
            const bool seqActive = processorRef.getGranularSeqState().active.load();
            const int playingStep = processorRef.getGranularPlayingStep();
            
            if (seqEnabled && seqActive && playingStep >= 0 && playingStep < 16)
            {
                StepSnapshot s = processorRef.getGranularSafeSnapshot(playingStep);
                
                switch (i) {
                    case 0: indicatorValue = (s.granular.sizeMs - 5.0f) / 195.0f; break; // Size (5-200ms)
                    case 1: indicatorValue = (s.granular.densityHz - 1.0f) / 39.0f; break; // Density (1-40Hz)
                    case 2: indicatorValue = s.granular.position; break; // Position (0-1)
                    case 3: indicatorValue = s.granular.sprayMs / 200.0f; break; // Spray (0-200ms)
                    case 4: indicatorValue = (s.granular.pitchSemi + 24.0f) / 48.0f; break; // Pitch (-24 to +24)
                    case 5: indicatorValue = s.granular.random; break; // Random (0-1)
                    case 6: indicatorValue = s.granular.texture; break; // Texture (0-1)
                    case 7: indicatorValue = s.granular.mix; break; // Mix (0-1)
                }
            }
            else
            {
                // Show current knob position normalized to 0-1
                switch (i) {
                    case 0: indicatorValue = (knobValue - 5.0f) / 195.0f; break; // Size
                    case 1: indicatorValue = (knobValue - 1.0f) / 39.0f; break; // Density
                    case 2: indicatorValue = knobValue; break; // Position
                    case 3: indicatorValue = knobValue / 200.0f; break; // Spray
                    case 4: indicatorValue = (knobValue + 24.0f) / 48.0f; break; // Pitch
                    case 5: indicatorValue = knobValue; break; // Random
                    case 6: indicatorValue = knobValue; break; // Texture
                    case 7: indicatorValue = knobValue; break; // Mix
                }
            }
            
            granularIndicatorBars[i]->setValue(indicatorValue);
        }
    }
    
    // Update Granular sequencer UI
    updateGranularSequencerUI();
    
    // Update Slicer sequencer UI
    updateSlicerSequencerUI();
    
    // Update Dub Delay sequencer UI
    updateDubDelaySequencerUI();
    
    // Form2 removed - updateForm2SequencerUI();
    
    // Update Formant sequencer UI
    updateFormantSequencerUI();
    
    // Update Saturate (Heat) sequencer UI
    updateSaturateSequencerUI();
    
    // Update Dub Delay time label (handles sync mode display)
    updateDubDelayTimeLabel();
    
    // Update gain reduction meter if COMPRESS+ is enabled
    if (compCrushEnabled && gainReductionMeter && gainReductionMeter->isVisible()) {
        float gainReduction = processorRef.getCompressEngine().getGainReductionDb();
        gainReductionMeter->setGainReduction(gainReduction);
    }
    
    // Update small gain reduction meter (always visible)
    if (smallGainReductionMeter) {
        float gainReduction = processorRef.getCompressEngine().getGainReductionDb();
        smallGainReductionMeter->setGainReduction(gainReduction);
        
        // Debug: Show that the meter is being updated
        static int debugCounter = 0;
        if ((debugCounter++ % 300) == 0) { // Every 5 seconds at 60Hz
            DBG("[SmallGainMeter] Updating with gain reduction: " << gainReduction << " dB");
        }
    }
    
    // Check license periodically
    if (licenseManager && !licenseManager->isLicenseValid())
    {
        // Repaint to show overlay
        repaint();
        
        // Check if we need to show/keep showing the license dialog
        auto licenseInfo = licenseManager->getCurrentLicense();
        // Re-open dialog if no license or invalid (check every 30 seconds)
        // Only show if dialog isn't already visible and wasn't recently dismissed
        static int checkCounter = 0;
        checkCounter++;
        
        if ((checkCounter % 1875) == 0) // 1875 * 16ms = ~30 seconds, check every 30 seconds
        {
            // Check if we should show dialog (no license or invalid)
            if (licenseInfo.licenseKey.isEmpty() || 
                licenseInfo.status == GumroadLicenseStatus::Invalid ||
                licenseInfo.status == GumroadLicenseStatus::Refunded ||
                licenseInfo.status == GumroadLicenseStatus::Disputed)
            {
                // Only show if we haven't shown one recently and user hasn't dismissed it
                auto timeSinceLastShow = juce::Time::getCurrentTime() - lastLicenseDialogShowTime;
                if (timeSinceLastShow.inMilliseconds() > 30000 && !licenseDialogDismissed) // 30 second minimum between shows
                {
                    // Check if dialog is already visible by checking for focused DialogWindow
                    bool dialogVisible = false;
                    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
                    {
                        if (focused->findParentComponentOfClass<juce::DialogWindow>() != nullptr)
                            dialogVisible = true;
                    }
                    // Also check all top-level windows
                    for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
                    {
                        if (auto* window = juce::TopLevelWindow::getTopLevelWindow(i))
                        {
                            if (auto* dw = dynamic_cast<juce::DialogWindow*>(window))
                            {
                                if (dw->isVisible())
                                {
                                    dialogVisible = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (!dialogVisible)
                    {
                        lastLicenseDialogShowTime = juce::Time::getCurrentTime();
                        licenseDialogDismissed = false; // Reset flag when showing again
        showLicenseDialog();
                    }
                }
            }
            else
            {
                // License is valid, reset dismissed flag
                licenseDialogDismissed = false;
            }
        }
        
        // Don't update UI if license is invalid
        return;
    }
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
    if (reverbStepAmountLabel && reverbStepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it
    }
    if (granularStepAmountLabel && granularStepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it
    }
    if (dubdelayStepAmountLabel && dubdelayStepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it
    }
    if (stepAmountLabel && stepAmountLabel->hasKeyboardFocus(true)) {
        return false; // Let the TextEditor handle it (if we convert Delay page too)
    }
    
    // License dialog shortcut: Cmd+L (Mac) or Ctrl+L (Windows)
    if (key.getKeyCode() == 'L' || key.getKeyCode() == 'l')
    {
        if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
        {
                showLicenseDialog();
            return true; // Consume the key
        }
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
    // ResonanceKnobLNF Implementation
    //==============================================================================
    
    void ResonanceKnobLNF::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPosProportional, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        
        // Draw white circular outline (partial arc based on value)
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centerX, centerY, radius, radius, 0.0f,
                                    rotaryStartAngle, rotaryEndAngle, true);
        g.strokePath(backgroundArc, juce::PathStrokeType(2.0f));
        
        // Draw value arc (filled portion)
        g.setColour(juce::Colours::white);
        juce::Path valueArc;
        valueArc.addCentredArc(centerX, centerY, radius, radius, 0.0f,
                              rotaryStartAngle, angle, true);
        g.strokePath(valueArc, juce::PathStrokeType(2.0f));
        
        // Draw "R" label in the center
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(radius * 1.0f, juce::Font::bold));
        g.drawText("R", bounds, juce::Justification::centred);
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
// PresetSelectorButton Implementation
//==============================================================================

PresetSelectorButton::PresetSelectorButton() : juce::Button("presetSelector")
{
    setClickingTogglesState(false);
}

void PresetSelectorButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw black background with 4px top corner radius (manually construct path)
    juce::Path background;
    const float cornerRadius = 4.0f;
    const float x = bounds.getX();
    const float y = bounds.getY();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    
    background.startNewSubPath(x + cornerRadius, y);
    background.lineTo(x + w - cornerRadius, y);
    background.addArc(x + w - cornerRadius * 2, y, cornerRadius * 2, cornerRadius * 2, 0, juce::MathConstants<float>::halfPi);
    background.lineTo(x + w, y + h);
    background.lineTo(x, y + h);
    background.lineTo(x, y + cornerRadius);
    background.addArc(x, y, cornerRadius * 2, cornerRadius * 2, juce::MathConstants<float>::pi, juce::MathConstants<float>::pi + juce::MathConstants<float>::halfPi);
    background.closeSubPath();
    
    g.setColour(juce::Colour(0xff131313));
    g.fillPath(background);
    
    // No hover highlight
    
    // Draw save icon on the left (16x16 size)
    if (saveIcon != nullptr)
    {
        saveIconBounds = juce::Rectangle<float>(bounds.getX() + 15, bounds.getY() + (bounds.getHeight() - 16) * 0.5f, 16, 16);
        saveIcon->drawWithin(g, saveIconBounds, juce::RectanglePlacement::centred, 1.0f);
    }
    
    // Draw preset name after the save icon, moved right 30px
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f, juce::Font::plain));
    auto textArea = juce::Rectangle<float>(bounds.getX() + 15 + 16 + 8 + 30, bounds.getY(), bounds.getWidth() - 80, bounds.getHeight());
    auto textBounds = g.getCurrentFont().getStringWidth(presetName);
    g.drawText(presetName, textArea, juce::Justification::centredLeft);
    
    // Draw carrot to the right of the text (12x12 size), moved right 30px with text
    if (carrotImage != nullptr)
    {
        float carrotX = bounds.getX() + 15 + 16 + 8 + 30 + textBounds + 8; // 8px padding after text
        auto carrotArea = juce::Rectangle<float>(carrotX, bounds.getY() + (bounds.getHeight() - 12) * 0.5f, 12, 12);
        carrotImage->drawWithin(g, carrotArea, juce::RectanglePlacement::centred, 1.0f);
    }
    
    // Draw large dice on the very right (22x22 size) - store bounds for click detection
    if (diceImage != nullptr)
    {
        diceBounds = juce::Rectangle<float>(bounds.getRight() - 22 - 8, bounds.getY() + (bounds.getHeight() - 22) * 0.5f, 22, 22);
        diceImage->drawWithin(g, diceBounds, juce::RectanglePlacement::centred, 1.0f);
    }
}

void PresetSelectorButton::mouseUp(const juce::MouseEvent& event)
{
    // Check if click is on the save icon
    if (saveIconBounds.contains(event.position))
    {
        if (onSaveClick)
            onSaveClick();
        return; // Don't trigger button click
    }
    
    // Check if click is on the dice button
    if (diceBounds.contains(event.position))
    {
        if (onDiceClick)
            onDiceClick();
        return; // Don't trigger button click
    }
    
    // Otherwise, handle as normal button click
    juce::Button::mouseUp(event);
}

void PresetSelectorButton::setPresetName(const juce::String& name)
{
    presetName = name;
    repaint();
}

void PresetSelectorButton::setCarrotImage(std::unique_ptr<juce::Drawable> carrot)
{
    carrotImage = std::move(carrot);
    repaint();
}

void PresetSelectorButton::setDiceImage(std::unique_ptr<juce::Drawable> dice)
{
    diceImage = std::move(dice);
    repaint();
}

void PresetSelectorButton::setSaveIcon(std::unique_ptr<juce::Drawable> save)
{
    saveIcon = std::move(save);
    repaint();
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
            if (i == 0 && timeSyncEnabled) {
                // Time knob in sync mode: 0-19 divisions (20 choices)
                knobs[i]->setRange(0.0, 19.0, 1.0);
                // Initialize from delayTimeDiv parameter instead of timeMs
                auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("delayTimeDiv"));
                if (divParam) {
                    knobs[i]->setValue(static_cast<float>(divParam->getIndex()), juce::dontSendNotification);
                } else {
                    knobs[i]->setValue(5.0, juce::dontSendNotification); // Default to 1/4 (index 5)
                }
            } else if (i == 0) {
                // Time knob in non-sync mode: 1-2000ms
                knobs[i]->setRange(1.0, 2000.0, 1.0);
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
                {
                    knobs[i]->setValue(floatParam->convertFrom0to1(param->getValue()), juce::dontSendNotification);
                }
                else
                {
                    knobs[i]->setValue(250.0, juce::dontSendNotification);
                }
            } else {
                // Other knobs: normalized 0-1 range
                knobs[i]->setRange(0.0, 1.0, 0.001);
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
                {
                    knobs[i]->setValue(param->getValue(), juce::dontSendNotification);
                }
                else
                {
                    knobs[i]->setValue(0.5, juce::dontSendNotification);
                }
        }
        
            // Add listener to update snapshots when knob changes
        knobs[i]->onValueChange = [this, i]() {
                if (i == 0 && timeSyncEnabled)
                {
                    // Handle sync mode: map knob position to division index
                    float knobValue = knobs[0]->getValue();
                    int divisionIndex = static_cast<int>(knobValue); // Direct mapping, no rounding needed since knob has step size of 1.0
                    divisionIndex = juce::jlimit(0, 19, divisionIndex); // 20 divisions = indices 0-19
                    
                    DBG("[SYNC] Knob value: " << knobValue << ", Division index: " << divisionIndex);
                    
                    // Update APVTS parameter
                    auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("delayTimeDiv"));
                    if (divParam) {
                        // AudioParameterChoice expects normalized value 0-1
                        float normalizedValue = static_cast<float>(divisionIndex) / 19.0f; // 19 steps for 20 choices
                        divParam->setValueNotifyingHost(normalizedValue);
                        DBG("[SYNC] Set division param to: " << normalizedValue << ", actual index: " << divParam->getIndex());
                    }
                    
                    // Update label via updateSpaceDelayTimeLabel
                    updateSpaceDelayTimeLabel();
                } else {
                    // For non-sync time knob, update the time label
                    if (i == 0) {
                        updateSpaceDelayTimeLabel();
                    }
                }
                
                // Always update parameter from knob for non-sync knobs or after sync handling
                if (i != 0 || !timeSyncEnabled) {
                    updateParameterFromKnob(i);
                } else {
                    DBG("[SYNC] Skipping updateParameterFromKnob for knob 0 in sync mode");
                }
                
                // If All Steps toggle is active, update all step snapshots
                if (spaceDelayAllStepsEnabled)
                {
                    DBG("[All Steps] Space Delay knob " << i << " changed, spaceDelayAllStepsEnabled=true");
                    
                    // Update all 16 step snapshots with the new value
                    float value = knobs[i]->getValue();
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getSpaceDelaySafeSnapshot(step);
                        switch (i) {
                            case 0: 
                                // In sync mode, skip updating timeMs directly (it's controlled by BPM + division)
                                if (!timeSyncEnabled) {
                                    snapshot.delay.timeMs = processorRef.getAPVTS().getParameter("timeMs")->convertFrom0to1(value);
                                }
                                break;
                            case 1: snapshot.delay.feedback = processorRef.getAPVTS().getParameter("feedback")->convertFrom0to1(value); break;
                            case 2: snapshot.delay.wowDepth = processorRef.getAPVTS().getParameter("wowDepth")->convertFrom0to1(value); break;
                            case 3: snapshot.delay.wowRate = processorRef.getAPVTS().getParameter("wowRate")->convertFrom0to1(value); break;
                            case 4: snapshot.delay.saturation = processorRef.getAPVTS().getParameter("drive")->convertFrom0to1(value); break;
                            case 5: snapshot.delay.highCut = processorRef.getAPVTS().getParameter("hiCut")->convertFrom0to1(value); break;
                            case 6: snapshot.delay.lowCut = processorRef.getAPVTS().getParameter("lowCut")->convertFrom0to1(value); break;
                            case 7: snapshot.delay.mix = processorRef.getAPVTS().getParameter("mix")->convertFrom0to1(value); break;
                        }
                        processorRef.setSpaceDelayStepSnapshot(step, snapshot);
                    }
                } else {
                    DBG("[All Steps] Space Delay knob " << i << " changed, spaceDelayAllStepsEnabled=false, skipping All Steps update");
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
        knobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        knobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        knobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobLabels[i].get());
        knobLabels[i]->setBounds(x, y - 15, knobSize, 20); // Moved down 5px from -20 to -15
        
        // Create value label
        valueLabels[i] = std::make_unique<juce::Label>();
        valueLabels[i]->setText("0", juce::dontSendNotification);
        valueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
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
    
    // Full master area for preset browser overlay (separate from knob positioning)
    auto fullMasterArea = juce::Rectangle<int>(460, 54, 495, 460);
    
    // Create "MASTER" title in master area (top-left corner, 10px right)
    masterTitle = std::make_unique<juce::Label>();
    masterTitle->setText("MASTER", juce::dontSendNotification);
    masterTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.5796875f, juce::Font::bold).withExtraKerningFactor(0.09f));
    masterTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    masterTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(masterTitle.get());
    masterTitle->setBounds(masterArea.getX() + 20, masterArea.getY() + 5, 150, 30); // 20px from left, moved down 5px
    masterTitle->toFront(false);
    
    // Create Master Dice Button (randomizes all effects, all steps) 
    masterDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(masterDiceButton.get());
    
    if (assets.diceLarge) {
        masterDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    masterDiceButton->onClick = [this]() {
        DBG("[UI] Master dice clicked");
        if (randomizationManager) {
            randomizationManager->requestRandomizeAllActivePages();
        }
    };
    
    // Position dice button 20px left from before (was 160, now 140), 20% smaller
    const int diceSizeMaster = 26; // 20% smaller: 32 * 0.8 = 25.6 ≈ 26
    masterDiceButton->setBounds(
        masterArea.getX() + 140,  // 20px left from before
        masterArea.getY() + 6,    // Aligned with MASTER title
            diceSizeMaster, diceSizeMaster
        );
        
        // Preset Browser Button (shows current preset name)
        presetBrowserButton = std::make_unique<PresetSelectorButton>();
        addAndMakeVisible(presetBrowserButton.get());
        
        // Set the SVG images
        if (assets.saveIcon != nullptr)
        {
            presetBrowserButton->setSaveIcon(assets.saveIcon->createCopy());
        }
        if (assets.diceLarge != nullptr)
        {
            presetBrowserButton->setDiceImage(assets.diceLarge->createCopy());
        }
        if (assets.presetMenuCarrot != nullptr)
        {
            presetBrowserButton->setCarrotImage(assets.presetMenuCarrot->createCopy());
        }
        
        // Handle save icon clicks
        presetBrowserButton->onSaveClick = [this]() {
            DBG("[PresetBrowser] Save icon clicked");
            
            // Create and show alert window with text editor
            auto* alertWindow = new juce::AlertWindow(
                "Save Preset",
                "Enter a name for your preset:",
                juce::AlertWindow::NoIcon,
                this
            );
            
            alertWindow->addTextEditor("presetName", "My Preset", "Preset Name:");
            alertWindow->getTextEditor("presetName")->setSelectAllWhenFocused(true);
            alertWindow->addButton("Save New", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alertWindow->addButton("Overwrite", 2);
            alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            
            alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([this, alertWindow](int result) {
                if (result == 1) // Save New button clicked
                {
                    juce::String presetName = alertWindow->getTextEditorContents("presetName");
                    
                    if (presetName.isNotEmpty())
                    {
                        // Save to "User" category
                        auto saveResult = presetManager->saveCurrentStateAsPreset(presetName, "User");
                        
                        if (saveResult.wasOk())
                        {
                            DBG("[PresetBrowser] Saved preset: " + presetName + " to User category");
                            
                            // Update button text
                            presetBrowserButton->setPresetName(presetName);
                            
                            // Refresh preset browser if it's open
                            if (presetBrowser != nullptr && presetBrowser->isVisible())
                            {
                                presetBrowser->show();
                            }
                        }
                        else
                        {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::AlertWindow::WarningIcon,
                                "Save Error",
                                saveResult.getErrorMessage()
                            );
                        }
                    }
                }
                else if (result == 2) // Overwrite button clicked
                {
                    delete alertWindow;
                    
                    // Show list of User presets to overwrite
                    auto userPresets = presetManager->getPresetsInGroup("User");
                    
                    if (userPresets.isEmpty())
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::InfoIcon,
                            "No Presets",
                            "You don't have any saved presets to overwrite yet. Save a new preset first!"
                        );
                        return;
                    }
                    
                    // Create list of preset names
                    juce::StringArray presetNames;
                    for (const auto& preset : userPresets)
                    {
                        presetNames.add(preset.name);
                    }
                    
                    // Show selection dialog
                    auto* overwriteWindow = new juce::AlertWindow(
                        "Overwrite Preset",
                        "Select a preset to overwrite:",
                        juce::AlertWindow::NoIcon,
                        static_cast<juce::Component*>(this)
                    );
                    
                    overwriteWindow->addComboBox("presetSelect", presetNames, "Select Preset:");
                    overwriteWindow->addButton("Overwrite", 1, juce::KeyPress(juce::KeyPress::returnKey));
                    overwriteWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                    
                    overwriteWindow->enterModalState(true, juce::ModalCallbackFunction::create([this, overwriteWindow, userPresets](int overwriteResult) {
                        if (overwriteResult == 1) // Overwrite confirmed
                        {
                            int selectedIndex = overwriteWindow->getComboBoxComponent("presetSelect")->getSelectedItemIndex();
                            
                            if (selectedIndex >= 0 && selectedIndex < userPresets.size())
                            {
                                juce::String presetName = userPresets[selectedIndex].name;
                                
                                // Save with same name (overwrite)
                                auto saveResult = presetManager->saveCurrentStateAsPreset(presetName, "User");
                                
                                if (saveResult.wasOk())
                                {
                                    DBG("[PresetBrowser] Overwritten preset: " + presetName);
                                    
                                    // Update button text
                                    presetBrowserButton->setPresetName(presetName);
                                    
                                    // Refresh preset browser if it's open
                                    if (presetBrowser != nullptr && presetBrowser->isVisible())
                                    {
                                        presetBrowser->show();
                                    }
                                }
                                else
                                {
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::AlertWindow::WarningIcon,
                                        "Save Error",
                                        saveResult.getErrorMessage()
                                    );
                                }
                            }
                        }
                        
                        delete overwriteWindow;
                    }), true);
                    
                    return; // Don't delete the first window yet
                }
                
                delete alertWindow;
            }), true);
        };
        
        // Handle dice clicks - load random preset from any category
        presetBrowserButton->onDiceClick = [this]() {
            DBG("[PresetBrowser] Dice clicked - loading random preset");
            
            // Get all presets from all groups
            auto allPresets = presetManager->getAllPresets();
            
            if (allPresets.isEmpty())
            {
                DBG("[PresetBrowser] No presets available for randomization");
                return;
            }
            
            // Pick a random preset
            int randomIndex = juce::Random::getSystemRandom().nextInt(allPresets.size());
            const auto& randomPreset = allPresets[randomIndex];
            
            // Load it
            auto result = presetManager->loadPreset(randomPreset.file);
            if (result.wasOk())
            {
                DBG("[PresetBrowser] Loaded random preset: " + randomPreset.name);
                presetBrowserButton->setPresetName(randomPreset.name);
                
                // Refresh effect selector dropdowns to reflect new router assignment
                auto& router = processorRef.getEffectRouter();
                if (effectSelector1) effectSelector1->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot1)) + 1, juce::dontSendNotification);
                if (effectSelector2) effectSelector2->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot2)) + 1, juce::dontSendNotification);
                if (effectSelector3) effectSelector3->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot3)) + 1, juce::dontSendNotification);
                if (effectSelector4) effectSelector4->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot4)) + 1, juce::dontSendNotification);
                
                // Refresh UI to show correct effect controls for current page
                showPage(currentPage);
                repaint();
            }
            else
            {
                DBG("[PresetBrowser] Failed to load random preset: " + result.getErrorMessage());
            }
        };
        
        // Size: 229 x 35 (457/2 x 70/2) positioned at top right of master area
        presetBrowserButton->setBounds(
            fullMasterArea.getX() + fullMasterArea.getWidth() - 229 - 10, // 10px from right edge
            fullMasterArea.getY() + 10 - 24 + 5 - 1, // 10px from top, moved up 24px, then down 5px, then up 1px
            229, 35
        );
        
        // Comp Crush Tab Button - positioned to the left of the save icon, 40% smaller
        compCrushTabButton = std::make_unique<juce::DrawableButton>("CompCrushTab", juce::DrawableButton::ImageFitted);
        compCrushTabButton->setClickingTogglesState(true);
        compCrushTabButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        compCrushTabButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(compCrushTabButton.get());
        
        // Set initial image (inactive)
        if (assets.compCrushTabInactive) {
            compCrushTabButton->setImages(assets.compCrushTabInactive.get());
        }
        
        // Position to the left of the preset browser button, 2.5x bigger then 6% smaller (87.5 * 0.94 = 82.25)
        const int compCrushSize = static_cast<int>(35 * 2.5 * 0.94); // 2.5x bigger then 6% smaller: 87.5 * 0.94 = 82.25
        const int compCrushSpacing = 10; // Space between buttons
        compCrushTabButton->setBounds(
            presetBrowserButton->getX() - compCrushSize - compCrushSpacing + 5 + 2 + 2 - 2, // Left of preset browser button + 5px right + 2px right + 2px right - 2px left
            fullMasterArea.getY() + 10 - 24 + 5 - 1 + (35 - compCrushSize) / 2 + 3 + 2 + 1, // Vertically centered with preset browser button + 3px down + 2px down + 1px down
            compCrushSize, compCrushSize
        );
        
        // Toggle functionality
        compCrushTabButton->onClick = [this]() {
            compCrushEnabled = compCrushTabButton->getToggleState();
            DBG("[CompCrush] Toggle state changed to: " << (compCrushEnabled ? "ON" : "OFF"));
            
            // Keep the compressEnabled parameter always true - effect is always on
            auto* compressEnabledParam = processorRef.getAPVTS().getRawParameterValue("compressEnabled");
            if (compressEnabledParam) {
                compressEnabledParam->store(1.0f); // Always enabled
                DBG("[CompCrush] compressEnabled parameter kept at 1.0f (always enabled)");
            }
            
            // Update button image based on state
            if (compCrushEnabled && assets.compCrushTabActive) {
                compCrushTabButton->setImages(assets.compCrushTabActive.get());
                // Show overlay and sliders when active
                if (compCrushOverlay) {
                    compCrushOverlay->setVisible(true);
                    compCrushOverlay->toFront(false); // Bring to front when shown
                }
                
        // Show gain reduction meter
        if (gainReductionMeter) {
            gainReductionMeter->setVisible(true);
            gainReductionMeter->toFront(false);
        }
        
        // Bring specific master area components to front (filter bar, resonance knobs, audio visualizer, pan bar)
        if (outputSpectrumView) {
            outputSpectrumView->toFront(false);
        }
        if (spectrumFilterSlider) {
            spectrumFilterSlider->toFront(false);
        }
        if (hpResonanceKnob) {
            hpResonanceKnob->toFront(false);
        }
        if (lpResonanceKnob) {
            lpResonanceKnob->toFront(false);
        }
        if (panBar) {
            panBar->toFront(false);
        }
                
                // Show and enable COMPRESS+ sliders
                compressDriveSlider->setVisible(true);
                compressDriveSlider->setEnabled(true);
                compressDriveSlider->toFront(false);
                compressThresholdSlider->setVisible(true);
                compressThresholdSlider->setEnabled(true);
                compressThresholdSlider->toFront(false);
                compressAttackSlider->setVisible(true);
                compressAttackSlider->setEnabled(true);
                compressAttackSlider->toFront(false);
                compressReleaseSlider->setVisible(true);
                compressReleaseSlider->setEnabled(true);
                compressReleaseSlider->toFront(false);
                compressRatioSlider->setVisible(true);
                compressRatioSlider->setEnabled(true);
                compressRatioSlider->toFront(false);
                compressLofiSlider->setVisible(true);
                compressLofiSlider->setEnabled(true);
                compressLofiSlider->toFront(false);
                compressMakeupGainSlider->setVisible(true);
                compressMakeupGainSlider->setEnabled(true);
                compressMakeupGainSlider->toFront(false);
                compressWetSlider->setVisible(true);
                compressWetSlider->setEnabled(true);
                compressWetSlider->toFront(false);
                
                // Show and enable slider labels
                compressDriveLabel->setVisible(true);
                compressDriveLabel->setEnabled(true);
                compressDriveLabel->toFront(false);
                compressThresholdLabel->setVisible(true);
                compressThresholdLabel->setEnabled(true);
                compressThresholdLabel->toFront(false);
                compressAttackLabel->setVisible(true);
                compressAttackLabel->setEnabled(true);
                compressAttackLabel->toFront(false);
                compressReleaseLabel->setVisible(true);
                compressReleaseLabel->setEnabled(true);
                compressReleaseLabel->toFront(false);
                compressRatioLabel->setVisible(true);
                compressRatioLabel->setEnabled(true);
                compressRatioLabel->toFront(false);
                compressLofiLabel->setVisible(true);
                compressLofiLabel->setEnabled(true);
                compressLofiLabel->toFront(false);
                compressMakeupGainLabel->setVisible(true);
                compressMakeupGainLabel->setEnabled(true);
                compressMakeupGainLabel->toFront(false);
                compressWetLabel->setVisible(true);
                compressWetLabel->setEnabled(true);
                compressWetLabel->toFront(false);
                
                // Show and enable value labels
                compressDriveValueLabel->setVisible(true);
                compressDriveValueLabel->setEnabled(true);
                compressDriveValueLabel->toFront(false);
                compressThresholdValueLabel->setVisible(true);
                compressThresholdValueLabel->setEnabled(true);
                compressThresholdValueLabel->toFront(false);
                compressAttackValueLabel->setVisible(true);
                compressAttackValueLabel->setEnabled(true);
                compressAttackValueLabel->toFront(false);
                compressReleaseValueLabel->setVisible(true);
                compressReleaseValueLabel->setEnabled(true);
                compressReleaseValueLabel->toFront(false);
                compressRatioValueLabel->setVisible(true);
                compressRatioValueLabel->setEnabled(true);
                compressRatioValueLabel->toFront(false);
                compressLofiValueLabel->setVisible(true);
                compressLofiValueLabel->setEnabled(true);
                compressLofiValueLabel->toFront(false);
                compressMakeupGainValueLabel->setVisible(true);
                compressMakeupGainValueLabel->setEnabled(true);
                compressMakeupGainValueLabel->toFront(false);
                compressWetValueLabel->setVisible(true);
                compressWetValueLabel->setEnabled(true);
                compressWetValueLabel->toFront(false);
                
                // Hide MASTER title and dice, change to COMPRESS+
                if (masterTitle) {
                    masterTitle->setText("COMPRESS+", juce::dontSendNotification);
                }
                if (masterDiceButton) {
                    masterDiceButton->setVisible(false);
                }
                
                // Update value labels when COMPRESS+ page is activated
                updateCompressValueLabels();
            } else if (!compCrushEnabled && assets.compCrushTabInactive) {
                compCrushTabButton->setImages(assets.compCrushTabInactive.get());
                // Hide overlay and sliders when inactive
                if (compCrushOverlay) {
                    compCrushOverlay->setVisible(false);
                }
                
                // Hide and disable COMPRESS+ sliders
                compressDriveSlider->setVisible(false);
                compressDriveSlider->setEnabled(false);
                compressThresholdSlider->setVisible(false);
                compressThresholdSlider->setEnabled(false);
                compressAttackSlider->setVisible(false);
                compressAttackSlider->setEnabled(false);
                compressReleaseSlider->setVisible(false);
                compressReleaseSlider->setEnabled(false);
                compressRatioSlider->setVisible(false);
                compressRatioSlider->setEnabled(false);
                compressLofiSlider->setVisible(false);
                compressLofiSlider->setEnabled(false);
                compressMakeupGainSlider->setVisible(false);
                compressMakeupGainSlider->setEnabled(false);
                compressWetSlider->setVisible(false);
                compressWetSlider->setEnabled(false);
                
        // Hide gain reduction meter
        if (gainReductionMeter) {
            gainReductionMeter->setVisible(false);
        }
        
        // Master area components will stay visible but behind other elements
                
                // Hide and disable slider labels
                compressDriveLabel->setVisible(false);
                compressDriveLabel->setEnabled(false);
                compressThresholdLabel->setVisible(false);
                compressThresholdLabel->setEnabled(false);
                compressAttackLabel->setVisible(false);
                compressAttackLabel->setEnabled(false);
                compressReleaseLabel->setVisible(false);
                compressReleaseLabel->setEnabled(false);
                compressRatioLabel->setVisible(false);
                compressRatioLabel->setEnabled(false);
                compressLofiLabel->setVisible(false);
                compressLofiLabel->setEnabled(false);
                compressMakeupGainLabel->setVisible(false);
                compressMakeupGainLabel->setEnabled(false);
                compressWetLabel->setVisible(false);
                compressWetLabel->setEnabled(false);
                
                // Hide and disable value labels
                compressDriveValueLabel->setVisible(false);
                compressDriveValueLabel->setEnabled(false);
                compressThresholdValueLabel->setVisible(false);
                compressThresholdValueLabel->setEnabled(false);
                compressAttackValueLabel->setVisible(false);
                compressAttackValueLabel->setEnabled(false);
                compressReleaseValueLabel->setVisible(false);
                compressReleaseValueLabel->setEnabled(false);
                compressRatioValueLabel->setVisible(false);
                compressRatioValueLabel->setEnabled(false);
                compressLofiValueLabel->setVisible(false);
                compressLofiValueLabel->setEnabled(false);
                compressMakeupGainValueLabel->setVisible(false);
                compressMakeupGainValueLabel->setEnabled(false);
                compressWetValueLabel->setVisible(false);
                compressWetValueLabel->setEnabled(false);
                // Restore MASTER title and dice
                if (masterTitle) {
                    masterTitle->setText("MASTER", juce::dontSendNotification);
                }
                if (masterDiceButton) {
                    masterDiceButton->setVisible(true);
                }
            }
        };
        
        // Make sure the button is visible
        compCrushTabButton->setVisible(true);
        
        // Setup small gain reduction meter - positioned above the compressor toggle
        smallGainReductionMeter = std::make_unique<SmallGainReductionMeter>();
        // Position it above the compressor toggle (proper size and position)
        const int smallMeterX = presetBrowserButton->getX() - compCrushSize - compCrushSpacing + 5 + 2 + 2 - 2 + (compCrushSize - 77) / 2;
        const int smallMeterY = fullMasterArea.getY() + 10 - 24 + 5 - 1 + (35 - compCrushSize) / 2 + 3 + 2 + 1 - 15 + 36; // Moved down 36px
        smallGainReductionMeter->setBounds(
            smallMeterX, // Center horizontally on the compressor toggle (77px wide)
            smallMeterY, // 15px above the compressor toggle + 36px down
            77, // 77px wide (3px less than 80px)
            6   // 6px tall (proper size)
        );
        addAndMakeVisible(smallGainReductionMeter.get());
        smallGainReductionMeter->setVisible(true); // Always visible
        smallGainReductionMeter->toFront(false); // Bring to front
        
        DBG("[SmallGainMeter] Positioned at: " << smallMeterX << ", " << smallMeterY << " (77x6) - ABOVE COMPRESSOR TOGGLE");
        
        // Create Comp Crush overlay - black background like presets area
        compCrushOverlay = std::make_unique<CompCrushOverlay>();
        addAndMakeVisible(compCrushOverlay.get());
        
        // Position overlay to match preset browser exactly
        auto browserBounds = juce::Rectangle<int>(
            fullMasterArea.getX(),
            fullMasterArea.getY() + 28, // Start at bottom of button (button is at Y-10 with height 35) + 3px down
            fullMasterArea.getWidth(),
            fullMasterArea.getHeight() - 38 // Reduce by 28px for button offset + 10px bigger (was -48)
        );
        compCrushOverlay->setBounds(browserBounds);
        compCrushOverlay->setVisible(false); // Initially hidden
        compCrushOverlay->toFront(false); // Ensure it's in front of master area components
        
        presetBrowserButton->onClick = [this, fullMasterArea]() {
            DBG("[PresetBrowser] Button clicked");
            try {
                // Calculate preset browser bounds to start at bottom of selector button and be 10px bigger
                auto browserBounds = juce::Rectangle<int>(
                    fullMasterArea.getX(),
                    fullMasterArea.getY() + 28, // Start at bottom of button (button is at Y-10 with height 35) + 3px down
                    fullMasterArea.getWidth(),
                    fullMasterArea.getHeight() - 38 // Reduce by 28px for button offset + 10px bigger (was -48)
                );
                
                if (presetBrowser == nullptr || !presetBrowser->isVisible()) {
                    DBG("[PresetBrowser] Creating/showing overlay");
                    // Create browser overlay if needed
                if (presetBrowser == nullptr) {
                    presetBrowser = std::make_unique<PresetBrowserOverlay>(*presetManager, assets);
                    addAndMakeVisible(presetBrowser.get());
                    
                    presetBrowser->onClose = [this]() {
                        DBG("[PresetBrowser] Closing");
                        presetBrowser->setVisible(false);
                            
                            // Restore MASTER text and show master dice (or COMPRESS+ if Comp Crush is enabled)
                            if (masterTitle) {
                                if (compCrushEnabled) {
                                    masterTitle->setText("COMPRESS+", juce::dontSendNotification);
                                } else {
                                masterTitle->setText("MASTER", juce::dontSendNotification);
                                }
                            }
                            if (masterDiceButton) {
                                if (compCrushEnabled) {
                                    masterDiceButton->setVisible(false); // Hide dice if Comp Crush is enabled
                                } else {
                                    masterDiceButton->setVisible(true); // Show dice if Comp Crush is disabled
                                }
                            }
                            if (compCrushTabButton)
                                compCrushTabButton->setVisible(true);
                            if (smallGainReductionMeter)
                                smallGainReductionMeter->setVisible(true);
                            
                            // Manage slider visibility based on compCrushEnabled state
                            if (compCrushEnabled) {
                                // Show COMPRESS+ sliders if enabled
                                compressDriveSlider->setVisible(true);
                                compressThresholdSlider->setVisible(true);
                                compressAttackSlider->setVisible(true);
                                compressReleaseSlider->setVisible(true);
                                compressRatioSlider->setVisible(true);
                                compressLofiSlider->setVisible(true);
                                compressMakeupGainSlider->setVisible(true);
                                compressWetSlider->setVisible(true);
                                
                                // Show slider labels
                                compressDriveLabel->setVisible(true);
                                compressThresholdLabel->setVisible(true);
                                compressAttackLabel->setVisible(true);
                                compressReleaseLabel->setVisible(true);
                                compressRatioLabel->setVisible(true);
                                compressLofiLabel->setVisible(true);
                                compressMakeupGainLabel->setVisible(true);
                                compressWetLabel->setVisible(true);
                                
                                // Show value labels
                                compressDriveValueLabel->setVisible(true);
                                compressThresholdValueLabel->setVisible(true);
                                compressAttackValueLabel->setVisible(true);
                                compressReleaseValueLabel->setVisible(true);
                                compressRatioValueLabel->setVisible(true);
                                compressLofiValueLabel->setVisible(true);
                                compressMakeupGainValueLabel->setVisible(true);
                                compressWetValueLabel->setVisible(true);
                                
                                // Show gain reduction meter and audio visualizer
                                if (gainReductionMeter) {
                                    gainReductionMeter->setVisible(true);
                                    gainReductionMeter->toFront(false);
                                }
                                // Bring specific master area components to front (filter bar, resonance knobs, audio visualizer, pan bar)
                                if (outputSpectrumView) {
                                    outputSpectrumView->toFront(false);
                                }
                                if (spectrumFilterSlider) {
                                    spectrumFilterSlider->toFront(false);
                                }
                                if (hpResonanceKnob) {
                                    hpResonanceKnob->toFront(false);
                                }
                                if (lpResonanceKnob) {
                                    lpResonanceKnob->toFront(false);
                                }
                                if (panBar) {
                                    panBar->toFront(false);
                                }
                            } else {
                                // Hide COMPRESS+ sliders if disabled
                                compressDriveSlider->setVisible(false);
                                compressThresholdSlider->setVisible(false);
                                compressAttackSlider->setVisible(false);
                                compressReleaseSlider->setVisible(false);
                                compressRatioSlider->setVisible(false);
                                compressLofiSlider->setVisible(false);
                                compressMakeupGainSlider->setVisible(false);
                                compressWetSlider->setVisible(false);
                                
                                // Hide slider labels
                                compressDriveLabel->setVisible(false);
                                compressThresholdLabel->setVisible(false);
                                compressAttackLabel->setVisible(false);
                                compressReleaseLabel->setVisible(false);
                                compressRatioLabel->setVisible(false);
                                compressLofiLabel->setVisible(false);
                                compressMakeupGainLabel->setVisible(false);
                                compressWetLabel->setVisible(false);
                                
                                // Hide value labels
                                compressDriveValueLabel->setVisible(false);
                                compressThresholdValueLabel->setVisible(false);
                                compressAttackValueLabel->setVisible(false);
                                compressReleaseValueLabel->setVisible(false);
                                compressRatioValueLabel->setVisible(false);
                                compressLofiValueLabel->setVisible(false);
                                compressMakeupGainValueLabel->setVisible(false);
                                compressWetValueLabel->setVisible(false);
                                
                                // Hide gain reduction meter and audio visualizer
                                if (gainReductionMeter) {
                                    gainReductionMeter->setVisible(false);
                                }
                                // Master area components will stay visible but behind other elements
                            }
                        };
                        
                        // Set up preset loaded callback (only once during creation)
                        presetBrowser->onPresetLoaded = [this](const juce::String& presetName) {
                        DBG("[PresetBrowser] Preset loaded: " + presetName);
                        if (presetBrowserButton)
                        {
                            presetBrowserButton->setPresetName(presetName);
                        }
                        
                        // Refresh effect selector dropdowns to reflect new router assignment
                        auto& router = processorRef.getEffectRouter();
                        if (effectSelector1) effectSelector1->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot1)) + 1, juce::dontSendNotification);
                        if (effectSelector2) effectSelector2->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot2)) + 1, juce::dontSendNotification);
                        if (effectSelector3) effectSelector3->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot3)) + 1, juce::dontSendNotification);
                        if (effectSelector4) effectSelector4->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot4)) + 1, juce::dontSendNotification);
                        
                        // Refresh UI to show correct effect controls for current page
                        showPage(currentPage);
                        
                        // Refresh tab backgrounds to show correct effect icons
                        repaint();
                        
                        DBG("[PresetBrowser] Effect selectors refreshed after preset load");
                        };
                    }
                    
                    // Show the overlay (scans and refreshes)
                    presetBrowser->setBounds(browserBounds);
                    presetBrowser->setVisible(true);
                    presetBrowser->toFront(true);
                    presetBrowser->show();
                    
                    // Change MASTER to PRESETS and hide master dice
                    if (masterTitle)
                        masterTitle->setText("PRESETS", juce::dontSendNotification);
                    if (masterDiceButton)
                        masterDiceButton->setVisible(false);
                        if (compCrushTabButton)
                            compCrushTabButton->setVisible(false);
                        if (smallGainReductionMeter)
                            smallGainReductionMeter->setVisible(false);
                        if (compCrushOverlay)
                            compCrushOverlay->setVisible(false);
                } else {
                    DBG("[PresetBrowser] Hiding overlay");
                    // Hide the overlay and restore MASTER title/dice
                    presetBrowser->setVisible(false);
                    
                    // Restore MASTER text and show master dice (or COMPRESS+ if Comp Crush is enabled)
                    if (masterTitle) {
                        if (compCrushEnabled) {
                            masterTitle->setText("COMPRESS+", juce::dontSendNotification);
                        } else {
                        masterTitle->setText("MASTER", juce::dontSendNotification);
                        }
                    }
                    if (masterDiceButton) {
                        if (compCrushEnabled) {
                            masterDiceButton->setVisible(false); // Hide dice if Comp Crush is enabled
                        } else {
                            masterDiceButton->setVisible(true); // Show dice if Comp Crush is disabled
                        }
                    }
                    if (compCrushTabButton)
                        compCrushTabButton->setVisible(true);
                    if (smallGainReductionMeter)
                        smallGainReductionMeter->setVisible(true);
                    if (compCrushOverlay)
                        compCrushOverlay->setVisible(compCrushEnabled); // Show overlay if Comp Crush is enabled
                    
                    // Manage slider visibility based on compCrushEnabled state
                    if (compCrushEnabled) {
                        // Show COMPRESS+ sliders if enabled
                        compressDriveSlider->setVisible(true);
                        compressThresholdSlider->setVisible(true);
                        compressAttackSlider->setVisible(true);
                        compressReleaseSlider->setVisible(true);
                        compressRatioSlider->setVisible(true);
                        compressLofiSlider->setVisible(true);
                        compressMakeupGainSlider->setVisible(true);
                        compressWetSlider->setVisible(true);
                        
                        // Show slider labels
                        compressDriveLabel->setVisible(true);
                        compressThresholdLabel->setVisible(true);
                        compressAttackLabel->setVisible(true);
                        compressReleaseLabel->setVisible(true);
                        compressRatioLabel->setVisible(true);
                        compressLofiLabel->setVisible(true);
                        compressMakeupGainLabel->setVisible(true);
                        compressWetLabel->setVisible(true);
                        
                        // Show value labels
                        compressDriveValueLabel->setVisible(true);
                        compressThresholdValueLabel->setVisible(true);
                        compressAttackValueLabel->setVisible(true);
                        compressReleaseValueLabel->setVisible(true);
                        compressRatioValueLabel->setVisible(true);
                        compressLofiValueLabel->setVisible(true);
                        compressMakeupGainValueLabel->setVisible(true);
                        compressWetValueLabel->setVisible(true);
                    } else {
                        // Hide COMPRESS+ sliders if disabled
                        compressDriveSlider->setVisible(false);
                        compressThresholdSlider->setVisible(false);
                        compressAttackSlider->setVisible(false);
                        compressReleaseSlider->setVisible(false);
                        compressRatioSlider->setVisible(false);
                        compressLofiSlider->setVisible(false);
                        compressMakeupGainSlider->setVisible(false);
                        compressWetSlider->setVisible(false);
                        
                        // Hide slider labels
                        compressDriveLabel->setVisible(false);
                        compressThresholdLabel->setVisible(false);
                        compressAttackLabel->setVisible(false);
                        compressReleaseLabel->setVisible(false);
                        compressRatioLabel->setVisible(false);
                        compressLofiLabel->setVisible(false);
                        compressMakeupGainLabel->setVisible(false);
                        compressWetLabel->setVisible(false);
                        
                        // Hide value labels
                        compressDriveValueLabel->setVisible(false);
                        compressThresholdValueLabel->setVisible(false);
                        compressAttackValueLabel->setVisible(false);
                        compressReleaseValueLabel->setVisible(false);
                        compressRatioValueLabel->setVisible(false);
                        compressLofiValueLabel->setVisible(false);
                        compressMakeupGainValueLabel->setVisible(false);
                        compressWetValueLabel->setVisible(false);
                    }
                }
            } catch (const std::exception& e) {
                DBG("[PresetBrowser] Exception: " + juce::String(e.what()));
            }
        };
        
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
            masterLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
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
            masterValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
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
        
        // Position PanManBar above the master knobs (70px wider [90-20], centered in master area, moved right 46px [30+16])
        const int panBarY = y - 285;
        const int originalVizWidth = (meterWidth * 2 + meterSpacing + totalKnobWidth - 20) - 90;
        const int newVizWidth = originalVizWidth + 78; // 78px wider (was 90, now -12 = 78)
        auto masterAreaFull = juce::Rectangle<int>(453, 54, 413, 296);
        const int vizX = masterAreaFull.getX() + (masterAreaFull.getWidth() - newVizWidth) / 2 + 48; // Centered in master area + 48px right
        panBar->setBounds(vizX, panBarY, newVizWidth, 24);
        
        // Create and position Output Visualizer (glowing waveform display)
        outputSpectrumView = std::make_unique<OutputSpectrumView>();
        addAndMakeVisible(*outputSpectrumView);
        
        // Connect drag-to-knob: dragging formant dots updates vowel knobs
        outputSpectrumView->onFormantDragged = [this](int formantIndex, float newFreq, float newQ)
        {
            // Map formant index to knob: 0=F1 (affects Vowel A), 1=F2 (affects Vowel A), 2=F3 (affects Vowel B)
            // For simplicity, let's just update Vowel A knob to indirectly control F1/F2
            // and Vowel B knob to control F2/F3
            
            DBG("[FORMANT DRAG] Formant " << formantIndex << " dragged to " << newFreq << " Hz, Q=" << newQ);
            
            // Update Resonance knob if dragging vertically (resonance adjustment)
            if (formantKnobs[1]) // Resonance knob is now index 1
            {
                formantKnobs[1]->setValue(newQ, juce::dontSendNotification);
            }
            
            // For now, just update the overlay - the knobs will update the overlay automatically
            // when they change, so dragging should just update visualization
        };
        
        // Connect spectrum analyzer to the view
        processorRef.spectrumAnalyzer.setOutputView(outputSpectrumView.get());
        
        // Position visualizer below PanManBar with 10px gap (70px wider, centered, moved right 46px)
        const int vizY = panBarY + 24 + 10; // Below PanManBar + 10px gap
        const int vizHeight = 190; // Reduced by 15px (was 205px) to move filter slider up
        outputSpectrumView->setBounds(vizX, vizY, newVizWidth, vizHeight);
        
        // Create and position spectrum filter slider (LP/HP control)
        spectrumFilterSlider = std::make_unique<SpectrumFilterSlider>();
        addAndMakeVisible(*spectrumFilterSlider);
        
        const int filterSliderY = vizY + vizHeight + 5; // 5px gap below spectrum
        const int filterSliderHeight = 20;
        spectrumFilterSlider->setBounds(vizX, filterSliderY, newVizWidth, filterSliderHeight);
        
        // Connect filter changes to both audio processor AND spectrum analyzer
        spectrumFilterSlider->onFilterChange = [this](float lowCut, float highCut) {
            // Update APVTS parameters (affects actual audio)
            auto* hpParam = processorRef.getAPVTS().getParameter("masterHPHz");
            auto* lpParam = processorRef.getAPVTS().getParameter("masterLPHz");
            
            if (hpParam)
                hpParam->setValueNotifyingHost(hpParam->convertTo0to1(lowCut));
            if (lpParam)
                lpParam->setValueNotifyingHost(lpParam->convertTo0to1(highCut));
            
            // Also update spectrum analyzer visualization
            processorRef.spectrumAnalyzer.setFilterFrequencies(lowCut, highCut);
        };
        
        // Create resonance knobs (HP on left, LP on right)
        const int resKnobSize = 30; // Small knobs
        const int resKnobY = filterSliderY + (filterSliderHeight - resKnobSize) / 2; // Vertically centered with filter bar
        
        // HP Resonance Knob (left side)
        hpResonanceKnob = std::make_unique<juce::Slider>();
        hpResonanceKnob->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        hpResonanceKnob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        hpResonanceKnob->setRange(0.5, 10.0, 0.01);
        hpResonanceKnob->setValue(0.707); // Butterworth default
        hpResonanceKnob->setBounds(vizX - resKnobSize - 6, resKnobY, resKnobSize, resKnobSize); // 6px left of filter bar (was 10, now -4 = 6)
        hpResonanceKnob->setLookAndFeel(&resonanceKnobLNF);
        addAndMakeVisible(*hpResonanceKnob);
        hpResonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), "masterHPQ", *hpResonanceKnob);
        
        // LP Resonance Knob (right side)
        lpResonanceKnob = std::make_unique<juce::Slider>();
        lpResonanceKnob->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        lpResonanceKnob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        lpResonanceKnob->setRange(0.5, 10.0, 0.01);
        lpResonanceKnob->setValue(0.707); // Butterworth default
        lpResonanceKnob->setBounds(vizX + newVizWidth + 6, resKnobY, resKnobSize, resKnobSize); // 6px right of filter bar (was 10, now -4 = 6)
        lpResonanceKnob->setLookAndFeel(&resonanceKnobLNF);
        addAndMakeVisible(*lpResonanceKnob);
        lpResonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), "masterLPQ", *lpResonanceKnob);
        
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
            macroKnobs[i]->setVisible(false); // Hide macro knobs for now
            
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
            macroLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
            macroLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
            macroLabels[i]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(macroLabels[i].get());
            macroLabels[i]->setBounds(startX, startY + i * spacing - 25 + yOffset, knobSize, 20);
            macroLabels[i]->setVisible(false); // Hide macro labels for now
            
            // Create macro assign button (15px wide, centered to the right of title)
            macroAssignButtons[i] = std::make_unique<juce::DrawableButton>("MacroAssign" + juce::String(i + 1), juce::DrawableButton::ButtonStyle::ImageStretched);
            addAndMakeVisible(macroAssignButtons[i].get());
            macroAssignButtons[i]->setVisible(false); // Hide macro assign buttons for now
            
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
        effectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
        
        // Update APVTS parameter
        auto* syncParam = processorRef.getAPVTS().getParameter("delaySync");
        if (syncParam) {
            syncParam->setValueNotifyingHost(timeSyncEnabled ? 1.0f : 0.0f);
        }
        
        if (timeSyncEnabled && knobs[0] != nullptr) {
            // Set knob range for sync mode (0-19 divisions = 20 choices)
            knobs[0]->setRange(0.0, 19.0, 1.0);
            // Read current division from parameter
            auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("delayTimeDiv"));
            if (divParam) {
                knobs[0]->setValue(static_cast<float>(divParam->getIndex()), juce::dontSendNotification);
            } else {
                knobs[0]->setValue(5.0, juce::dontSendNotification); // Start at 1/4 (index 5)
            }
        } else if (knobs[0] != nullptr) {
            // Switch to 1-2000ms range for non-sync mode
            knobs[0]->setRange(1.0, 2000.0, 1.0);
            // Read current time from parameter
            auto* timeParam = processorRef.getAPVTS().getRawParameterValue("timeMs");
            if (timeParam) {
                knobs[0]->setValue(timeParam->load(), juce::dontSendNotification);
            } else {
                knobs[0]->setValue(250.0, juce::dontSendNotification); // Start at 250ms
            }
        }
        
        // Update the time label display
        updateSpaceDelayTimeLabel();
        repaint();
    };
        
        DBG("[UI] Effects area setup complete");
    }

void PluginEditor::setupSpaceDelayUI()
{
    DBG("[UI] Setting up space delay UI...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // OLD spaceDelayTitle removed - replaced by dynamic tab button titles in router system
    
    // Create effect type dropdown (DEPRECATED - replaced by router dropdowns)
    effectTypeDropdown = std::make_unique<juce::ComboBox>();
    // Don't add to UI - replaced by new router effect selectors
    // addAndMakeVisible(effectTypeDropdown.get());
    
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
    effectTypeDropdown->addItem("Auto Pan", 2);
    effectTypeDropdown->addItem("Dirt", 3);
    effectTypeDropdown->addItem("Chorus", 4);
    effectTypeDropdown->addItem("Hall", 5);
    effectTypeDropdown->addItem("Grain", 6);
    effectTypeDropdown->addItem("Slicer", 7);
    effectTypeDropdown->addItem("Dub Echo", 8);
    effectTypeDropdown->addItem("Redux", 9);
    effectTypeDropdown->addItem("PhaseBloom", 10);
    effectTypeDropdown->addItem("Form", 12);
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
        
        // Update the specific effect parameter based on current page
        auto& router = processorRef.getEffectRouter();
        int slotIndex = static_cast<int>(currentPage);
        EffectID assignedEffect = router.getEffectInSlot(static_cast<SlotID>(slotIndex));
        
        switch (assignedEffect) {
            case EffectID::SpaceDelay: {
                auto* delayEnabledParam = processorRef.getAPVTS().getParameter("delayEnabled");
                if (delayEnabledParam) {
                    delayEnabledParam->setValueNotifyingHost(fxAreaEnabled ? 1.0f : 0.0f);
                }
                break;
            }
            // Add other effects as needed
            default:
                break;
        }
        
        repaint();
    };
}

void PluginEditor::updateFxAreaVisibility()
{
    float alpha = fxAreaEnabled ? 1.0f : 0.3f;

    // Grey title, effect dropdown and dice
    if (effectsTitle) effectsTitle->setAlpha(alpha);
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
        DBG("[UI] All Steps toggle: " + juce::String(allStepsEnabled ? "ON" : "OFF") + " toggleState=" + juce::String(allStepsToggle->getToggleState() ? 1 : 0));
        DBG("[UI] All Steps toggle clicked - current page: " + juce::String(static_cast<int>(currentPage)));
        
        // Test if All Steps is working by checking if we're on Space Delay page
        if (currentPage == FxPageID::SpaceDelay) {
            DBG("[UI] Space Delay All Steps toggle state: " + juce::String(allStepsEnabled ? "ON" : "OFF"));
        }
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
    stepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    
    // Set up step dice button callback to randomize all Space Delay step snapshots
    stepDiceButton->onClick = [this]() {
        DBG("[UI] Space Delay step dice button clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getSpaceDelaySafeSnapshot(step);
            
            // Randomize each parameter (respecting lock states from knobLocked array)
            if (!knobLocked[0]) {
                snapshot.delay.timeMs = juce::Random::getSystemRandom().nextFloat() * (2000.0f - 10.0f) + 10.0f; // 10-2000ms
            }
            if (!knobLocked[1]) {
                snapshot.delay.feedback = juce::Random::getSystemRandom().nextFloat() * 0.95f; // 0-0.95
            }
            if (!knobLocked[2]) {
                snapshot.delay.wowDepth = juce::Random::getSystemRandom().nextFloat(); // 0-1
            }
            if (!knobLocked[3]) {
                snapshot.delay.wowRate = juce::Random::getSystemRandom().nextFloat() * (8.0f - 0.1f) + 0.1f; // 0.1-8.0
            }
            if (!knobLocked[4]) {
                snapshot.delay.saturation = juce::Random::getSystemRandom().nextFloat(); // 0-1 (drive)
            }
            if (!knobLocked[5]) {
                snapshot.delay.highCut = juce::Random::getSystemRandom().nextFloat() * (20000.0f - 1000.0f) + 1000.0f; // 1000-20000Hz
            }
            if (!knobLocked[6]) {
                snapshot.delay.lowCut = juce::Random::getSystemRandom().nextFloat() * (2000.0f - 20.0f) + 20.0f; // 20-2000Hz
            }
            if (!knobLocked[7]) {
                snapshot.delay.mix = juce::Random::getSystemRandom().nextFloat(); // 0-1
            }
            
            processorRef.setSpaceDelayStepSnapshot(step, snapshot);
        }
        
        // Update UI to reflect changes
        updateSequencerUI();
        
        // Load the selected step's new values into the knobs
        int selectedStep = processorRef.getSpaceDelayUiSelectedStep();
        auto updatedSnapshot = processorRef.getSpaceDelaySafeSnapshot(selectedStep);
        
        // Load snapshot values into knobs using normalized values and sendNotification for proper UI updates
        if (knobs[0]) {
            auto* param = processorRef.getAPVTS().getParameter("timeMs");
            if (param) knobs[0]->setValue(param->convertTo0to1(updatedSnapshot.delay.timeMs), juce::sendNotification);
        }
        if (knobs[1]) {
            auto* param = processorRef.getAPVTS().getParameter("feedback");
            if (param) knobs[1]->setValue(param->convertTo0to1(updatedSnapshot.delay.feedback), juce::sendNotification);
        }
        if (knobs[2]) {
            auto* param = processorRef.getAPVTS().getParameter("wowDepth");
            if (param) knobs[2]->setValue(param->convertTo0to1(updatedSnapshot.delay.wowDepth), juce::sendNotification);
        }
        if (knobs[3]) {
            auto* param = processorRef.getAPVTS().getParameter("wowRate");
            if (param) knobs[3]->setValue(param->convertTo0to1(updatedSnapshot.delay.wowRate), juce::sendNotification);
        }
        if (knobs[4]) {
            auto* param = processorRef.getAPVTS().getParameter("drive");
            if (param) knobs[4]->setValue(param->convertTo0to1(updatedSnapshot.delay.saturation), juce::sendNotification);
        }
        if (knobs[5]) {
            auto* param = processorRef.getAPVTS().getParameter("hiCut");
            if (param) knobs[5]->setValue(param->convertTo0to1(updatedSnapshot.delay.highCut), juce::sendNotification);
        }
        if (knobs[6]) {
            auto* param = processorRef.getAPVTS().getParameter("lowCut");
            if (param) knobs[6]->setValue(param->convertTo0to1(updatedSnapshot.delay.lowCut), juce::sendNotification);
        }
        if (knobs[7]) {
            auto* param = processorRef.getAPVTS().getParameter("mix");
            if (param) knobs[7]->setValue(param->convertTo0to1(updatedSnapshot.delay.mix), juce::sendNotification);
        }
        
        // Update value labels
        for (int i = 0; i < 8; ++i) {
            if (valueLabels[i] && knobs[i]) {
                float value = knobs[i]->getValue();
                juce::String valueText;
                if (i == 0) valueText = juce::String(static_cast<int>(value)) + "ms";
                else if (i == 1) valueText = juce::String(value, 2);
                else if (i == 2) valueText = juce::String(value, 2);
                else if (i == 3) valueText = juce::String(value, 2) + "Hz";
                else if (i == 4) valueText = juce::String(value, 2);
                else if (i == 5) valueText = juce::String(static_cast<int>(value)) + "Hz";
                else if (i == 6) valueText = juce::String(static_cast<int>(value)) + "Hz";
                else if (i == 7) valueText = juce::String(value, 2);
                valueLabels[i]->setText(valueText, juce::dontSendNotification);
            }
        }
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
    // Force to 16 by default, then sync with processor state
    processorRef.setSpaceDelayStepsUsed(16);
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
            processorRef.setSpaceDelayStepsUsed(value);
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
            processorRef.setSpaceDelayDivisionIndex(newDivisionIndex);
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
            processorRef.setSpaceDelaySequencerEnabled(false);
            processorRef.resetSequencerState();
            DBG("[UI] Space Delay sequencer STOPPED by user");
        } else {
            // Enable sequencer when turning ON
            processorRef.setSpaceDelaySequencerEnabled(true);
            DBG("[UI] Space Delay sequencer ENABLED by user");
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
    // Safety: never update timeMs parameter (index 0) when in sync mode
    if (knobIndex == 0 && timeSyncEnabled) {
        DBG("[SYNC] Blocked updateParameterFromKnob for knob 0 in sync mode");
        return;
    }
    
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
        // Check if we're on Space Delay page and use dedicated sequencer
        bool isSpaceDelayPage = (currentPage == FxPageID::SpaceDelay);
        if (isSpaceDelayPage) {
            processorRef.updateSpaceDelayCurrentStepSnapshot(knobIndex, actualValue);
        } else {
            processorRef.updateCurrentStepSnapshot(knobIndex, actualValue);
        }
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
        
        // Check if we're on Space Delay page and use dedicated sequencer
        bool isSpaceDelayPage = (currentPage == FxPageID::SpaceDelay);
        
        DBG("[All Steps] isSpaceDelayPage=" << (isSpaceDelayPage ? "true" : "false") << " knobIndex=" << knobIndex 
            << " actualValue=" << actualValue);
        
        // Update all 16 step snapshots with the new value
        for (int step = 0; step < 16; ++step)
        {
            // Get current snapshot for this step (use dedicated sequencer for Space Delay)
            StepSnapshot snapshot = isSpaceDelayPage ? 
                processorRef.getSpaceDelaySafeSnapshot(step) : 
                processorRef.getSafeSnapshot(step);
            
            // Update the specific parameter in the snapshot with proper conversion (matching APVTS ranges)
            switch (knobIndex)
            {
                case 0: snapshot.delay.timeMs = actualValue; break; // 10-2000ms
                case 1: snapshot.delay.feedback = actualValue; break; // 0-0.95 (not percentage!)
                case 2: snapshot.delay.wowDepth = actualValue; break; // 0-1 (not percentage!)
                case 3: snapshot.delay.wowRate = actualValue; break; // 0.1-8.0
                case 4: snapshot.delay.saturation = actualValue; break; // 0-1 (not percentage!)
                case 5: snapshot.delay.highCut = actualValue; break; // 1000-20000Hz
                case 6: snapshot.delay.lowCut = actualValue; break; // 20-2000Hz
                case 7: snapshot.delay.mix = actualValue; break; // 0-1 (not percentage!)
            }
            
            // Set the updated snapshot back (use dedicated sequencer for Space Delay)
            if (isSpaceDelayPage) {
                processorRef.setSpaceDelayStepSnapshot(step, snapshot);
            } else {
            processorRef.setStepSnapshot(step, snapshot);
            }
        }
        
        DBG("[UI] Updated all 16 step snapshots for knob " << knobIndex << " with value " << actualValue);
    }
}

void PluginEditor::onStepButtonClicked(int stepIndex)
{
    DBG("[UI] Step button " << stepIndex << " clicked");
    
    // Save current step's snapshot before switching
    int currentStep = processorRef.getSpaceDelayUiSelectedStep();
    if (currentStep >= 0 && currentStep < 16) {
        StepSnapshot currentSnapshot;
        currentSnapshot.delay.timeMs = processorRef.getAPVTS().getParameter("timeMs")->convertFrom0to1(processorRef.getAPVTS().getParameter("timeMs")->getValue());
        currentSnapshot.delay.feedback = processorRef.getAPVTS().getParameter("feedback")->convertFrom0to1(processorRef.getAPVTS().getParameter("feedback")->getValue());
        currentSnapshot.delay.wowDepth = processorRef.getAPVTS().getParameter("wowDepth")->convertFrom0to1(processorRef.getAPVTS().getParameter("wowDepth")->getValue());
        currentSnapshot.delay.wowRate = processorRef.getAPVTS().getParameter("wowRate")->convertFrom0to1(processorRef.getAPVTS().getParameter("wowRate")->getValue());
        currentSnapshot.delay.saturation = processorRef.getAPVTS().getParameter("drive")->convertFrom0to1(processorRef.getAPVTS().getParameter("drive")->getValue());
        currentSnapshot.delay.highCut = processorRef.getAPVTS().getParameter("hiCut")->convertFrom0to1(processorRef.getAPVTS().getParameter("hiCut")->getValue());
        currentSnapshot.delay.lowCut = processorRef.getAPVTS().getParameter("lowCut")->convertFrom0to1(processorRef.getAPVTS().getParameter("lowCut")->getValue());
        currentSnapshot.delay.mix = processorRef.getAPVTS().getParameter("mix")->convertFrom0to1(processorRef.getAPVTS().getParameter("mix")->getValue());
        processorRef.setSpaceDelayStepSnapshot(currentStep, currentSnapshot);
        DBG("[UI] Saved current step " << currentStep << " snapshot before switching");
    }
    
    // Update selected step in processor
    processorRef.setSpaceDelaySelectedStep(stepIndex);
    
    // Update UI to show which step is selected
    updateSequencerUI();
    
    // Load the snapshot for this step into the knobs
    auto snapshot = processorRef.getSpaceDelaySafeSnapshot(stepIndex);
    
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
    knobs[1]->setValue(processorRef.getAPVTS().getParameter("feedback")->convertTo0to1(snapshot.delay.feedback), juce::dontSendNotification);
    knobs[2]->setValue(processorRef.getAPVTS().getParameter("wowDepth")->convertTo0to1(snapshot.delay.wowDepth), juce::dontSendNotification);
    knobs[3]->setValue(processorRef.getAPVTS().getParameter("wowRate")->convertTo0to1(snapshot.delay.wowRate), juce::dontSendNotification);
    knobs[4]->setValue(processorRef.getAPVTS().getParameter("drive")->convertTo0to1(snapshot.delay.saturation), juce::dontSendNotification);
    knobs[5]->setValue(processorRef.getAPVTS().getParameter("hiCut")->convertTo0to1(snapshot.delay.highCut), juce::dontSendNotification);
    knobs[6]->setValue(processorRef.getAPVTS().getParameter("lowCut")->convertTo0to1(snapshot.delay.lowCut), juce::dontSendNotification);
    knobs[7]->setValue(processorRef.getAPVTS().getParameter("mix")->convertTo0to1(snapshot.delay.mix), juce::dontSendNotification);
    
    // Also update the APVTS parameters to match the snapshot
    processorRef.getAPVTS().getParameter("timeMs")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("timeMs")->convertTo0to1(snapshot.delay.timeMs));
    processorRef.getAPVTS().getParameter("feedback")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("feedback")->convertTo0to1(snapshot.delay.feedback));
    processorRef.getAPVTS().getParameter("wowDepth")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("wowDepth")->convertTo0to1(snapshot.delay.wowDepth));
    processorRef.getAPVTS().getParameter("wowRate")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("wowRate")->convertTo0to1(snapshot.delay.wowRate));
    processorRef.getAPVTS().getParameter("drive")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("drive")->convertTo0to1(snapshot.delay.saturation));
    processorRef.getAPVTS().getParameter("hiCut")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("hiCut")->convertTo0to1(snapshot.delay.highCut));
    processorRef.getAPVTS().getParameter("lowCut")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("lowCut")->convertTo0to1(snapshot.delay.lowCut));
    processorRef.getAPVTS().getParameter("mix")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("mix")->convertTo0to1(snapshot.delay.mix));
}

void PluginEditor::updateSequencerUI()
{
    // Update step button selection states
    int selectedStep = processorRef.getSpaceDelayUiSelectedStep();
    int playingStep = processorRef.getSpaceDelayPlayingStep(); // Read from audio thread
    const int stepsUsed = processorRef.getSpaceDelaySeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (stepButtons[i] != nullptr) {
            // Only the selected step should show as selected
            stepButtons[i]->setSelected(i == selectedStep);
            // Show playing highlight only if sequencer is enabled
            bool sequencerEnabled = processorRef.getSpaceDelaySeqState().enabled.load();
            stepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            // Grey out inactive steps beyond stepsUsed
            bool shouldBeEnabled = i < stepsUsed;
            stepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't steal focus if editing)
    if (stepAmountLabel != nullptr && ! stepAmountLabel->isBeingEdited()) {
        int stepsUsed = processorRef.getSpaceDelaySeqState().stepsUsed.load();
        juce::String currentText = stepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            stepAmountLabel->setText(newText, juce::dontSendNotification);
        }
    }
    
    // Update rate dropdown
    if (rateDropdown != nullptr) {
        int divisionIndex = processorRef.getSpaceDelaySeqState().divisionIndex.load();
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
    
    // === CREATE TAB BUTTONS WITH DYNAMIC EFFECT TITLE SVGs ===
    // Use ImageFitted to remove any background and fit SVG to button bounds
    tabSpaceDelay = std::make_unique<juce::DrawableButton>("tabSlot1", juce::DrawableButton::ImageFitted);
    tabPanner = std::make_unique<juce::DrawableButton>("tabSlot2", juce::DrawableButton::ImageFitted);
    tabDirt = std::make_unique<juce::DrawableButton>("tabSlot3", juce::DrawableButton::ImageFitted);
    tabChorus = std::make_unique<juce::DrawableButton>("tabSlot4", juce::DrawableButton::ImageFitted);
    
    // Set initial tab images based on router assignment (will update dynamically)
    updateTabButtonImages();
    
    // Make tab backgrounds transparent (just show the SVG title)
    for (auto* tab : {tabSpaceDelay.get(), tabPanner.get(), tabDirt.get(), tabChorus.get()})
    {
        tab->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        tab->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    }
    
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
    
    // Position tabs - 20% smaller than current size (113→90, 33→26)
    const int tabW = 90;   // 20% smaller than current 113px
    const int tabH = 26;   // 20% smaller than current 33px
    // Custom spacing: 3rd moved left 16px, 4th moved left 24px
    // Moved down 5px: Y=0→5
    tabSpaceDelay->setBounds(12, 5, tabW, tabH);
    tabPanner->setBounds(148, 5, tabW, tabH);
    tabDirt->setBounds(268, 5, tabW, tabH);     // 284-16 = 268
    tabChorus->setBounds(396, 5, tabW, tabH);   // 420-24 = 396
    
    // Force tab buttons to clip their content to their bounds
    for (auto* tab : {tabSpaceDelay.get(), tabPanner.get(), tabDirt.get(), tabChorus.get()})
    {
        tab->setPaintingIsUnclipped(false);
    }
    
    DBG("[UI] Tab buttons created and added to editor");
    DBG("[UI] tabSpaceDelay bounds: " << tabSpaceDelay->getBounds().toString());
    DBG("[UI] tabPanner bounds: " << tabPanner->getBounds().toString());
    
    // === EFFECT SELECTOR DROPDOWNS ===
    // Create custom LookAndFeel for large popup menus with small buttons
    routerComboLNF = std::make_unique<RouterComboLookAndFeel>();
    
    // Create dropdowns next to each tab button for dynamic effect assignment
    effectSelector1 = std::make_unique<juce::ComboBox>("EffectSelector1");
    effectSelector2 = std::make_unique<juce::ComboBox>("EffectSelector2");
    effectSelector3 = std::make_unique<juce::ComboBox>("EffectSelector3");
    effectSelector4 = std::make_unique<juce::ComboBox>("EffectSelector4");
    
    // Populate dropdown items (1-based IDs for JUCE ComboBox)
    for (auto* selector : {effectSelector1.get(), effectSelector2.get(), effectSelector3.get(), effectSelector4.get()})
    {
        // Clear any existing items first
        selector->clear(juce::dontSendNotification);
        
        selector->addItem("Space Delay", 1);
        selector->addItem("Auto Pan", 2);
        selector->addItem("Dirt", 3);
        selector->addItem("Chorus", 4);
        selector->addItem("Hall", 5);
        selector->addItem("Grain", 6);
        selector->addItem("Slicer", 7);
        selector->addItem("Dub Echo", 8);
        selector->addItem("Redux", 9);
        selector->addItem("PhaseBloom", 10);
        selector->addItem("Form", 11);
        // Form2 removed - using Formant instead
        // selector->addItem("Form 2", 12); // EffectID::Form2 (11)
        selector->addItem("Heat", 13); // EffectID::Saturate (12)
        selector->addItem("Filter", 14); // EffectID::Filter (13)
        
        int numItems = selector->getNumItems();
        DBG("[UI] Added dropdown items - total: " << numItems);
        if (numItems >= 12) {
            DBG("[UI] Last item (index 11): " << selector->getItemText(11));
        }
        
        // Hide text when closed - just show carrot icon
        selector->setTextWhenNothingSelected("");
        selector->setTextWhenNoChoicesAvailable("");
        
        // Completely transparent - no ComboBox drawing, just our carrot SVG
        selector->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        selector->setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
        selector->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        selector->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
        selector->setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
        selector->setColour(juce::ComboBox::focusedOutlineColourId, juce::Colours::transparentBlack);
        
        // Popup menu styling (visible when opened)
        selector->setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF2A2A2A));
        selector->setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        selector->setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF4A4A4A));
        
        // Apply custom LookAndFeel for large popup menu with small button
        selector->setLookAndFeel(routerComboLNF.get());
    }
    
    // Position dropdowns as small carrot buttons (40% smaller = 12x12px)
    // Gap reduced by 20px (from 5px to 0px)
    const int carrotSize = 12;  // 40% smaller than 20px
    const int carrotY = 12;  // Moved down 5px: 7→12
    // Updated positions to match new tab positions (custom spacing)
    effectSelector1->setBounds(12 + tabW + 0, carrotY, carrotSize, carrotSize);
    effectSelector2->setBounds(148 + tabW + 0, carrotY, carrotSize, carrotSize);
    effectSelector3->setBounds(268 + tabW + 0, carrotY, carrotSize, carrotSize);  // 284-16 = 268
    effectSelector4->setBounds(396 + tabW + 0, carrotY, carrotSize, carrotSize);  // 420-24 = 396
    
    // Set initial selections based on current router assignment
    auto& router = processorRef.getEffectRouter();
    effectSelector1->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot1)) + 1, juce::dontSendNotification);
    effectSelector2->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot2)) + 1, juce::dontSendNotification);
    effectSelector3->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot3)) + 1, juce::dontSendNotification);
    effectSelector4->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot4)) + 1, juce::dontSendNotification);
    
    // Add change listeners (lambda captures slot index)
    effectSelector1->onChange = [this]() { onEffectSelectorChanged(0); };
    effectSelector2->onChange = [this]() { onEffectSelectorChanged(1); };
    effectSelector3->onChange = [this]() { onEffectSelectorChanged(2); };
    effectSelector4->onChange = [this]() { onEffectSelectorChanged(3); };
    
    // Make dropdowns always on top and non-clickable-through
    effectSelector1->setAlwaysOnTop(true);
    effectSelector2->setAlwaysOnTop(true);
    effectSelector3->setAlwaysOnTop(true);
    effectSelector4->setAlwaysOnTop(true);
    
    addAndMakeVisible(*effectSelector1);
    addAndMakeVisible(*effectSelector2);
    addAndMakeVisible(*effectSelector3);
    addAndMakeVisible(*effectSelector4);
    
    DBG("[UI] Effect selector dropdowns created and positioned");
    
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
    // OLD spaceDelayTitle removed - replaced by dynamic tab button titles
    // OLD effectTypeDropdown removed - replaced by router dropdowns
    if (diceButton) spaceDelayGroup.push_back(diceButton.get());
    if (timeSyncToggle) spaceDelayGroup.push_back(timeSyncToggle.get());
    if (fxPowerButton) spaceDelayGroup.push_back(fxPowerButton.get());
    
    // Add sequencer components
    if (spaceDelayAllStepsToggle) spaceDelayGroup.push_back(spaceDelayAllStepsToggle.get());
    if (spaceDelayAllStepsLabel) spaceDelayGroup.push_back(spaceDelayAllStepsLabel.get());
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
    
    // Populate Reverb group
    reverbGroup.clear();
    
    // Add Reverb knobs and related components
    for (int i = 0; i < 8; ++i) {
        if (reverbKnobs[i]) reverbGroup.push_back(reverbKnobs[i].get());
        if (reverbKnobLabels[i]) reverbGroup.push_back(reverbKnobLabels[i].get());
        if (reverbValueLabels[i]) reverbGroup.push_back(reverbValueLabels[i].get());
        if (reverbIndicatorBars[i]) reverbGroup.push_back(reverbIndicatorBars[i].get());
        if (reverbLockButtons[i]) reverbGroup.push_back(reverbLockButtons[i].get());
    }
    
    // Add Reverb effects area components
    if (reverbEffectsTitle) reverbGroup.push_back(reverbEffectsTitle.get());
    if (reverbDiceButton) reverbGroup.push_back(reverbDiceButton.get());
    if (reverbFxPowerButton) reverbGroup.push_back(reverbFxPowerButton.get());
    
    // Add Reverb sequencer components
    if (reverbAllStepsToggle) reverbGroup.push_back(reverbAllStepsToggle.get());
    if (reverbAllStepsLabel) reverbGroup.push_back(reverbAllStepsLabel.get());
    for (int i = 0; i < 16; ++i) {
        if (reverbStepButtons[i]) reverbGroup.push_back(reverbStepButtons[i].get());
    }
    if (reverbStepAmountLabel) reverbGroup.push_back(reverbStepAmountLabel.get());
    if (reverbRateDropdown) reverbGroup.push_back(reverbRateDropdown.get());
    if (reverbStdToggle) reverbGroup.push_back(reverbStdToggle.get());
    if (reverbStepTitle) reverbGroup.push_back(reverbStepTitle.get());
    if (reverbStepDiceButton) reverbGroup.push_back(reverbStepDiceButton.get());
    if (reverbStepPowerButton) reverbGroup.push_back(reverbStepPowerButton.get());
    
    // Initialize with SpaceDelay page visible
    showPage(FxPageID::SpaceDelay);
    
    DBG("[UI] Tab system setup complete. SpaceDelay components: " << spaceDelayGroup.size());
    DBG("[UI] AutoPan components: " << pannerGroup.size());
    DBG("[UI] Dirt components: " << dirtGroup.size());
    DBG("[UI] Chorus components: " << chorusGroup.size());
    DBG("[UI] Reverb components: " << reverbGroup.size());
    DBG("[UI] Granular components: " << granularGroup.size());
    DBG("[UI] Slicer components: " << slicerGroup.size());
}

void PluginEditor::showPage(FxPageID id)
{
    // Always allow the function to continue (for repaint and cascading backgrounds)
    // even if the same page is selected
    bool pageChanged = (currentPage != id);
    currentPage = id;

    // Update processor parameters only if page actually changed
    if (pageChanged)
    {
    auto* currentPageParam = processorRef.getAPVTS().getParameter("currentPage");
    if (currentPageParam) {
        float pageValue = 0.0f;
        if (id == FxPageID::Panner) pageValue = 1.0f;
        else if (id == FxPageID::Dirt) pageValue = 2.0f;
        else if (id == FxPageID::Chorus) pageValue = 3.0f;
        else if (id == FxPageID::Reverb) pageValue = 4.0f;
        else if (id == FxPageID::Granular) pageValue = 5.0f;
        else if (id == FxPageID::Slicer) pageValue = 6.0f;
        else if (id == FxPageID::Redux) pageValue = 8.0f; // Redux is index 8 in the parameter array
        else if (id == FxPageID::PhaseBloom) pageValue = 9.0f; // PhaseBloom is index 9 in the parameter array
        currentPageParam->setValueNotifyingHost(pageValue);
        }
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
    
    // Update Reverb UI to reflect current parameter state (don't force it on)
    if (id == FxPageID::Reverb) {
        auto* verbEnabledParam = processorRef.getAPVTS().getRawParameterValue("verbEnabled");
        if (verbEnabledParam) {
            reverbFxAreaEnabled = verbEnabledParam->load() > 0.5f;
            if (reverbFxPowerButton) {
                reverbFxPowerButton->setToggleState(reverbFxAreaEnabled, juce::dontSendNotification);
            }
        }
        updateReverbFxAreaVisibility();
    }
    
    // Update Granular UI to reflect current parameter state
    if (id == FxPageID::Granular) {
        auto* granEnabledParam = processorRef.getAPVTS().getRawParameterValue("granEnabled");
        if (granEnabledParam) {
            granularFxAreaEnabled = granEnabledParam->load() > 0.5f;
            if (granularFxPowerButton) {
                granularFxPowerButton->setToggleState(granularFxAreaEnabled, juce::dontSendNotification);
            }
        }
        updateGranularFxAreaVisibility();
        
        // Trigger initial value label updates
        for (int i = 0; i < 8; ++i) {
            if (granularKnobs[i]) {
                granularKnobs[i]->onValueChange();
            }
        }
    }

    // Show/Hide without touching parents or bounds
    auto setVisibleVec = [](const std::vector<juce::Component*>& v, bool vis)
    {
        DBG("[setVisibleVec] Setting " << std::to_string(v.size()) << " components to visible=" << (vis ? "true" : "false"));
        for (auto* c : v) {
            if (c) {
                DBG("[setVisibleVec] Setting component to visible=" << (vis ? "true" : "false"));
                c->setVisible(vis);
            } else {
                DBG("[setVisibleVec] Warning: null component in vector");
            }
        }
    };

    // === ROUTER-AWARE VISIBILITY ===
    // Show the effect assigned to the current slot, not hardcoded by page
    auto& router = processorRef.getEffectRouter();
    int slotIndex = static_cast<int>(id);  // Page maps to slot (0=Slot1, 1=Slot2, etc.)
    EffectID assignedEffect = router.getEffectInSlot(static_cast<SlotID>(slotIndex));
    
    // Hide all groups first
    setVisibleVec(spaceDelayGroup, false);
    setVisibleVec(pannerGroup, false);
    setVisibleVec(dirtGroup, false);
    setVisibleVec(chorusGroup, false);
    setVisibleVec(reverbGroup, false);
    setVisibleVec(granularGroup, false);
    setVisibleVec(slicerGroup, false);
    setVisibleVec(dubdelayGroup, false);
    setVisibleVec(reduxGroup, false);
    setVisibleVec(phaseBloomGroup, false);
    setVisibleVec(formantGroup, false);
    setVisibleVec(saturateGroup, false);
    setVisibleVec(form2Group, false);
    setVisibleVec(filterGroup, false);
    
    // Show only the group for the effect assigned to this slot
    switch (assignedEffect)
    {
        case EffectID::SpaceDelay:
            setVisibleVec(spaceDelayGroup, true);
            DBG("[ROUTER] Showing SpaceDelay UI for slot " << slotIndex);
            
            // Restore UI state from processor/APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("delayEnabled");
                fxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true;
                if (fxPowerButton) {
                    fxPowerButton->setToggleState(fxAreaEnabled, juce::dontSendNotification);
                }

                // Step power reflects processor sequencer enabled state
                stepAreaEnabled = processorRef.getSpaceDelaySeqState().enabled.load();
                if (stepPowerButton) {
                    stepPowerButton->setToggleState(stepAreaEnabled, juce::dontSendNotification);
                }

                // Update All Steps toggle state
                if (spaceDelayAllStepsToggle) {
                    spaceDelayAllStepsToggle->setToggleState(spaceDelayAllStepsEnabled, juce::dontSendNotification);
                }

                updateFxAreaVisibility();
                updateStepAreaVisibility();
            }
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (knobs[i]) {
                    knobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            processorRef.setSpaceDelaySelectedStep(0);
            updateSequencerUI();
            break;
        case EffectID::AutoPan:
            setVisibleVec(pannerGroup, true);
            DBG("[ROUTER] Showing AutoPan UI for slot " << slotIndex);
            break;
        case EffectID::Dirt:
            setVisibleVec(dirtGroup, true);
            DBG("[ROUTER] Showing Dirt UI for slot " << slotIndex);
            break;
        case EffectID::Chorus:
            setVisibleVec(chorusGroup, true);
            DBG("[ROUTER] Showing Chorus UI for slot " << slotIndex);
            break;
        case EffectID::Reverb:
            setVisibleVec(reverbGroup, true);
            DBG("[ROUTER] Showing Reverb UI for slot " << slotIndex);
            break;
        case EffectID::Granular:
            setVisibleVec(granularGroup, true);
            DBG("[ROUTER] Showing Granular UI for slot " << slotIndex);
            break;
        case EffectID::Slicer:
            setVisibleVec(slicerGroup, true);
            DBG("[ROUTER] Showing Slicer UI for slot " << slotIndex);
            
            // Restore UI state from processor/APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("slicerEnabled");
                slicerFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true;
                if (slicerFxPowerButton) {
                    slicerFxPowerButton->setToggleState(slicerFxAreaEnabled, juce::dontSendNotification);
                }

                // Step power reflects processor sequencer enabled state
                slicerStepAreaEnabled = processorRef.getSlicerSeqState().enabled.load();
                if (slicerStepPowerButton) {
                    slicerStepPowerButton->setToggleState(slicerStepAreaEnabled, juce::dontSendNotification);
                }

                updateSlicerFxAreaVisibility();
                updateSlicerStepAreaVisibility();
            }
            
            // Trigger initial value label updates (6 knobs, not 8)
            for (int i = 0; i < 6; ++i) {
                if (slicerKnobs[i]) {
                    slicerKnobs[i]->onValueChange();
                }
            }
            break;
        case EffectID::DubDelay:
            setVisibleVec(dubdelayGroup, true);
            DBG("[ROUTER] Showing DubDelay UI for slot " << slotIndex);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("dubEnabled");
                dubdelayFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                if (dubdelayFxPowerButton) {
                    dubdelayFxPowerButton->setToggleState(dubdelayFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                dubdelayStepAreaEnabled = processorRef.getDubDelaySeqState().enabled.load();
                if (dubdelayStepPowerButton) {
                    dubdelayStepPowerButton->setToggleState(dubdelayStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateDubDelayFxAreaVisibility();
                updateDubDelayStepAreaVisibility();
            }
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (dubdelayKnobs[i]) {
                    dubdelayKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            dubdelayUiSelectedStep = 0;
            processorRef.setDubDelaySelectedStep(0);
            updateDubDelaySequencerUI();
            
            break;
        case EffectID::Redux:
            DBG("[ROUTER] Redux case triggered for slot " << slotIndex);
            DBG("[ROUTER] reduxGroup size: " << reduxGroup.size());
            setVisibleVec(reduxGroup, true);
            DBG("[ROUTER] Showing Redux UI for slot " << slotIndex);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("reduxEnabled");
                reduxFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                // DBG("[ROUTER] Redux FX area enabled: " << reduxFxAreaEnabled);
                
                if (reduxFxPowerButton) {
                    reduxFxPowerButton->setToggleState(reduxFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                reduxStepAreaEnabled = true; // Start enabled
                if (reduxStepPowerButton) {
                    reduxStepPowerButton->setToggleState(reduxStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateReduxFxAreaVisibility();
                updateReduxStepAreaVisibility();
            }
            
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (reduxKnobs[i]) {
                    reduxKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            reduxUiSelectedStep = 0;
            updateReduxSequencerUI();
            
            break;
        case EffectID::PhaseBloom:
            setVisibleVec(phaseBloomGroup, true);
            DBG("[ROUTER] Showing PhaseBloom UI for slot " << slotIndex);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("phasebloomEnabled");
                phaseBloomFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true;
                
                if (phaseBloomFxPowerButton) {
                    phaseBloomFxPowerButton->setToggleState(phaseBloomFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                phaseBloomStepAreaEnabled = processorRef.getPhaseBloomSeqState().enabled.load();
                if (phaseBloomStepPowerButton) {
                    phaseBloomStepPowerButton->setToggleState(phaseBloomStepAreaEnabled, juce::dontSendNotification);
                }
                
                updatePhaseBloomFxAreaVisibility();
                updatePhaseBloomStepAreaVisibility();
            }
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (phaseBloomKnobs[i]) {
                    phaseBloomKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            phaseBloomUiSelectedStep = 0;
            processorRef.setPhaseBloomSelectedStep(0);
            updatePhaseBloomSequencerUI();
            
            break;
        case EffectID::Formant:
            DBG("[ROUTER] Formant case triggered - formantGroup size: " << formantGroup.size());
            setVisibleVec(formantGroup, true);
            DBG("[ROUTER] Showing Formant UI for slot " << slotIndex);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("formantEnabled");
                formantFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                
                if (formantFxPowerButton) {
                    formantFxPowerButton->setToggleState(formantFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                formantStepAreaEnabled = processorRef.getFormantSeqState().enabled.load();
                if (formantStepPowerButton) {
                    formantStepPowerButton->setToggleState(formantStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateFormantFxAreaVisibility();
                updateFormantStepAreaVisibility();
            }
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (formantKnobs[i]) {
                    formantKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            formantUiSelectedStep = 0;
            processorRef.setFormantSelectedStep(0);
            updateFormantSequencerUI();
            
            break;
        case EffectID::Saturate:
            DBG("[ROUTER] Showing Saturate UI for slot " << slotIndex);
            setVisibleVec(saturateGroup, true);
            
            // Reset processor to prevent pop when switching to Heat page
            processorRef.resetSaturateProcessor();
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("saturateEnabled");
                saturateFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true; // Default ON
                
                if (saturateFxPowerButton) {
                    saturateFxPowerButton->setToggleState(saturateFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                saturateStepAreaEnabled = processorRef.getSaturateSeqState().enabled.load();
                if (saturateStepPowerButton) {
                    saturateStepPowerButton->setToggleState(saturateStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateSaturateFxAreaVisibility();
                updateSaturateStepAreaVisibility();
            }
            
            {
                // Always Clean mode (Type parameter removed)
                // Update knob labels for Clean mode
                updateSaturateKnobLabels(0); // Always Clean mode
            }
            
            // Trigger initial value label updates (6 knobs)
            for (int i = 0; i < 6; ++i) {
                if (saturateKnobs[i]) {
                    saturateKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            saturateUiSelectedStep = 0;
            processorRef.setSaturateSelectedStep(0);
            updateSaturateSequencerUI();
            
            DBG("[ROUTER] ✓ Saturate group shown");
            break;
        // Form2 page removed - using Formant instead
        // case EffectID::Form2:
        //     ... (removed)
        //     break;
            
        case EffectID::Filter:
            DBG("[ROUTER] Showing Filter UI for slot " << slotIndex);
            setVisibleVec(filterGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("filterEnabled");
                filterFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true; // Default ON
                
                if (filterFxPowerButton) {
                    filterFxPowerButton->setToggleState(filterFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                filterStepAreaEnabled = processorRef.getFilterSeqState().enabled.load();
                if (filterStepPowerButton) {
                    filterStepPowerButton->setToggleState(filterStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateFilterFxAreaVisibility();
                updateFilterStepAreaVisibility();
            }
            
            // Trigger initial value label updates
            if (filterTypeKnob && filterTypeKnob->onValueChange) filterTypeKnob->onValueChange();
            if (filterSlopeKnob && filterSlopeKnob->onValueChange) filterSlopeKnob->onValueChange();
            for (int i = 0; i < 5; ++i) {
                if (filterKnobs[i] && filterKnobs[i]->onValueChange) {
                    filterKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            filterUiSelectedStep = 0;
            processorRef.setFilterSelectedStep(0);
            updateFilterSequencerUI();
            
            break;
    }

    // Raise the active tab to front
    if (id == FxPageID::SpaceDelay && tabSpaceDelay) tabSpaceDelay->toFront(false);
    else if (id == FxPageID::Panner && tabPanner) tabPanner->toFront(false);
    else if (id == FxPageID::Dirt && tabDirt) tabDirt->toFront(false);
    else if (id == FxPageID::Chorus && tabChorus) tabChorus->toFront(false);
    else if (id == FxPageID::PhaseBloom && tabPhaseBloom) tabPhaseBloom->toFront(false);
    else if (id == FxPageID::Formant && tabFormant) tabFormant->toFront(false);
    else if (id == FxPageID::Form2 && tabFormant) tabFormant->toFront(false); // Form 2 uses same tab as Formant
    
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
    else if (id == FxPageID::Reverb && reverbStepAmountLabel) {
        reverbStepAmountLabel->toFront(true);
        reverbStepAmountLabel->setWantsKeyboardFocus(true);
    }
    else if (id == FxPageID::PhaseBloom && phaseBloomStepAmountLabel) {
        phaseBloomStepAmountLabel->toFront(true);
        phaseBloomStepAmountLabel->setWantsKeyboardFocus(true);
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
            // Skip if loading from snapshot (prevents circular updates during randomization)
            if (isLoadingFromSnapshot.load())
                return;
            
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
        autopanKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        autopanKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        autopanKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(autopanKnobLabels[i].get());
        autopanKnobLabels[i]->setVisible(false); // Initially hidden until AutoPan page is selected
        autopanKnobLabels[i]->setBounds(x, y - 15, knobSize, 20); // EXACT same as delay page
        
        // Create value label (EXACT same positioning as delay page)
        autopanValueLabels[i] = std::make_unique<juce::Label>();
        autopanValueLabels[i]->setText("0", juce::dontSendNotification);
        autopanValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
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
    autopanEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    autopanStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    // Force to 16 by default, then sync with processor state
    processorRef.setAutoPanStepsUsed(16);
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
        DBG("[UI] AutoPan seq.enabled=" + juce::String(processorRef.getAutoPanSeqState().enabled.load() ? 1 : 0) 
            + " active=" + juce::String(processorRef.getAutoPanSeqState().active.load() ? 1 : 0));
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
        DBG("[UI] AutoPan All Steps toggle: " + juce::String(autopanAllStepsEnabled ? "ON" : "OFF") + " toggleState=" + juce::String(autopanAllStepsToggle->getToggleState() ? 1 : 0));
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

void PluginEditor::setupSpaceDelayAllStepsToggle()
{
    DBG("[UI] Setting up Space Delay All Steps toggle...");
    
    // Effect area bounds (EXACT same as delay page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "All Steps" toggle button - using AllStepsToggleButton like delay page
    spaceDelayAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(spaceDelayAllStepsToggle.get());
    spaceDelayAllStepsToggle->setVisible(false); // Initially hidden until Space Delay page is selected
    
    // Position button in EXACT same location as delay page
    const int buttonSize = 29; // 24 * 1.2 = 28.8, rounded to 29
    spaceDelayAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, effectArea.getY() - 1, buttonSize, buttonSize);
    
    // Set up images (EXACT same as delay page)
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr)
    {
        static_cast<AllStepsToggleButton*>(spaceDelayAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    // Set up click handler (EXACT same as delay page)
    spaceDelayAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    spaceDelayAllStepsToggle->onClick = [this]() {
        spaceDelayAllStepsEnabled = spaceDelayAllStepsToggle->getToggleState();
        DBG("[UI] Space Delay All Steps toggle: " + juce::String(spaceDelayAllStepsEnabled ? "ON" : "OFF") + " toggleState=" + juce::String(spaceDelayAllStepsToggle->getToggleState() ? 1 : 0));
    };
    
    // Create "All Steps" label
    spaceDelayAllStepsLabel = std::make_unique<juce::Label>();
    spaceDelayAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    spaceDelayAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold)); // 12.0f * 1.2 = 14.4f (20% bigger)
    spaceDelayAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    spaceDelayAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(spaceDelayAllStepsLabel.get());
    spaceDelayAllStepsLabel->setVisible(false); // Initially hidden until Space Delay page is selected
    
    // Position label to the right of the button, moved 30px right, moved up 4px
    spaceDelayAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, effectArea.getY() + 1, 80, 24); // Moved up 4px from 5 to 1
    
    DBG("[UI] Space Delay All Steps toggle setup complete");
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
        dirtKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        dirtKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        dirtKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(dirtKnobLabels[i].get());
        dirtKnobLabels[i]->setVisible(false);
        dirtKnobLabels[i]->setBounds(x, y - 15, knobSize, 20); // EXACT same as AutoPan page
        
        // Create value label (EXACT same positioning as AutoPan page)
        dirtValueLabels[i] = std::make_unique<juce::Label>();
        dirtValueLabels[i]->setText("0", juce::dontSendNotification);
        dirtValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
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
    dirtEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    dirtStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    // Force to 16 by default, then sync with processor state
    processorRef.setDirtStepsUsed(16);
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
    
    // Load new step's snapshot into knobs (CRITICAL: Use dontSendNotification to prevent All Steps trigger)
    StepSnapshot newSnapshot = processorRef.getAutoPanSafeSnapshot(stepIndex);
    if (autopanKnobs[0]) autopanKnobs[0]->setValue(newSnapshot.autopan.rate, juce::dontSendNotification);
    if (autopanKnobs[1]) autopanKnobs[1]->setValue(newSnapshot.autopan.phase, juce::dontSendNotification);
    if (autopanKnobs[2]) autopanKnobs[2]->setValue((float)newSnapshot.autopan.waveType, juce::dontSendNotification);
    if (autopanKnobs[3]) autopanKnobs[3]->setValue(newSnapshot.autopan.waveShape, juce::dontSendNotification);
    if (autopanKnobs[4]) autopanKnobs[4]->setValue(newSnapshot.autopan.inverted ? 1.0f : 0.0f, juce::dontSendNotification);
    if (autopanKnobs[5]) autopanKnobs[5]->setValue(newSnapshot.autopan.amount, juce::dontSendNotification);
    
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
    
    // Load new step's snapshot into knobs (CRITICAL: Use dontSendNotification to prevent All Steps trigger)
    StepSnapshot newSnapshot = processorRef.getDirtSafeSnapshot(stepIndex);
    if (dirtKnobs[0]) dirtKnobs[0]->setValue(newSnapshot.dirt.drive, juce::dontSendNotification);
    if (dirtKnobs[1]) dirtKnobs[1]->setValue(newSnapshot.dirt.color, juce::dontSendNotification);
    if (dirtKnobs[2]) dirtKnobs[2]->setValue(newSnapshot.dirt.asym, juce::dontSendNotification);
    if (dirtKnobs[3]) dirtKnobs[3]->setValue(newSnapshot.dirt.texture, juce::dontSendNotification);
    if (dirtKnobs[4]) dirtKnobs[4]->setValue(newSnapshot.dirt.lowCut, juce::dontSendNotification);
    if (dirtKnobs[5]) dirtKnobs[5]->setValue(newSnapshot.dirt.highCut, juce::dontSendNotification);
    if (dirtKnobs[6]) dirtKnobs[6]->setValue(newSnapshot.dirt.tone, juce::dontSendNotification);
    if (dirtKnobs[7]) dirtKnobs[7]->setValue(newSnapshot.dirt.mix, juce::dontSendNotification);
    
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
        juce::String currentText = dirtStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            dirtStepAmountLabel->setText(newText, false); // TextEditor uses bool, not notification enum
        }
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
        juce::String currentText = autopanStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            autopanStepAmountLabel->setText(newText, false); // TextEditor uses bool, not notification enum
        }
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
            // Skip if loading from snapshot (prevents circular updates during randomization)
            if (isLoadingFromSnapshot.load())
                return;
            
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
        chorusKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        chorusKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        chorusKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(chorusKnobLabels[i].get());
        chorusKnobLabels[i]->setVisible(false);
        chorusKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);

        // Create value label
        chorusValueLabels[i] = std::make_unique<juce::Label>();
        chorusValueLabels[i]->setText("0", juce::dontSendNotification);
        chorusValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
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
    chorusEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    chorusStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
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
    // Force to 16 by default, then sync with processor state
    processorRef.setChorusStepsUsed(16);
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
    if (chorusKnobs[0]) chorusKnobs[0]->setValue(newSnapshot.chorus.delayTime, juce::dontSendNotification);  // Delay
    if (chorusKnobs[1]) chorusKnobs[1]->setValue(newSnapshot.chorus.rate, juce::dontSendNotification);        // Rate
    if (chorusKnobs[2]) chorusKnobs[2]->setValue(newSnapshot.chorus.depth, juce::dontSendNotification);       // Depth
    if (chorusKnobs[3]) chorusKnobs[3]->setValue(newSnapshot.chorus.feedback, juce::dontSendNotification);    // Feedback
    if (chorusKnobs[4]) chorusKnobs[4]->setValue(newSnapshot.chorus.voices, juce::dontSendNotification);      // Voices
    if (chorusKnobs[5]) chorusKnobs[5]->setValue(newSnapshot.chorus.width, juce::dontSendNotification);       // Width
    if (chorusKnobs[6]) chorusKnobs[6]->setValue(newSnapshot.chorus.tone, juce::dontSendNotification);        // Shape
    if (chorusKnobs[7]) chorusKnobs[7]->setValue(newSnapshot.chorus.mix, juce::dontSendNotification);         // Mix
    
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
        juce::String currentText = chorusStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            chorusStepAmountLabel->setText(newText, false);
        }
    }
    
    if (chorusRateDropdown != nullptr) {
        int divisionIndex = processorRef.getChorusSeqState().divisionIndex.load();
        chorusRateDropdown->setSelectedId(divisionIndex + 1);
    }
}

// ===============================================================================
// REVERB PAGE SETUP METHODS
// ===============================================================================

void PluginEditor::setupReverbKnobs()
{
    DBG("[UI] Setting up Reverb knobs...");

    // Reverb knob names (8 knobs): Width, Size, Predelay, Damping, Diffusion, Early, Decay, Mix
    std::vector<juce::String> reverbKnobNames = {
        "Width", "Size", "Predelay", "Damping", "Diffusion", "Early", "Decay", "Mix"
    };

    // Effect area bounds (EXACT same as Chorus page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    for (int i = 0; i < 8; ++i)
    {
        reverbKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(reverbKnobs[i].get());
        reverbKnobs[i]->setVisible(false);
        
        reverbKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        reverbKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Set parameter ranges based on knob index
        switch (i) {
            case 0: // Width (0-1: mono to wide)
                reverbKnobs[i]->setRange(0.0, 1.0, 0.01);
                reverbKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 1: // Size (0.1-1.5)
                reverbKnobs[i]->setRange(0.1, 1.5, 0.01);
                reverbKnobs[i]->setValue(0.7, juce::dontSendNotification);
                break;
            case 2: // Predelay (0-200ms)
                reverbKnobs[i]->setRange(0.0, 200.0, 0.1);
                reverbKnobs[i]->setValue(20.0, juce::dontSendNotification);
                break;
            case 3: // Damping (1000-20000 Hz)
                reverbKnobs[i]->setRange(1000.0, 20000.0, 10.0);
                reverbKnobs[i]->setValue(8000.0, juce::dontSendNotification);
                break;
            case 4: // Diffusion (0-1)
                reverbKnobs[i]->setRange(0.0, 1.0, 0.01);
                reverbKnobs[i]->setValue(0.7, juce::dontSendNotification);
                break;
            case 5: // Early reflections (0-1)
                reverbKnobs[i]->setRange(0.0, 1.0, 0.01);
                reverbKnobs[i]->setValue(0.35, juce::dontSendNotification);
                break;
            case 6: // Decay (0.2-20s)
                reverbKnobs[i]->setRange(0.2, 20.0, 0.1);
                reverbKnobs[i]->setValue(4.0, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                reverbKnobs[i]->setRange(0.0, 1.0, 0.01);
                reverbKnobs[i]->setValue(0.25, juce::dontSendNotification);
                break;
        }

        reverbKnobs[i]->onValueChange = [this, i]() {
            if (reverbKnobs[i] != nullptr) {
                updateReverbParameterFromKnob(i);

                if (reverbAllStepsEnabled) {
                    // Update all steps with current knob value
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getReverbSafeSnapshot(step);
                        float value = reverbKnobs[i]->getValue();
                        switch (i) {
                            case 0: snapshot.reverb.type = value; break;
                            case 1: snapshot.reverb.size = value; break;
                            case 2: snapshot.reverb.predelayMs = value; break;
                            case 3: snapshot.reverb.dampHz = value; break;
                            case 4: snapshot.reverb.diffusion = value; break;
                            case 5: snapshot.reverb.early = value; break;
                            case 6: snapshot.reverb.decaySec = value; break;
                            case 7: snapshot.reverb.mix = value; break;
                        }
                        processorRef.setReverbStepSnapshot(step, snapshot);
                    }
                }
            }
        };

        if (assets.knobRing != nullptr)
            reverbKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            reverbKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);

        if (i < 4)
            y -= 23;
        else
            y -= 1;

        reverbKnobs[i]->setBounds(x, y, knobSize, knobSize);

        // Create label
        reverbKnobLabels[i] = std::make_unique<juce::Label>();
        reverbKnobLabels[i]->setText(reverbKnobNames[i], juce::dontSendNotification);
        reverbKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        reverbKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        reverbKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(reverbKnobLabels[i].get());
        reverbKnobLabels[i]->setVisible(false);
        reverbKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);

        // Create value label
        reverbValueLabels[i] = std::make_unique<juce::Label>();
        reverbValueLabels[i]->setText("0", juce::dontSendNotification);
        reverbValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        reverbValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        reverbValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(reverbValueLabels[i].get());
        reverbValueLabels[i]->setVisible(false);
        reverbValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);

        // Create indicator bar
        reverbIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(reverbIndicatorBars[i].get());
        reverbIndicatorBars[i]->setVisible(false);
        reverbIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        reverbIndicatorBars[i]->setValue(0.5f);

        // Create dice button
        reverbDiceButtons[i] = std::make_unique<CustomDiceButton>();
        reverbDiceButtons[i]->onClick = [this, i]() { randomizeIndividualReverbKnob(i); };

        // Create lock button
        reverbLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(reverbLockButtons[i].get());
        reverbLockButtons[i]->setVisible(false);
        
        const int diceSize = 10;
        const int diceSpacing = 5;
        
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(reverbKnobNames[i]);
        
        int lockX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
        int lockY = y - 10;
        
        reverbLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);
        
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            reverbLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        
        reverbLockButtons[i]->setToggleState(reverbKnobLocked[i], juce::dontSendNotification);
        reverbLockButtons[i]->onClick = [this, i]() {
            reverbKnobLocked[i] = reverbLockButtons[i]->getToggleState();
            DBG("[UI] Reverb knob " << i << " lock: " << (reverbKnobLocked[i] ? "LOCKED" : "UNLOCKED"));
        };
    }

    // Create parameter attachments to connect knobs to APVTS
    std::vector<juce::String> reverbParamIds = {
        "verbWidth", "verbSize", "verbPredelayMs", "verbDampHz", 
        "verbDiffusion", "verbEarlyLevel", "verbDecaySec", "verbMix"
    };
    
    for (int i = 0; i < 8; ++i) {
        reverbAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), reverbParamIds[i], *reverbKnobs[i]);
    }

    DBG("[UI] Reverb knobs setup complete");
}

void PluginEditor::setupReverbEffectsArea()
{
    DBG("[UI] Setting up Reverb effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title
    reverbEffectsTitle = std::make_unique<juce::Label>();
    reverbEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    reverbEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    reverbEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    reverbEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(reverbEffectsTitle.get());
    reverbEffectsTitle->setVisible(false);
    reverbEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Create dice button
    reverbDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(reverbDiceButton.get());
    reverbDiceButton->setVisible(false);
    
    const int diceSize = 32;
    reverbDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    
    if (assets.diceLarge != nullptr)
    {
        reverbDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    reverbDiceButton->onClick = [this]() {
        DBG("[UI] Reverb dice button clicked - randomizing all knobs");
        randomizeReverbKnobValues();
    };
    
    // Create FX power button (EXACT same as Chorus)
    reverbFxPowerButton = std::make_unique<juce::DrawableButton>("reverbFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(reverbFxPowerButton.get());
    reverbFxPowerButton->setVisible(false);

    const int buttonSize = 46;
    reverbFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                 effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);

    reverbFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    reverbFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn != nullptr) {
        reverbFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }
    
    reverbFxPowerButton->setClickingTogglesState(true);
    
    auto* verbEnabledParam = processorRef.getAPVTS().getRawParameterValue("verbEnabled");
    if (verbEnabledParam) {
        reverbFxAreaEnabled = verbEnabledParam->load() > 0.5f;
        reverbFxPowerButton->setToggleState(reverbFxAreaEnabled, juce::dontSendNotification);
    }
    
    reverbFxPowerButton->onClick = [this]() {
        reverbFxAreaEnabled = reverbFxPowerButton->getToggleState();
        DBG("[UI] Reverb FX power: " << (reverbFxAreaEnabled ? "ON" : "OFF"));
        
        auto* param = processorRef.getAPVTS().getParameter("verbEnabled");
        if (param) {
            param->setValueNotifyingHost(reverbFxAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateReverbFxAreaVisibility();
    };
    
    DBG("[UI] Reverb effects area setup complete");
}

void PluginEditor::setupReverbSequencerArea()
{
    DBG("[UI] Setting up Reverb sequencer area...");
    
    // Use EXACT same bounds as Dirt/Chorus sequencer (bottom area)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    reverbStepTitle = std::make_unique<juce::Label>();
    reverbStepTitle->setText("STEP", juce::dontSendNotification);
    reverbStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    reverbStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    reverbStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(reverbStepTitle.get());
    reverbStepTitle->setVisible(false);
    reverbStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step buttons (2 rows of 8)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i)
    {
        reverbStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(reverbStepButtons[i].get());
        reverbStepButtons[i]->setVisible(false);
        
        // 2 rows of 8 buttons (same as Dirt/Chorus)
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        reverbStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        if (assets.stepActive != nullptr) {
            reverbStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive != nullptr) {
            reverbStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        reverbStepButtons[i]->onClick = [this, i]() { onReverbStepButtonClicked(i); };
    }
    
    // Step amount label (TextEditor)
    reverbStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setReverbStepsUsed(16);
    reverbStepAmountLabel->setText("16");
    reverbStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    reverbStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    reverbStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    reverbStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    reverbStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    reverbStepAmountLabel->setJustification(juce::Justification::centred);
    reverbStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    reverbStepAmountLabel->setIndents(0, 0);
    reverbStepAmountLabel->setInputRestrictions(2, "0123456789");
    reverbStepAmountLabel->setWantsKeyboardFocus(true);
    reverbStepAmountLabel->setMouseClickGrabsKeyboardFocus(true);
    reverbStepAmountLabel->setCaretVisible(true);
    reverbStepAmountLabel->setPopupMenuEnabled(false);
    reverbStepAmountLabel->setScrollbarsShown(false);
    reverbStepAmountLabel->setMultiLine(false);
    reverbStepAmountLabel->setReturnKeyStartsNewLine(false);
    reverbStepAmountLabel->setInterceptsMouseClicks(true, false);
    reverbStepAmountLabel->setAlwaysOnTop(true);
    
    reverbStepAmountLabel->onReturnKey = [this]() {
        int value = juce::jlimit(1, 16, reverbStepAmountLabel->getText().getIntValue());
        processorRef.setReverbStepsUsed(value);
        reverbStepAmountLabel->setText(juce::String(value), false);
        updateReverbSequencerUI();
        reverbStepAmountLabel->giveAwayKeyboardFocus();
    };
    
    reverbStepAmountLabel->onFocusLost = [this]() {
        int value = juce::jlimit(1, 16, reverbStepAmountLabel->getText().getIntValue());
        processorRef.setReverbStepsUsed(value);
        reverbStepAmountLabel->setText(juce::String(value), false);
        updateReverbSequencerUI();
    };
    
    reverbStepAmountLabel->onTextChange = [this]() {
        // Empty - validation happens on return/focus lost
    };
    
    addAndMakeVisible(reverbStepAmountLabel.get());
    reverbStepAmountLabel->setVisible(false);
    reverbStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    reverbStepAmountLabel->setAlwaysOnTop(true);
    
    // Rate dropdown (EXACT same as Dirt)
    reverbRateDropdown = std::make_unique<juce::ComboBox>();
    
    reverbRateDropdown->addItem("4", 1);
    reverbRateDropdown->addItem("2", 2);
    reverbRateDropdown->addItem("1", 3);
    reverbRateDropdown->addItem("1/2", 4);
    reverbRateDropdown->addItem("1/4", 5);
    reverbRateDropdown->addItem("1/8", 6);
    reverbRateDropdown->addItem("1/16", 7);
    reverbRateDropdown->addItem("1/32", 8);
    
    reverbRateDropdown->setSelectedId(6);
    reverbRateDropdown->onChange = [this]() {
        int selectedIndex = reverbRateDropdown->getSelectedId() - 1;
        processorRef.setReverbDivisionIndex(selectedIndex);
        DBG("[UI] Reverb rate changed to index: " << selectedIndex);
    };
    
    // Make dropdown background and outline transparent
    reverbRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    reverbRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    reverbRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    
    addAndMakeVisible(reverbRateDropdown.get());
    reverbRateDropdown->setVisible(false);
    reverbRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    // STD toggle (EXACT same positioning as Dirt)
    reverbStdToggle = std::make_unique<CircularToggleButton>();
    reverbStdToggle->setButtonText("-");
    addAndMakeVisible(reverbStdToggle.get());
    reverbStdToggle->setVisible(false);
    reverbStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    reverbStdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        
        switch (stdState) {
            case 0: reverbStdToggle->setButtonText("-"); break;
            case 1: reverbStdToggle->setButtonText("t"); break;
            case 2: reverbStdToggle->setButtonText("."); break;
        }
        
        int nextMode = (stdState) % 3;
        processorRef.setReverbStdMode(nextMode);
        DBG("[UI] Reverb STD mode: " << nextMode);
    };
    
    // Step dice button (EXACT same positioning as Dirt)
    reverbStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(reverbStepDiceButton.get());
    reverbStepDiceButton->setVisible(false);
    int reverbStepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    reverbStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, reverbStepDiceSize, reverbStepDiceSize);
    
    if (assets.diceLarge != nullptr) {
        reverbStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    reverbStepDiceButton->onClick = [this]() {
        DBG("[UI] Reverb step dice clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getReverbSafeSnapshot(step);
            
            if (!reverbKnobLocked[0]) snapshot.reverb.type = juce::Random::getSystemRandom().nextFloat() * 2.0f;
            if (!reverbKnobLocked[1]) snapshot.reverb.size = 0.1f + juce::Random::getSystemRandom().nextFloat() * 1.4f;
            if (!reverbKnobLocked[2]) snapshot.reverb.predelayMs = juce::Random::getSystemRandom().nextFloat() * 200.0f;
            if (!reverbKnobLocked[3]) snapshot.reverb.dampHz = 1000.0f + juce::Random::getSystemRandom().nextFloat() * 19000.0f;
            if (!reverbKnobLocked[4]) snapshot.reverb.diffusion = juce::Random::getSystemRandom().nextFloat();
            if (!reverbKnobLocked[5]) snapshot.reverb.early = juce::Random::getSystemRandom().nextFloat();
            if (!reverbKnobLocked[6]) snapshot.reverb.decaySec = 0.2f + juce::Random::getSystemRandom().nextFloat() * 19.8f;
            if (!reverbKnobLocked[7]) snapshot.reverb.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setReverbStepSnapshot(step, snapshot);
        }
        
        // Update UI to show the new values
        updateReverbSequencerUI();
    };
    
    // Step power button (EXACT same positioning as Dirt)
    reverbStepPowerButton = std::make_unique<juce::DrawableButton>("reverbStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(reverbStepPowerButton.get());
    reverbStepPowerButton->setVisible(false);
    
    const int powerButtonSize = 40;
    reverbStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - powerButtonSize - 5 + 15 - 5 - 1, 
                                   sequencerArea.getY() - 5 - 40 + 25 + 5, powerButtonSize, powerButtonSize);
    
    reverbStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    reverbStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn != nullptr) {
        reverbStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    reverbStepPowerButton->setClickingTogglesState(true);
    reverbStepAreaEnabled = processorRef.getReverbSeqState().enabled.load();
    reverbStepPowerButton->setToggleState(reverbStepAreaEnabled, juce::dontSendNotification);
    
    reverbStepPowerButton->onClick = [this]() {
        reverbStepAreaEnabled = reverbStepPowerButton->getToggleState();
        DBG("[UI] Reverb sequencer power: " << (reverbStepAreaEnabled ? "ON" : "OFF"));
        processorRef.setReverbSequencerEnabled(reverbStepAreaEnabled);
        updateReverbStepAreaVisibility();
    };
    
    DBG("[UI] Reverb sequencer area setup complete");
}

void PluginEditor::setupReverbAllStepsToggle()
{
    DBG("[UI] Setting up Reverb All Steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    reverbAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(reverbAllStepsToggle.get());
    reverbAllStepsToggle->setVisible(false);
    
    // Match Dirt's exact positioning and size
    const int buttonSize = 29;
    reverbAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                  effectArea.getY() - 1, buttonSize, buttonSize);
    
    // Set the toggle images (stepTopInactive/stepTopActive)
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr) {
        static_cast<AllStepsToggleButton*>(reverbAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    reverbAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    reverbAllStepsToggle->onClick = [this]() {
        reverbAllStepsEnabled = reverbAllStepsToggle->getToggleState();
        DBG("[UI] Reverb All Steps toggle: " << (reverbAllStepsEnabled ? "ON" : "OFF"));
        reverbAllStepsLabel->setAlpha(reverbAllStepsEnabled ? 1.0f : 0.5f);
        
        if (reverbAllStepsEnabled) {
            DBG("[UI] Reverb All Steps enabled - randomizing all step snapshots");
            
            for (int step = 0; step < 16; ++step) {
                auto snapshot = processorRef.getReverbSafeSnapshot(step);
                
                if (!reverbKnobLocked[0]) snapshot.reverb.type = juce::Random::getSystemRandom().nextFloat() * 2.0f;
                if (!reverbKnobLocked[1]) snapshot.reverb.size = 0.1f + juce::Random::getSystemRandom().nextFloat() * 1.4f;
                if (!reverbKnobLocked[2]) snapshot.reverb.predelayMs = juce::Random::getSystemRandom().nextFloat() * 200.0f;
                if (!reverbKnobLocked[3]) snapshot.reverb.dampHz = 1000.0f + juce::Random::getSystemRandom().nextFloat() * 19000.0f;
                if (!reverbKnobLocked[4]) snapshot.reverb.diffusion = juce::Random::getSystemRandom().nextFloat();
                if (!reverbKnobLocked[5]) snapshot.reverb.early = juce::Random::getSystemRandom().nextFloat();
                if (!reverbKnobLocked[6]) snapshot.reverb.decaySec = 0.2f + juce::Random::getSystemRandom().nextFloat() * 19.8f;
                if (!reverbKnobLocked[7]) snapshot.reverb.mix = juce::Random::getSystemRandom().nextFloat();
                
                processorRef.setReverbStepSnapshot(step, snapshot);
            }
            
            // Update UI to show the new values
            updateReverbSequencerUI();
        }
    };
    
    // Label - match Dirt's exact positioning
    reverbAllStepsLabel = std::make_unique<juce::Label>();
    reverbAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    reverbAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    reverbAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    reverbAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(reverbAllStepsLabel.get());
    reverbAllStepsLabel->setVisible(false);
    reverbAllStepsLabel->setAlpha(1.0f); // Start fully visible (will grey when toggled off)
    reverbAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                effectArea.getY() + 1, 80, 24);
    
    DBG("[UI] Reverb All Steps toggle setup complete");
}

void PluginEditor::updateReverbFxAreaVisibility()
{
    float alpha = reverbFxAreaEnabled ? 1.0f : 0.3f;
    
    if (reverbEffectsTitle) reverbEffectsTitle->setAlpha(alpha);
    if (reverbDiceButton) { reverbDiceButton->setAlpha(alpha); reverbDiceButton->setEnabled(reverbFxAreaEnabled); }
    
    for (int i = 0; i < 8; ++i) {
        if (reverbKnobs[i]) { reverbKnobs[i]->setAlpha(alpha); reverbKnobs[i]->setEnabled(reverbFxAreaEnabled); }
        if (reverbKnobLabels[i]) reverbKnobLabels[i]->setAlpha(alpha);
        if (reverbValueLabels[i]) reverbValueLabels[i]->setAlpha(alpha);
        if (reverbIndicatorBars[i]) reverbIndicatorBars[i]->setAlpha(alpha);
        if (reverbLockButtons[i]) { 
            reverbLockButtons[i]->setEnabled(reverbFxAreaEnabled);
            reverbLockButtons[i]->setAlpha(alpha);
        }
    }
    
    if (reverbAllStepsToggle) { reverbAllStepsToggle->setAlpha(alpha); reverbAllStepsToggle->setEnabled(reverbFxAreaEnabled); }
    if (reverbAllStepsLabel) reverbAllStepsLabel->setAlpha(reverbAllStepsEnabled && reverbFxAreaEnabled ? 1.0f : 0.3f);
}

void PluginEditor::updateReverbStepAreaVisibility()
{
    float alpha = reverbStepAreaEnabled ? 1.0f : 0.3f;
    
    if (reverbStepTitle) reverbStepTitle->setAlpha(alpha);
    if (reverbStepDiceButton) { reverbStepDiceButton->setAlpha(alpha); reverbStepDiceButton->setEnabled(reverbStepAreaEnabled); }
    if (reverbStepAmountLabel) reverbStepAmountLabel->setAlpha(alpha);
    if (reverbRateDropdown) { reverbRateDropdown->setAlpha(alpha); reverbRateDropdown->setEnabled(reverbStepAreaEnabled); }
    if (reverbStdToggle) { reverbStdToggle->setAlpha(alpha); reverbStdToggle->setEnabled(reverbStepAreaEnabled); }
    
    for (int i = 0; i < 16; ++i) {
        if (reverbStepButtons[i]) { 
            reverbStepButtons[i]->setAlpha(alpha); 
            reverbStepButtons[i]->setEnabled(reverbStepAreaEnabled);
        }
    }
}

void PluginEditor::randomizeReverbKnobValues()
{
    for (int i = 0; i < 8; ++i) {
        if (!reverbKnobLocked[i] && reverbKnobs[i] != nullptr) {
            randomizeIndividualReverbKnob(i);
        }
    }
}

void PluginEditor::randomizeIndividualReverbKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || reverbKnobs[knobIndex] == nullptr) return;
    if (reverbKnobLocked[knobIndex]) return;
    
    juce::Random& rng = juce::Random::getSystemRandom();
    float randomValue = 0.0f;
    
    switch (knobIndex) {
        case 0: randomValue = rng.nextFloat(); break; // Width (0-1)
        case 1: randomValue = 0.1f + rng.nextFloat() * 1.4f; break; // Size (0.1-1.5)
        case 2: randomValue = rng.nextFloat() * 200.0f; break; // Predelay (0-200)
        case 3: randomValue = 1000.0f + rng.nextFloat() * 19000.0f; break; // Damping (1k-20k)
        case 4: randomValue = rng.nextFloat(); break; // Diffusion (0-1)
        case 5: randomValue = rng.nextFloat(); break; // Early (0-1)
        case 6: randomValue = 0.2f + rng.nextFloat() * 19.8f; break; // Decay (0.2-20s)
        case 7: randomValue = rng.nextFloat(); break; // Mix (0-1)
    }
    
    reverbKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
}

void PluginEditor::updateReverbParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || reverbKnobs[knobIndex] == nullptr) return;
    
    float value = reverbKnobs[knobIndex]->getValue();
    
    // Update current step snapshot
    int currentStep = reverbUiSelectedStep;
    if (currentStep >= 0 && currentStep < 16) {
        auto currentSnapshot = processorRef.getReverbSafeSnapshot(currentStep);
        
        switch (knobIndex) {
            case 0: currentSnapshot.reverb.type = value; break;
            case 1: currentSnapshot.reverb.size = value; break;
            case 2: currentSnapshot.reverb.predelayMs = value; break;
            case 3: currentSnapshot.reverb.dampHz = value; break;
            case 4: currentSnapshot.reverb.diffusion = value; break;
            case 5: currentSnapshot.reverb.early = value; break;
            case 6: currentSnapshot.reverb.decaySec = value; break;
            case 7: currentSnapshot.reverb.mix = value; break;
        }
        
        processorRef.setReverbStepSnapshot(currentStep, currentSnapshot);
    }
}


void PluginEditor::onReverbStepButtonClicked(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= 16) return;
    
    reverbUiSelectedStep = stepIndex;
    DBG("[UI] Reverb step " << stepIndex << " selected");
    
    auto snapshot = processorRef.getReverbSafeSnapshot(stepIndex);
    
    for (int i = 0; i < 8; ++i) {
        if (reverbKnobs[i] != nullptr) {
            float value = 0.0f;
            switch (i) {
                case 0: value = snapshot.reverb.type; break;
                case 1: value = snapshot.reverb.size; break;
                case 2: value = snapshot.reverb.predelayMs; break;
                case 3: value = snapshot.reverb.dampHz; break;
                case 4: value = snapshot.reverb.diffusion; break;
                case 5: value = snapshot.reverb.early; break;
                case 6: value = snapshot.reverb.decaySec; break;
                case 7: value = snapshot.reverb.mix; break;
            }
            reverbKnobs[i]->setValue(value, juce::dontSendNotification);
        }
    }
    
    updateReverbSequencerUI();
}

void PluginEditor::updateReverbSequencerUI()
{
    int selectedStep = reverbUiSelectedStep;
    int playingStep = processorRef.getReverbPlayingStep();
    const int stepsUsed = processorRef.getReverbSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (reverbStepButtons[i] != nullptr) {
            reverbStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getReverbSeqState().enabled.load();
            reverbStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
            bool shouldBeEnabled = i < stepsUsed;
            reverbStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    if (reverbStepAmountLabel != nullptr && !reverbStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = reverbStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            reverbStepAmountLabel->setText(newText, false);
        }
    }
    
    if (reverbRateDropdown != nullptr) {
        int divisionIndex = processorRef.getReverbSeqState().divisionIndex.load();
        reverbRateDropdown->setSelectedId(divisionIndex + 1);
    }
}

// ===============================================================================
// EFFECT ROUTER UI HELPERS
// ===============================================================================

juce::ComboBox* PluginEditor::getEffectSelectorForSlot(int slotIndex)
{
    switch (slotIndex)
    {
        case 0: return effectSelector1.get();
        case 1: return effectSelector2.get();
        case 2: return effectSelector3.get();
        case 3: return effectSelector4.get();
        default: return nullptr;
    }
}

void PluginEditor::onEffectSelectorChanged(int slotIndex)
{
    DBG("[ROUTER] ========== Effect selector changed for slot " << slotIndex << " ==========");
    
    auto* selector = getEffectSelectorForSlot(slotIndex);
    if (!selector) {
        DBG("[ROUTER] ERROR: Null selector for slot " << slotIndex);
        return;
    }
    
    int selectedId = selector->getSelectedId(); // ComboBox IDs are 1-based (1, 2, 3, ..., 14)
    DBG("[ROUTER] Selected dropdown ID: " << selectedId);
    
    // Map dropdown IDs to EffectIDs
    // Dropdown IDs: 1-11 map sequentially to EffectIDs 0-10 (SpaceDelay through Formant)
    // Dropdown ID 12 maps to EffectID 11 (Form2)
    // Dropdown ID 13 maps to EffectID 12 (Saturate/Heat)
    // Dropdown ID 14 maps to EffectID 13 (Filter)
    int selectedEffectID;
    if (selectedId >= 1 && selectedId <= 11) {
        selectedEffectID = selectedId - 1; // SpaceDelay(0) through Formant(10)
    } else if (selectedId == 12) {
        selectedEffectID = 11; // Form2 (EffectID::Form2)
    } else if (selectedId == 13) {
        selectedEffectID = 12; // Saturate/Heat (EffectID::Saturate)
    } else if (selectedId == 14) {
        selectedEffectID = 13; // Filter (EffectID::Filter)
    } else {
        DBG("[ROUTER] ERROR: Invalid dropdown ID " << selectedId);
        return;
    }
    
    DBG("[ROUTER] Mapped to EffectID: " << selectedEffectID);
    
    auto& router = processorRef.getEffectRouter();
    EffectID targetEffect = static_cast<EffectID>(selectedEffectID);
    SlotID targetSlot = static_cast<SlotID>(slotIndex);
    
    // Get the effect currently in target slot (before swap)
    EffectID oldEffect = router.getEffectInSlot(targetSlot);
    DBG("[ROUTER] Old effect in slot: " << static_cast<int>(oldEffect));
    
    // Check if we're selecting the same effect (no-op)
    if (targetEffect == oldEffect)
    {
        DBG("[ROUTER] No change - same effect selected");
        return;
    }
    
    // ===== PREFLIGHT CHECKS FOR REVERB =====
    if (targetEffect == EffectID::Reverb)
    {
        DBG("[ROUTER] Reverb selected - running preflight checks...");
        
        // Check APVTS parameters
        if (!processorRef.getAPVTS().getParameter("verbWidth") ||
            !processorRef.getAPVTS().getParameter("verbSize") ||
            !processorRef.getAPVTS().getParameter("verbPredelayMs") ||
            !processorRef.getAPVTS().getParameter("verbDampHz") ||
            !processorRef.getAPVTS().getParameter("verbDiffusion") ||
            !processorRef.getAPVTS().getParameter("verbEarlyLevel") ||
            !processorRef.getAPVTS().getParameter("verbDecaySec") ||
            !processorRef.getAPVTS().getParameter("verbMix") ||
            !processorRef.getAPVTS().getParameter("verbEnabled"))
        {
            DBG("[ROUTER] ERROR: Missing Reverb APVTS parameters! Cannot assign.");
            jassertfalse;
            return;
        }
        DBG("[ROUTER] ✓ All Reverb APVTS parameters exist");
        
        // Check background assets for this slot
        int tabNum = slotIndex + 1;
        juce::Drawable* testBg = nullptr;
        if (tabNum == 1) testBg = assets.reverbBackgroundTab1.get();
        else if (tabNum == 2) testBg = assets.reverbBackgroundTab2.get();
        else if (tabNum == 3) testBg = assets.reverbBackgroundTab3.get();
        else if (tabNum == 4) testBg = assets.reverbBackgroundTab4.get();
        
        if (!testBg) {
            DBG("[ROUTER] ERROR: Missing Reverb background asset for tab " << tabNum);
            jassertfalse;
            return;
        }
        DBG("[ROUTER] ✓ Reverb background asset exists for tab " << tabNum);
        
        // Check if Reverb knobs exist
        bool allKnobsExist = true;
        for (int i = 0; i < 8; ++i) {
            if (!reverbKnobs[i]) {
                DBG("[ROUTER] ERROR: Reverb knob " << i << " is null!");
                allKnobsExist = false;
            }
        }
        if (!allKnobsExist) {
            DBG("[ROUTER] ERROR: Some Reverb knobs are null! Cannot assign.");
            jassertfalse;
            return;
        }
        DBG("[ROUTER] ✓ All 8 Reverb knobs exist");
    }
    
    // This triggers a swap if the effect is already used elsewhere
    DBG("[ROUTER] Assigning effect " << static_cast<int>(targetEffect) << " to slot " << slotIndex);
    router.assignEffectToSlot(targetEffect, targetSlot);
    DBG("[ROUTER] ✓ Router assignment complete");
    
    // Update all dropdowns to reflect the swap (skip the one that was just changed)
    updateAllEffectSelectors(slotIndex);
    DBG("[ROUTER] ✓ Dropdowns updated");
    
    // Update backgrounds for affected slots
    DBG("[ROUTER] Updating backgrounds...");
    updateBackgroundsAfterSwap();
    DBG("[ROUTER] ✓ Backgrounds updated");
    
    // Force refresh the UI visibility (bypass the early return in showPage)
    // Hide all groups first
    DBG("[ROUTER] Hiding all groups...");
    auto setVisibleVec = [](const std::vector<juce::Component*>& v, bool vis)
    {
        for (auto* c : v) if (c) c->setVisible(vis);
    };
    
    setVisibleVec(spaceDelayGroup, false);
    setVisibleVec(pannerGroup, false);
    setVisibleVec(dirtGroup, false);
    setVisibleVec(chorusGroup, false);
    setVisibleVec(reverbGroup, false);
    setVisibleVec(granularGroup, false);
    setVisibleVec(slicerGroup, false);
    setVisibleVec(dubdelayGroup, false);
    setVisibleVec(reduxGroup, false);
    setVisibleVec(phaseBloomGroup, false);
    setVisibleVec(formantGroup, false);
    setVisibleVec(saturateGroup, false);
    setVisibleVec(form2Group, false);
    setVisibleVec(filterGroup, false);
    DBG("[ROUTER] ✓ All groups hidden");
    
    // Show the correct effect for the current page based on new assignment
    EffectID newAssignment = router.getEffectInSlot(static_cast<SlotID>(static_cast<int>(currentPage)));
    DBG("[ROUTER] New assignment for current page: " << static_cast<int>(newAssignment));
    DBG("[ROUTER] reverbGroup size: " << reverbGroup.size());
    
    switch (newAssignment)
    {
        case EffectID::SpaceDelay:
            DBG("[ROUTER] Showing SpaceDelay group");
            setVisibleVec(spaceDelayGroup, true);
            break;
        case EffectID::AutoPan:
            DBG("[ROUTER] Showing AutoPan group");
            setVisibleVec(pannerGroup, true);
            break;
        case EffectID::Dirt:
            DBG("[ROUTER] Showing Dirt group");
            setVisibleVec(dirtGroup, true);
            break;
        case EffectID::Chorus:
            DBG("[ROUTER] Showing Chorus group");
            setVisibleVec(chorusGroup, true);
            break;
        case EffectID::Reverb:
            DBG("[ROUTER] Showing Reverb group (" << reverbGroup.size() << " components)");
            setVisibleVec(reverbGroup, true);
            DBG("[ROUTER] ✓ Reverb group shown");
            break;
        case EffectID::Granular:
            DBG("[ROUTER] Showing Granular group (" << granularGroup.size() << " components)");
            setVisibleVec(granularGroup, true);
            DBG("[ROUTER] ✓ Granular group shown");
            break;
        case EffectID::Slicer:
            DBG("[ROUTER] Showing Slicer group (" << slicerGroup.size() << " components)");
            setVisibleVec(slicerGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("slicerEnabled");
                slicerFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true;
                if (slicerFxPowerButton) {
                    slicerFxPowerButton->setToggleState(slicerFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state from processor
                slicerStepAreaEnabled = processorRef.getSlicerSeqState().enabled.load();
                if (slicerStepPowerButton) {
                    slicerStepPowerButton->setToggleState(slicerStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateSlicerFxAreaVisibility();
                updateSlicerStepAreaVisibility();
            }
            
            // Trigger initial value label updates (6 knobs, not 8)
            for (int i = 0; i < 6; ++i) {
                if (slicerKnobs[i]) {
                    slicerKnobs[i]->onValueChange();
                }
            }
            
            DBG("[ROUTER] ✓ Slicer group shown");
            break;
        case EffectID::DubDelay:
            DBG("[ROUTER] Showing DubDelay group (" << dubdelayGroup.size() << " components)");
            setVisibleVec(dubdelayGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("dubEnabled");
                dubdelayFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                if (dubdelayFxPowerButton) {
                    dubdelayFxPowerButton->setToggleState(dubdelayFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                dubdelayStepAreaEnabled = processorRef.getDubDelaySeqState().enabled.load();
                if (dubdelayStepPowerButton) {
                    dubdelayStepPowerButton->setToggleState(dubdelayStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateDubDelayFxAreaVisibility();
                updateDubDelayStepAreaVisibility();
            }
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (dubdelayKnobs[i]) {
                    dubdelayKnobs[i]->onValueChange();
                }
            }
            
            DBG("[ROUTER] ✓ DubDelay group shown");
            break;
        case EffectID::Redux:
        {
            DBG("[ROUTER] Showing Redux group (" << reduxGroup.size() << " components)");
            setVisibleVec(reduxGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("reduxEnabled");
                reduxFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                if (reduxFxPowerButton) {
                    reduxFxPowerButton->setToggleState(reduxFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                reduxStepAreaEnabled = true; // Start enabled
                if (reduxStepPowerButton) {
                    reduxStepPowerButton->setToggleState(reduxStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateReduxFxAreaVisibility();
                updateReduxStepAreaVisibility();
            }
            
            // Trigger initial value label updates (8 knobs)
            for (int i = 0; i < 8; ++i) {
                if (reduxKnobs[i] && reduxAttachments[i]) {
                    reduxKnobs[i]->onValueChange();
                } else if (i == 0 && reduxKnobs[i]) {
                    // Bit depth knob - manually trigger value change without APVTS
                    reduxKnobs[i]->onValueChange();
                }
            }
            
            // Load current snapshot values into knobs with proper conversion
            StepSnapshot currentSnapshot = processorRef.getReduxSafeSnapshot(0); // Load step 0
            if (reduxKnobs[0]) {
                float uiBitDepth = (float)currentSnapshot.redux.bitDepth - 3.0f;
                reduxKnobs[0]->setValue(uiBitDepth, juce::dontSendNotification);
            }
            if (reduxKnobs[1]) reduxKnobs[1]->setValue((float)currentSnapshot.redux.sampleRateReduction, juce::dontSendNotification);
            if (reduxKnobs[2]) reduxKnobs[2]->setValue(currentSnapshot.redux.jitter, juce::dontSendNotification);
            if (reduxKnobs[3]) reduxKnobs[3]->setValue(currentSnapshot.redux.preFilter, juce::dontSendNotification);
            if (reduxKnobs[4]) reduxKnobs[4]->setValue(currentSnapshot.redux.postFilter, juce::dontSendNotification);
            if (reduxKnobs[5]) reduxKnobs[5]->setValue(currentSnapshot.redux.drive, juce::dontSendNotification);
            if (reduxKnobs[6]) reduxKnobs[6]->setValue(currentSnapshot.redux.emphasis, juce::dontSendNotification);
            if (reduxKnobs[7]) reduxKnobs[7]->setValue(currentSnapshot.redux.mix, juce::dontSendNotification);
            
            DBG("[ROUTER] ✓ Redux group shown");
            break;
        }
        case EffectID::PhaseBloom:
        {
            DBG("[ROUTER] Showing PhaseBloom group (" << phaseBloomGroup.size() << " components)");
            setVisibleVec(phaseBloomGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("phasebloomEnabled");
                phaseBloomFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true;
                if (phaseBloomFxPowerButton) {
                    phaseBloomFxPowerButton->setToggleState(phaseBloomFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                phaseBloomStepAreaEnabled = processorRef.getPhaseBloomSeqState().enabled.load();
                if (phaseBloomStepPowerButton) {
                    phaseBloomStepPowerButton->setToggleState(phaseBloomStepAreaEnabled, juce::dontSendNotification);
                }
                
                updatePhaseBloomFxAreaVisibility();
                updatePhaseBloomStepAreaVisibility();
            }
            
            // Load current snapshot values into sliders
            StepSnapshot phaseBloomSnapshot = processorRef.getPhaseBloomSafeSnapshot(0); // Load step 0
            if (phaseBloomKnobs[0]) phaseBloomKnobs[0]->setValue(phaseBloomSnapshot.phasebloom.depth, juce::dontSendNotification);
            if (phaseBloomKnobs[1]) phaseBloomKnobs[1]->setValue(phaseBloomSnapshot.phasebloom.rate, juce::dontSendNotification);
            if (phaseBloomKnobs[2]) phaseBloomKnobs[2]->setValue(phaseBloomSnapshot.phasebloom.feedback, juce::dontSendNotification);
            if (phaseBloomKnobs[3]) phaseBloomKnobs[3]->setValue(phaseBloomSnapshot.phasebloom.center, juce::dontSendNotification);
            if (phaseBloomKnobs[4]) phaseBloomKnobs[4]->setValue(phaseBloomSnapshot.phasebloom.bloom, juce::dontSendNotification);
            if (phaseBloomKnobs[5]) phaseBloomKnobs[5]->setValue(phaseBloomSnapshot.phasebloom.spread, juce::dontSendNotification);
            if (phaseBloomKnobs[6]) phaseBloomKnobs[6]->setValue(phaseBloomSnapshot.phasebloom.resonance, juce::dontSendNotification);
            if (phaseBloomKnobs[7]) phaseBloomKnobs[7]->setValue(phaseBloomSnapshot.phasebloom.mix, juce::dontSendNotification);
            
            DBG("[ROUTER] ✓ PhaseBloom group shown");
            break;
        }
        case EffectID::Formant:
        {
            DBG("[ROUTER] Showing Formant group (" << formantGroup.size() << " components)");
            setVisibleVec(formantGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("formantEnabled");
                formantFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                if (formantFxPowerButton) {
                    formantFxPowerButton->setToggleState(formantFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                formantStepAreaEnabled = processorRef.getFormantSeqState().enabled.load();
                if (formantStepPowerButton) {
                    formantStepPowerButton->setToggleState(formantStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateFormantFxAreaVisibility();
                updateFormantStepAreaVisibility();
            }
            
            // Load current snapshot values into knobs (now only 4)
            StepSnapshot formantSnapshot = processorRef.getFormantSafeSnapshot(0); // Load step 0
            if (formantKnobs[0]) formantKnobs[0]->setValue((float)formantSnapshot.formant.vowel, juce::dontSendNotification);
            if (formantKnobs[1]) formantKnobs[1]->setValue(formantSnapshot.formant.resonance, juce::dontSendNotification);
            if (formantKnobs[2]) formantKnobs[2]->setValue(formantSnapshot.formant.intensity, juce::dontSendNotification);
            if (formantKnobs[3]) formantKnobs[3]->setValue(formantSnapshot.formant.mix, juce::dontSendNotification);
            
            DBG("[ROUTER] ✓ Formant group shown");
            
            // Enable formant overlay on spectrum analyzer
            updateFormantOverlay();
            break;
        }
        case EffectID::Saturate:
        {
            DBG("[ROUTER] Showing Saturate group (" << saturateGroup.size() << " components)");
            setVisibleVec(saturateGroup, true);
            
            // Restore UI state from APVTS parameters
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("saturateEnabled");
                saturateFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : true; // Default ON
                
                if (saturateFxPowerButton) {
                    saturateFxPowerButton->setToggleState(saturateFxAreaEnabled, juce::dontSendNotification);
                }
                
                // Restore sequencer enabled state
                saturateStepAreaEnabled = processorRef.getSaturateSeqState().enabled.load();
                if (saturateStepPowerButton) {
                    saturateStepPowerButton->setToggleState(saturateStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateSaturateFxAreaVisibility();
                updateSaturateStepAreaVisibility();
            }
            
            // Trigger initial value label updates (6 knobs)
            for (int i = 0; i < 6; ++i) {
                if (saturateKnobs[i]) {
                    saturateKnobs[i]->onValueChange();
                }
            }
            
            // Update sequencer UI to show first step as selected
            saturateUiSelectedStep = 0;
            processorRef.setSaturateSelectedStep(0);
            updateSaturateSequencerUI();
            
            DBG("[ROUTER] ✓ Saturate group shown");
            break;
        }
        // Form2 page removed - using Formant instead
        // Form2 page removed - convert to Formant if Form2 is assigned
        case EffectID::Form2:
        {
            // Show Formant instead of Form2
            DBG("[ROUTER] Form2 detected - showing Formant instead");
            setVisibleVec(formantGroup, true);
            
            // Restore UI state from APVTS parameters (use Formant parameters)
            {
                auto* fxEnabledParam = processorRef.getAPVTS().getRawParameterValue("formantEnabled");
                formantFxAreaEnabled = fxEnabledParam ? (fxEnabledParam->load() > 0.5f) : false;
                if (formantFxPowerButton) {
                    formantFxPowerButton->setToggleState(formantFxAreaEnabled, juce::dontSendNotification);
                }
                
                formantStepAreaEnabled = processorRef.getFormantSeqState().enabled.load();
                if (formantStepPowerButton) {
                    formantStepPowerButton->setToggleState(formantStepAreaEnabled, juce::dontSendNotification);
                }
                
                updateFormantFxAreaVisibility();
                updateFormantStepAreaVisibility();
            }
            
            // Load Formant snapshot values into knobs (Formant has 4 knobs: vowel, resonance, intensity, mix)
            StepSnapshot formantSnapshot = processorRef.getFormantSafeSnapshot(0);
            if (formantKnobs[0]) formantKnobs[0]->setValue((float)formantSnapshot.formant.vowel, juce::dontSendNotification);
            if (formantKnobs[1]) formantKnobs[1]->setValue(formantSnapshot.formant.resonance, juce::dontSendNotification);
            if (formantKnobs[2]) formantKnobs[2]->setValue(formantSnapshot.formant.intensity, juce::dontSendNotification);
            if (formantKnobs[3]) formantKnobs[3]->setValue(formantSnapshot.formant.mix, juce::dontSendNotification);
            
            formantUiSelectedStep = 0;
            updateFormantSequencerUI();
            break;
        }
    }
    
    // Update formant overlay (disable if not Formant effect)
    updateFormantOverlay();
    
    // Call showPage to ensure proper initialization and visibility for current page
    // This ensures all components are properly hidden/shown and initialized
    DBG("[ROUTER] Calling showPage to refresh current page...");
    showPage(currentPage);
    
    // Repaint to show new background
    DBG("[ROUTER] Calling repaint...");
    repaint();
    DBG("[ROUTER] ✓ Repaint complete");
    
    DBG("[ROUTER] ========== Swap complete. Router version: " << router.getRouterVersion() << " ==========");
}

void PluginEditor::updateAllEffectSelectors()
{
    updateAllEffectSelectors(-1); // -1 means update all
}

void PluginEditor::updateAllEffectSelectors(int skipSlot)
{
    auto& router = processorRef.getEffectRouter();
    
    // Update each dropdown to show its current assignment (without triggering onChange)
    for (int i = 0; i < 4; ++i)
    {
        if (i == skipSlot) continue; // Skip the slot that was just changed
        
        auto* selector = getEffectSelectorForSlot(i);
        if (selector)
        {
            EffectID effect = router.getEffectInSlot(static_cast<SlotID>(i));
            int comboId = static_cast<int>(effect) + 1; // ComboBox IDs are 1-based
            selector->setSelectedId(comboId, juce::dontSendNotification);
        }
    }
}

void PluginEditor::updateBackgroundsAfterSwap()
{
    // Background update will happen in paint() method
    // which reads from router dynamically
    DBG("[ROUTER] Background update triggered (will apply in next paint)");
    
    // Update tab button images to show correct effect titles
    updateTabButtonImages();
}

void PluginEditor::updateTabButtonImages()
{
    // Icons are now drawn as part of the cascading background system
    // Tab buttons no longer need individual icons since they're "glued" to their backgrounds
    // This function is kept for potential future use but currently does nothing
    
    DBG("[ROUTER] Tab button images updated (icons now drawn in cascading backgrounds)");
}
//==============================================================================
// Granular Page Implementation
//==============================================================================

void PluginEditor::setupGranularKnobs()
{
    DBG("[UI] Setting up Granular knobs...");

    // Granular knob names (8 knobs)
    std::vector<juce::String> granularKnobNames = {
        "Size", "Density", "Position", "Spray", "Pitch", "Random", "Texture", "Mix"
    };

    // Effect area bounds (EXACT same as Reverb page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    for (int i = 0; i < 8; ++i)
    {
        granularKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(granularKnobs[i].get());
        granularKnobs[i]->setVisible(false);
        
        granularKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        granularKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Set parameter ranges based on knob index
        switch (i) {
            case 0: // Size (5-200ms)
                granularKnobs[i]->setRange(5.0, 200.0, 0.1);
                granularKnobs[i]->setValue(80.0, juce::dontSendNotification); // Longer for smoothness
                break;
            case 1: // Density (1-40 Hz)
                granularKnobs[i]->setRange(1.0, 40.0, 0.1); // Max 40Hz
                granularKnobs[i]->setValue(8.0, juce::dontSendNotification); // Lower for clarity
                break;
            case 2: // Position (0-1)
                granularKnobs[i]->setRange(0.0, 1.0, 0.01);
                granularKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 3: // Spray (0-200ms)
                granularKnobs[i]->setRange(0.0, 200.0, 0.1);
                granularKnobs[i]->setValue(20.0, juce::dontSendNotification); // Less spray
                break;
            case 4: // Pitch (-24 to +24 semitones)
                granularKnobs[i]->setRange(-24.0, 24.0, 0.01);
                granularKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 5: // Random (0-1)
                granularKnobs[i]->setRange(0.0, 1.0, 0.01);
                granularKnobs[i]->setValue(0.15, juce::dontSendNotification); // Less chaos
                break;
            case 6: // Texture (0-1)
                granularKnobs[i]->setRange(0.0, 1.0, 0.01);
                granularKnobs[i]->setValue(0.2, juce::dontSendNotification); // Smoother window
                break;
            case 7: // Mix (0-1)
                granularKnobs[i]->setRange(0.0, 1.0, 0.01);
                granularKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
        }
        
        // Add value change callback to update value label and save to snapshot
        granularKnobs[i]->onValueChange = [this, i]() {
            if (granularKnobs[i] != nullptr) {
                // Mix (knob 7) is global, not saved to snapshots
                if (i != 7) {
                    updateGranularParameterFromKnob(i);
                }
                
                if (granularAllStepsEnabled && i != 7) {
                    // Update all steps with current knob value (except mix)
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getGranularSafeSnapshot(step);
                        float value = granularKnobs[i]->getValue();
                        switch (i) {
                            case 0: snapshot.granular.sizeMs = value; break;
                            case 1: snapshot.granular.densityHz = value; break;
                            case 2: snapshot.granular.position = value; break;
                            case 3: snapshot.granular.sprayMs = value; break;
                            case 4: snapshot.granular.pitchSemi = value; break;
                            case 5: snapshot.granular.random = value; break;
                            case 6: snapshot.granular.texture = value; break;
                        }
                        processorRef.setGranularStepSnapshot(step, snapshot);
                    }
                }
                
                // Update value label
                if (granularValueLabels[i]) {
                    float value = granularKnobs[i]->getValue();
                    juce::String valueText;
                    
                    switch (i) {
                        case 0: valueText = juce::String(value, 1) + "ms"; break; // Size
                        case 1: valueText = juce::String(value, 1) + "Hz"; break; // Density
                        case 2: valueText = juce::String(int(value * 100)) + "%"; break; // Position
                        case 3: valueText = juce::String(value, 1) + "ms"; break; // Spray
                        case 4: valueText = juce::String(value, 1) + "st"; break; // Pitch (semitones)
                        case 5: valueText = juce::String(int(value * 100)) + "%"; break; // Random
                        case 6: valueText = juce::String(int(value * 100)) + "%"; break; // Texture
                        case 7: valueText = juce::String(int(value * 100)) + "%"; break; // Mix
                    }
                    
                    granularValueLabels[i]->setText(valueText, juce::dontSendNotification);
                }
            }
        };

        // Set knob images
        if (assets.knobRing != nullptr)
            granularKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            granularKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);

        if (i < 4)
            y -= 23;
        else
            y -= 1;

        granularKnobs[i]->setBounds(x, y, knobSize, knobSize);

        // Create label
        granularKnobLabels[i] = std::make_unique<juce::Label>();
        granularKnobLabels[i]->setText(granularKnobNames[i], juce::dontSendNotification);
        granularKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        granularKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        granularKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(granularKnobLabels[i].get());
        granularKnobLabels[i]->setVisible(false);
        granularKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);

        // Create value label
        granularValueLabels[i] = std::make_unique<juce::Label>();
        granularValueLabels[i]->setText("0", juce::dontSendNotification);
        granularValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        granularValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        granularValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(granularValueLabels[i].get());
        granularValueLabels[i]->setVisible(false);
        granularValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);

        // Create indicator bar
        granularIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(granularIndicatorBars[i].get());
        granularIndicatorBars[i]->setVisible(false);
        granularIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        granularIndicatorBars[i]->setValue(0.5f);

        // Create dice button
        granularDiceButtons[i] = std::make_unique<CustomDiceButton>();
        granularDiceButtons[i]->onClick = [this, i]() { randomizeIndividualGranularKnob(i); };

        // Create lock button
        granularLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(granularLockButtons[i].get());
        granularLockButtons[i]->setVisible(false);
        
        const int diceSize = 10;
        const int diceSpacing = 5;
        
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(granularKnobNames[i]);
        
        int lockX = x + (knobSize / 2) + (textWidth / 2) + diceSpacing;
        int lockY = y - 10;
        
        granularLockButtons[i]->setBounds(lockX, lockY, diceSize, diceSize);
        
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            granularLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        
        granularLockButtons[i]->setToggleState(granularKnobLocked[i], juce::dontSendNotification);
        granularLockButtons[i]->onClick = [this, i]() {
            granularKnobLocked[i] = granularLockButtons[i]->getToggleState();
            DBG("[UI] Granular knob " << i << " lock: " << (granularKnobLocked[i] ? "LOCKED" : "UNLOCKED"));
        };
    }

    // Create Density sync toggle (next to Density knob - knob 1)
    // Create Density sync toggle (next to Density knob - knob 1) - using SSyncButton like delay page
    struct GranularSSyncButton : public juce::Button {
        GranularSSyncButton() : juce::Button("GranularSSync") {}
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
    granularDensitySyncToggle = std::make_unique<GranularSSyncButton>();
    addAndMakeVisible(granularDensitySyncToggle.get());
    granularDensitySyncToggle->setVisible(false);
    
    // Position relative to Density knob label (knob 1) - moved left 5px from delay page
    if (granularKnobLabels[1] != nullptr) {
        auto lb = granularKnobLabels[1]->getBounds();
        granularDensitySyncToggle->setBounds(lb.getX() + 5, lb.getY() + 4, 12, 12);
    }
    
    auto* densitySyncParam = processorRef.getAPVTS().getRawParameterValue("granDensitySync");
    if (densitySyncParam) {
        granularDensitySyncEnabled = densitySyncParam->load() > 0.5f;
        granularDensitySyncToggle->setToggleState(granularDensitySyncEnabled, juce::dontSendNotification);
        
        // If sync is enabled on startup, set density knob to division mode
        if (granularDensitySyncEnabled && granularKnobs[1]) {
            granularKnobs[1]->setRange(0.0, 7.0, 1.0);
            // Find current Hz value and convert to nearest division
            float currentHz = 8.0f; // Default
            int nearestIdx = 4; // Default to 1/8
            granularKnobs[1]->setValue(nearestIdx, juce::dontSendNotification);
        }
    }
    
    granularDensitySyncToggle->onClick = [this]() {
        // Toggle the state manually (CircularToggleButton doesn't auto-toggle)
        granularDensitySyncEnabled = !granularDensitySyncEnabled;
        granularDensitySyncToggle->setToggleState(granularDensitySyncEnabled, juce::dontSendNotification);
        
        auto* param = processorRef.getAPVTS().getParameter("granDensitySync");
        if (param) {
            param->setValueNotifyingHost(granularDensitySyncEnabled ? 1.0f : 0.0f);
        }
        
        if (granularDensitySyncEnabled && granularKnobs[1]) {
            // When enabling sync, convert current Hz to nearest division
            float currentHz = granularKnobs[1]->getValue();
            float bpm = processorRef.getBpmOrDefault(120.0);
            
            // Convert Hz to division (0-7 maps to divisions)
            // At 120 BPM: 1/4 = 2Hz, 1/8 = 4Hz, 1/16 = 8Hz, 1/32 = 16Hz
            std::vector<float> divHz = {
                bpm/30.0f,  // 2 bars
                bpm/60.0f,  // 1 bar
                bpm/120.0f, // 1/2
                bpm/240.0f, // 1/4
                bpm/480.0f, // 1/8
                bpm/960.0f, // 1/16
                bpm/1920.0f, // 1/32
                bpm/3840.0f  // 1/64
            };
            
            // Find nearest division
            int nearestIdx = 0;
            float minDiff = 999.0f;
            for (int i = 0; i < 8; ++i) {
                float diff = std::abs(currentHz - divHz[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    nearestIdx = i;
                }
            }
            
            // Set knob to division index (0-7 range when synced)
            granularKnobs[1]->setRange(0.0, 7.0, 1.0);
            granularKnobs[1]->setValue(nearestIdx, juce::sendNotification);
        } else if (granularKnobs[1]) {
            // When disabling sync, convert division back to Hz
            int divIdx = (int)granularKnobs[1]->getValue();
            float bpm = processorRef.getBpmOrDefault(120.0);
            
            std::vector<float> divHz = {
                bpm/30.0f, bpm/60.0f, bpm/120.0f, bpm/240.0f,
                bpm/480.0f, bpm/960.0f, bpm/1920.0f, bpm/3840.0f
            };
            
            float hz = (divIdx >= 0 && divIdx < 8) ? divHz[divIdx] : 8.0f;
            
            // Restore Hz range
            granularKnobs[1]->setRange(1.0, 40.0, 0.1);
            granularKnobs[1]->setValue(hz, juce::sendNotification);
        }
        
        DBG("[UI] Granular density sync: " << (granularDensitySyncEnabled ? "ON" : "OFF"));
    };
    
    granularGroup.push_back(granularDensitySyncToggle.get());
    
    // Create parameter attachments to connect knobs to APVTS
    std::vector<juce::String> granularParamIds = {
        "granSizeMs", "granDensityHz", "granPosition", "granSprayMs",
        "granPitchSemi", "granRandom", "granTexture", "granMix"
    };

    for (int i = 0; i < 8; ++i)
    {
        granularAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), granularParamIds[i], *granularKnobs[i]);
        
        // Add to granularGroup for visibility toggling
        granularGroup.push_back(granularKnobs[i].get());
        granularGroup.push_back(granularKnobLabels[i].get());
        granularGroup.push_back(granularValueLabels[i].get());
        granularGroup.push_back(granularIndicatorBars[i].get());
        granularGroup.push_back(granularLockButtons[i].get());
    }
    
    DBG("[UI] Granular knobs setup complete");
}

void PluginEditor::setupGranularEffectsArea()
{
    DBG("[UI] Setting up Granular effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title
    granularEffectsTitle = std::make_unique<juce::Label>();
    granularEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    granularEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    granularEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    granularEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(granularEffectsTitle.get());
    granularEffectsTitle->setVisible(false);
    granularEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Create dice button
    granularDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(granularDiceButton.get());
    granularDiceButton->setVisible(false);
    
    const int diceSize = 32;
    granularDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    
    if (assets.diceLarge != nullptr)
    {
        granularDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    granularDiceButton->onClick = [this]() {
        DBG("[UI] Granular dice button clicked - randomizing all knobs");
        randomizeGranularKnobValues();
    };
    
    // Create FX power button (EXACT same as Reverb)
    granularFxPowerButton = std::make_unique<juce::DrawableButton>("granularFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(granularFxPowerButton.get());
    granularFxPowerButton->setVisible(false);

    const int buttonSize = 46;
    granularFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                 effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);

    granularFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    granularFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn != nullptr) {
        granularFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }
    
    granularFxPowerButton->setClickingTogglesState(true);
    
    auto* granEnabledParam = processorRef.getAPVTS().getRawParameterValue("granEnabled");
    if (granEnabledParam) {
        granularFxAreaEnabled = granEnabledParam->load() > 0.5f;
        granularFxPowerButton->setToggleState(granularFxAreaEnabled, juce::dontSendNotification);
    }
    
    granularFxPowerButton->onClick = [this]() {
        granularFxAreaEnabled = granularFxPowerButton->getToggleState();
        DBG("[UI] Granular FX power: " << (granularFxAreaEnabled ? "ON" : "OFF"));
        
        auto* param = processorRef.getAPVTS().getParameter("granEnabled");
        if (param) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(granularFxAreaEnabled ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
        
        updateGranularFxAreaVisibility();
        repaint();
    };
    
    // Add to granularGroup
    granularGroup.push_back(granularEffectsTitle.get());
    granularGroup.push_back(granularDiceButton.get());
    granularGroup.push_back(granularFxPowerButton.get());
    
    DBG("[UI] Granular effects area setup complete");
}

void PluginEditor::updateGranularFxAreaVisibility()
{
    float alpha = granularFxAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 8; ++i) {
        if (granularKnobs[i]) granularKnobs[i]->setAlpha(alpha);
        if (granularKnobLabels[i]) granularKnobLabels[i]->setAlpha(alpha);
        if (granularValueLabels[i]) granularValueLabels[i]->setAlpha(alpha);
        if (granularIndicatorBars[i]) granularIndicatorBars[i]->setAlpha(alpha);
        if (granularLockButtons[i]) granularLockButtons[i]->setAlpha(alpha);
    }
}

void PluginEditor::randomizeGranularKnobValues()
{
    juce::Random& rng = juce::Random::getSystemRandom();
    
    for (int i = 0; i < 8; ++i) {
        if (granularKnobLocked[i] || !granularKnobs[i])
            continue;
        
        randomizeIndividualGranularKnob(i);
    }
}

void PluginEditor::randomizeIndividualGranularKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || granularKnobs[knobIndex] == nullptr)
        return;
    if (granularKnobLocked[knobIndex])
        return;
    
    juce::Random& rng = juce::Random::getSystemRandom();
    float randomValue = 0.0f;
    
    switch (knobIndex) {
        case 0: randomValue = 5.0f + rng.nextFloat() * 195.0f; break; // Size (5-200)
        case 1: randomValue = 1.0f + rng.nextFloat() * 79.0f; break; // Density (1-80)
        case 2: randomValue = rng.nextFloat(); break; // Position (0-1)
        case 3: randomValue = rng.nextFloat() * 200.0f; break; // Spray (0-200)
        case 4: randomValue = -24.0f + rng.nextFloat() * 48.0f; break; // Pitch (-24 to +24)
        case 5: randomValue = rng.nextFloat(); break; // Random (0-1)
        case 6: randomValue = rng.nextFloat(); break; // Texture (0-1)
        case 7: randomValue = rng.nextFloat(); break; // Mix (0-1)
    }
    
    granularKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
}


void PluginEditor::setupGranularSequencerArea()
{
    DBG("[UI] Setting up Granular sequencer area...");
    
    // Use EXACT same bounds as Reverb sequencer (bottom area)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title (EXACT same as Reverb)
    granularStepTitle = std::make_unique<juce::Label>();
    granularStepTitle->setText("STEP", juce::dontSendNotification);
    granularStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    granularStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    granularStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(granularStepTitle.get());
    granularStepTitle->setVisible(false);
    granularStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step buttons (2 rows of 8) - EXACT same as Reverb
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i)
    {
        granularStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(granularStepButtons[i].get());
        granularStepButtons[i]->setVisible(false);
        
        // 2 rows of 8 buttons
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        granularStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        if (assets.stepActive != nullptr) {
            granularStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive != nullptr) {
            granularStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        granularStepButtons[i]->onClick = [this, i]() { onGranularStepButtonClicked(i); };
    }
    
    // Step amount label (TextEditor)
    granularStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setGranularStepsUsed(16);
    granularStepAmountLabel->setText("16");
    granularStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    granularStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    granularStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    granularStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    granularStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    granularStepAmountLabel->setJustification(juce::Justification::centred);
    granularStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    granularStepAmountLabel->setIndents(0, 0);
    granularStepAmountLabel->setInputRestrictions(2, "0123456789");
    granularStepAmountLabel->setWantsKeyboardFocus(true);
    granularStepAmountLabel->setMouseClickGrabsKeyboardFocus(true);
    granularStepAmountLabel->setCaretVisible(true);
    granularStepAmountLabel->setPopupMenuEnabled(false);
    granularStepAmountLabel->setScrollbarsShown(false);
    granularStepAmountLabel->setMultiLine(false);
    granularStepAmountLabel->setReturnKeyStartsNewLine(false);
    granularStepAmountLabel->setInterceptsMouseClicks(true, false);
    granularStepAmountLabel->setAlwaysOnTop(true);
    
    granularStepAmountLabel->onReturnKey = [this]() {
        int value = juce::jlimit(1, 16, granularStepAmountLabel->getText().getIntValue());
        processorRef.setGranularStepsUsed(value);
        granularStepAmountLabel->setText(juce::String(value), false);
        updateGranularSequencerUI();
        granularStepAmountLabel->giveAwayKeyboardFocus();
    };
    
    granularStepAmountLabel->onFocusLost = [this]() {
        int value = juce::jlimit(1, 16, granularStepAmountLabel->getText().getIntValue());
        processorRef.setGranularStepsUsed(value);
        granularStepAmountLabel->setText(juce::String(value), false);
        updateGranularSequencerUI();
    };
    
    granularStepAmountLabel->onTextChange = [this]() {
        // Empty - validation happens on return/focus lost
    };
    
    addAndMakeVisible(granularStepAmountLabel.get());
    granularStepAmountLabel->setVisible(false);
    granularStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    granularStepAmountLabel->setAlwaysOnTop(true);
    
    // Rate dropdown (EXACT same as Reverb)
    granularRateDropdown = std::make_unique<juce::ComboBox>();
    
    granularRateDropdown->addItem("4", 1);
    granularRateDropdown->addItem("2", 2);
    granularRateDropdown->addItem("1", 3);
    granularRateDropdown->addItem("1/2", 4);
    granularRateDropdown->addItem("1/4", 5);
    granularRateDropdown->addItem("1/8", 6);
    granularRateDropdown->addItem("1/16", 7);
    granularRateDropdown->addItem("1/32", 8);
    
    granularRateDropdown->setSelectedId(6);
    granularRateDropdown->onChange = [this]() {
        int selectedIndex = granularRateDropdown->getSelectedId() - 1;
        processorRef.setGranularDivisionIndex(selectedIndex);
        DBG("[UI] Granular rate changed to index: " << selectedIndex);
    };
    
    // Make dropdown background and outline transparent
    granularRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    granularRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    granularRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    
    addAndMakeVisible(granularRateDropdown.get());
    granularRateDropdown->setVisible(false);
    granularRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    // STD toggle (EXACT same positioning as Reverb)
    granularStdToggle = std::make_unique<CircularToggleButton>();
    granularStdToggle->setButtonText("-");
    addAndMakeVisible(granularStdToggle.get());
    granularStdToggle->setVisible(false);
    granularStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    granularStdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        
        switch (stdState) {
            case 0: granularStdToggle->setButtonText("-"); break;
            case 1: granularStdToggle->setButtonText("t"); break;
            case 2: granularStdToggle->setButtonText("."); break;
        }
        
        int nextMode = (stdState) % 3;
        // TODO: Add setGranularStdMode if needed
        DBG("[UI] Granular STD mode: " << nextMode);
    };
    
    // Step dice button (EXACT same positioning as Reverb)
    granularStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(granularStepDiceButton.get());
    granularStepDiceButton->setVisible(false);
    int granularStepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller = ~24px
    granularStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, granularStepDiceSize, granularStepDiceSize);
    
    if (assets.diceLarge != nullptr) {
        granularStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    granularStepDiceButton->onClick = [this]() {
        DBG("[UI] Granular step dice clicked - randomizing all steps");
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getGranularSafeSnapshot(step);
            juce::Random& rng = juce::Random::getSystemRandom();
            
                snapshot.granular.sizeMs = 5.0f + rng.nextFloat() * 195.0f;
                snapshot.granular.densityHz = 1.0f + rng.nextFloat() * 39.0f;
                snapshot.granular.position = rng.nextFloat();
                snapshot.granular.sprayMs = rng.nextFloat() * 200.0f;
                snapshot.granular.pitchSemi = -24.0f + rng.nextFloat() * 48.0f;
                snapshot.granular.random = rng.nextFloat();
                snapshot.granular.texture = rng.nextFloat();
                snapshot.granular.mix = rng.nextFloat();
            
            processorRef.setGranularStepSnapshot(step, snapshot);
        }
        updateGranularSequencerUI();
    };
    
    // Step power button (EXACT same positioning as Reverb)
    granularStepPowerButton = std::make_unique<juce::DrawableButton>("granularStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(granularStepPowerButton.get());
    granularStepPowerButton->setVisible(false);
    
    const int powerButtonSize = 40;
    granularStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - powerButtonSize - 5 + 15 - 5 - 1, 
                                   sequencerArea.getY() - 5 - 40 + 25 + 5, powerButtonSize, powerButtonSize);
    
    granularStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    granularStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn != nullptr) {
        granularStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    granularStepPowerButton->setClickingTogglesState(true);
    granularStepAreaEnabled = processorRef.getGranularSeqState().enabled.load();
    granularStepPowerButton->setToggleState(granularStepAreaEnabled, juce::dontSendNotification);
    
    granularStepPowerButton->onClick = [this]() {
        granularStepAreaEnabled = granularStepPowerButton->getToggleState();
        DBG("[UI] Granular sequencer power: " << (granularStepAreaEnabled ? "ON" : "OFF"));
        processorRef.setGranularSequencerEnabled(granularStepAreaEnabled);
        updateGranularStepAreaVisibility();
    };
    
    // Add sequencer components to granularGroup
    for (int i = 0; i < 16; ++i) {
        if (granularStepButtons[i]) granularGroup.push_back(granularStepButtons[i].get());
    }
    if (granularStepAmountLabel) granularGroup.push_back(granularStepAmountLabel.get());
    if (granularRateDropdown) granularGroup.push_back(granularRateDropdown.get());
    if (granularStdToggle) granularGroup.push_back(granularStdToggle.get());
    if (granularStepTitle) granularGroup.push_back(granularStepTitle.get());
    if (granularStepDiceButton) granularGroup.push_back(granularStepDiceButton.get());
    if (granularStepPowerButton) granularGroup.push_back(granularStepPowerButton.get());
    
    DBG("[UI] Granular sequencer area setup complete");
}

void PluginEditor::setupGranularAllStepsToggle()
{
    DBG("[UI] Setting up Granular All Steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    granularAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(granularAllStepsToggle.get());
    granularAllStepsToggle->setVisible(false);
    
    // Match Reverb's exact positioning and size
    const int buttonSize = 29;
    granularAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                  effectArea.getY() - 1, buttonSize, buttonSize);
    
    // Set the toggle images (stepTopInactive/stepTopActive)
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr) {
        static_cast<AllStepsToggleButton*>(granularAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    granularAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    granularAllStepsToggle->onClick = [this]() {
        granularAllStepsEnabled = granularAllStepsToggle->getToggleState();
        DBG("[UI] Granular All Steps toggle: " << (granularAllStepsEnabled ? "ON" : "OFF"));
        granularAllStepsLabel->setAlpha(granularAllStepsEnabled ? 1.0f : 0.5f);
        
        if (granularAllStepsEnabled) {
            DBG("[UI] Granular All Steps enabled - randomizing all step snapshots");
            
            for (int step = 0; step < 16; ++step) {
                auto snapshot = processorRef.getGranularSafeSnapshot(step);
                
                if (!granularKnobLocked[0]) snapshot.granular.sizeMs = 5.0f + juce::Random::getSystemRandom().nextFloat() * 195.0f;
                if (!granularKnobLocked[1]) snapshot.granular.densityHz = 1.0f + juce::Random::getSystemRandom().nextFloat() * 39.0f;
                if (!granularKnobLocked[2]) snapshot.granular.position = juce::Random::getSystemRandom().nextFloat();
                if (!granularKnobLocked[3]) snapshot.granular.sprayMs = juce::Random::getSystemRandom().nextFloat() * 200.0f;
                if (!granularKnobLocked[4]) snapshot.granular.pitchSemi = -24.0f + juce::Random::getSystemRandom().nextFloat() * 48.0f;
                if (!granularKnobLocked[5]) snapshot.granular.random = juce::Random::getSystemRandom().nextFloat();
                if (!granularKnobLocked[6]) snapshot.granular.texture = juce::Random::getSystemRandom().nextFloat();
                if (!granularKnobLocked[7]) snapshot.granular.mix = juce::Random::getSystemRandom().nextFloat();
                
                processorRef.setGranularStepSnapshot(step, snapshot);
            }
            
            // Update UI to show the new values
            updateGranularSequencerUI();
        }
    };
    
    // Label - match Reverb's exact positioning
    granularAllStepsLabel = std::make_unique<juce::Label>();
    granularAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    granularAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    granularAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    granularAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(granularAllStepsLabel.get());
    granularAllStepsLabel->setVisible(false);
    granularAllStepsLabel->setAlpha(1.0f); // Start fully visible (will grey when toggled off)
    granularAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                effectArea.getY() + 1, 80, 24);
    
    // Add to granularGroup
    granularGroup.push_back(granularAllStepsToggle.get());
    granularGroup.push_back(granularAllStepsLabel.get());
    
    DBG("[UI] Granular All Steps toggle setup complete");
}

void PluginEditor::updateGranularStepAreaVisibility()
{
    float alpha = granularStepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i) {
        if (granularStepButtons[i]) granularStepButtons[i]->setAlpha(alpha);
    }
    if (granularStepAmountLabel) granularStepAmountLabel->setAlpha(alpha);
    if (granularRateDropdown) granularRateDropdown->setAlpha(alpha);
    if (granularStdToggle) granularStdToggle->setAlpha(alpha);
    if (granularStepTitle) granularStepTitle->setAlpha(alpha);
}

void PluginEditor::onGranularStepButtonClicked(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= 16) return;
    
    granularUiSelectedStep = stepIndex;
    processorRef.setGranularSelectedStep(stepIndex);
    
    auto snapshot = processorRef.getGranularSafeSnapshot(stepIndex);
    
    if (granularKnobs[0]) granularKnobs[0]->setValue(snapshot.granular.sizeMs, juce::dontSendNotification);
    if (granularKnobs[1]) granularKnobs[1]->setValue(snapshot.granular.densityHz, juce::dontSendNotification);
    if (granularKnobs[2]) granularKnobs[2]->setValue(snapshot.granular.position, juce::dontSendNotification);
    if (granularKnobs[3]) granularKnobs[3]->setValue(snapshot.granular.sprayMs, juce::dontSendNotification);
    if (granularKnobs[4]) granularKnobs[4]->setValue(snapshot.granular.pitchSemi, juce::dontSendNotification);
    if (granularKnobs[5]) granularKnobs[5]->setValue(snapshot.granular.random, juce::dontSendNotification);
    if (granularKnobs[6]) granularKnobs[6]->setValue(snapshot.granular.texture, juce::dontSendNotification);
    if (granularKnobs[7]) granularKnobs[7]->setValue(snapshot.granular.mix, juce::dontSendNotification);
    
    for (int i = 0; i < 8; ++i) {
        if (granularKnobs[i] && granularKnobs[i]->onValueChange) {
            granularKnobs[i]->onValueChange();
        }
    }
    
    repaint();
}

void PluginEditor::updateGranularSequencerUI()
{
    const auto& seqState = processorRef.getGranularSeqState();
    int stepsUsed = seqState.stepsUsed.load();
    int selectedStep = granularUiSelectedStep;
    int playingStep = processorRef.getGranularPlayingStep();
    
    if (granularStepAmountLabel != nullptr && !granularStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = granularStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            granularStepAmountLabel->setText(newText, false);
        }
    }
    
    for (int i = 0; i < 16; ++i) {
        if (granularStepButtons[i] != nullptr) {
            granularStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getGranularSeqState().enabled.load();
            granularStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
            bool shouldBeEnabled = i < stepsUsed;
            granularStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
}

void PluginEditor::updateGranularParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || !granularKnobs[knobIndex])
        return;
    
    float value = granularKnobs[knobIndex]->getValue();
    processorRef.updateGranularCurrentStepSnapshot(knobIndex, value);
}

//==============================================================================
// Slicer Page Implementation
//==============================================================================

void PluginEditor::setupSlicerKnobs()
{
    DBG("[UI] Setting up Slicer knobs...");

    // Slicer knob names (6 knobs)
    std::vector<juce::String> slicerKnobNames = {
        "Pattern", "Division", "Offset", "Shape", "Release", "Mix"
    };

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    for (int i = 0; i < 6; ++i)
    {
        slicerKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(slicerKnobs[i].get());
        slicerKnobs[i]->setVisible(false);
        
        slicerKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slicerKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Set parameter ranges based on knob index
        switch (i) {
            case 0: // Pattern (0-7)
                slicerKnobs[i]->setRange(0.0, 7.0, 1.0);
                slicerKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 1: // Division - 27 discrete positions (9 divisions × 3 grids)
                slicerKnobs[i]->setRange(0.0, 26.0, 1.0); // 0-26 = 27 positions
                slicerKnobs[i]->setValue(15.0, juce::dontSendNotification); // 1/8 straight default (5*3+0=15)
                break;
            case 2: // Offset (0-1, bipolar: 0.5=center)
                slicerKnobs[i]->setRange(0.0, 1.0, 0.01);
                slicerKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 3: // Shape (0-1)
                slicerKnobs[i]->setRange(0.0, 1.0, 0.01);
                slicerKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 4: // Release (5-80 ms)
                slicerKnobs[i]->setRange(5.0, 80.0, 0.1);
                slicerKnobs[i]->setValue(20.0, juce::dontSendNotification);
                break;
            case 5: // Mix (0-1)
                slicerKnobs[i]->setRange(0.0, 1.0, 0.01);
                slicerKnobs[i]->setValue(0.75, juce::dontSendNotification);
                break;
        }
        
        // Add value change callback to update value label
        slicerKnobs[i]->onValueChange = [this, i]() {
            if (slicerKnobs[i] != nullptr) {
                updateSlicerParameterFromKnob(i);
                
                // Update value label
                if (slicerValueLabels[i]) {
                    float value = slicerKnobs[i]->getValue();
                    juce::String valueText;
                    
                    switch (i) {
                        case 0: { // Pattern
                            std::vector<juce::String> patternNames = {
                                "Straight8", "Offbeat", "HalfTime", "Syncop", "Triplet", "BuildUp", "Choke16", "Gallop"
                            };
                            int patIdx = juce::jlimit(0, 7, (int)value);
                            valueText = patternNames[patIdx];
                            break;
                        }
                        case 1: { // Division - 27 discrete positions (0-26)
                            if (slicerKnobs[1]) {
                                // Direct mapping: knob value 0-26 → division index 0-26
                                int divisionIndex = static_cast<int>(value);
                                divisionIndex = juce::jlimit(0, 26, divisionIndex);
                                
                                int baseDivIdx = divisionIndex / 3; // 0-8 (4, 2, 1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64)
                                int gridMode = divisionIndex % 3; // 0=straight, 1=triplet, 2=dotted
                                
                                // Update APVTS parameters
                                auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("slicerDivision"));
                                auto* gridParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("slicerGrid"));
                                
                                if (divParam && divParam->getIndex() != baseDivIdx) {
                                    divParam->setValueNotifyingHost(static_cast<float>(baseDivIdx) / 8.0f);
                                }
                                if (gridParam && gridParam->getIndex() != gridMode) {
                                    gridParam->setValueNotifyingHost(static_cast<float>(gridMode) / 2.0f);
                                }
                                
                                // Display label (knob left=slow/4 bars, right=fast/1/64)
                                static const char* divStrings[] = {"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
                                valueText = divStrings[baseDivIdx];
                                if (gridMode == 1) valueText += "T"; // Triplet
                                else if (gridMode == 2) valueText += "."; // Dotted
                            }
                            break;
                        }
                        case 2: { // Offset (bipolar: 0.5=0%, 0=-50%, 1=+50%)
                            int bipolar = int((value - 0.5f) * 100.0f);
                            valueText = (bipolar >= 0 ? "+" : "") + juce::String(bipolar) + "%";
                            break;
                        }
                        case 3: valueText = juce::String(int(value * 100)) + "%"; break; // Shape
                        case 4: valueText = juce::String(int(value)) + "ms"; break; // Release (5-80ms)
                        case 5: valueText = juce::String(int(value * 100)) + "%"; break; // Mix
                    }
                    
                    slicerValueLabels[i]->setText(valueText, juce::dontSendNotification);
                }
            }
        };

        // Set knob images
        if (assets.knobRing != nullptr)
            slicerKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            slicerKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);

        if (i < 4)
            y -= 23;
        else
            y -= 1;

        slicerKnobs[i]->setBounds(x, y, knobSize, knobSize);

        // Create label
        slicerKnobLabels[i] = std::make_unique<juce::Label>();
        slicerKnobLabels[i]->setText(slicerKnobNames[i], juce::dontSendNotification);
        slicerKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        slicerKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        slicerKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(slicerKnobLabels[i].get());
        slicerKnobLabels[i]->setVisible(false);
        slicerKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);

        // Create value label
        slicerValueLabels[i] = std::make_unique<juce::Label>();
        slicerValueLabels[i]->setText("0", juce::dontSendNotification);
        slicerValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        slicerValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        slicerValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(slicerValueLabels[i].get());
        slicerValueLabels[i]->setVisible(false);
        slicerValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);

        // Create indicator bar
        slicerIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(slicerIndicatorBars[i].get());
        slicerIndicatorBars[i]->setVisible(false);
        slicerIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        slicerIndicatorBars[i]->setValue(0.5f);
        
        // Create lock button
        slicerLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(slicerLockButtons[i].get());
        slicerLockButtons[i]->setVisible(false);
        
        // Position lock button at end of title text (same as other effects)
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(slicerKnobNames[i]);
        const int lockSize = 10; // Same size as other effects
        const int lockSpacing = 5; // Fixed distance from end of title text
        int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
        int lockY = y - 10; // Same position as other effects
        slicerLockButtons[i]->setBounds(lockX, lockY, lockSize, lockSize);
        
        // Set lock button images
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            slicerLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        
        // Add click handler
        slicerLockButtons[i]->onClick = [this, i]() {
            slicerKnobLocked[i] = slicerLockButtons[i]->getToggleState();
            DBG("[UI] Slicer knob " << i << " locked: " << (slicerKnobLocked[i] ? "YES" : "NO"));
        };
    }

    // Slicer division is always tempo-synced (no sync toggle needed)
    
    // Create parameter attachments to connect knobs to APVTS
    // Note: Division knob (index 1) has NO attachment - it manually updates dubDivision + dubGrid in onValueChange
    std::vector<juce::String> slicerParamIds = {
        "slicerPattern", "", "slicerOffset", "slicerShape",  // Empty string for division (no attachment)
        "slicerReleaseMs", "slicerMix"
    };
    
    for (int i = 0; i < 6; ++i)
    {
        // Create attachment only if paramId is not empty
        if (!slicerParamIds[i].isEmpty()) {
            slicerAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.getAPVTS(), slicerParamIds[i], *slicerKnobs[i]);
        }
        
        // Add to slicerGroup for visibility toggling
        slicerGroup.push_back(slicerKnobs[i].get());
        slicerGroup.push_back(slicerKnobLabels[i].get());
        slicerGroup.push_back(slicerValueLabels[i].get());
        slicerGroup.push_back(slicerIndicatorBars[i].get());
        slicerGroup.push_back(slicerLockButtons[i].get());
    }
    
    DBG("[UI] Slicer knobs setup complete");
}

void PluginEditor::setupSlicerEffectsArea()
{
    DBG("[UI] Setting up Slicer effects area...");
    
    // Effect area bounds
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title label (match other pages)
    slicerEffectsTitle = std::make_unique<juce::Label>();
    slicerEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    slicerEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    slicerEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    slicerEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(slicerEffectsTitle.get());
    slicerEffectsTitle->setVisible(false);
    slicerEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // FX Power button
    slicerFxPowerButton = std::make_unique<juce::DrawableButton>("slicerPower", juce::DrawableButton::ImageFitted);
    addAndMakeVisible(slicerFxPowerButton.get());
    slicerFxPowerButton->setVisible(false);
    slicerFxPowerButton->setClickingTogglesState(true);
    
    // Make button background transparent (match other pages)
    slicerFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    slicerFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn) {
        slicerFxPowerButton->setImages(assets.fxPowerOn.get());
    }
    
    const int buttonSize = 46;
    slicerFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                   effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    
    slicerFxPowerButton->onClick = [this]() {
        slicerFxAreaEnabled = slicerFxPowerButton->getToggleState();
        
        // Update APVTS parameter
        auto* param = processorRef.getAPVTS().getParameter("slicerEnabled");
        if (param) {
            param->setValueNotifyingHost(slicerFxAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateSlicerFxAreaVisibility();
        DBG("[UI] Slicer FX power: " << (slicerFxAreaEnabled ? "ON" : "OFF"));
    };
    
    // Main dice button (randomize all unlocked knobs)
    slicerDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(slicerDiceButton.get());
    slicerDiceButton->setVisible(false);
    
    if (assets.diceLarge != nullptr) {
        slicerDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    // Position next to title (match Grain page)
    const int diceSize = 32;
    slicerDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    
    slicerDiceButton->onClick = [this]() {
        DBG("[UI] Slicer main dice clicked - randomizing current step knobs");
        randomizeSlicerKnobValues();
    };
    
    slicerGroup.push_back(slicerEffectsTitle.get());
    slicerGroup.push_back(slicerFxPowerButton.get());
    slicerGroup.push_back(slicerDiceButton.get());
    
    DBG("[UI] Slicer effects area setup complete");
}

void PluginEditor::updateSlicerFxAreaVisibility()
{
    float alpha = slicerFxAreaEnabled ? 1.0f : 0.3f;
    
    // Update knobs and labels alpha (6 knobs, not 8)
    for (int i = 0; i < 6; ++i) {
        if (slicerKnobs[i]) { 
            slicerKnobs[i]->setAlpha(alpha); 
            slicerKnobs[i]->setEnabled(slicerFxAreaEnabled);
        }
        if (slicerKnobLabels[i]) slicerKnobLabels[i]->setAlpha(alpha);
        if (slicerValueLabels[i]) slicerValueLabels[i]->setAlpha(alpha);
        if (slicerIndicatorBars[i]) slicerIndicatorBars[i]->setAlpha(alpha);
        if (slicerLockButtons[i]) slicerLockButtons[i]->setAlpha(alpha);
    }
    
    // Slicer division is always tempo-synced (no sync toggle)
    
    // Update LED strip
    for (int i = 0; i < 16; ++i) {
        if (slicerLEDStrip[i]) {
            slicerLEDStrip[i]->setAlpha(alpha);
        }
    }
    
    repaint();
}

void PluginEditor::updateSlicerStepAreaVisibility()
{
    float alpha = slicerStepAreaEnabled ? 1.0f : 0.3f;
    
    // Update step buttons
    for (int i = 0; i < 16; ++i) {
        if (slicerStepButtons[i]) {
            slicerStepButtons[i]->setAlpha(alpha);
            slicerStepButtons[i]->setEnabled(slicerStepAreaEnabled);
        }
    }
    
    // Update sequencer controls
    if (slicerStepAmountLabel) {
        slicerStepAmountLabel->setAlpha(alpha);
        slicerStepAmountLabel->setEnabled(slicerStepAreaEnabled);
    }
    if (slicerRateDropdown) {
        slicerRateDropdown->setAlpha(alpha);
        slicerRateDropdown->setEnabled(slicerStepAreaEnabled);
    }
    if (slicerStdToggle) {
        slicerStdToggle->setAlpha(alpha);
        slicerStdToggle->setEnabled(slicerStepAreaEnabled);
    }
    if (slicerStepTitle) slicerStepTitle->setAlpha(alpha);
    if (slicerStepDiceButton) {
        slicerStepDiceButton->setAlpha(alpha);
        slicerStepDiceButton->setEnabled(slicerStepAreaEnabled);
    }
    
    repaint();
}

void PluginEditor::updateSlicerParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 6 || !slicerKnobs[knobIndex])
        return;
    
    float value = slicerKnobs[knobIndex]->getValue();
    
    std::vector<juce::String> slicerParamIds = {
        "slicerPattern", "slicerDivision", "slicerOffset", "slicerShape",
        "slicerReleaseMs", "slicerMix"
    };
    
    // Update APVTS parameter
    auto* param = processorRef.getAPVTS().getParameter(slicerParamIds[knobIndex]);
    if (param) {
        float normalizedValue = param->convertTo0to1(value);
        param->setValueNotifyingHost(normalizedValue);
    }
    
    // Update snapshots
    if (slicerAllStepsEnabled) {
        // Update all steps' snapshots directly
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getSlicerSafeSnapshot(step);
            
            // Update the specific parameter (Mix is global, so skip it)
            if (knobIndex != 5) { // Mix is knob 5, don't snapshot it
                switch (knobIndex) {
                    case 0: snapshot.slicer.pattern = value; break;
                    case 1: snapshot.slicer.division = value; break;
                    case 2: snapshot.slicer.offset = value; break;
                    case 3: snapshot.slicer.shape = value; break;
                    case 4: snapshot.slicer.releaseMs = value; break;
                }
                processorRef.setSlicerStepSnapshot(step, snapshot);
            }
        }
    } else {
        // Update only the current step's snapshot
        processorRef.updateSlicerCurrentStepSnapshot(knobIndex, value);
    }
}

void PluginEditor::randomizeSlicerKnobValues()
{
    DBG("[UI] Randomizing ALL Slicer knob values (AllSteps=" << (slicerAllStepsEnabled ? "ON" : "OFF") << ")");
    
    for (int i = 0; i < 8; ++i)
    {
        randomizeIndividualSlicerKnob(i);
    }
    
    DBG("[UI] Slicer randomization complete");
}

void PluginEditor::randomizeIndividualSlicerKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 6 || !slicerKnobs[knobIndex])
        return;
    
    float randomValue = juce::Random::getSystemRandom().nextFloat();
    
    switch (knobIndex) {
        case 0: // Pattern (0-7)
            randomValue = std::floor(juce::Random::getSystemRandom().nextFloat() * 8.0f);
            break;
        case 1: // Division (0-5)
            randomValue = std::floor(juce::Random::getSystemRandom().nextFloat() * 6.0f);
            break;
        case 2: // Offset (0-1)
            // randomValue already 0-1
            break;
        case 3: // Shape (0-1)
            randomValue = 0.2f + juce::Random::getSystemRandom().nextFloat() * 0.6f; // 0.2-0.8
            break;
        case 4: // Release (0-1)
            randomValue = juce::Random::getSystemRandom().nextFloat() * 0.8f; // 0-80%
            break;
        case 5: // Reverse (0-1)
            randomValue = juce::Random::getSystemRandom().nextFloat() * 0.7f; // 0-0.7
            break;
        case 6: // Glitch (0-1)
            randomValue = juce::Random::getSystemRandom().nextFloat() * 0.6f; // 0-0.6
            break;
        case 7: // Mix (0-1)
            randomValue = 0.5f + juce::Random::getSystemRandom().nextFloat() * 0.5f; // 0.5-1.0
            break;
    }
    
    slicerKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
    DBG("[UI] Randomized Slicer knob " << knobIndex << " to " << randomValue);
}

void PluginEditor::updateSlicerLEDStrip()
{
    // Update LED strip based on current pattern and playhead position
    // This would be called from a timer or repaint callback
    // For now, just a placeholder - the LEDs will be painted in the paint() method
}

void PluginEditor::updateSlicerSequencerUI()
{
    int selectedStep = slicerUiSelectedStep;
    int playingStep = processorRef.getSlicerCurrentStep();
    const int stepsUsed = processorRef.getSlicerSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (slicerStepButtons[i] != nullptr) {
            slicerStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getSlicerSeqState().enabled.load();
            slicerStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            bool shouldBeEnabled = i < stepsUsed;
            slicerStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't overwrite if user is editing)
    if (slicerStepAmountLabel != nullptr && !slicerStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = slicerStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            slicerStepAmountLabel->setText(newText, false);
        }
    }
    
    repaint();
}

void PluginEditor::onSlicerStepButtonClicked(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= 16) return;
    
    // Update UI selected step
    slicerUiSelectedStep = stepIndex;
    processorRef.setSlicerSelectedStep(stepIndex);
    
    // Load the snapshot for this step and update knobs
    StepSnapshot snapshot = processorRef.getSlicerSafeSnapshot(stepIndex);
    
    if (!slicerAllStepsEnabled) {
    // Update knobs with values from the snapshot
    if (slicerKnobs[0]) slicerKnobs[0]->setValue(snapshot.slicer.pattern, juce::dontSendNotification);
    if (slicerKnobs[1]) slicerKnobs[1]->setValue(snapshot.slicer.division, juce::dontSendNotification);
    if (slicerKnobs[2]) slicerKnobs[2]->setValue(snapshot.slicer.offset, juce::dontSendNotification);
    if (slicerKnobs[3]) slicerKnobs[3]->setValue(snapshot.slicer.shape, juce::dontSendNotification);
    if (slicerKnobs[4]) slicerKnobs[4]->setValue(snapshot.slicer.releaseMs, juce::dontSendNotification);
    // Knob 5 (Mix) is global, not per-step
    
    // Trigger value change callbacks to update labels
    for (int i = 0; i < 5; ++i) {
        if (slicerKnobs[i]) {
            slicerKnobs[i]->onValueChange();
            }
        }
    }
    
    updateSlicerSequencerUI();
    
    DBG("[UI] Switched to Slicer step " << stepIndex);
}

void PluginEditor::setupSlicerSequencerArea()
{
    DBG("[UI] Setting up Slicer sequencer area...");
    
    // Sequencer area bounds (EXACT same as other pages)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    slicerStepTitle = std::make_unique<juce::Label>();
    slicerStepTitle->setText("STEP", juce::dontSendNotification);
    slicerStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    slicerStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    slicerStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(slicerStepTitle.get());
    slicerStepTitle->setVisible(false);
    slicerStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step buttons (2 rows of 8)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        slicerStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(slicerStepButtons[i].get());
        slicerStepButtons[i]->setVisible(false);
        
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        slicerStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        if (assets.stepActive) {
            slicerStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive) {
            slicerStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        // Wire up step button click handler
        slicerStepButtons[i]->onClick = [this, i]() {
            onSlicerStepButtonClicked(i);
        };
    }
    
    // Create step amount editor
    slicerStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setSlicerStepsUsed(16);
    slicerStepAmountLabel->setText("16");
    slicerStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    slicerStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    slicerStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    slicerStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    slicerStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    slicerStepAmountLabel->setJustification(juce::Justification::centred);
    slicerStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    slicerStepAmountLabel->setIndents(0, 0);
    slicerStepAmountLabel->setInputRestrictions(2, "0123456789");
    slicerStepAmountLabel->setReadOnly(true); // Read-only for now as Slicer uses fixed 16-step patterns
    addAndMakeVisible(slicerStepAmountLabel.get());
    slicerStepAmountLabel->setVisible(false);
    slicerStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    
    // Create rate dropdown
    slicerRateDropdown = std::make_unique<juce::ComboBox>();
    slicerRateDropdown->addItem("4", 1);
    slicerRateDropdown->addItem("2", 2);
    slicerRateDropdown->addItem("1", 3);
    slicerRateDropdown->addItem("1/2", 4);
    slicerRateDropdown->addItem("1/4", 5);
    slicerRateDropdown->addItem("1/8", 6);
    slicerRateDropdown->addItem("1/16", 7);
    slicerRateDropdown->addItem("1/32", 8);
    slicerRateDropdown->setSelectedId(4); // Default to 1/8 (matches slicerDivision default of 3)
    slicerRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    slicerRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    slicerRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    slicerRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    slicerRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    slicerRateDropdown->onChange = [this]() {
        if (slicerRateDropdown) {
            const int selected = slicerRateDropdown->getSelectedId();
            if (selected >= 1 && selected <= 8) {
                const int newDivisionIndex = selected - 1;
                // Update the slicerDivision parameter
                auto* param = processorRef.getAPVTS().getParameter("slicerDivision");
                if (param) {
                    param->setValueNotifyingHost(param->convertTo0to1((float)newDivisionIndex));
                }
                // Update processor sequencer division
                processorRef.setSlicerDivisionIndex(newDivisionIndex);
                updateSlicerSequencerUI();
            }
        }
    };
    addAndMakeVisible(slicerRateDropdown.get());
    slicerRateDropdown->setVisible(false);
    slicerRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    // Create STD toggle
    slicerStdToggle = std::make_unique<CircularToggleButton>();
    slicerStdToggle->setButtonText("-");
    addAndMakeVisible(slicerStdToggle.get());
    slicerStdToggle->setVisible(false);
    slicerStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    // STD toggle is visual only for Slicer (no functionality needed)
    
    // Create step dice button (EXACT same as AutoPan page)
    slicerStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(slicerStepDiceButton.get());
    slicerStepDiceButton->setVisible(false);
    int slicerStepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    slicerStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, slicerStepDiceSize, slicerStepDiceSize);
    
    // Set up step dice button SVG (EXACT same as AutoPan page)
    if (assets.diceLarge != nullptr)
    {
        slicerStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    slicerStepDiceButton->onClick = [this]() {
        DBG("[UI] Slicer step dice clicked - randomizing all 16 steps");
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getSlicerSafeSnapshot(step);
            juce::Random& rng = juce::Random::getSystemRandom();
            
            snapshot.slicer.pattern = std::floor(rng.nextFloat() * 8.0f);
            snapshot.slicer.division = std::floor(rng.nextFloat() * 6.0f);
            snapshot.slicer.offset = rng.nextFloat(); // 0-1 (will be bipolar in engine)
            snapshot.slicer.shape = 0.2f + rng.nextFloat() * 0.6f; // 0.2-0.8 (musical range)
            snapshot.slicer.releaseMs = 10.0f + rng.nextFloat() * 50.0f; // 10-60ms (musical range)
            
            processorRef.setSlicerStepSnapshot(step, snapshot);
        }
        
        // Reload current step to UI (Mix is global, not per-step)
        auto currentSnapshot = processorRef.getSlicerSafeSnapshot(slicerUiSelectedStep);
        if (slicerKnobs[0]) slicerKnobs[0]->setValue(currentSnapshot.slicer.pattern, juce::sendNotification);
        if (slicerKnobs[1]) slicerKnobs[1]->setValue(currentSnapshot.slicer.division, juce::sendNotification);
        if (slicerKnobs[2]) slicerKnobs[2]->setValue(currentSnapshot.slicer.offset, juce::sendNotification);
        if (slicerKnobs[3]) slicerKnobs[3]->setValue(currentSnapshot.slicer.shape, juce::sendNotification);
        if (slicerKnobs[4]) slicerKnobs[4]->setValue(currentSnapshot.slicer.releaseMs, juce::sendNotification);
        
        DBG("[UI] All 16 Slicer steps randomized");
    };
    
    // Create step power button
    slicerStepPowerButton = std::make_unique<juce::DrawableButton>("slicerStepPower", juce::DrawableButton::ImageFitted);
    
    // Make button background transparent (match other pages)
    slicerStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    slicerStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn) {
        slicerStepPowerButton->setImages(assets.stepPowerOn.get());
    }
    addAndMakeVisible(slicerStepPowerButton.get());
    slicerStepPowerButton->setVisible(false);
    slicerStepPowerButton->setClickingTogglesState(true);
    const int stepPowerSize = 40;
    slicerStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - stepPowerSize - 5 + 15 - 5 - 1, sequencerArea.getY() - 5 - stepPowerSize + 25 + 5, stepPowerSize, stepPowerSize);
    
    slicerStepPowerButton->onClick = [this]() {
        slicerStepAreaEnabled = slicerStepPowerButton->getToggleState();
        processorRef.setSlicerSequencerEnabled(slicerStepAreaEnabled); // Sync with processor
        updateSlicerStepAreaVisibility();
        DBG("[UI] Slicer step power: " << (slicerStepAreaEnabled ? "ON" : "OFF"));
    };
    
    // Add all sequencer components to slicerGroup
    slicerGroup.push_back(slicerStepTitle.get());
    slicerGroup.push_back(slicerStepAmountLabel.get());
    slicerGroup.push_back(slicerRateDropdown.get());
    slicerGroup.push_back(slicerStdToggle.get());
    slicerGroup.push_back(slicerStepDiceButton.get());
    slicerGroup.push_back(slicerStepPowerButton.get());
    
    for (int i = 0; i < 16; ++i) {
        if (slicerStepButtons[i]) {
            slicerGroup.push_back(slicerStepButtons[i].get());
        }
    }
    
    DBG("[UI] Slicer sequencer area setup complete");
}

void PluginEditor::setupSlicerAllStepsToggle()
{
    DBG("[UI] Setting up Slicer All Steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    slicerAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(slicerAllStepsToggle.get());
    slicerAllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    slicerAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                     effectArea.getY() - 1, buttonSize, buttonSize);
    
    if (assets.stepTopInactive && assets.stepTopActive) {
        static_cast<AllStepsToggleButton*>(slicerAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    slicerAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    slicerAllStepsToggle->onClick = [this]() {
        slicerAllStepsEnabled = slicerAllStepsToggle->getToggleState();
        DBG("[UI] Slicer All Steps toggle: " << (slicerAllStepsEnabled ? "ON" : "OFF"));
        slicerAllStepsLabel->setAlpha(slicerAllStepsEnabled ? 1.0f : 0.5f);
        // Slicer doesn't have per-step snapshots, so this is visual only
    };
    
    slicerAllStepsLabel = std::make_unique<juce::Label>();
    slicerAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    slicerAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    slicerAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    slicerAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(slicerAllStepsLabel.get());
    slicerAllStepsLabel->setVisible(false);
    slicerAllStepsLabel->setAlpha(1.0f);
    slicerAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                    effectArea.getY() + 1, 80, 24);
    
    slicerGroup.push_back(slicerAllStepsToggle.get());
    slicerGroup.push_back(slicerAllStepsLabel.get());
    
    DBG("[UI] Slicer All Steps toggle setup complete");
}

//==============================================================================
// Dub Delay Page Implementation
//==============================================================================

void PluginEditor::setupDubDelayKnobs()
{
    DBG("[UI] Setting up Dub Delay knobs...");

    // Dub Delay knob names (8 knobs)
    std::vector<juce::String> dubdelayKnobNames = {
        "Time", "Feedback", "Tone", "Drive", "PingPong", "WowFlut", "RegenDmp", "Mix"
    };
    
    // Parameter IDs for APVTS attachments (CRITICAL: must match exact APVTS parameter names)
    std::vector<juce::String> dubdelayParamIds = {
        "dubTimeMs", "dubFeedback", "dubToneHz", "dubDrive", 
        "dubPingPong", "dubWowFlutter", "dubRegenDamp", "dubMix"
    };

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    for (int i = 0; i < 8; ++i)
    {
        dubdelayKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(dubdelayKnobs[i].get());
        dubdelayKnobs[i]->setVisible(false);
        
        dubdelayKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dubdelayKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Set parameter ranges based on knob index
        switch (i) {
            case 0: // Time (1-2000 ms)
                dubdelayKnobs[i]->setRange(1.0, 2000.0, 1.0);
                dubdelayKnobs[i]->setValue(450.0, juce::dontSendNotification);
                break;
            case 1: // Feedback (0-0.98)
                dubdelayKnobs[i]->setRange(0.0, 0.98, 0.01);
                dubdelayKnobs[i]->setValue(0.45, juce::dontSendNotification);
                break;
            case 2: // Tone (200-20000 Hz)
                dubdelayKnobs[i]->setRange(200.0, 20000.0, 1.0);
                dubdelayKnobs[i]->setValue(6500.0, juce::dontSendNotification);
                break;
            case 3: // Drive (0-1)
                dubdelayKnobs[i]->setRange(0.0, 1.0, 0.01);
                dubdelayKnobs[i]->setValue(0.15, juce::dontSendNotification);
                break;
            case 4: // PingPong (0-1, boolean)
                dubdelayKnobs[i]->setRange(0.0, 1.0, 1.0);
                dubdelayKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 5: // WowFlutter (0-1)
                dubdelayKnobs[i]->setRange(0.0, 1.0, 0.01);
                dubdelayKnobs[i]->setValue(0.35, juce::dontSendNotification);
                break;
            case 6: // RegenDamp (0-1)
                dubdelayKnobs[i]->setRange(0.0, 1.0, 0.01);
                dubdelayKnobs[i]->setValue(0.25, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                dubdelayKnobs[i]->setRange(0.0, 1.0, 0.01);
                dubdelayKnobs[i]->setValue(0.35, juce::dontSendNotification);
                break;
        }
        
        // Add value change callback to update value label
        dubdelayKnobs[i]->onValueChange = [this, i]() {
            if (dubdelayKnobs[i] != nullptr) {
                // Skip if loading from snapshot (prevents circular updates during randomization)
                if (isLoadingFromSnapshot.load())
                    return;
                
                // Update current step snapshot with new value
                float value = dubdelayKnobs[i]->getValue();
                processorRef.updateDubDelayCurrentStepSnapshot(i, value);
                
                // If All Steps toggle is active, update all step snapshots
                if (dubdelayAllStepsEnabled) {
                    DBG("[All Steps] Dub Delay knob " << i << " changed, dubdelayAllStepsEnabled=true");
                    
                    // Convert knob value to actual parameter value using APVTS parameter ranges
                    float actualValue = value;
                    switch (i) {
                        case 0: { // Time - convert from normalized to milliseconds (1-2000)
                            auto* param = processorRef.getAPVTS().getParameter("dubTimeMs");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 1: { // Feedback - convert from normalized to feedback amount (0-0.98)
                            auto* param = processorRef.getAPVTS().getParameter("dubFeedback");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 2: { // Tone - convert from normalized to frequency (200-20000 Hz)
                            auto* param = processorRef.getAPVTS().getParameter("dubToneHz");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 3: { // Drive - already normalized (0-1)
                            actualValue = value;
                            break;
                        }
                        case 4: { // PingPong - boolean from normalized
                            actualValue = value > 0.5f ? 1.0f : 0.0f;
                            break;
                        }
                        case 5: { // WowFlutter - already normalized (0-1)
                            actualValue = value;
                            break;
                        }
                        case 6: { // RegenDamp - already normalized (0-1)
                            actualValue = value;
                            break;
                        }
                        case 7: { // Mix - already normalized (0-1)
                            actualValue = value;
                            break;
                        }
                    }
                    
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getDubDelaySafeSnapshot(step);
                        switch (i) {
                            case 0: snapshot.dubdelay.timeMs = actualValue; break;
                            case 1: snapshot.dubdelay.feedback = actualValue; break;
                            case 2: snapshot.dubdelay.toneHz = actualValue; break;
                            case 3: snapshot.dubdelay.drive = actualValue; break;
                            case 4: snapshot.dubdelay.pingPong = actualValue > 0.5f; break;
                            case 5: snapshot.dubdelay.wowFlutter = actualValue; break;
                            case 6: snapshot.dubdelay.regenDamp = actualValue; break;
                            case 7: snapshot.dubdelay.mix = actualValue; break;
                        }
                        processorRef.setDubDelayStepSnapshot(step, snapshot);
                    }
                } else {
                    DBG("[All Steps] Dub Delay knob " << i << " changed, dubdelayAllStepsEnabled=false, skipping All Steps update");
                }
                
                // Update value label
                if (dubdelayValueLabels[i]) {
                    float value = dubdelayKnobs[i]->getValue();
                    juce::String valueText;
                    
                    switch (i) {
                        case 0: { // Time - handle sync mode knob mapping
                            if (dubdelaySyncEnabled && dubdelayKnobs[0]) {
                                // Map knob position (0-1) to 27 divisions (9 base × 3 grids)
                                float normPos = (dubdelayKnobs[0]->getValue() - dubdelayKnobs[0]->getMinimum()) / 
                                               (dubdelayKnobs[0]->getMaximum() - dubdelayKnobs[0]->getMinimum());
                                normPos = juce::jlimit(0.0f, 1.0f, normPos);
                                
                                int totalDivisions = 9 * 3; // 9 base divisions × 3 grid modes
                                int divisionIndex = static_cast<int>(normPos * (totalDivisions - 1));
                                divisionIndex = juce::jlimit(0, totalDivisions - 1, divisionIndex);
                                
                                int baseDivIdx = divisionIndex / 3; // 0-8 (4, 2, 1, ... 1/64)
                                int gridMode = divisionIndex % 3; // 0=straight, 1=triplet, 2=dotted
                                
                                // Update APVTS parameters
                                auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("dubTimeDiv"));
                                auto* gridParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("dubTimeGrid"));
                                
                                if (divParam && divParam->getIndex() != baseDivIdx) {
                                    divParam->setValueNotifyingHost(static_cast<float>(baseDivIdx) / 8.0f);
                                }
                                if (gridParam && gridParam->getIndex() != gridMode) {
                                    gridParam->setValueNotifyingHost(static_cast<float>(gridMode) / 2.0f);
                                }
                                
                                // Update label via updateDubDelayTimeLabel
                                updateDubDelayTimeLabel();
                                valueText = dubdelayValueLabels[0]->getText();
                            } else {
                                valueText = juce::String(int(value)) + "ms";
                            }
                            break;
                        }
                        case 1: valueText = juce::String(int(value * 100)) + "%"; break; // Feedback
                        case 2: { // Tone (Hz)
                            if (value >= 1000.0f)
                                valueText = juce::String(value / 1000.0f, 1) + "kHz";
                            else
                                valueText = juce::String(int(value)) + "Hz";
                            break;
                        }
                        case 3: valueText = juce::String(int(value * 100)) + "%"; break; // Drive
                        case 4: valueText = (value > 0.5f) ? "ON" : "OFF"; break; // PingPong
                        case 5: valueText = juce::String(int(value * 100)) + "%"; break; // WowFlutter
                        case 6: valueText = juce::String(int(value * 100)) + "%"; break; // RegenDamp
                        case 7: valueText = juce::String(int(value * 100)) + "%"; break; // Mix
                    }
                    
                    dubdelayValueLabels[i]->setText(valueText, juce::dontSendNotification);
                }
                
                // Update indicator bar
                if (dubdelayIndicatorBars[i]) {
                    float normValue = 0.0f;
                    switch (i) {
                        case 0: normValue = (dubdelayKnobs[i]->getValue() - 1.0f) / 1999.0f; break; // Time
                        case 1: normValue = dubdelayKnobs[i]->getValue() / 0.98f; break; // Feedback
                        case 2: normValue = (dubdelayKnobs[i]->getValue() - 200.0f) / 19800.0f; break; // Tone
                        case 3: case 5: case 6: case 7: normValue = dubdelayKnobs[i]->getValue(); break; // 0-1 params
                        case 4: normValue = dubdelayKnobs[i]->getValue(); break; // PingPong
                    }
                    dubdelayIndicatorBars[i]->setValue(normValue);
                }
            }
        };

        // Set knob images
        if (assets.knobRing != nullptr)
            dubdelayKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            dubdelayKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);

        if (i < 4)
            y -= 23;
        else
            y -= 1;

        dubdelayKnobs[i]->setBounds(x, y, knobSize, knobSize);

        // Create label
        dubdelayKnobLabels[i] = std::make_unique<juce::Label>();
        dubdelayKnobLabels[i]->setText(dubdelayKnobNames[i], juce::dontSendNotification);
        dubdelayKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        dubdelayKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        dubdelayKnobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(dubdelayKnobLabels[i].get());
        dubdelayKnobLabels[i]->setVisible(false);
        dubdelayKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);

        // Create value label
        dubdelayValueLabels[i] = std::make_unique<juce::Label>();
        dubdelayValueLabels[i]->setText("0", juce::dontSendNotification);
        dubdelayValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        dubdelayValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        dubdelayValueLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(dubdelayValueLabels[i].get());
        dubdelayValueLabels[i]->setVisible(false);
        dubdelayValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);

        // Create indicator bar
        dubdelayIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(dubdelayIndicatorBars[i].get());
        dubdelayIndicatorBars[i]->setVisible(false);
        dubdelayIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        dubdelayIndicatorBars[i]->setValue(0.5f);
        
        // Create dice button (hidden like other pages - NOT added to component tree)
        dubdelayDiceButtons[i] = std::make_unique<CustomDiceButton>();
        // Do NOT call addAndMakeVisible - keep it hidden
        dubdelayDiceButtons[i]->onClick = [this, i]() { randomizeIndividualDubDelayKnob(i); };

        // Create lock button
        dubdelayLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(dubdelayLockButtons[i].get());
        dubdelayLockButtons[i]->setVisible(false);
        
        // Calculate the width of the knob title text
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(dubdelayKnobNames[i]);
        
        // Use same sizing as space delay
        const int lockSize = 10; // Same as space delay
        const int lockSpacing = 5; // Fixed distance from end of title text
        
        // Position lock button at the end of the title text + fixed spacing
        int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
        int lockY = y - 10;
        
        dubdelayLockButtons[i]->setBounds(lockX, lockY, lockSize, lockSize);
        
        // Set lock button images
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            dubdelayLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        dubdelayLockButtons[i]->setToggleState(dubdelayKnobLocked[i], juce::dontSendNotification);
        
        dubdelayLockButtons[i]->onClick = [this, i]() {
            dubdelayKnobLocked[i] = !dubdelayKnobLocked[i];
            dubdelayLockButtons[i]->setToggleState(dubdelayKnobLocked[i], juce::dontSendNotification);
        };
        
        // Create APVTS attachment
        dubdelayAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), dubdelayParamIds[i], *dubdelayKnobs[i]);
        
        // Initialize lock state
        dubdelayKnobLocked[i] = false;
    }

    DBG("[UI] Dub Delay knobs setup complete");
}

void PluginEditor::setupDubDelayEffectsArea()
{
    DBG("[UI] Setting up Dub Delay effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Effect title (ALWAYS "EFFECT", NOT the effect name!)
    dubdelayEffectsTitle = std::make_unique<juce::Label>();
    dubdelayEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    dubdelayEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    dubdelayEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    dubdelayEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dubdelayEffectsTitle.get());
    dubdelayEffectsTitle->setVisible(false);
    dubdelayEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Dub Delay dice button (main randomize button - match Slicer/Grain page)
    dubdelayDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(dubdelayDiceButton.get());
    dubdelayDiceButton->setVisible(false);
    
    const int diceSize = 32;
    dubdelayDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    if (assets.diceLarge != nullptr) {
        dubdelayDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    dubdelayDiceButton->onClick = [this]() {
        randomizeDubDelayKnobValues();
    };
    
    // FX Power Button
    dubdelayFxPowerButton = std::make_unique<juce::DrawableButton>("DubDelayFxPower", juce::DrawableButton::ImageFitted);
    addAndMakeVisible(dubdelayFxPowerButton.get());
    dubdelayFxPowerButton->setVisible(false);
    dubdelayFxPowerButton->setClickingTogglesState(true);
    
    // Make button background transparent (match Slicer)
    dubdelayFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    dubdelayFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn) {
        dubdelayFxPowerButton->setImages(assets.fxPowerOn.get());
    }
    
    const int buttonSize = 46;
    dubdelayFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                     effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    dubdelayFxPowerButton->setToggleState(dubdelayFxAreaEnabled, juce::dontSendNotification);
    dubdelayFxPowerButton->setClickingTogglesState(true);
    dubdelayFxPowerButton->onClick = [this]() {
        dubdelayFxAreaEnabled = dubdelayFxPowerButton->getToggleState();
        updateDubDelayFxAreaVisibility();
        auto* param = processorRef.getAPVTS().getParameter("dubEnabled");
        if (param)
            param->setValueNotifyingHost(dubdelayFxAreaEnabled ? 1.0f : 0.0f);
    };
    
    // Time Sync Toggle Button (S circle - matches Space Delay style)
    class DubDelaySyncButton : public juce::Button {
    public:
        DubDelaySyncButton() : juce::Button("DubDelaySync") {}
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
    
    dubdelaySyncToggle = std::make_unique<DubDelaySyncButton>();
    addAndMakeVisible(dubdelaySyncToggle.get());
    dubdelaySyncToggle->setVisible(false);
    
    // Position relative to knob[0] label (Time knob)
    if (dubdelayKnobLabels[0] != nullptr) {
        auto lb = dubdelayKnobLabels[0]->getBounds();
        dubdelaySyncToggle->setBounds(lb.getX() + 10, lb.getY() + 4, 12, 12);
    } else {
        dubdelaySyncToggle->setBounds(effectArea.getX() + 10, effectArea.getY() + 10, 12, 12);
    }
    
    dubdelaySyncToggle->setClickingTogglesState(true);
    dubdelaySyncToggle->onClick = [this]() {
        if (!dubdelaySyncToggle) return;
        
        dubdelaySyncEnabled = dubdelaySyncToggle->getToggleState();
        
        // Update APVTS parameter
        auto* syncParam = processorRef.getAPVTS().getParameter("dubSync");
        if (syncParam) {
            syncParam->setValueNotifyingHost(dubdelaySyncEnabled ? 1.0f : 0.0f);
        }
        
        // Update the time knob value label
        updateDubDelayTimeLabel();
    };
    
    dubdelayGroup.push_back(dubdelayEffectsTitle.get());
    dubdelayGroup.push_back(dubdelayDiceButton.get());
    dubdelayGroup.push_back(dubdelayFxPowerButton.get());
    dubdelayGroup.push_back(dubdelaySyncToggle.get());
    
    for (int i = 0; i < 8; ++i) {
        dubdelayGroup.push_back(dubdelayKnobs[i].get());
        dubdelayGroup.push_back(dubdelayKnobLabels[i].get());
        dubdelayGroup.push_back(dubdelayValueLabels[i].get());
        dubdelayGroup.push_back(dubdelayIndicatorBars[i].get());
        dubdelayGroup.push_back(dubdelayLockButtons[i].get());
    }
    
    DBG("[UI] Dub Delay effects area setup complete");
}

void PluginEditor::setupDubDelaySequencerArea()
{
    DBG("[UI] Setting up Dub Delay sequencer area...");
    
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // "STEP" title
    dubdelayStepTitle = std::make_unique<juce::Label>();
    dubdelayStepTitle->setText("STEP", juce::dontSendNotification);
    dubdelayStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    dubdelayStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    dubdelayStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dubdelayStepTitle.get());
    dubdelayStepTitle->setVisible(false);
    dubdelayStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Step buttons (2x8 grid)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i)
    {
        dubdelayStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(dubdelayStepButtons[i].get());
        dubdelayStepButtons[i]->setVisible(false);
        
        int col = i % 8;
        int row = i / 8;
        int x = startX + col * (buttonSize + buttonSpacing);
        int y = startY + row * (buttonSize + buttonSpacing);
        
        dubdelayStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set images for step buttons
        if (assets.stepActive != nullptr)
            dubdelayStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        if (assets.stepInactive != nullptr)
            dubdelayStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        
        dubdelayStepButtons[i]->onClick = [this, i]() {
            onDubDelayStepButtonClicked(i);
        };
    }
    
    // Step amount label (TextEditor - match Slicer with white outline)
    dubdelayStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setDubDelayStepsUsed(16);
    dubdelayStepAmountLabel->setText("16");
    dubdelayStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    dubdelayStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    dubdelayStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    dubdelayStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    dubdelayStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    dubdelayStepAmountLabel->setJustification(juce::Justification::centred);
    dubdelayStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    dubdelayStepAmountLabel->setIndents(0, 0);
    dubdelayStepAmountLabel->setInputRestrictions(2, "0123456789");
    addAndMakeVisible(dubdelayStepAmountLabel.get());
    dubdelayStepAmountLabel->setVisible(false);
    dubdelayStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    dubdelayStepAmountLabel->onReturnKey = [this]() {
        int steps = dubdelayStepAmountLabel->getText().getIntValue();
        steps = juce::jlimit(1, 16, steps);
        processorRef.setDubDelayStepsUsed(steps);
        dubdelayStepAmountLabel->setText(juce::String(steps), juce::dontSendNotification);
        // TODO: Update sequencer UI to show/hide steps
    };
    
    // Rate dropdown (match Slicer styling)
    dubdelayRateDropdown = std::make_unique<juce::ComboBox>();
    dubdelayRateDropdown->addItem("4", 1);
    dubdelayRateDropdown->addItem("2", 2);
    dubdelayRateDropdown->addItem("1", 3);
    dubdelayRateDropdown->addItem("1/2", 4);
    dubdelayRateDropdown->addItem("1/4", 5);
    dubdelayRateDropdown->addItem("1/8", 6);
    dubdelayRateDropdown->addItem("1/16", 7);
    dubdelayRateDropdown->addItem("1/32", 8);
    dubdelayRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    dubdelayRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    dubdelayRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    dubdelayRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    dubdelayRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    
    int divIdx = processorRef.getDubDelaySeqState().divisionIndex.load();
    dubdelayRateDropdown->setSelectedId(divIdx + 1, juce::dontSendNotification);
    
    addAndMakeVisible(dubdelayRateDropdown.get());
    dubdelayRateDropdown->setVisible(false);
    dubdelayRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    dubdelayRateDropdown->onChange = [this]() {
        int selectedId = dubdelayRateDropdown->getSelectedId();
        if (selectedId > 0) {
            int divisionIndex = selectedId - 1;
            processorRef.setDubDelayDivisionIndex(divisionIndex);
            DBG("[UI] DubDelay rate changed to index: " << divisionIndex);
        }
    };
    
    // STD toggle (match Slicer)
    dubdelayStdToggle = std::make_unique<CircularToggleButton>();
    dubdelayStdToggle->setButtonText("-");
    addAndMakeVisible(dubdelayStdToggle.get());
    dubdelayStdToggle->setVisible(false);
    dubdelayStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    // Step dice button (CRITICAL: CustomDiceButton, NOT DrawableButton! 30% smaller = ~24px)
    dubdelayStepDiceButton = std::make_unique<CustomDiceButton>();
    dubdelayStepDiceButton->setVisible(false);
    int stepDiceSize = static_cast<int>(35 * 0.7);
    dubdelayStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, stepDiceSize, stepDiceSize);
    addAndMakeVisible(dubdelayStepDiceButton.get());
    if (assets.diceLarge != nullptr) {
        dubdelayStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    dubdelayStepDiceButton->onClick = [this]() {
        DBG("[UI] Dub Delay step dice button clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getDubDelaySafeSnapshot(step);
            
            // Randomize all Dub Delay parameters for this step (respecting lock states)
            // Only randomize if lock button is NOT toggled (locked)
            if (!dubdelayKnobLocked[0]) {
                snapshot.dubdelay.timeMs = 100.0f + juce::Random::getSystemRandom().nextFloat() * 1400.0f; // 100-1500ms
            }
            if (!dubdelayKnobLocked[1]) {
                snapshot.dubdelay.feedback = 0.2f + juce::Random::getSystemRandom().nextFloat() * 0.65f; // 0.2-0.85
            }
            if (!dubdelayKnobLocked[2]) {
                snapshot.dubdelay.toneHz = 1000.0f + juce::Random::getSystemRandom().nextFloat() * 14000.0f; // 1-15kHz
            }
            if (!dubdelayKnobLocked[3]) {
                snapshot.dubdelay.drive = juce::Random::getSystemRandom().nextFloat() * 0.6f; // 0-0.6
            }
            if (!dubdelayKnobLocked[4]) {
                snapshot.dubdelay.pingPong = juce::Random::getSystemRandom().nextBool();
            }
            if (!dubdelayKnobLocked[5]) {
                snapshot.dubdelay.wowFlutter = juce::Random::getSystemRandom().nextFloat() * 0.5f; // 0-0.5
            }
            if (!dubdelayKnobLocked[6]) {
                snapshot.dubdelay.regenDamp = juce::Random::getSystemRandom().nextFloat() * 0.6f; // 0-0.6
            }
            if (!dubdelayKnobLocked[7]) {
                snapshot.dubdelay.mix = 0.2f + juce::Random::getSystemRandom().nextFloat() * 0.6f; // 0.2-0.8
            }
            
            processorRef.setDubDelayStepSnapshot(step, snapshot);
        }
        
        // Reload current step's values into knobs
        int selectedStep = processorRef.getDubDelayUiSelectedStep();
        auto updatedSnapshot = processorRef.getDubDelaySafeSnapshot(selectedStep);
        
        // Load snapshot values into knobs using normalized values and sendNotification for proper UI updates
        if (dubdelayKnobs[0]) {
            auto* param = processorRef.getAPVTS().getParameter("dubTimeMs");
            if (param) dubdelayKnobs[0]->setValue(param->convertTo0to1(updatedSnapshot.dubdelay.timeMs), juce::sendNotification);
        }
        if (dubdelayKnobs[1]) {
            auto* param = processorRef.getAPVTS().getParameter("dubFeedback");
            if (param) dubdelayKnobs[1]->setValue(param->convertTo0to1(updatedSnapshot.dubdelay.feedback), juce::sendNotification);
        }
        if (dubdelayKnobs[2]) {
            auto* param = processorRef.getAPVTS().getParameter("dubToneHz");
            if (param) dubdelayKnobs[2]->setValue(param->convertTo0to1(updatedSnapshot.dubdelay.toneHz), juce::sendNotification);
        }
        if (dubdelayKnobs[3]) {
            dubdelayKnobs[3]->setValue(updatedSnapshot.dubdelay.drive, juce::sendNotification);
        }
        if (dubdelayKnobs[4]) {
            dubdelayKnobs[4]->setValue(updatedSnapshot.dubdelay.pingPong ? 1.0f : 0.0f, juce::sendNotification);
        }
        if (dubdelayKnobs[5]) {
            dubdelayKnobs[5]->setValue(updatedSnapshot.dubdelay.wowFlutter, juce::sendNotification);
        }
        if (dubdelayKnobs[6]) {
            dubdelayKnobs[6]->setValue(updatedSnapshot.dubdelay.regenDamp, juce::sendNotification);
        }
        if (dubdelayKnobs[7]) {
            dubdelayKnobs[7]->setValue(updatedSnapshot.dubdelay.mix, juce::sendNotification);
        }
        
        // Update the sequencer UI to reflect the changes
        updateDubDelaySequencerUI();
        
        DBG("[UI] Dub Delay randomization complete - all 16 steps randomized");
    };
    
    // Step power button (match Slicer)
    dubdelayStepPowerButton = std::make_unique<juce::DrawableButton>("DubDelayStepPower", juce::DrawableButton::ImageFitted);
    
    // Make button background transparent (match Slicer)
    dubdelayStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    dubdelayStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn) {
        dubdelayStepPowerButton->setImages(assets.stepPowerOn.get());
    }
    addAndMakeVisible(dubdelayStepPowerButton.get());
    dubdelayStepPowerButton->setVisible(false);
    dubdelayStepPowerButton->setClickingTogglesState(true);
    const int stepPowerSize = 40;
    dubdelayStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - stepPowerSize - 5 + 15 - 5 - 1, sequencerArea.getY() - 5 - stepPowerSize + 25 + 5, stepPowerSize, stepPowerSize);
    dubdelayStepPowerButton->setToggleState(dubdelayStepAreaEnabled, juce::dontSendNotification);
    dubdelayStepPowerButton->setClickingTogglesState(true);
    dubdelayStepPowerButton->onClick = [this]() {
        dubdelayStepAreaEnabled = dubdelayStepPowerButton->getToggleState();
        processorRef.setDubDelaySequencerEnabled(dubdelayStepAreaEnabled);
        updateDubDelayStepAreaVisibility();
    };
    
    dubdelayGroup.push_back(dubdelayStepTitle.get());
    dubdelayGroup.push_back(dubdelayStepAmountLabel.get());
    dubdelayGroup.push_back(dubdelayRateDropdown.get());
    dubdelayGroup.push_back(dubdelayStdToggle.get());
    dubdelayGroup.push_back(dubdelayStepDiceButton.get());
    dubdelayGroup.push_back(dubdelayStepPowerButton.get());
    
    for (int i = 0; i < 16; ++i) {
        dubdelayGroup.push_back(dubdelayStepButtons[i].get());
    }
    
    DBG("[UI] Dub Delay sequencer area setup complete");
}

void PluginEditor::setupDubDelayAllStepsToggle()
{
    DBG("[UI] Setting up Dub Delay All Steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    dubdelayAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(dubdelayAllStepsToggle.get());
    dubdelayAllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    dubdelayAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                       effectArea.getY() - 1, buttonSize, buttonSize);
    
    if (assets.stepTopInactive && assets.stepTopActive) {
        static_cast<AllStepsToggleButton*>(dubdelayAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    dubdelayAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    dubdelayAllStepsToggle->onClick = [this]() {
        dubdelayAllStepsEnabled = dubdelayAllStepsToggle->getToggleState();
        DBG("[UI] DubDelay All Steps toggle: " << (dubdelayAllStepsEnabled ? "ON" : "OFF"));
        dubdelayAllStepsLabel->setAlpha(dubdelayAllStepsEnabled ? 1.0f : 0.5f);
    };
    
    dubdelayAllStepsLabel = std::make_unique<juce::Label>();
    dubdelayAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    dubdelayAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    dubdelayAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    dubdelayAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dubdelayAllStepsLabel.get());
    dubdelayAllStepsLabel->setVisible(false);
    dubdelayAllStepsLabel->setAlpha(1.0f);
    dubdelayAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                      effectArea.getY() + 1, 80, 24);
    
    dubdelayGroup.push_back(dubdelayAllStepsToggle.get());
    dubdelayGroup.push_back(dubdelayAllStepsLabel.get());
    
    DBG("[UI] Dub Delay All Steps toggle setup complete");
}

void PluginEditor::updateDubDelayFxAreaVisibility()
{
    float alpha = dubdelayFxAreaEnabled ? 1.0f : 0.3f;
    
    // Update knobs and labels alpha (match Slicer)
    for (int i = 0; i < 8; ++i) {
        if (dubdelayKnobs[i]) { 
            dubdelayKnobs[i]->setAlpha(alpha); 
            dubdelayKnobs[i]->setEnabled(dubdelayFxAreaEnabled);
        }
        if (dubdelayKnobLabels[i]) dubdelayKnobLabels[i]->setAlpha(alpha);
        if (dubdelayValueLabels[i]) dubdelayValueLabels[i]->setAlpha(alpha);
        if (dubdelayIndicatorBars[i]) dubdelayIndicatorBars[i]->setAlpha(alpha);
    }
    
    if (dubdelayDiceButton) {
        dubdelayDiceButton->setAlpha(alpha);
        dubdelayDiceButton->setEnabled(dubdelayFxAreaEnabled);
    }
    if (dubdelayEffectsTitle) dubdelayEffectsTitle->setAlpha(alpha);
    
    // Grey the time sync button
    if (dubdelaySyncToggle) { 
        dubdelaySyncToggle->setAlpha(alpha); 
        dubdelaySyncToggle->setEnabled(dubdelayFxAreaEnabled); 
    }
    
    repaint();
}

void PluginEditor::updateDubDelayStepAreaVisibility()
{
    float alpha = dubdelayStepAreaEnabled ? 1.0f : 0.3f;
    
    // Update step buttons (match Slicer)
    for (int i = 0; i < 16; ++i) {
        if (dubdelayStepButtons[i]) {
            dubdelayStepButtons[i]->setAlpha(alpha);
            dubdelayStepButtons[i]->setEnabled(dubdelayStepAreaEnabled);
        }
    }
    
    // Update sequencer controls
    if (dubdelayStepAmountLabel) {
        dubdelayStepAmountLabel->setAlpha(alpha);
        dubdelayStepAmountLabel->setEnabled(dubdelayStepAreaEnabled);
    }
    if (dubdelayRateDropdown) {
        dubdelayRateDropdown->setAlpha(alpha);
        dubdelayRateDropdown->setEnabled(dubdelayStepAreaEnabled);
    }
    if (dubdelayStdToggle) {
        dubdelayStdToggle->setAlpha(alpha);
        dubdelayStdToggle->setEnabled(dubdelayStepAreaEnabled);
    }
    if (dubdelayStepTitle) dubdelayStepTitle->setAlpha(alpha);
    if (dubdelayStepDiceButton) {
        dubdelayStepDiceButton->setAlpha(alpha);
        dubdelayStepDiceButton->setEnabled(dubdelayStepAreaEnabled);
    }
    
    repaint();
}

void PluginEditor::randomizeDubDelayKnobValues()
{
    DBG("[UI] Randomizing Dub Delay knob values...");
    for (int i = 0; i < 8; ++i) {
        if (i != 7 && dubdelayKnobs[i]) { // Skip Mix (knob 7)
            randomizeIndividualDubDelayKnob(i);
        }
    }
}

void PluginEditor::randomizeIndividualDubDelayKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8) return;
    if (!dubdelayKnobs[knobIndex]) return;
    
    juce::Random rand;
    float newValue = 0.0f;
    
    switch (knobIndex) {
        case 0: newValue = 100.0f + rand.nextFloat() * 1400.0f; break; // Time: 100-1500ms
        case 1: newValue = 0.2f + rand.nextFloat() * 0.65f; break; // Feedback: 0.2-0.85
        case 2: newValue = 1000.0f + rand.nextFloat() * 14000.0f; break; // Tone: 1-15kHz
        case 3: newValue = rand.nextFloat() * 0.6f; break; // Drive: 0-0.6
        case 4: newValue = (rand.nextFloat() > 0.5f) ? 1.0f : 0.0f; break; // PingPong: random
        case 5: newValue = rand.nextFloat() * 0.5f; break; // WowFlutter: 0-0.5
        case 6: newValue = rand.nextFloat() * 0.6f; break; // RegenDamp: 0-0.6
        case 7: newValue = 0.2f + rand.nextFloat() * 0.6f; break; // Mix: 0.2-0.8
    }
    
    dubdelayKnobs[knobIndex]->setValue(newValue, juce::sendNotification);
}

void PluginEditor::updateSpaceDelayTimeLabel()
{
    if (!valueLabels[0]) return;
    
    if (timeSyncEnabled) {
        // Show division label
        auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("delayTimeDiv"));
        
        int divIdx = divParam ? divParam->getIndex() : 5; // Default 1/4 (index 5)
        divIdx = juce::jlimit(0, 20, divIdx);
        
        // Use the same labels as the parameter array
        static const char* divStrings[] = {
            "2", "1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", 
            "1/8", "1/8D", "1/8T", "1/16", "1/16D", "1/16T", "1/32", "1/32D", "1/32T", 
            "1/64", "1/64D", "1/64T"
        };
        juce::String label = divStrings[divIdx];
        
        valueLabels[0]->setText(label, juce::dontSendNotification);
    } else {
        // Show ms
        if (knobs[0]) {
            float timeMs = knobs[0]->getValue();
            valueLabels[0]->setText(juce::String(int(timeMs)) + "ms", juce::dontSendNotification);
        }
    }
}

// Formant UI helper methods
void PluginEditor::updateFormantFxAreaVisibility()
{
    float alpha = formantFxAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 8; ++i) { // All 8 knobs
        if (formantKnobs[i]) { 
            formantKnobs[i]->setAlpha(alpha); 
            formantKnobs[i]->setEnabled(formantFxAreaEnabled); 
        }
        if (formantKnobLabels[i]) formantKnobLabels[i]->setAlpha(alpha);
        if (formantValueLabels[i]) formantValueLabels[i]->setAlpha(alpha);
        if (formantIndicatorBars[i]) formantIndicatorBars[i]->setAlpha(alpha);
        if (formantDiceButtons[i]) { 
            formantDiceButtons[i]->setEnabled(formantFxAreaEnabled);
            formantDiceButtons[i]->setAlpha(alpha);
        }
    }
}

void PluginEditor::updateFormantStepAreaVisibility()
{
    float alpha = formantStepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i) {
        if (formantStepButtons[i]) {
            formantStepButtons[i]->setAlpha(alpha);
            formantStepButtons[i]->setEnabled(formantStepAreaEnabled);
            formantStepButtons[i]->setVisible(true);
        }
    }
}

void PluginEditor::updateFormantSequencerUI()
{
    // Update step buttons to show selected and playing states
    int selectedStep = formantUiSelectedStep.load();
    int playingStep = processorRef.getFormantPlayingStep();
    bool sequencerEnabled = processorRef.getFormantSeqState().enabled.load();
    
    for (int i = 0; i < 16; ++i) {
        if (formantStepButtons[i] != nullptr) {
            formantStepButtons[i]->setSelected(i == selectedStep);
            formantStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            formantStepButtons[i]->setEnabledStep(true); // All steps enabled for now
        }
    }
}

void PluginEditor::setupFormantKnobs()
{
    // Formant knob titles - 8 controls
    const juce::StringArray formantKnobTitles = {
        "Vowel",
        "Sharpness",
        "Emphasis",
        "Shift",
        "Brightness",
        "Motion",
        "Air",
        "Mix"
    };
    
    // Formant parameter IDs (must match APVTS order)
    const juce::StringArray formantParamIDs = {
        "vowel",
        "resonance",
        "intensity",
        "formantShift",
        "formantBrightness",
        "formantMotion",
        "formantAir",
        "mix"
    };
    
    DBG("[UI] Setting up Formant knobs...");

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    // Create and setup knobs - 8 knobs
    for (int i = 0; i < 8; ++i)
    {
        // Position 8 knobs in 2 rows of 4 (EXACT same as other effects)
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px (EXACT same as other effects)
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1; // Moved up 6px from +5 to -1
        
        // Create knob
        formantKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(formantKnobs[i].get());
        formantKnobs[i]->setVisible(false);
        formantKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        formantKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        // Set knob ranges based on parameter (8 knobs)
        switch (i) {
            case 0: // Vowel (0-4, continuous)
                formantKnobs[i]->setRange(0.0, 4.0, 0.01);
                formantKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 1: // Sharpness (Q: 0.4-18)
                formantKnobs[i]->setRange(0.4, 18.0, 0.1);
                formantKnobs[i]->setValue(10.0, juce::dontSendNotification);
                break;
            case 2: // Emphasis (-6..+18 dB)
                formantKnobs[i]->setRange(-6.0, 18.0, 0.1);
                formantKnobs[i]->setValue(12.0, juce::dontSendNotification);
                break;
            case 3: // Shift (0.5-2.0)
                formantKnobs[i]->setRange(0.5, 2.0, 0.01);
                formantKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 4: // Brightness (-12..+12 dB)
                formantKnobs[i]->setRange(-12.0, 12.0, 0.1);
                formantKnobs[i]->setValue(3.0, juce::dontSendNotification);
                break;
            case 5: // Motion (0-1)
                formantKnobs[i]->setRange(0.0, 1.0, 0.01);
                formantKnobs[i]->setValue(0.25, juce::dontSendNotification);
                break;
            case 6: // Air (0-1)
                formantKnobs[i]->setRange(0.0, 1.0, 0.01);
                formantKnobs[i]->setValue(0.2, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                formantKnobs[i]->setRange(0.0, 1.0, 0.01);
                formantKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
        }
        
        // Set knob images (CRITICAL - this makes them look like proper knobs!)
        if (assets.knobRing != nullptr)
            formantKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            formantKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        // Position knob
        formantKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label
        formantKnobLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(formantKnobLabels[i].get());
        formantKnobLabels[i]->setVisible(false);
        formantKnobLabels[i]->setText(formantKnobTitles[i], juce::dontSendNotification);
        formantKnobLabels[i]->setJustificationType(juce::Justification::centred);
        formantKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        formantKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        formantKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);
        
        // Create value label
        formantValueLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(formantValueLabels[i].get());
        formantValueLabels[i]->setVisible(false);
        formantValueLabels[i]->setText("0", juce::dontSendNotification);
        formantValueLabels[i]->setJustificationType(juce::Justification::centred);
        formantValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        formantValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        formantValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);
        
        // Create indicator bar
        formantIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(formantIndicatorBars[i].get());
        formantIndicatorBars[i]->setVisible(false);
        formantIndicatorBars[i]->setValue(0.5f);
        formantIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        
        // Create dice button
        formantDiceButtons[i] = std::make_unique<CustomDiceButton>();
        addAndMakeVisible(formantDiceButtons[i].get());
        formantDiceButtons[i]->setVisible(false);
        
        // Create APVTS attachment
        formantAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), formantParamIDs[i], *formantKnobs[i]);
        
        // Add value change callback to update value label and indicator bar
        formantKnobs[i]->onValueChange = [this, i]() {
            // Skip if loading from snapshot (prevents circular updates during randomization)
            if (isLoadingFromSnapshot.load())
                return;
            
            if (formantKnobs[i] && formantValueLabels[i] && formantIndicatorBars[i]) {
                float value = formantKnobs[i]->getValue();
                juce::String valueText;
                
                switch (i) {
                    case 0: // Vowel
                        {
                        static const char* vowelNames[] = {"A", "E", "I", "O", "U"};
                        int vowelIndex = static_cast<int>(value);
                        vowelIndex = juce::jlimit(0, 4, vowelIndex);
                        valueText = vowelNames[vowelIndex];
                    }
                        break;
                    case 1: valueText = juce::String(value, 1); break; // Resonance (Q)
                    case 2: valueText = juce::String(value, 1) + " dB"; break; // Intensity
                    case 3: valueText = juce::String(value, 2); break; // Mix
                }

                formantValueLabels[i]->setText(valueText, juce::dontSendNotification);

                // Update indicator bar
                formantIndicatorBars[i]->setValue(value);
                
                // Update current step snapshot with new value
                processorRef.updateFormantCurrentStepSnapshot(i, value);
                
                // If All Steps toggle is active, update all step snapshots
                if (formantAllStepsEnabled) {
                    DBG("[All Steps] Formant knob " << i << " changed, formantAllStepsEnabled=true");
                    
                    // Update all 16 step snapshots with the new value
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getFormantSafeSnapshot(step);
                        switch (i) {
                            case 0: snapshot.formant.vowel = value; break;
                            case 1: snapshot.formant.resonance = value; break;
                            case 2: snapshot.formant.intensity = value; break;
                            case 3: snapshot.formant.mix = value; break;
                        }
                        processorRef.setFormantStepSnapshot(step, snapshot);
                    }
                }
                
                // Update formant overlay when knobs change
                updateFormantOverlay();
            }
        };
    }
    
    DBG("[UI] Formant knobs setup complete");
}

void PluginEditor::setupFormantEffectsArea()
{
    DBG("[UI] Setting up Formant effects area...");
    
    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title label (ALWAYS "EFFECT", NOT the effect name!)
    formantEffectsTitle = std::make_unique<juce::Label>();
    formantEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    formantEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    formantEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    formantEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(formantEffectsTitle.get());
    formantEffectsTitle->setVisible(false); // Initially hidden until Formant page is selected
    formantEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Create dice button (EXACT same positioning as other pages)
    formantDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(formantDiceButton.get());
    formantDiceButton->setVisible(false); // Initially hidden until Formant page is selected
    
    // Position dice button to the right of the title (EXACT same as other pages)
    const int diceSize = 32; // 20% smaller: 40 * 0.8 = 32
    formantDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    
    // Set the dice image (EXACT same as other pages)
    if (assets.diceLarge != nullptr)
    {
        formantDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    formantDiceButton->onClick = [this]() { randomizeFormantKnobValues(); };
    
    // Create FX power button (EXACT same positioning as other pages)
    formantFxPowerButton = std::make_unique<juce::DrawableButton>("formantFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(formantFxPowerButton.get());
    formantFxPowerButton->setVisible(false); // Initially hidden until Formant page is selected
    formantFxPowerButton->setClickingTogglesState(true);
    
    // Make button background transparent (match other pages)
    formantFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    formantFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    const int buttonSize = 46;
    formantFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                   effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    
    // Set up power button images
    if (assets.fxPowerOn != nullptr)
    {
        formantFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }
    
    // Set up power button click handler
    formantFxPowerButton->onClick = [this]() {
        formantFxAreaEnabled = formantFxPowerButton->getToggleState();
        
        // Update APVTS parameter
        auto* param = processorRef.getAPVTS().getParameter("formantEnabled");
        if (param) {
            param->setValueNotifyingHost(formantFxAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateFormantFxAreaVisibility();
        DBG("[UI] Formant FX power: " << (formantFxAreaEnabled ? "ON" : "OFF"));
    };
    
    DBG("[UI] Formant effects area setup complete");
}

void PluginEditor::setupFormantSequencerArea()
{
    DBG("[UI] Setting up Formant sequencer area...");
    
    // Step area bounds (EXACT same as Space Delay page)
    auto stepArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create STEP title (EXACT same as Space Delay page)
    formantStepTitle = std::make_unique<juce::Label>();
    formantStepTitle->setText("STEP", juce::dontSendNotification);
    formantStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    formantStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    formantStepTitle->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    formantStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(formantStepTitle.get());
    formantStepTitle->setVisible(false);
    formantStepTitle->setBounds(stepArea.getX() + 10, stepArea.getY(), 80, 30); // Moved down 2px more (total 10px down from original -10)
    
    // Create step dice button (next to STEP title, moved down 15px total and left 15px total)
    formantStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(formantStepDiceButton.get());
    int stepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    formantStepDiceButton->setBounds(stepArea.getX() + 75, stepArea.getY() + 5, stepDiceSize, stepDiceSize); // Moved down another 10px and left another 10px
    
    // Set up step dice button callback to randomize all Formant step snapshots
    formantStepDiceButton->onClick = [this]() {
        DBG("[UI] Formant step dice button clicked - randomizing all step snapshots");
        
        for (int step = 0; step < 16; ++step) {
            randomizeEffectStepSnapshot(FxPageID::Formant, step);
        }
        
        // Update UI to show the changes
        updateFormantSequencerUI();
        
        // Load the current step's randomized values into the knobs
        loadSelectedStepIntoKnobs(FxPageID::Formant);
    };
    
    // Set the dice image
    if (assets.diceLarge != nullptr)
    {
        formantStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    
    // Create step amount label (EXACT same as Space Delay page)
    formantStepAmountLabel = std::make_unique<juce::Label>();
    // Force to 16 by default, then sync with processor state
    processorRef.setFormantStepsUsed(16);
    formantStepAmountLabel->setText("16", juce::dontSendNotification);
    formantStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    formantStepAmountLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    formantStepAmountLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    formantStepAmountLabel->setColour(juce::Label::outlineColourId, juce::Colours::white);
    formantStepAmountLabel->setJustificationType(juce::Justification::centred);
    formantStepAmountLabel->setBorderSize(juce::BorderSize<int>(2));
    // Allow direct editing for step count (1..16)
    formantStepAmountLabel->setEditable(true, true, false);
    formantStepAmountLabel->onEditorHide = [this]() {
        if (formantStepAmountLabel != nullptr)
        {
            int value = formantStepAmountLabel->getText().getIntValue();
            value = juce::jlimit(1, 16, value);
            processorRef.setFormantStepsUsed(value);
            formantStepAmountLabel->setText(juce::String(value), juce::dontSendNotification);
            updateFormantSequencerUI();
        }
    };
    addAndMakeVisible(formantStepAmountLabel.get());
    // Move step amount left by 80px
    formantStepAmountLabel->setBounds(stepArea.getX() + 180, stepArea.getY() - 10, 30, 25);
    
    // Create rate dropdown (EXACT same as Space Delay page)
    formantRateDropdown = std::make_unique<juce::ComboBox>();
    // Slower divisions added: 4 and 2 bars; and rename 1/1 to 1
    formantRateDropdown->addItem("4", 1);      // 4 bars (16 beats)
    formantRateDropdown->addItem("2", 2);      // 2 bars (8 beats)
    formantRateDropdown->addItem("1", 3);      // 1 bar  (4 beats)
    formantRateDropdown->addItem("1/2", 4);
    formantRateDropdown->addItem("1/4", 5);
    formantRateDropdown->addItem("1/8", 6);
    formantRateDropdown->addItem("1/16", 7);
    formantRateDropdown->addItem("1/32", 8);
    formantRateDropdown->addItem("1/64", 9);
    formantRateDropdown->addItem("1/128", 10);
    formantRateDropdown->addItem("1/256", 11);
    formantRateDropdown->addItem("1/512", 12);
    formantRateDropdown->addItem("1/1024", 13);
    formantRateDropdown->addItem("1/2048", 14);
    formantRateDropdown->addItem("1/4096", 15);
    formantRateDropdown->addItem("1/8192", 16);
    formantRateDropdown->addItem("1/16384", 17);
    formantRateDropdown->addItem("1/32768", 18);
    formantRateDropdown->addItem("1/65536", 19);
    formantRateDropdown->addItem("1/131072", 20);
    formantRateDropdown->setSelectedId(5); // Default to 1/4
    // Make dropdown transparent (no background or border) - EXACT same as Space Delay
    formantRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    formantRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    formantRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    formantRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    formantRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible(formantRateDropdown.get());
    formantRateDropdown->setVisible(false);
    formantRateDropdown->setBounds(stepArea.getX() + 220, stepArea.getY() - 10, 74, 25); // EXACT same as Space Delay
    
    // Set up rate dropdown callback
    formantRateDropdown->onChange = [this]() {
        int divisionIndex = formantRateDropdown->getSelectedId() - 1;
        processorRef.setFormantDivisionIndex(divisionIndex);
        DBG("[UI] Formant rate changed to division index: " << divisionIndex);
    };
    
    // Create STD toggle (EXACT same as Space Delay page)
    formantStdToggle = std::make_unique<CircularToggleButton>();
    formantStdToggle->setButtonText("-");
    addAndMakeVisible(formantStdToggle.get());
    formantStdToggle->setVisible(false);
    formantStdToggle->setBounds(stepArea.getX() + 288, stepArea.getY() - 14, 30, 30); // EXACT same as Space Delay
    
    // Set up STD toggle callback
    formantStdToggle->onClick = [this]() {
        // Cycle through -/t/. states - EXACT same as Space Delay
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        const char* labels[] = {"-", "t", "."};
        formantStdToggle->setButtonText(labels[stdState]);
        // Inform processor of new STD mode for timing
        processorRef.setFormantStdMode(stdState);
        DBG("[UI] Formant STD toggle clicked: state=" << stdState << " label=" << labels[stdState]);
    };
    
    // Create step buttons (EXACT same as Space Delay page)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = stepArea.getX() + 20;
    const int startY = stepArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        formantStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(formantStepButtons[i].get());
        formantStepButtons[i]->setVisible(false);
        
        // Position buttons in 2 rows of 8
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        formantStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set step button images
        if (assets.stepActive) {
            formantStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive) {
            formantStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        // Set click handler
        formantStepButtons[i]->onClick = [this, i]() {
            // Save current step's snapshot before switching (if All Steps is OFF)
            if (!formantAllStepsEnabled) {
                saveCurrentStepSnapshot();
            }
            
            formantUiSelectedStep.store(i);
            processorRef.setFormantSelectedStep(i);
            updateFormantSequencerUI();
            
            // Load the snapshot for this step into the knobs
            loadSelectedStepIntoKnobs(FxPageID::Formant);
            
            DBG("[UI] Formant step " << i << " selected");
        };
    }
    
    // Create step power button (EXACT same positioning as Space Delay page)
    formantStepPowerButton = std::make_unique<juce::DrawableButton>("formantStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(formantStepPowerButton.get());
    formantStepPowerButton->setVisible(false);
    formantStepPowerButton->setClickingTogglesState(true);
    
    // Position at top right corner of step area, 20% smaller than 50px and adjusted position
    const int stepButtonSize = 40; // 50 * 0.8 = 40 (20% smaller)
    formantStepPowerButton->setBounds(stepArea.getX() + stepArea.getWidth() - stepButtonSize - 5 + 15 - 5 - 1, stepArea.getY() - 5 - 40 + 25 + 5, stepButtonSize, stepButtonSize); // 1px right, 5px up
    
    // Remove background colors
    formantStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    formantStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    // Set up image
    if (assets.stepPowerOn != nullptr)
    {
        formantStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    
    // Set up click handler
    formantStepPowerButton->onClick = [this]() {
        formantStepAreaEnabled = formantStepPowerButton->getToggleState();
        
        // Update sequencer state
        processorRef.setFormantSequencerEnabled(formantStepAreaEnabled);
        
        // Update APVTS parameter
        auto* param = processorRef.getAPVTS().getParameter("formantStepEnabled");
        if (param) {
            param->setValueNotifyingHost(formantStepAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateFormantStepAreaVisibility();
        DBG("[UI] Formant step power: " << (formantStepAreaEnabled ? "ON" : "OFF"));
    };
    
    DBG("[UI] Formant sequencer area setup complete");
}

void PluginEditor::setupFormantAllStepsToggle()
{
    DBG("[UI] Setting up Formant all steps toggle...");
    
    // Effect area bounds (EXACT same as Space Delay page)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create All Steps toggle button (EXACT same as Space Delay page)
    formantAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(formantAllStepsToggle.get());
    formantAllStepsToggle->setVisible(false);
    
    // Position at top middle of effect area, moved 30px right, increased 20% and moved up 6px total
    const int buttonSize = 29; // 24 * 1.2 = 28.8, rounded to 29
    formantAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, effectArea.getY() - 1, buttonSize, buttonSize); // Moved up 2px more from 1 to -1
    
    // Set up images (EXACT same as Space Delay page)
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr)
    {
        formantAllStepsToggle->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    // Set up click handler (EXACT same as Space Delay page)
    formantAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    formantAllStepsToggle->onClick = [this]() {
        formantAllStepsEnabled = formantAllStepsToggle->getToggleState();
        DBG("[UI] All Steps toggle: " + juce::String(formantAllStepsEnabled ? "ON" : "OFF") + " toggleState=" + juce::String(formantAllStepsToggle->getToggleState() ? 1 : 0));
        DBG("[UI] All Steps toggle clicked - current page: " + juce::String(static_cast<int>(currentPage)));
        
        // Test if All Steps is working by checking if we're on Formant page
        if (currentPage == FxPageID::Formant) {
            DBG("[UI] Formant All Steps toggle state: " + juce::String(formantAllStepsEnabled ? "ON" : "OFF"));
        }
    };
    
    // Create "All Steps" label (EXACT same as Space Delay page)
    formantAllStepsLabel = std::make_unique<juce::Label>();
    formantAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    formantAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold)); // 12.0f * 1.2 = 14.4f (20% bigger)
    formantAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    formantAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(formantAllStepsLabel.get());
    formantAllStepsLabel->setVisible(false);
    
    // Position label to the right of the button, moved 30px right, moved up 4px
    formantAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, effectArea.getY() + 1, 80, 24); // Moved up 4px from 5 to 1
    
    DBG("[UI] Formant all steps toggle setup complete");
    
    // Populate Formant group for visibility management (same pattern as other effects)
    formantGroup.clear();
    
    // Add all Formant components to the group
    for (int i = 0; i < 8; ++i) { // All 8 knobs
        if (formantKnobs[i]) formantGroup.push_back(formantKnobs[i].get());
        if (formantKnobLabels[i]) formantGroup.push_back(formantKnobLabels[i].get());
        if (formantValueLabels[i]) formantGroup.push_back(formantValueLabels[i].get());
        if (formantIndicatorBars[i]) formantGroup.push_back(formantIndicatorBars[i].get());
        if (formantDiceButtons[i]) formantGroup.push_back(formantDiceButtons[i].get());
    }
    
    // Add other Formant components to group
    if (formantEffectsTitle) formantGroup.push_back(formantEffectsTitle.get());
    if (formantDiceButton) formantGroup.push_back(formantDiceButton.get());
    if (formantFxPowerButton) formantGroup.push_back(formantFxPowerButton.get());
    if (formantStepTitle) formantGroup.push_back(formantStepTitle.get());
    if (formantStepDiceButton) formantGroup.push_back(formantStepDiceButton.get());
    if (formantStepAmountLabel) formantGroup.push_back(formantStepAmountLabel.get());
    if (formantRateDropdown) formantGroup.push_back(formantRateDropdown.get());
    if (formantStdToggle) formantGroup.push_back(formantStdToggle.get());
    if (formantStepPowerButton) formantGroup.push_back(formantStepPowerButton.get());
    if (formantAllStepsToggle) formantGroup.push_back(formantAllStepsToggle.get());
    if (formantAllStepsLabel) formantGroup.push_back(formantAllStepsLabel.get());
    
    // Add step buttons to group
    for (int i = 0; i < 16; ++i) {
        if (formantStepButtons[i]) formantGroup.push_back(formantStepButtons[i].get());
    }
    
    DBG("[UI] Formant page setup complete");
}

//==============================================================================
// Filter Page Implementation
//==============================================================================

void PluginEditor::setupFilterKnobs()
{
    DBG("[UI] Setting up Filter knobs...");
    
    // Filter controls: 8 knobs total
    // Layout: 2 rows of 4 knobs
    // Row 1: Type knob, Cutoff knob, Res knob, Slope knob
    // Row 2: Drive knob, Spread knob, Key Track knob, Mix knob
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;
    
    // Parameter IDs for 7 knobs (Type, Cutoff, Res, Slope, Drive, Key Track, Mix) - Spread removed
    std::vector<juce::String> filterParamIds = {
        "fType", "cutoff", "res", "slope", "filterDrive", "keytrack", "filterMix"
    };
    
    // Knob names for 7 knobs (Spread removed)
    std::vector<juce::String> filterKnobNames = {
        "Type", "Cutoff", "Resonance", "Slope", "Drive", "Key Track", "Mix"
    };
    
    // Create 8 knobs in a 2x4 grid (skip spread position at knobIdx 5)
    for (int knobIdx = 0; knobIdx < 8; ++knobIdx)
    {
        int col = knobIdx % 4;
        int row = knobIdx / 4;
        int x = startX + col * (knobSize + knobSpacing);
        int y = startY + row * (knobSize + 20);
        if (row == 0) y -= 23; else y -= 1;
        
        // Create knob (Type and Slope are special cases)
        if (knobIdx == 0) {
            // Type knob
            filterTypeKnob = std::make_unique<CustomKnob>();
            filterTypeKnob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            filterTypeKnob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            filterTypeKnob->setRange(0.0, 4.0, 1.0); // 0=LP, 1=HP, 2=BP, 3=Comb-, 4=Comb+
            filterTypeKnob->setValue(0.0, juce::dontSendNotification); // Default LP
            
            if (assets.knobRing != nullptr)
                filterTypeKnob->setRingImage(assets.knobRing->createCopy());
            if (assets.knobInside != nullptr)
                filterTypeKnob->setInnerImage(assets.knobInside->createCopy());
            
            filterTypeKnob->setBounds(x, y, knobSize, knobSize);
            addAndMakeVisible(filterTypeKnob.get());
            filterTypeKnob->setVisible(false);
        } else if (knobIdx == 3) {
            // Slope knob
            filterSlopeKnob = std::make_unique<CustomKnob>();
            filterSlopeKnob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            filterSlopeKnob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            filterSlopeKnob->setRange(0.0, 1.0, 0.01); // 0=12dB, 1=24dB
            filterSlopeKnob->setValue(1.0, juce::dontSendNotification); // Default 24dB
            
            if (assets.knobRing != nullptr)
                filterSlopeKnob->setRingImage(assets.knobRing->createCopy());
            if (assets.knobInside != nullptr)
                filterSlopeKnob->setInnerImage(assets.knobInside->createCopy());
            
            filterSlopeKnob->setBounds(x, y, knobSize, knobSize);
            addAndMakeVisible(filterSlopeKnob.get());
            filterSlopeKnob->setVisible(false);
        } else if (knobIdx == 5 || knobIdx == 6) {
            // Regular knobs (Key Track, Mix) - Now filling positions 5 and 6
            // Map: knobIdx 5→3 (Key Track), 6→4 (Mix)
            int regularKnobIdx = (knobIdx == 5) ? 3 : 4;
            
            filterKnobs[regularKnobIdx] = std::make_unique<CustomKnob>();
            filterKnobs[regularKnobIdx]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            filterKnobs[regularKnobIdx]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            
            // Set parameter ranges
            if (regularKnobIdx == 3) { // Key Track
                filterKnobs[regularKnobIdx]->setRange(0.0, 1.0, 0.01);
                filterKnobs[regularKnobIdx]->setValue(0.0, juce::dontSendNotification);
            } else { // Mix (regularKnobIdx == 4)
                filterKnobs[regularKnobIdx]->setRange(0.0, 1.0, 0.01);
                filterKnobs[regularKnobIdx]->setValue(1.0, juce::dontSendNotification);
            }
            
            if (assets.knobRing != nullptr)
                filterKnobs[regularKnobIdx]->setRingImage(assets.knobRing->createCopy());
            if (assets.knobInside != nullptr)
                filterKnobs[regularKnobIdx]->setInnerImage(assets.knobInside->createCopy());
            
            filterKnobs[regularKnobIdx]->setBounds(x, y, knobSize, knobSize);
            addAndMakeVisible(filterKnobs[regularKnobIdx].get());
            filterKnobs[regularKnobIdx]->setVisible(false);
            
            // Create attachment - Map knobIdx to filterParamIds index: 5→5 (Key Track), 6→6 (Mix)
            int paramIdsIdx = (knobIdx == 5) ? 5 : 6;
            filterAttachments[regularKnobIdx] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.getAPVTS(), filterParamIds[paramIdsIdx], *filterKnobs[regularKnobIdx]);
        } else if (knobIdx == 1 || knobIdx == 2 || knobIdx == 4) {
            // Regular knobs (Cutoff, Res, Drive) - Skip Type (0), Slope (3), and empty position 7
            // Map: knobIdx 1→0 (Cutoff), 2→1 (Res), 4→2 (Drive)
            int regularKnobIdx = (knobIdx == 1) ? 0 : (knobIdx == 2) ? 1 : 2;
            
            filterKnobs[regularKnobIdx] = std::make_unique<CustomKnob>();
            filterKnobs[regularKnobIdx]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            filterKnobs[regularKnobIdx]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            
            // Set parameter ranges
            switch (regularKnobIdx) {
                case 0: // Cutoff - linear rotation (0-1), custom frequency mapping in processor
                    // Knob rotates linearly 0-1, processor converts to frequency: 0-0.75 → 20-5000Hz, 0.75-1.0 → 5000-20000Hz
                    filterKnobs[regularKnobIdx]->setRange(0.0, 1.0, 0.001);
                    filterKnobs[regularKnobIdx]->setValue(0.18, juce::dontSendNotification); // ~0.18 = 1200Hz
                    break;
                case 1: // Res
                    filterKnobs[regularKnobIdx]->setRange(0.0, 1.0, 0.01);
                    filterKnobs[regularKnobIdx]->setValue(0.35, juce::dontSendNotification);
                    break;
                case 2: // Drive - larger increments for more noticeable changes
                    filterKnobs[regularKnobIdx]->setRange(0.0, 36.0, 0.5);
                    filterKnobs[regularKnobIdx]->setValue(6.0, juce::dontSendNotification);
                    break;
            }
            
            if (assets.knobRing != nullptr)
                filterKnobs[regularKnobIdx]->setRingImage(assets.knobRing->createCopy());
            if (assets.knobInside != nullptr)
                filterKnobs[regularKnobIdx]->setInnerImage(assets.knobInside->createCopy());
            
            filterKnobs[regularKnobIdx]->setBounds(x, y, knobSize, knobSize);
            addAndMakeVisible(filterKnobs[regularKnobIdx].get());
            filterKnobs[regularKnobIdx]->setVisible(false);
            
            // Create attachment - Map knobIdx to filterParamIds index: 1→1 (cutoff), 2→2 (res), 4→4 (drive)
            int paramIdsIdx = knobIdx;
            filterAttachments[regularKnobIdx] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.getAPVTS(), filterParamIds[paramIdsIdx], *filterKnobs[regularKnobIdx]);
        }
        // Note: knobIdx 7 is now empty (was Mix, moved to position 6)
        
        // Create label for all knobs (skip empty position - knobIdx 7, and skip Type/Slope which have separate labels)
        if (knobIdx != 7 && knobIdx != 0 && knobIdx != 3) { // Skip empty position 7, Type (0), and Slope (3) - they have separate labels
            // Map knobIdx to filterKnobNames index: 1→1 (Cutoff), 2→2 (Resonance), 4→4 (Drive), 5→5 (Key Track), 6→6 (Mix)
            int namesIdx = knobIdx; // Direct mapping for non-special knobs
            // Label indices match knobIdx directly (0-7), except knobIdx 0 (Type), 3 (Slope), 5 (spread removed), and 7 (empty)
            int labelIdx = knobIdx; // Use same index as knobIdx (labels array has 8 elements)
            filterKnobLabels[labelIdx] = std::make_unique<juce::Label>();
            filterKnobLabels[labelIdx]->setText(filterKnobNames[namesIdx], juce::dontSendNotification);
            filterKnobLabels[labelIdx]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
            filterKnobLabels[labelIdx]->setColour(juce::Label::textColourId, juce::Colours::white);
            filterKnobLabels[labelIdx]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(filterKnobLabels[labelIdx].get());
            filterKnobLabels[labelIdx]->setVisible(false);
            filterKnobLabels[labelIdx]->setBounds(x, y - 15, knobSize, 20);
            
            // Create value label for all knobs
            filterValueLabels[labelIdx] = std::make_unique<juce::Label>();
            filterValueLabels[labelIdx]->setText("0", juce::dontSendNotification);
            filterValueLabels[labelIdx]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
            filterValueLabels[labelIdx]->setColour(juce::Label::textColourId, juce::Colours::white);
            filterValueLabels[labelIdx]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(filterValueLabels[labelIdx].get());
            filterValueLabels[labelIdx]->setVisible(false);
            filterValueLabels[labelIdx]->setBounds(x, y + knobSize - 10, knobSize, 15);
            
            // Create indicator bar for all knobs - FIX 1: Correct bounds
            filterIndicatorBars[labelIdx] = std::make_unique<IndicatorBar>();
            addAndMakeVisible(filterIndicatorBars[labelIdx].get());
            filterIndicatorBars[labelIdx]->setVisible(false);
            filterIndicatorBars[labelIdx]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
            filterIndicatorBars[labelIdx]->setValue(0.5f);
            
            // Create lock button for all knobs (except Type and Slope which are created separately)
            if (knobIdx != 0 && knobIdx != 3) {
                filterLockButtons[labelIdx] = std::make_unique<LockButton>();
                addAndMakeVisible(filterLockButtons[labelIdx].get());
                filterLockButtons[labelIdx]->setVisible(false);
                
                // Calculate lock button position (same pattern as Saturate)
                juce::Font labelFont(12.0f, juce::Font::bold);
                int textWidth = labelFont.getStringWidth(filterKnobNames[namesIdx]);
                const int lockSize = 10;
                const int lockSpacing = 5;
                int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
                int lockY = y - 10;
                filterLockButtons[labelIdx]->setBounds(lockX, lockY, lockSize, lockSize);
                
                if (assets.unlockedIcon && assets.lockedIcon) {
                    auto imgUnlocked = assets.unlockedIcon->createCopy();
                    auto imgLocked = assets.lockedIcon->createCopy();
                    filterLockButtons[labelIdx]->setImages(std::move(imgUnlocked), std::move(imgLocked));
                }
                
                filterLockButtons[labelIdx]->setToggleState(filterKnobLocked[labelIdx], juce::dontSendNotification);
                filterLockButtons[labelIdx]->setClickingTogglesState(true);
                filterLockButtons[labelIdx]->onClick = [this, labelIdx]() {
                    filterKnobLocked[labelIdx] = filterLockButtons[labelIdx]->getToggleState();
                    repaint();
                };
            }
        }
    }
    
    // Create Type label
    filterTypeLabel = std::make_unique<juce::Label>();
    filterTypeLabel->setText("Type", juce::dontSendNotification);
    filterTypeLabel->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
    filterTypeLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    filterTypeLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterTypeLabel.get());
    filterTypeLabel->setVisible(false);
    int typeX = startX;
    int typeY = startY - 23 - 15; // Label Y position (knob Y is startY - 23)
    filterTypeLabel->setBounds(typeX, typeY, knobSize, 20);
    
    // Create lock button for Type knob (index 0 in filterKnobLocked array)
    // Position it relative to the label, same as regular knobs
    filterLockButtons[0] = std::make_unique<LockButton>();
    addAndMakeVisible(filterLockButtons[0].get());
    filterLockButtons[0]->setVisible(false);
    filterLockButtons[0]->setClickingTogglesState(true);
    
    juce::Font typeLabelFont(12.0f, juce::Font::bold);
    int typeTextWidth = typeLabelFont.getStringWidth("Type");
    int typeKnobY = startY - 23;
    // Position Type lock button (same pattern as Saturate)
    const int lockSize = 10;
    const int lockSpacing = 5;
    int typeLockX = typeX + (knobSize / 2) + (typeTextWidth / 2) + lockSpacing;
    int typeLockY = typeKnobY - 10;
    filterLockButtons[0]->setBounds(typeLockX, typeLockY, lockSize, lockSize);
    
    if (assets.unlockedIcon && assets.lockedIcon) {
        auto imgUnlocked = assets.unlockedIcon->createCopy();
        auto imgLocked = assets.lockedIcon->createCopy();
        filterLockButtons[0]->setImages(std::move(imgUnlocked), std::move(imgLocked));
    }
    
    filterLockButtons[0]->setToggleState(filterKnobLocked[0], juce::dontSendNotification);
    filterLockButtons[0]->onClick = [this]() {
        filterKnobLocked[0] = filterLockButtons[0]->getToggleState();
        repaint();
    };
    
    // Create value label and indicator bar for Type knob (position 0)
    filterValueLabels[0] = std::make_unique<juce::Label>();
    filterValueLabels[0]->setText("LP", juce::dontSendNotification);
    filterValueLabels[0]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
    filterValueLabels[0]->setColour(juce::Label::textColourId, juce::Colours::white);
    filterValueLabels[0]->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterValueLabels[0].get());
    filterValueLabels[0]->setVisible(false);
    filterValueLabels[0]->setBounds(typeX, typeKnobY + knobSize - 10, knobSize, 15);
    
    filterIndicatorBars[0] = std::make_unique<IndicatorBar>();
    addAndMakeVisible(filterIndicatorBars[0].get());
    filterIndicatorBars[0]->setVisible(false);
    filterIndicatorBars[0]->setBounds(typeX + 10, typeKnobY + knobSize + 8, knobSize - 20, 13);
    filterIndicatorBars[0]->setValue(0.0f);
    
    // Create Slope label
    filterSlopeLabel = std::make_unique<juce::Label>();
    filterSlopeLabel->setText("Slope", juce::dontSendNotification);
    filterSlopeLabel->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
    filterSlopeLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    filterSlopeLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterSlopeLabel.get());
    filterSlopeLabel->setVisible(false);
    int slopeX = startX + 3 * (knobSize + knobSpacing);
    int slopeY = startY - 23 - 15;
    filterSlopeLabel->setBounds(slopeX, slopeY, knobSize, 20);
    
    // Create lock button for Slope knob (index 3 in filterKnobLocked array)
    // Use exact same calculation as in the loop for knobIdx=3
    filterLockButtons[3] = std::make_unique<LockButton>();
    addAndMakeVisible(filterLockButtons[3].get());
    filterLockButtons[3]->setVisible(false);
    filterLockButtons[3]->setClickingTogglesState(true);
    
    // Calculate slope lock position - match regular knobs exactly
    // Regular knobs: label at y - 15, lock at y - 10 (5px below label top)
    // Slope: label at slopeY = startY - 23 - 15 = startY - 38
    // So lock should be at slopeY + 5 = startY - 33, which equals (startY - 23) - 10
    auto slopeLabelBounds = filterSlopeLabel->getBounds();
    int slopeKnobY = startY - 23;
    int slopeKnobX = slopeLabelBounds.getX();
    // Position Slope lock button (same pattern as Saturate)
    juce::Font slopeLabelFont(12.0f, juce::Font::bold);
    int slopeTextWidth = slopeLabelFont.getStringWidth("Slope");
    int slopeLockX = slopeX + (knobSize / 2) + (slopeTextWidth / 2) + lockSpacing;
    int slopeLockY = slopeKnobY - 10;
    filterLockButtons[3]->setBounds(slopeLockX, slopeLockY, lockSize, lockSize);
    
    if (assets.unlockedIcon && assets.lockedIcon) {
        auto imgUnlocked = assets.unlockedIcon->createCopy();
        auto imgLocked = assets.lockedIcon->createCopy();
        filterLockButtons[3]->setImages(std::move(imgUnlocked), std::move(imgLocked));
    }
    
    filterLockButtons[3]->setToggleState(filterKnobLocked[3], juce::dontSendNotification);
    filterLockButtons[3]->onClick = [this]() {
        filterKnobLocked[3] = filterLockButtons[3]->getToggleState();
        repaint();
    };
    
    // Create value label and indicator bar for Slope knob (position 3)
    filterValueLabels[3] = std::make_unique<juce::Label>();
    filterValueLabels[3]->setText("24dB", juce::dontSendNotification);
    filterValueLabels[3]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
    filterValueLabels[3]->setColour(juce::Label::textColourId, juce::Colours::white);
    filterValueLabels[3]->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterValueLabels[3].get());
    filterValueLabels[3]->setVisible(false);
    filterValueLabels[3]->setBounds(slopeX, slopeKnobY + knobSize - 10, knobSize, 15);
    
    filterIndicatorBars[3] = std::make_unique<IndicatorBar>();
    addAndMakeVisible(filterIndicatorBars[3].get());
    filterIndicatorBars[3]->setVisible(false);
    filterIndicatorBars[3]->setBounds(slopeX + 10, slopeKnobY + knobSize + 8, knobSize - 20, 13);
    filterIndicatorBars[3]->setValue(1.0f);
    
    // Create attachments for Type and Slope
    filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "fType", *filterTypeKnob);
    filterSlopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "slope", *filterSlopeKnob);
    
    // Setup value change callbacks for all knobs
    filterTypeKnob->onValueChange = [this]() {
        // Skip if loading from snapshot (prevents circular updates)
        if (isLoadingFromSnapshot.load())
            return;
        
        // Respect lock state - if locked, restore previous value
        if (filterKnobLocked[0]) {
            // Restore to previous value
            auto* typeParam = processorRef.getAPVTS().getParameter("fType");
            if (typeParam) {
                float prevValue = typeParam->getValue() * 4.0f; // Convert from normalized to 0-4
                filterTypeKnob->setValue(prevValue, juce::dontSendNotification);
            }
            return;
        }
            
        float value = filterTypeKnob->getValue();
        juce::String types[] = {"LP", "HP", "BP", "Comb-", "Comb+"};
        int idx = static_cast<int>(value);
        if (idx >= 0 && idx < 5) {
            filterValueLabels[0]->setText(types[idx], juce::dontSendNotification);
        }
        if (filterIndicatorBars[0]) filterIndicatorBars[0]->setValue(value / 4.0f);
        
        // Save to snapshot
        updateFilterParameterFromKnob(-1); // -1 = Type knob
        
        // Handle all steps toggle - update all steps when enabled
        if (filterAllStepsEnabled) {
            int currentStep = filterUiSelectedStep;
            for (int step = 0; step < 16; ++step) {
                if (step != currentStep) {
                    auto snapshot = processorRef.getFilterSafeSnapshot(step);
                    snapshot.filter.type = value;
                    processorRef.setFilterStepSnapshot(step, snapshot);
                }
            }
        }
    };
    
    filterSlopeKnob->onValueChange = [this]() {
        // Skip if loading from snapshot (prevents circular updates)
        if (isLoadingFromSnapshot.load())
            return;
        
        // Respect lock state - if locked, restore previous value
        if (filterKnobLocked[3]) {
            // Restore to previous value
            auto* slopeParam = processorRef.getAPVTS().getParameter("slope");
            if (slopeParam) {
                float prevValue = slopeParam->getValue(); // 0-1 normalized
                filterSlopeKnob->setValue(prevValue, juce::dontSendNotification);
            }
            return;
        }
            
        float value = filterSlopeKnob->getValue();
        filterValueLabels[3]->setText(value > 0.5f ? "24dB" : "12dB", juce::dontSendNotification);
        if (filterIndicatorBars[3]) filterIndicatorBars[3]->setValue(value);
        
        // Save to snapshot
        updateFilterParameterFromKnob(-2); // -2 = Slope knob
        
        // Handle all steps toggle - update all steps when enabled
        if (filterAllStepsEnabled) {
            int currentStep = filterUiSelectedStep;
            for (int step = 0; step < 16; ++step) {
                if (step != currentStep) {
                    auto snapshot = processorRef.getFilterSafeSnapshot(step);
                    snapshot.filter.slope = value;
                    processorRef.setFilterStepSnapshot(step, snapshot);
                }
            }
        }
    };
    
    for (int i = 0; i < 5; ++i) { // 5 knobs: Cutoff, Res, Drive, Key Track, Mix (Spread removed)
        if (filterKnobs[i]) {
            filterKnobs[i]->onValueChange = [this, i]() {
                // Skip if loading from snapshot (prevents circular updates)
                if (isLoadingFromSnapshot.load())
                    return;
                    
                if (!filterKnobs[i]) return;
                float value = filterKnobs[i]->getValue();
                juce::String text;
                // Map filterKnobs index to knobLabelIdx: 0→1 (Cutoff), 1→2 (Res), 2→4 (Drive), 3→5 (Key Track), 4→6 (Mix)
                // Skip empty position (knobIdx 7) in labels
                int knobLabelIdx = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 4 : (i == 3) ? 5 : 6;
                
                switch (i) {
                    case 0: {
                        // Convert normalized value (0-1) to frequency for display
                        float freq = value <= 0.75f 
                            ? 20.0f + (5000.0f - 20.0f) * (value / 0.75f)
                            : 5000.0f * std::pow(4.0f, (value - 0.75f) / 0.25f);
                        text = juce::String(static_cast<int>(freq)) + " Hz";
                        break;
                    }
                    case 1: text = juce::String(static_cast<int>(value * 100)) + "%"; break;
                    case 2: {
                        // Drive: convert 0-36 dB to 0-100% for display
                        // Read the actual parameter value instead of knob value (SliderAttachment may normalize it)
                        auto* driveParam = processorRef.getAPVTS().getParameter("filterDrive");
                        float driveDb = 0.0f;
                        
                        if (driveParam) {
                            // Get normalized value (0-1) and convert to actual dB value (0-36)
                            float normalized = driveParam->getValue(); // Returns 0-1
                            driveDb = driveParam->convertFrom0to1(normalized); // Converts to 0-36 dB
                        } else {
                            // Fallback: use knob value directly if parameter not available
                            driveDb = juce::jlimit(0.0f, 36.0f, value);
                        }
                        
                        // Ensure we have a valid range
                        driveDb = juce::jlimit(0.0f, 36.0f, driveDb);
                        float drivePercent = (driveDb / 36.0f) * 100.0f;
                        text = juce::String(static_cast<int>(drivePercent)) + "%";
                        break;
                    }
                    case 3: text = juce::String(static_cast<int>(value * 100)) + "%"; break; // Key Track
                    case 4: text = juce::String(static_cast<int>(value * 100)) + "%"; break; // Mix
                }
                if (filterValueLabels[knobLabelIdx]) {
                    filterValueLabels[knobLabelIdx]->setText(text, juce::dontSendNotification);
                }
                
                if (filterIndicatorBars[knobLabelIdx]) {
                    float norm = 0.0f;
                    switch (i) {
                        case 0: norm = value; break; // Cutoff - already normalized 0-1
                        case 1: norm = value; break; // Res - already normalized 0-1
                        case 2: norm = value / 36.0f; break; // Drive - normalize 0-36 to 0-1
                        case 3: norm = value; break; // Key Track - already normalized 0-1
                        case 4: norm = value; break; // Mix - already normalized 0-1
                    }
                    filterIndicatorBars[knobLabelIdx]->setValue(norm);
                }
                
                // Save to snapshot (Mix knob at index 4 is global, not saved per step)
                if (i != 4) { // Mix is at index 4 now
                    updateFilterParameterFromKnob(i);
                    
                    // Handle all steps toggle - update all steps when enabled
                    if (filterAllStepsEnabled) {
                        // Save current selected step first (done above), then update all others
                        int currentStep = filterUiSelectedStep;
                        float value = filterKnobs[i]->getValue();
                        for (int step = 0; step < 16; ++step) {
                            if (step != currentStep) {
                                auto snapshot = processorRef.getFilterSafeSnapshot(step);
                                switch (i) {
                                    case 0: { // Cutoff - convert normalized value to frequency
                                        // Use the same conversion logic as in processor
                                        float freq = value <= 0.75f 
                                            ? 20.0f + (5000.0f - 20.0f) * (value / 0.75f)
                                            : 5000.0f * std::pow(4.0f, (value - 0.75f) / 0.25f);
                                        snapshot.filter.cutoff = juce::jlimit(20.0f, 20000.0f, freq);
                                        break;
                                    }
                                    case 1: // Resonance
                                        snapshot.filter.resonance = value;
                                        break;
                                    case 2: // Drive
                                        snapshot.filter.drive = value;
                                        break;
                                    case 3: // Key Track
                                        snapshot.filter.keytrack = value;
                                        break;
                                }
                                processorRef.setFilterStepSnapshot(step, snapshot);
                            }
                        }
                    }
                }
            };
        }
    }
    
    DBG("[UI] Filter knobs setup complete");
}

void PluginEditor::setupFilterEffectsArea()
{
    DBG("[UI] Setting up Filter effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title
    filterEffectsTitle = std::make_unique<juce::Label>();
    filterEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    filterEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.1f)); // 10% smaller: 20.736f * 0.9 = 18.6624f, with letter spacing
    filterEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    filterEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(filterEffectsTitle.get());
    filterEffectsTitle->setVisible(false);
    filterEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Create dice button - FIX 2: Correct size (32)
    filterDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(filterDiceButton.get());
    filterDiceButton->setVisible(false);
    const int diceSize = 32;
    filterDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    if (assets.diceLarge != nullptr) {
        filterDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    filterDiceButton->onClick = [this]() {
        randomizeFilterKnobValues();
    };
    
    // Create FX power button
    filterFxPowerButton = std::make_unique<juce::DrawableButton>("filterFxPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    addAndMakeVisible(filterFxPowerButton.get());
    filterFxPowerButton->setVisible(false);
    filterFxPowerButton->setClickingTogglesState(true);
    filterFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    filterFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    const int buttonSize = 46;
    filterFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3,
                                   effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    if (assets.fxPowerOn != nullptr) {
        filterFxPowerButton->setImages(assets.fxPowerOn->createCopy().get());
    }
    
    filterFxPowerButton->setToggleState(filterFxAreaEnabled, juce::dontSendNotification);
    // FIX 3: Working power button with visibility function
    filterFxPowerButton->onClick = [this]() {
        filterFxAreaEnabled = filterFxPowerButton->getToggleState();
        updateFilterFxAreaVisibility();
        auto* filterEnabledParam = processorRef.getAPVTS().getParameter("filterEnabled");
        if (filterEnabledParam) {
            filterEnabledParam->setValueNotifyingHost(filterFxAreaEnabled ? 1.0f : 0.0f);
        }
    };
    
    DBG("[UI] Filter effects area setup complete");
}

void PluginEditor::setupFilterSequencerArea()
{
    DBG("[UI] Setting up Filter sequencer area...");
    
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create "STEP" title
    filterStepTitle = std::make_unique<juce::Label>();
    filterStepTitle->setText("STEP", juce::dontSendNotification);
    filterStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f)); // Adjusted size with 10% less letter spacing: 0.1 * 0.9 = 0.09
    filterStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    filterStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(filterStepTitle.get());
    filterStepTitle->setVisible(false);
    filterStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step dice button
    filterStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(filterStepDiceButton.get());
    filterStepDiceButton->setVisible(false);
    int stepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    filterStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, stepDiceSize, stepDiceSize);
    if (assets.diceLarge != nullptr) {
        filterStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    filterStepDiceButton->onClick = [this]() {
        DBG("[UI] Filter step dice button clicked - randomizing all step snapshots");
        
        juce::Random& rng = juce::Random::getSystemRandom();
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getFilterSafeSnapshot(step);
            
            // Randomize all Filter parameters for this step (respecting lock states)
            // Lock indices: 0=Type, 1=Cutoff, 2=Res, 3=Slope, 4=Drive, 5=Key Track, 6=Mix
            if (!filterKnobLocked[0]) {
                snapshot.filter.type = static_cast<float>(rng.nextInt(5)); // 0-4 (LP, HP, BP, Comb-, Comb+)
            }
            if (!filterKnobLocked[1]) {
                snapshot.filter.cutoff = 20.0f + rng.nextFloat() * 19800.0f; // 20-20000 Hz
            }
            if (!filterKnobLocked[2]) {
                snapshot.filter.resonance = rng.nextFloat(); // 0-1
            }
            if (!filterKnobLocked[3]) {
                snapshot.filter.slope = rng.nextFloat(); // 0-1 (12dB or 24dB)
            }
            if (!filterKnobLocked[4]) {
                snapshot.filter.drive = rng.nextFloat() * 36.0f; // 0-36 dB
            }
            if (!filterKnobLocked[5]) {
                snapshot.filter.keytrack = rng.nextFloat(); // 0-1
            }
            // Mix is global, not randomized per step
            
            processorRef.setFilterStepSnapshot(step, snapshot);
        }
        
        // Update sequencer UI
        updateFilterSequencerUI();
        
        // Reload current step to UI (don't send notification to avoid triggering save)
        isLoadingFromSnapshot.store(true);
        auto currentSnapshot = processorRef.getFilterSafeSnapshot(filterUiSelectedStep);
        if (filterTypeKnob) filterTypeKnob->setValue(currentSnapshot.filter.type, juce::dontSendNotification);
        if (filterSlopeKnob) filterSlopeKnob->setValue(currentSnapshot.filter.slope, juce::dontSendNotification);
        // Convert frequency from snapshot to normalized value for cutoff knob
        if (filterKnobs[0]) {
            float freq = currentSnapshot.filter.cutoff;
            float normalized = freq <= 5000.0f 
                ? 0.75f * (freq - 20.0f) / (5000.0f - 20.0f)
                : 0.75f + 0.25f * (std::log(freq / 5000.0f) / std::log(20000.0f / 5000.0f));
            filterKnobs[0]->setValue(juce::jlimit(0.0f, 1.0f, normalized), juce::dontSendNotification);
        }
        if (filterKnobs[1]) filterKnobs[1]->setValue(currentSnapshot.filter.resonance, juce::dontSendNotification);
        if (filterKnobs[2]) filterKnobs[2]->setValue(currentSnapshot.filter.drive, juce::dontSendNotification);
        if (filterKnobs[3]) filterKnobs[3]->setValue(currentSnapshot.filter.keytrack, juce::dontSendNotification);
        isLoadingFromSnapshot.store(false);
        
        // Trigger value change callbacks to update labels
        if (filterTypeKnob && filterTypeKnob->onValueChange) filterTypeKnob->onValueChange();
        if (filterSlopeKnob && filterSlopeKnob->onValueChange) filterSlopeKnob->onValueChange();
        for (int i = 0; i < 5; ++i) {
            if (filterKnobs[i] && filterKnobs[i]->onValueChange) {
                filterKnobs[i]->onValueChange();
            }
        }
        
        DBG("[UI] All 16 Filter steps randomized");
    };
    
    // Create step buttons (16 steps)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        filterStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(filterStepButtons[i].get());
        filterStepButtons[i]->setVisible(false);
        
        int col = i % 8;
        int row = i / 8;
        int x = startX + col * (buttonSize + buttonSpacing);
        int y = startY + row * (buttonSize + buttonSpacing);
        filterStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        if (assets.stepActive != nullptr) {
            filterStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive != nullptr) {
            filterStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        filterStepButtons[i]->onClick = [this, i]() {
            onFilterStepButtonClicked(i);
        };
    }
    
    // Create step amount label
    filterStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setFilterStepsUsed(16);
    filterStepAmountLabel->setText("16");
    filterStepAmountLabel->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 16.0f, juce::Font::bold));
    filterStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    filterStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    filterStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    filterStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    filterStepAmountLabel->setJustification(juce::Justification::centred);
    filterStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    filterStepAmountLabel->setIndents(0, 0);
    filterStepAmountLabel->setInputRestrictions(2, "0123456789");
    addAndMakeVisible(filterStepAmountLabel.get());
    filterStepAmountLabel->setVisible(false);
    filterStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    filterStepAmountLabel->onTextChange = [this]() {
        int newAmount = filterStepAmountLabel->getText().getIntValue();
        if (newAmount >= 1 && newAmount <= 16) {
            processorRef.setFilterStepsUsed(newAmount);
            updateFilterSequencerUI();
        }
    };
    filterStepAmountLabel->onReturnKey = [this]() {
        filterStepAmountLabel->unfocusAllComponents();
    };
    
    // Create rate dropdown
    filterRateDropdown = std::make_unique<juce::ComboBox>();
    filterRateDropdown->addItem("4", 1);
    filterRateDropdown->addItem("2", 2);
    filterRateDropdown->addItem("1", 3);
    filterRateDropdown->addItem("1/2", 4);
    filterRateDropdown->addItem("1/4", 5);
    filterRateDropdown->addItem("1/8", 6);
    filterRateDropdown->addItem("1/16", 7);
    filterRateDropdown->addItem("1/32", 8);
    filterRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    filterRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    filterRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    filterRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    filterRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    // Set selected ID from processor state (like Saturate)
    int divIdx = processorRef.getFilterSeqState().divisionIndex.load();
    filterRateDropdown->setSelectedId(divIdx + 1, juce::dontSendNotification);
    
    addAndMakeVisible(filterRateDropdown.get());
    filterRateDropdown->setVisible(false);
    filterRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    filterRateDropdown->onChange = [this]() {
        int selectedId = filterRateDropdown->getSelectedId();
        if (selectedId > 0) {
            int divisionIndex = selectedId - 1;
            processorRef.setFilterDivisionIndex(divisionIndex);
            DBG("[UI] Filter rate changed to index: " << divisionIndex);
        }
    };
    
    // Create STD toggle
    filterStdToggle = std::make_unique<CircularToggleButton>();
    filterStdToggle->setButtonText("-");
    addAndMakeVisible(filterStdToggle.get());
    filterStdToggle->setVisible(false);
    filterStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    // Create step power button
    filterStepPowerButton = std::make_unique<juce::DrawableButton>("filterStepPower", juce::DrawableButton::ButtonStyle::ImageFitted);
    filterStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    filterStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    if (assets.stepPowerOn != nullptr) {
        filterStepPowerButton->setImages(assets.stepPowerOn->createCopy().get());
    }
    filterStepPowerButton->setClickingTogglesState(true);
    const int stepPowerSize = 40;
    filterStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - stepPowerSize - 5 + 15 - 5 - 1, 
                                      sequencerArea.getY() - 5 - stepPowerSize + 25 + 5, stepPowerSize, stepPowerSize);
    addAndMakeVisible(filterStepPowerButton.get());
    filterStepPowerButton->setVisible(false);
    filterStepPowerButton->setToggleState(filterStepAreaEnabled, juce::dontSendNotification);
    filterStepPowerButton->onClick = [this]() {
        // Read the button state after it has toggled
        filterStepAreaEnabled = filterStepPowerButton->getToggleState();
        DBG("[UI] Filter sequencer power button clicked. New state: " << (filterStepAreaEnabled ? "ON" : "OFF"));
        
        // Update processor state
        processorRef.setFilterSequencerEnabled(filterStepAreaEnabled);
        
        // Update APVTS parameter if it exists (for state persistence)
        auto* param = processorRef.getAPVTS().getParameter("filterStepEnabled");
        if (param) {
            param->setValueNotifyingHost(filterStepAreaEnabled ? 1.0f : 0.0f);
        }
        
        // Update UI visibility
        updateFilterStepAreaVisibility();
        
        // Refresh sequencer UI to show current state
        updateFilterSequencerUI();
    };
    
    DBG("[UI] Filter sequencer area setup complete");
}

void PluginEditor::setupFilterAllStepsToggle()
{
    DBG("[UI] Setting up Filter all steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    filterAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(filterAllStepsToggle.get());
    filterAllStepsToggle->setVisible(false);
    const int buttonSize = 29;
    filterAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                       effectArea.getY() - 1, buttonSize, buttonSize);
    if (assets.stepTopInactive && assets.stepTopActive) {
        static_cast<AllStepsToggleButton*>(filterAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    filterAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    filterAllStepsEnabled = false;
    filterAllStepsToggle->onClick = [this]() {
        filterAllStepsEnabled = filterAllStepsToggle->getToggleState();
        DBG("[UI] Filter All Steps toggle: " << (filterAllStepsEnabled ? "ON" : "OFF"));
        if (filterAllStepsLabel) {
            filterAllStepsLabel->setAlpha(filterAllStepsEnabled ? 1.0f : 0.5f);
        }
    };
    
    filterAllStepsLabel = std::make_unique<juce::Label>();
    filterAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    filterAllStepsLabel->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 14.4f, juce::Font::bold));
    filterAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    filterAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(filterAllStepsLabel.get());
    filterAllStepsLabel->setVisible(false);
    filterAllStepsLabel->setAlpha(1.0f);
    filterAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                effectArea.getY() + 1, 80, 24);
    
    DBG("[UI] Filter all steps toggle setup complete");
}

void PluginEditor::populateFilterGroup()
{
    // Populate Filter group for visibility management (similar to Saturate)
    filterGroup.clear();
    
    if (filterEffectsTitle) filterGroup.push_back(filterEffectsTitle.get());
    if (filterDiceButton) filterGroup.push_back(filterDiceButton.get());
    if (filterFxPowerButton) filterGroup.push_back(filterFxPowerButton.get());
    
    // Add Type and Slope knobs (separate unique_ptrs)
    if (filterTypeKnob) filterGroup.push_back(filterTypeKnob.get());
    if (filterSlopeKnob) filterGroup.push_back(filterSlopeKnob.get());
    if (filterTypeLabel) filterGroup.push_back(filterTypeLabel.get());
    if (filterSlopeLabel) filterGroup.push_back(filterSlopeLabel.get());
    
    // Add regular knobs (5 knobs: Cutoff, Res, Drive, Key Track, Mix - Spread removed)
    for (int i = 0; i < 5; ++i) {
        if (filterKnobs[i]) filterGroup.push_back(filterKnobs[i].get());
    }
    
    // Add all labels, value labels, indicator bars, and lock buttons (8 total: one for each knob)
    for (int i = 0; i < 8; ++i) {
        if (filterKnobLabels[i]) filterGroup.push_back(filterKnobLabels[i].get());
        if (filterValueLabels[i]) filterGroup.push_back(filterValueLabels[i].get());
        if (filterIndicatorBars[i]) filterGroup.push_back(filterIndicatorBars[i].get());
        if (filterLockButtons[i]) filterGroup.push_back(filterLockButtons[i].get());
    }
    
    // Add sequencer components
    if (filterStepTitle) filterGroup.push_back(filterStepTitle.get());
    if (filterStepAmountLabel) filterGroup.push_back(filterStepAmountLabel.get());
    if (filterRateDropdown) filterGroup.push_back(filterRateDropdown.get());
    if (filterStdToggle) filterGroup.push_back(filterStdToggle.get());
    if (filterStepDiceButton) filterGroup.push_back(filterStepDiceButton.get());
    if (filterStepPowerButton) filterGroup.push_back(filterStepPowerButton.get());
    if (filterAllStepsToggle) filterGroup.push_back(filterAllStepsToggle.get());
    if (filterAllStepsLabel) filterGroup.push_back(filterAllStepsLabel.get());
    
    for (int i = 0; i < 16; ++i) {
        if (filterStepButtons[i]) filterGroup.push_back(filterStepButtons[i].get());
    }
    
    DBG("[UI] Filter group populated with " << filterGroup.size() << " components");
}

void PluginEditor::updateFilterFxAreaVisibility()
{
    float alpha = filterFxAreaEnabled ? 1.0f : 0.3f;
    
    if (filterTypeKnob) {
        filterTypeKnob->setAlpha(alpha);
        filterTypeKnob->setEnabled(filterFxAreaEnabled);
    }
    if (filterSlopeKnob) {
        filterSlopeKnob->setAlpha(alpha);
        filterSlopeKnob->setEnabled(filterFxAreaEnabled);
    }
    if (filterTypeLabel) filterTypeLabel->setAlpha(alpha);
    if (filterSlopeLabel) filterSlopeLabel->setAlpha(alpha);
    if (filterValueLabels[0]) filterValueLabels[0]->setAlpha(alpha);
    if (filterValueLabels[3]) filterValueLabels[3]->setAlpha(alpha);
    if (filterIndicatorBars[0]) filterIndicatorBars[0]->setAlpha(alpha);
    if (filterIndicatorBars[3]) filterIndicatorBars[3]->setAlpha(alpha);
    if (filterLockButtons[0]) filterLockButtons[0]->setAlpha(alpha);
    if (filterLockButtons[3]) filterLockButtons[3]->setAlpha(alpha);
    
    for (int i = 0; i < 5; ++i) { // 5 knobs (Spread removed)
        // Map filterKnobs index to label index: 0→1 (Cutoff), 1→2 (Res), 2→4 (Drive), 3→5 (Key Track), 4→6 (Mix)
        int labelIdx = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 4 : (i == 3) ? 5 : 6;
        
        if (filterKnobs[i]) {
            filterKnobs[i]->setAlpha(alpha);
            filterKnobs[i]->setEnabled(filterFxAreaEnabled);
        }
        if (filterKnobLabels[labelIdx]) filterKnobLabels[labelIdx]->setAlpha(alpha);
        if (filterValueLabels[labelIdx]) filterValueLabels[labelIdx]->setAlpha(alpha);
        if (filterIndicatorBars[labelIdx]) filterIndicatorBars[labelIdx]->setAlpha(alpha);
        if (filterLockButtons[labelIdx]) filterLockButtons[labelIdx]->setAlpha(alpha);
    }
    if (filterDiceButton) {
        filterDiceButton->setAlpha(alpha);
        filterDiceButton->setEnabled(filterFxAreaEnabled);
    }
    if (filterEffectsTitle) filterEffectsTitle->setAlpha(alpha);
    repaint();
}

void PluginEditor::updateFilterStepAreaVisibility()
{
    float alpha = filterStepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i) {
        if (filterStepButtons[i]) {
            filterStepButtons[i]->setAlpha(alpha);
            filterStepButtons[i]->setEnabled(filterStepAreaEnabled);
        }
    }
    if (filterStepAmountLabel) {
        filterStepAmountLabel->setAlpha(alpha);
        filterStepAmountLabel->setEnabled(filterStepAreaEnabled);
    }
    if (filterRateDropdown) {
        filterRateDropdown->setAlpha(alpha);
        filterRateDropdown->setEnabled(filterStepAreaEnabled);
    }
    if (filterStdToggle) {
        filterStdToggle->setAlpha(alpha);
        filterStdToggle->setEnabled(filterStepAreaEnabled);
    }
    if (filterStepTitle) filterStepTitle->setAlpha(alpha);
    if (filterStepDiceButton) {
        filterStepDiceButton->setAlpha(alpha);
        filterStepDiceButton->setEnabled(filterStepAreaEnabled);
    }
    if (filterStepPowerButton) {
        // Power button should always be enabled so user can turn sequencer back on
        // Only change alpha to show visual state
        filterStepPowerButton->setAlpha(filterStepAreaEnabled ? 1.0f : 0.3f);
        filterStepPowerButton->setEnabled(true); // Always enabled so it can be toggled
    }
    repaint();
}

void PluginEditor::randomizeFilterKnobValues()
{
    juce::Random& rng = juce::Random::getSystemRandom();
    
    // Randomize Type knob (respecting lock)
    if (!filterKnobLocked[0] && filterTypeKnob) {
        float randomType = static_cast<float>(rng.nextInt(5)); // 0-4
        filterTypeKnob->setValue(randomType, juce::sendNotification);
    }
    
    // Randomize Slope knob (respecting lock)
    if (!filterKnobLocked[3] && filterSlopeKnob) {
        float randomSlope = rng.nextFloat(); // 0-1
        filterSlopeKnob->setValue(randomSlope, juce::sendNotification);
    }
    
    // Randomize regular knobs (respecting locks)
    for (int i = 0; i < 5; ++i) { // 5 knobs: Cutoff, Res, Drive, Key Track, Mix
        // Map filterKnobs index to label index: 0→1 (Cutoff), 1→2 (Res), 2→4 (Drive), 3→5 (Key Track), 4→6 (Mix)
        int labelIdx = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 4 : (i == 3) ? 5 : 6;
        
        if (!filterKnobLocked[labelIdx] && filterKnobs[i]) {
            float randomValue = 0.0f;
            switch (i) {
                case 0: // Cutoff - random normalized value (0-1)
                    randomValue = rng.nextFloat();
                    break;
                case 1: // Resonance
                    randomValue = rng.nextFloat();
                    break;
                case 2: // Drive
                    randomValue = rng.nextFloat() * 36.0f; // 0-36 dB
                    break;
                case 3: // Key Track
                    randomValue = rng.nextFloat();
                    break;
                case 4: // Mix
                    randomValue = rng.nextFloat();
                    break;
            }
            filterKnobs[i]->setValue(randomValue, juce::sendNotification);
        }
    }
}

void PluginEditor::randomizeIndividualFilterKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8) return;
    if (filterKnobLocked[knobIndex]) return;
    
    juce::Random& rng = juce::Random::getSystemRandom();
    
    // Handle special knobs (Type and Slope)
    if (knobIndex == 0 && filterTypeKnob) { // Type
        float randomType = static_cast<float>(rng.nextInt(5)); // 0-4
        filterTypeKnob->setValue(randomType, juce::sendNotification);
    } else if (knobIndex == 3 && filterSlopeKnob) { // Slope
        float randomSlope = rng.nextFloat(); // 0-1
        filterSlopeKnob->setValue(randomSlope, juce::sendNotification);
    } else {
        // Regular knobs: map knobIndex to filterKnobs array
        // knobIndex 1→filterKnobs[0] (Cutoff), 2→filterKnobs[1] (Res), 4→filterKnobs[2] (Drive), 5→filterKnobs[3] (Key Track), 6→filterKnobs[4] (Mix)
        int filterKnobIdx = (knobIndex == 1) ? 0 : (knobIndex == 2) ? 1 : (knobIndex == 4) ? 2 : (knobIndex == 5) ? 3 : (knobIndex == 6) ? 4 : -1;
        
        if (filterKnobIdx >= 0 && filterKnobIdx < 5 && filterKnobs[filterKnobIdx]) {
            float randomValue = 0.0f;
            switch (filterKnobIdx) {
                case 0: // Cutoff
                    randomValue = rng.nextFloat();
                    break;
                case 1: // Resonance
                    randomValue = rng.nextFloat();
                    break;
                case 2: // Drive
                    randomValue = rng.nextFloat() * 36.0f;
                    break;
                case 3: // Key Track
                    randomValue = rng.nextFloat();
                    break;
                case 4: // Mix
                    randomValue = rng.nextFloat();
                    break;
            }
            filterKnobs[filterKnobIdx]->setValue(randomValue, juce::sendNotification);
        }
    }
}

void PluginEditor::updateFilterParameterFromKnob(int knobIndex)
{
    // knobIndex: -1 = Type, -2 = Slope, 0-3 = filterKnobs[0-3] (Cutoff, Res, Drive, Key Track)
    // Mix knob (filterKnobs[4]) is global, not saved per step
    
    // Get current step
    int currentStep = filterUiSelectedStep;
    if (currentStep < 0 || currentStep >= 16) return;
    
    // Get value and save to snapshot
    float value = 0.0f;
    if (knobIndex == -1) {
        value = filterTypeKnob->getValue();
    } else if (knobIndex == -2) {
        value = filterSlopeKnob->getValue();
    } else if (knobIndex >= 0 && knobIndex < 5) {
        if (knobIndex == 0) { // Cutoff - convert normalized value to frequency
            float normalized = filterKnobs[0]->getValue();
            float freq = normalized <= 0.75f 
                ? 20.0f + (5000.0f - 20.0f) * (normalized / 0.75f)
                : 5000.0f * std::pow(4.0f, (normalized - 0.75f) / 0.25f);
            value = juce::jlimit(20.0f, 20000.0f, freq);
        } else {
            value = filterKnobs[knobIndex]->getValue();
        }
    }
    processorRef.updateFilterCurrentStepSnapshot(knobIndex, value);
}

void PluginEditor::updateFilterSequencerUI()
{
    int stepsUsed = 16; // Default value
    try {
        int selectedStep = filterUiSelectedStep;
        int playingStep = processorRef.getFilterPlayingStep();
        const auto& seqState = processorRef.getFilterSeqState();
        stepsUsed = seqState.stepsUsed.load();
        
    for (int i = 0; i < 16; ++i) {
            if (filterStepButtons[i] != nullptr) {
                bool isSelected = (i == selectedStep);
                filterStepButtons[i]->setSelected(isSelected);
                bool sequencerEnabled = seqState.enabled.load();
                filterStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
                bool shouldBeEnabled = i < stepsUsed;
                filterStepButtons[i]->setEnabledStep(shouldBeEnabled);
            }
        }
    } catch (...) {
        DBG("[UI] Error updating Filter sequencer UI");
    }
    
    // Update step amount display
    if (filterStepAmountLabel != nullptr && !filterStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = filterStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            filterStepAmountLabel->setText(newText, false);
        }
    }
    
    repaint();
}

void PluginEditor::onFilterStepButtonClicked(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= 16) return;
    
    filterUiSelectedStep = stepIndex;
    processorRef.setFilterSelectedStep(stepIndex);
    
    auto snapshot = processorRef.getFilterSafeSnapshot(stepIndex);
    
    // Set flag to prevent snapshot saving during load
    isLoadingFromSnapshot.store(true);
    
    if (!filterAllStepsEnabled) {
        // Update knobs with values from the snapshot
        if (filterTypeKnob) filterTypeKnob->setValue(snapshot.filter.type, juce::dontSendNotification);
        // Convert frequency from snapshot to normalized value for cutoff knob
        if (filterKnobs[0]) {
            float freq = snapshot.filter.cutoff;
            float normalized = freq <= 5000.0f 
                ? 0.75f * (freq - 20.0f) / (5000.0f - 20.0f)
                : 0.75f + 0.25f * (std::log(freq / 5000.0f) / std::log(4.0f));
            normalized = juce::jlimit(0.0f, 1.0f, normalized);
            filterKnobs[0]->setValue(normalized, juce::dontSendNotification);
        }
        if (filterKnobs[1]) filterKnobs[1]->setValue(snapshot.filter.resonance, juce::dontSendNotification);
        if (filterSlopeKnob) filterSlopeKnob->setValue(snapshot.filter.slope, juce::dontSendNotification);
        if (filterKnobs[2]) filterKnobs[2]->setValue(snapshot.filter.drive, juce::dontSendNotification);
        // Key Track knob (filterKnobs[3])
        if (filterKnobs[3]) filterKnobs[3]->setValue(snapshot.filter.keytrack, juce::dontSendNotification);
        // Mix knob (filterKnobs[4]) is global, not per-step
        
        // Clear flag before triggering callbacks so labels update
        isLoadingFromSnapshot.store(false);
        
        // Trigger value change callbacks to update labels (but not save snapshots)
        if (filterTypeKnob && filterTypeKnob->onValueChange) filterTypeKnob->onValueChange();
        if (filterSlopeKnob && filterSlopeKnob->onValueChange) filterSlopeKnob->onValueChange();
        for (int i = 0; i < 5; ++i) { // Only first 5 knobs (Mix knob 5 is global)
            if (filterKnobs[i] && filterKnobs[i]->onValueChange) {
                // Temporarily set flag to prevent saving during callback
                isLoadingFromSnapshot.store(true);
                filterKnobs[i]->onValueChange();
                isLoadingFromSnapshot.store(false);
            }
        }
    }
    
    // Clear flag
    isLoadingFromSnapshot.store(false);
    
    updateFilterSequencerUI();
    repaint();
}

//==============================================================================
// Saturate Page Implementation
//==============================================================================

void PluginEditor::setupSaturateKnobs()
{
    DBG("[UI] Setting up Saturate knobs...");

    // Saturate knob names (6 knobs - Type removed, only Clean mode)
    std::vector<juce::String> saturateKnobNames = {
        "Drive", "Color", "Shape", "Bias", "Output", "Mix"
    };
    
    // Parameter IDs for APVTS attachments (6 knobs now, Type removed)
    std::vector<juce::String> saturateParamIds = {
        "satDrive", "satColor", "satShape", 
        "satBias", "satOut", "satMix"
    };

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;
    
    // Set oversample to max (3 = 8×) in APVTS and snapshots
    auto* osParam = processorRef.getAPVTS().getParameter("satOsMode");
    if (osParam) {
        osParam->setValueNotifyingHost(1.0f); // 3/3 = max (8×)
    }
    
    // Initialize all snapshots with max oversample
    for (int step = 0; step < 16; ++step) {
        auto snapshot = processorRef.getSaturateSafeSnapshot(step);
        snapshot.saturate.oversample = 3.0f; // Max (8×)
        processorRef.setSaturateStepSnapshot(step, snapshot);
    }
    
    // Create 6 knobs (Type removed, only Clean mode)
    for (int i = 0; i < 6; ++i)
    {
        saturateKnobs[i] = std::make_unique<CustomKnob>();
            addAndMakeVisible(saturateKnobs[i].get());
        saturateKnobs[i]->setVisible(false);
        
        saturateKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        saturateKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Set parameter ranges based on knob index
        switch (i) {
            case 0: // Drive (0-36 dB)
                saturateKnobs[i]->setRange(0.0, 36.0, 0.1);
                saturateKnobs[i]->setValue(12.0, juce::dontSendNotification);
                break;
            case 1: // Color (0-1, dynamic)
                saturateKnobs[i]->setRange(0.0, 1.0, 0.01);
                saturateKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 2: // Shape (0-1, dynamic)
                saturateKnobs[i]->setRange(0.0, 1.0, 0.01);
                saturateKnobs[i]->setValue(0.4, juce::dontSendNotification);
                break;
            case 3: // Bias (-0.2 to 0.2, dynamic)
                saturateKnobs[i]->setRange(-0.2, 0.2, 0.01);
                saturateKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 4: // Output (-24 to +12 dB)
                saturateKnobs[i]->setRange(-24.0, 12.0, 0.1);
                saturateKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 5: // Mix (0-1)
                saturateKnobs[i]->setRange(0.0, 1.0, 0.01);
                saturateKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
        }
        
        // Set knob images
        if (assets.knobRing != nullptr)
            saturateKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            saturateKnobs[i]->setInnerImage(assets.knobInside->createCopy());
        
        // Position knobs in 2 rows of 4 (EXACT same as other effects)
        int x, y;
        x = startX + (i % 4) * (knobSize + knobSpacing);
        y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px (EXACT same as other effects)
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1;
            
        saturateKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label
        saturateKnobLabels[i] = std::make_unique<juce::Label>();
        saturateKnobLabels[i]->setText(saturateKnobNames[i], juce::dontSendNotification);
        saturateKnobLabels[i]->setJustificationType(juce::Justification::centred);
        saturateKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        saturateKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        saturateKnobLabels[i]->setBounds(x, y - 15, knobSize, 20);
            addAndMakeVisible(saturateKnobLabels[i].get());
        saturateKnobLabels[i]->setVisible(false);
        
        // Create value label
        saturateValueLabels[i] = std::make_unique<juce::Label>();
        saturateValueLabels[i]->setText("0", juce::dontSendNotification);
        saturateValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        saturateValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        saturateValueLabels[i]->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(saturateValueLabels[i].get());
        saturateValueLabels[i]->setVisible(false);
        saturateValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);
        
        // Create indicator bar
        saturateIndicatorBars[i] = std::make_unique<IndicatorBar>();
            addAndMakeVisible(saturateIndicatorBars[i].get());
        saturateIndicatorBars[i]->setVisible(false);
        saturateIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        saturateIndicatorBars[i]->setValue(0.5f);
        
        // Create dice button (hidden like other pages - NOT added to component tree)
        saturateDiceButtons[i] = std::make_unique<CustomDiceButton>();
        // Do NOT call addAndMakeVisible - keep it hidden
        saturateDiceButtons[i]->onClick = [this, i]() { 
            randomizeIndividualSaturateKnob(i);
        };
        
        // Create lock button (only for first 5 knobs, Mix at index 5 doesn't have lock)
        if (i < 5) {
            saturateLockButtons[i] = std::make_unique<LockButton>();
            addAndMakeVisible(saturateLockButtons[i].get());
            saturateLockButtons[i]->setVisible(false);
            
            // Calculate the width of the knob title text
            juce::Font labelFont(12.0f, juce::Font::bold);
            int textWidth = labelFont.getStringWidth(saturateKnobNames[i]);
            
            // Use same sizing as dub delay
            const int lockSize = 10;
            const int lockSpacing = 5; // Fixed distance from end of title text
            
            // Position lock button at the end of the title text + fixed spacing
            int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
            int lockY = y - 10;
            
            saturateLockButtons[i]->setBounds(lockX, lockY, lockSize, lockSize);
            
            // Set lock button images
            if (assets.unlockedIcon && assets.lockedIcon) {
                auto imgUnlocked = assets.unlockedIcon->createCopy();
                auto imgLocked = assets.lockedIcon->createCopy();
                saturateLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
            }
            saturateLockButtons[i]->setToggleState(saturateKnobLocked[i], juce::dontSendNotification);
            
            saturateLockButtons[i]->onClick = [this, i]() {
                saturateKnobLocked[i] = !saturateKnobLocked[i];
                saturateLockButtons[i]->setToggleState(saturateKnobLocked[i], juce::dontSendNotification);
            };
        }
        
        // Create APVTS attachment
        saturateAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), saturateParamIds[i], *saturateKnobs[i]);
        
        // Add value change callback
        saturateKnobs[i]->onValueChange = [this, i]() {
            // Skip if loading from snapshot (prevents circular updates during randomization)
            if (isLoadingFromSnapshot.load())
                return;
            
            if (saturateKnobs[i] != nullptr && saturateValueLabels[i] != nullptr) {
                float value = saturateKnobs[i]->getValue();
                
                // Update value label with appropriate formatting
                juce::String valueText;
                switch (i) {
                    case 0: // Drive - show dB
                        valueText = juce::String(value, 1) + " dB";
                        break;
                    case 1: // Color - show as percentage
                        valueText = juce::String(static_cast<int>(value * 100)) + "%";
                        break;
                    case 2: // Shape - show as percentage
                        valueText = juce::String(static_cast<int>(value * 100)) + "%";
                        break;
                    case 3: // Bias - show as is
                        valueText = juce::String(value, 2);
                        break;
                    case 4: // Output - show dB
                        valueText = juce::String(value, 1) + " dB";
                        break;
                    case 5: // Mix - show as percentage
                        valueText = juce::String(static_cast<int>(value * 100)) + "%";
                        break;
                }
                saturateValueLabels[i]->setText(valueText, juce::dontSendNotification);
                
                // Update indicator bar
                if (saturateIndicatorBars[i]) {
                    float normValue = 0.0f;
                    switch (i) {
                        case 0: normValue = value / 36.0f; break; // Drive
                        case 1: case 2: case 5: normValue = value; break; // Color, Shape, Mix (0-1)
                        case 3: normValue = (value + 0.2f) / 0.4f; break; // Bias (-0.2 to 0.2)
                        case 4: normValue = (value + 24.0f) / 36.0f; break; // Output (-24 to 12)
                    }
                    saturateIndicatorBars[i]->setValue(normValue);
                }
                
                // Save snapshot for current step
                updateSaturateParameterFromKnob(i);
                
                // Handle all steps toggle - update all steps when enabled
                if (saturateAllStepsEnabled && i != 5) { // Skip Mix (knob 5) which is global
                    // Save current selected step first (done above), then update all others
                    int currentStep = saturateUiSelectedStep;
                    for (int step = 0; step < 16; ++step) {
                        if (step != currentStep) {
                            auto snapshot = processorRef.getSaturateSafeSnapshot(step);
                            switch (i) {
                                case 0: snapshot.saturate.drive = value; break;
                                case 1: snapshot.saturate.color = value; break;
                                case 2: snapshot.saturate.shape = value; break;
                                case 3: snapshot.saturate.bias = value; break;
                                case 4: snapshot.saturate.output = value; break;
                                // Mix (case 5) is global, not saved per step
                            }
                            processorRef.setSaturateStepSnapshot(step, snapshot);
                        }
                    }
                }
            }
        };
    }
    
    // Initialize dynamic labels for default type (Spiral2)
    updateSaturateKnobLabels(0);
    
    // Note: saturateGroup is populated in setupSaturateEffectsArea and setupSaturateSequencerArea
    // Do not populate here to avoid double-adds
    
    DBG("[UI] Saturate knobs setup complete");
}

void PluginEditor::setupSaturateEffectsArea()
{
    DBG("[UI] Setting up Saturate effects area...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Effect title
    saturateEffectsTitle = std::make_unique<juce::Label>();
    saturateEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    saturateEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    saturateEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    saturateEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(saturateEffectsTitle.get());
    saturateEffectsTitle->setVisible(false);
    saturateEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // Saturate dice button
    saturateDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(saturateDiceButton.get());
    saturateDiceButton->setVisible(false);
    
    const int diceSize = 32;
    saturateDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    if (assets.diceLarge != nullptr) {
        saturateDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    saturateDiceButton->onClick = [this]() {
        randomizeSaturateKnobValues();
    };
    
    // FX Power Button
    saturateFxPowerButton = std::make_unique<juce::DrawableButton>("SaturateFxPower", juce::DrawableButton::ImageFitted);
    addAndMakeVisible(saturateFxPowerButton.get());
    saturateFxPowerButton->setVisible(false);
    saturateFxPowerButton->setClickingTogglesState(true);
    
    saturateFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    saturateFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn) {
        saturateFxPowerButton->setImages(assets.fxPowerOn.get());
    }
    
    const int buttonSize = 46;
    saturateFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                     effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    saturateFxPowerButton->setToggleState(true, juce::dontSendNotification); // Default ON
    saturateFxAreaEnabled = true;
    saturateFxPowerButton->onClick = [this]() {
        saturateFxAreaEnabled = saturateFxPowerButton->getToggleState();
        updateSaturateFxAreaVisibility();
        auto* param = processorRef.getAPVTS().getParameter("saturateEnabled");
        if (param)
            param->setValueNotifyingHost(saturateFxAreaEnabled ? 1.0f : 0.0f);
    };
    
    // Populate Saturate group for visibility management (EXACT match Dub Delay)
    saturateGroup.clear();
    
    saturateGroup.push_back(saturateEffectsTitle.get());
    saturateGroup.push_back(saturateDiceButton.get());
    saturateGroup.push_back(saturateFxPowerButton.get());
    
    // Add knobs and related components
    for (int i = 0; i < 6; ++i) { // 6 knobs (Type removed)
        if (saturateKnobs[i]) saturateGroup.push_back(saturateKnobs[i].get());
        if (saturateKnobLabels[i]) saturateGroup.push_back(saturateKnobLabels[i].get());
        if (saturateValueLabels[i]) saturateGroup.push_back(saturateValueLabels[i].get());
        if (saturateIndicatorBars[i]) saturateGroup.push_back(saturateIndicatorBars[i].get());
        if (saturateDiceButtons[i]) saturateGroup.push_back(saturateDiceButtons[i].get());
        // Only add lock button for first 5 knobs (Mix at index 5 doesn't have lock)
        if (i < 5 && saturateLockButtons[i]) saturateGroup.push_back(saturateLockButtons[i].get());
    }
    
    DBG("[UI] Saturate effects area setup complete");
}

void PluginEditor::setupSaturateSequencerArea()
{
    DBG("[UI] Setting up Saturate sequencer area...");
    
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // "STEP" title
    saturateStepTitle = std::make_unique<juce::Label>();
    saturateStepTitle->setText("STEP", juce::dontSendNotification);
    saturateStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    saturateStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    saturateStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(saturateStepTitle.get());
    saturateStepTitle->setVisible(false);
    saturateStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Step buttons (2x8 grid)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i)
    {
        saturateStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(saturateStepButtons[i].get());
        saturateStepButtons[i]->setVisible(false);
        
        int col = i % 8;
        int row = i / 8;
        int x = startX + col * (buttonSize + buttonSpacing);
        int y = startY + row * (buttonSize + buttonSpacing);
        
        saturateStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        // Set images for step buttons
        if (assets.stepActive != nullptr)
            saturateStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        if (assets.stepInactive != nullptr)
            saturateStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        
        saturateStepButtons[i]->onClick = [this, i]() {
            onSaturateStepButtonClicked(i);
        };
    }
    
    // Step amount label
    saturateStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setSaturateStepsUsed(16);
    saturateStepAmountLabel->setText("16");
    saturateStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    saturateStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    saturateStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    saturateStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    saturateStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    saturateStepAmountLabel->setJustification(juce::Justification::centred);
    saturateStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    saturateStepAmountLabel->setIndents(0, 0);
    saturateStepAmountLabel->setInputRestrictions(2, "0123456789");
    addAndMakeVisible(saturateStepAmountLabel.get());
    saturateStepAmountLabel->setVisible(false);
    saturateStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    
    // Rate dropdown (EXACT match Dub Delay)
    saturateRateDropdown = std::make_unique<juce::ComboBox>();
    saturateRateDropdown->addItem("4", 1);
    saturateRateDropdown->addItem("2", 2);
    saturateRateDropdown->addItem("1", 3);
    saturateRateDropdown->addItem("1/2", 4);
    saturateRateDropdown->addItem("1/4", 5);
    saturateRateDropdown->addItem("1/8", 6);
    saturateRateDropdown->addItem("1/16", 7);
    saturateRateDropdown->addItem("1/32", 8);
    saturateRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    saturateRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    saturateRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    saturateRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    saturateRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    
    // Set selected ID from processor state (like Dub Delay)
    int divIdx = processorRef.getSaturateSeqState().divisionIndex.load();
    saturateRateDropdown->setSelectedId(divIdx + 1, juce::dontSendNotification);
    
    addAndMakeVisible(saturateRateDropdown.get());
    saturateRateDropdown->setVisible(false);
    saturateRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    
    saturateRateDropdown->onChange = [this]() {
        int selectedId = saturateRateDropdown->getSelectedId();
        if (selectedId > 0) {
            int divisionIndex = selectedId - 1;
            processorRef.setSaturateDivisionIndex(divisionIndex);
            DBG("[UI] Saturate rate changed to index: " << divisionIndex);
        }
    };
    
    // STD toggle
    saturateStdToggle = std::make_unique<CircularToggleButton>();
    saturateStdToggle->setButtonText("-");
    addAndMakeVisible(saturateStdToggle.get());
    saturateStdToggle->setVisible(false);
    saturateStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    // Step dice button
    saturateStepDiceButton = std::make_unique<CustomDiceButton>();
    saturateStepDiceButton->setVisible(false);
    int stepDiceSize = static_cast<int>(35 * 0.7);
    saturateStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, stepDiceSize, stepDiceSize);
    addAndMakeVisible(saturateStepDiceButton.get());
    if (assets.diceLarge != nullptr) {
        saturateStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    saturateStepDiceButton->onClick = [this]() {
        DBG("[UI] Saturate step dice button clicked - randomizing all step snapshots");
        
        juce::Random& rng = juce::Random::getSystemRandom();
        
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getSaturateSafeSnapshot(step);
            
            // Randomize all Saturate parameters for this step
            snapshot.saturate.type = static_cast<float>(rng.nextInt(8)); // 0-7
            snapshot.saturate.drive = 6.0f + rng.nextFloat() * 24.0f; // 6-30 dB
            snapshot.saturate.color = 0.3f + rng.nextFloat() * 0.5f; // 0.3-0.8
            snapshot.saturate.shape = 0.2f + rng.nextFloat() * 0.6f; // 0.2-0.8
            snapshot.saturate.bias = -0.15f + rng.nextFloat() * 0.3f; // -0.15 to 0.15
            snapshot.saturate.output = -12.0f + rng.nextFloat() * 18.0f; // -12 to +6 dB
            snapshot.saturate.oversample = static_cast<float>(rng.nextInt(4)); // 0-3
            
            processorRef.setSaturateStepSnapshot(step, snapshot);
        }
        
        // Reload current step to UI (don't send notification to avoid triggering save)
        auto currentSnapshot = processorRef.getSaturateSafeSnapshot(saturateUiSelectedStep);
        // Type removed, only 6 knobs now: Drive, Color, Shape, Bias, Output, Mix
        if (saturateKnobs[0]) saturateKnobs[0]->setValue(currentSnapshot.saturate.drive, juce::dontSendNotification);
        if (saturateKnobs[1]) saturateKnobs[1]->setValue(currentSnapshot.saturate.color, juce::dontSendNotification);
        if (saturateKnobs[2]) saturateKnobs[2]->setValue(currentSnapshot.saturate.shape, juce::dontSendNotification);
        if (saturateKnobs[3]) saturateKnobs[3]->setValue(currentSnapshot.saturate.bias, juce::dontSendNotification);
        if (saturateKnobs[4]) saturateKnobs[4]->setValue(currentSnapshot.saturate.output, juce::dontSendNotification);
        if (saturateKnobs[5]) saturateKnobs[5]->setValue(currentSnapshot.saturate.mix, juce::dontSendNotification);
        // Oversample is always max (3 = 8×), not a knob anymore
        
        // Update labels (no longer needed, but keep call for compatibility)
        updateSaturateKnobLabels(0);
        for (int i = 0; i < 6; ++i) { // All 6 knobs
            if (saturateKnobs[i] && saturateKnobs[i]->onValueChange) {
                saturateKnobs[i]->onValueChange();
            }
        }
        
        DBG("[UI] All 16 Saturate steps randomized");
    };
    
    // Step power button (EXACT match Dub Delay)
    saturateStepPowerButton = std::make_unique<juce::DrawableButton>("SaturateStepPower", juce::DrawableButton::ImageFitted);
    
    // Make button background transparent (match Dub Delay)
    saturateStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    saturateStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn) {
        saturateStepPowerButton->setImages(assets.stepPowerOn.get());
    }
    addAndMakeVisible(saturateStepPowerButton.get());
    saturateStepPowerButton->setVisible(false);
    saturateStepPowerButton->setClickingTogglesState(true);
    const int stepPowerSize = 40;
    saturateStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - stepPowerSize - 5 + 15 - 5 - 1, sequencerArea.getY() - 5 - stepPowerSize + 25 + 5, stepPowerSize, stepPowerSize);
    saturateStepPowerButton->setToggleState(saturateStepAreaEnabled, juce::dontSendNotification);
    saturateStepPowerButton->setClickingTogglesState(true);
    saturateStepPowerButton->onClick = [this]() {
        saturateStepAreaEnabled = saturateStepPowerButton->getToggleState();
        processorRef.setSaturateSequencerEnabled(saturateStepAreaEnabled);
        updateSaturateStepAreaVisibility();
    };
    
    // Add sequencer components to group
    if (saturateStepTitle) saturateGroup.push_back(saturateStepTitle.get());
    if (saturateStepAmountLabel) saturateGroup.push_back(saturateStepAmountLabel.get());
    if (saturateRateDropdown) saturateGroup.push_back(saturateRateDropdown.get());
    if (saturateStdToggle) saturateGroup.push_back(saturateStdToggle.get());
    if (saturateStepDiceButton) saturateGroup.push_back(saturateStepDiceButton.get());
    if (saturateStepPowerButton) saturateGroup.push_back(saturateStepPowerButton.get());
        
        for (int i = 0; i < 16; ++i) {
        if (saturateStepButtons[i]) saturateGroup.push_back(saturateStepButtons[i].get());
    }
    
    DBG("[UI] Saturate sequencer area setup complete");
}

void PluginEditor::setupSaturateAllStepsToggle()
{
    DBG("[UI] Setting up Saturate all steps toggle...");
    
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // All Steps toggle button (EXACT match Dub Delay)
    saturateAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(saturateAllStepsToggle.get());
    saturateAllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    saturateAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                       effectArea.getY() - 1, buttonSize, buttonSize);
    
    // CRITICAL: Must use static_cast and setImages BEFORE any other setup
    if (assets.stepTopInactive && assets.stepTopActive) {
        static_cast<AllStepsToggleButton*>(saturateAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    saturateAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    saturateAllStepsToggle->onClick = [this]() {
        saturateAllStepsEnabled = saturateAllStepsToggle->getToggleState();
        DBG("[UI] Saturate All Steps toggle: " << (saturateAllStepsEnabled ? "ON" : "OFF"));
        saturateAllStepsLabel->setAlpha(saturateAllStepsEnabled ? 1.0f : 0.5f);
    };
    
    // All Steps label (match Dub Delay)
    saturateAllStepsLabel = std::make_unique<juce::Label>();
    saturateAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    saturateAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    saturateAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    saturateAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(saturateAllStepsLabel.get());
    saturateAllStepsLabel->setVisible(false);
    saturateAllStepsLabel->setAlpha(1.0f); // Start fully visible (will grey when toggled off)
    saturateAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                effectArea.getY() + 1, 80, 24);
    
    // Add all steps toggle to group
    if (saturateAllStepsToggle) saturateGroup.push_back(saturateAllStepsToggle.get());
    if (saturateAllStepsLabel) saturateGroup.push_back(saturateAllStepsLabel.get());
    
    DBG("[UI] Saturate all steps toggle setup complete. Total components in saturateGroup: " << saturateGroup.size());
}

void PluginEditor::updateSaturateFxAreaVisibility()
{
    float alpha = saturateFxAreaEnabled ? 1.0f : 0.3f;
    
    // Update knobs and labels alpha (6 knobs total)
    for (int i = 0; i < 6; ++i) {
        if (saturateKnobs[i]) { 
            saturateKnobs[i]->setAlpha(alpha); 
            saturateKnobs[i]->setEnabled(saturateFxAreaEnabled);
        }
        if (saturateKnobLabels[i]) saturateKnobLabels[i]->setAlpha(alpha);
        if (saturateValueLabels[i]) saturateValueLabels[i]->setAlpha(alpha);
        if (saturateIndicatorBars[i]) saturateIndicatorBars[i]->setAlpha(alpha);
        // Only first 5 knobs have lock buttons (Mix at index 5 doesn't have lock)
        if (i < 5 && saturateLockButtons[i]) {
            saturateLockButtons[i]->setAlpha(alpha);
            saturateLockButtons[i]->setEnabled(saturateFxAreaEnabled);
        }
    }
    
    if (saturateDiceButton) {
        saturateDiceButton->setAlpha(alpha);
        saturateDiceButton->setEnabled(saturateFxAreaEnabled);
    }
    if (saturateEffectsTitle) saturateEffectsTitle->setAlpha(alpha);
    
    repaint();
}

void PluginEditor::updateSaturateStepAreaVisibility()
{
    float alpha = saturateStepAreaEnabled ? 1.0f : 0.3f;
    
    // Update step buttons
    for (int i = 0; i < 16; ++i) {
        if (saturateStepButtons[i]) {
            saturateStepButtons[i]->setAlpha(alpha);
            saturateStepButtons[i]->setEnabled(saturateStepAreaEnabled);
        }
    }
    
    // Update sequencer controls
    if (saturateStepAmountLabel) {
        saturateStepAmountLabel->setAlpha(alpha);
        saturateStepAmountLabel->setEnabled(saturateStepAreaEnabled);
    }
    if (saturateRateDropdown) {
        saturateRateDropdown->setAlpha(alpha);
        saturateRateDropdown->setEnabled(saturateStepAreaEnabled);
    }
    if (saturateStdToggle) {
        saturateStdToggle->setAlpha(alpha);
        saturateStdToggle->setEnabled(saturateStepAreaEnabled);
    }
    if (saturateStepDiceButton) {
        saturateStepDiceButton->setAlpha(alpha);
        saturateStepDiceButton->setEnabled(saturateStepAreaEnabled);
    }
    
    repaint();
}

void PluginEditor::updateSaturateKnobLabels(int type)
{
    struct ModelInfo {
        const char* label2;
        const char* label3;
        const char* label4;
    };
    
    static const ModelInfo models[8] = {
        {"Density", "Asym", "Bias"}, // Spiral2
        {"Thick", "Focus", "LoCut"}, // Density2
        {"Hard", "Asym", "LoCut"}, // Drive
        {"Heat", "AirGain", "LoCut"}, // PurestDrive
        {"Warm", "Presence", "HiCut"}, // Mojo
        {"Trim", "Focus", "HiCut"}, // Console
        {"Iron", "Asym", "Bump"}, // Coils
        {"Blend", "Sag", "LoCut"}  // Tubey
    };
    
    if (type < 0 || type >= 8) return;
    
    auto& info = models[type];
    
    // Update labels
    if (saturateKnobLabels[2]) saturateKnobLabels[2]->setText(info.label2, juce::dontSendNotification);
    if (saturateKnobLabels[3]) saturateKnobLabels[3]->setText(info.label3, juce::dontSendNotification);
    if (saturateKnobLabels[4]) saturateKnobLabels[4]->setText(info.label4, juce::dontSendNotification);
    
    // No longer needed - only Clean mode, all knobs always visible
}

void PluginEditor::onSaturateStepButtonClicked(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= 16) return;
    
    saturateUiSelectedStep = stepIndex;
    processorRef.setSaturateSelectedStep(stepIndex);
    
    auto snapshot = processorRef.getSaturateSafeSnapshot(stepIndex);
    
    // Set flag to prevent snapshot saving during load
    isLoadingFromSnapshot.store(true);
    
    if (!saturateAllStepsEnabled) {
        // Update knobs with values from the snapshot (Type removed)
        if (saturateKnobs[0]) saturateKnobs[0]->setValue(snapshot.saturate.drive, juce::dontSendNotification);
        if (saturateKnobs[1]) saturateKnobs[1]->setValue(snapshot.saturate.color, juce::dontSendNotification);
        if (saturateKnobs[2]) saturateKnobs[2]->setValue(snapshot.saturate.shape, juce::dontSendNotification);
        if (saturateKnobs[3]) saturateKnobs[3]->setValue(snapshot.saturate.bias, juce::dontSendNotification);
        if (saturateKnobs[4]) saturateKnobs[4]->setValue(snapshot.saturate.output, juce::dontSendNotification);
        if (saturateKnobs[5]) saturateKnobs[5]->setValue(snapshot.saturate.mix, juce::dontSendNotification);
        // Oversample is always max (3 = 8×), not a knob anymore
        
        // Update labels (no longer needed, but keep call for compatibility)
        updateSaturateKnobLabels(0);
        
        // Trigger value change callbacks to update labels (but not save snapshots)
        for (int i = 0; i < 6; ++i) { // All 6 knobs
            if (saturateKnobs[i] && saturateKnobs[i]->onValueChange) {
                saturateKnobs[i]->onValueChange();
            }
        }
    }
    
    // Clear flag
    isLoadingFromSnapshot.store(false);
    
    updateSaturateSequencerUI();
    repaint();
}

void PluginEditor::updateSaturateSequencerUI()
{
    int selectedStep = saturateUiSelectedStep;
    int playingStep = processorRef.getSaturatePlayingStep(); // Use playingStep, not currentStep
    const int stepsUsed = processorRef.getSaturateSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (saturateStepButtons[i] != nullptr) {
            saturateStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getSaturateSeqState().enabled.load();
            saturateStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
            bool shouldBeEnabled = i < stepsUsed;
            saturateStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display
    if (saturateStepAmountLabel != nullptr && !saturateStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = saturateStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            saturateStepAmountLabel->setText(newText, false);
        }
    }
    
    repaint();
}

void PluginEditor::updateSaturateParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 6 || !saturateKnobs[knobIndex])
        return;
    
        float value = saturateKnobs[knobIndex]->getValue();
        
        // Update current step snapshot (Mix knob 5 is global, not saved)
        // Map knob index: 0=Drive, 1=Color, 2=Shape, 3=Bias, 4=Output, 5=Mix
        // Snapshot indices: 0=type (unused), 1=drive, 2=color, 3=shape, 4=bias, 5=output, 6=mix
        // Pass knob index directly - function will map it correctly
        processorRef.updateSaturateCurrentStepSnapshot(knobIndex, value);
}

void PluginEditor::randomizeSaturateKnobValues()
{
    DBG("[UI] Randomizing Saturate knob values...");
    for (int i = 0; i < 6; ++i) {
        if (saturateKnobs[i]) {
            // Check if knob is locked - skip if locked
            if (i < 6 && saturateKnobLocked[i]) continue;
            randomizeIndividualSaturateKnob(i);
        }
    }
}

void PluginEditor::randomizeIndividualSaturateKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 6) return;
    if (!saturateKnobs[knobIndex]) return;
    
    // Check if knob is locked - don't randomize if locked
    if (knobIndex < 6 && saturateKnobLocked[knobIndex]) {
        DBG("[UI] Skipping locked Saturate knob " << knobIndex);
        return;
    }
    
    juce::Random rand;
    float newValue = 0.0f;
    
    switch (knobIndex) {
        case 0: // Drive (0-36 dB)
            newValue = 6.0f + rand.nextFloat() * 24.0f; // 6-30 dB (musical range)
            break;
        case 1: // Color (0-1)
            newValue = 0.3f + rand.nextFloat() * 0.5f; // 0.3-0.8
            break;
        case 2: // Shape (0-1)
            newValue = 0.2f + rand.nextFloat() * 0.6f; // 0.2-0.8
            break;
        case 3: // Bias (-0.2 to 0.2)
            newValue = -0.15f + rand.nextFloat() * 0.3f; // -0.15 to 0.15
            break;
        case 4: // Output (-24 to +12 dB)
            newValue = -12.0f + rand.nextFloat() * 18.0f; // -12 to +6 dB
            break;
        case 5: // Mix (0-1)
            newValue = 0.3f + rand.nextFloat() * 0.5f; // 0.3-0.8
            break;
    }
    
    saturateKnobs[knobIndex]->setValue(newValue, juce::sendNotification);
}

void PluginEditor::setupForm2Knobs()
{
    // Form 2 knob titles - 8 controls
    const juce::StringArray form2KnobTitles = {
        "Root Note",
        "Scale",
        "Chord Size",
        "Shift",
        "Color",
        "Motion",
        "Resynth",
        "Mix"
    };
    
    // Form 2 parameter IDs (must match APVTS order)
    const juce::StringArray form2ParamIDs = {
        "form2RootNote",
        "form2Scale",
        "form2ChordSize",
        "form2Shift",
        "form2Brightness",
        "form2Motion",
        "form2Air",
        "form2Mix"
    };
    
    DBG("[UI] Setting up Form 2 knobs...");

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    // Create and setup 8 knobs
    for (int i = 0; i < 8; ++i)
    {
        // Position 8 knobs in 2 rows of 4
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Position adjustments
        if (i < 4)
            y -= 23;
        else
            y -= 1;
        
        // Create knob
        form2Knobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(form2Knobs[i].get());
        form2Knobs[i]->setVisible(false);
        form2Knobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        form2Knobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        // Set knob ranges based on parameter
        switch (i) {
            case 0: // Root Note (C-B) - Map to 0.0-1.0 for AudioParameterInt
                form2Knobs[i]->setRange(0.0, 1.0, 0.01);
                form2Knobs[i]->setValue(0.0, juce::dontSendNotification);
                form2Knobs[i]->textFromValueFunction = [](double v) {
                    const char* notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                    return notes[static_cast<int>(v * 12)];
                };
                break;
            case 1: // Scale (Major, Minor, Pent, Blues, Dorian, Whole, Chrom) - Map to 0.0-1.0
                form2Knobs[i]->setRange(0.0, 1.0, 0.01);
                form2Knobs[i]->setValue(0.0, juce::dontSendNotification);
                form2Knobs[i]->textFromValueFunction = [](double v) {
                    const char* scales[7] = {"Major", "Minor", "Pent", "Blues", "Dorian", "Whole", "Chrom"};
                    return scales[static_cast<int>(v * 7)];
                };
                break;
            case 2: // Chord Size (1-8) - Map to 0.0-1.0
                form2Knobs[i]->setRange(0.0, 1.0, 0.01);
                form2Knobs[i]->setValue((4.0 - 1.0) / 7.0, juce::dontSendNotification); // 4 mapped to 0.0-1.0
                break;
            case 3: // Shift (0.5-2.0)
                form2Knobs[i]->setRange(0.5, 2.0, 0.01);
                form2Knobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 4: // Color (-12 to +12 dB)
                form2Knobs[i]->setRange(-12.0, 12.0, 0.1);
                form2Knobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 5: // Motion (0-1)
                form2Knobs[i]->setRange(0.0, 1.0, 0.01);
                form2Knobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 6: // Resynth (0-1)
                form2Knobs[i]->setRange(0.0, 1.0, 0.01);
                form2Knobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                form2Knobs[i]->setRange(0.0, 1.0, 0.01);
                form2Knobs[i]->setValue(0.8, juce::dontSendNotification);
                break;
        }
        
        // Set knob images
        if (assets.knobRing != nullptr)
            form2Knobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            form2Knobs[i]->setInnerImage(assets.knobInside->createCopy());

        // Position knob
        form2Knobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label
        form2KnobLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(form2KnobLabels[i].get());
        form2KnobLabels[i]->setVisible(false);
        form2KnobLabels[i]->setText(form2KnobTitles[i], juce::dontSendNotification);
        form2KnobLabels[i]->setJustificationType(juce::Justification::centred);
        form2KnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        form2KnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        form2KnobLabels[i]->setBounds(x, y - 15, knobSize, 20);
        
        // Create value label
        form2ValueLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(form2ValueLabels[i].get());
        form2ValueLabels[i]->setVisible(false);
        form2ValueLabels[i]->setText("0", juce::dontSendNotification);
        form2ValueLabels[i]->setJustificationType(juce::Justification::centred);
        form2ValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        form2ValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        form2ValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15);
        
        // Create indicator bar
        form2IndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(form2IndicatorBars[i].get());
        form2IndicatorBars[i]->setVisible(false);
        form2IndicatorBars[i]->setValue(0.5f);
        form2IndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13);
        
        // Create lock button
        form2LockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(form2LockButtons[i].get());
        form2LockButtons[i]->setVisible(false);
        
        // Position lock button at end of title text
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(form2KnobTitles[i]);
        const int lockSize = 10;
        const int lockSpacing = 5;
        int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
        int lockY = y - 10;
        
        form2LockButtons[i]->setBounds(lockX, lockY, lockSize, lockSize);
        
        // Set lock button images
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            form2LockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        form2LockButtons[i]->setToggleState(form2KnobLocked[i], juce::dontSendNotification);
        form2LockButtons[i]->onClick = [this, i]() {
            form2KnobLocked[i] = form2LockButtons[i]->getToggleState();
        };
        
        // Create APVTS attachment
        form2Attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), form2ParamIDs[i], *form2Knobs[i]);
        
        // Add value change callback
        form2Knobs[i]->onValueChange = [this, i]() {
            if (isLoadingFromSnapshot.load()) return;
            
            if (form2Knobs[i] && form2ValueLabels[i] && form2IndicatorBars[i]) {
                float value = form2Knobs[i]->getValue();
                juce::String valueText;
                
                switch (i) {
                    case 0: { // Root Note - show note (0.0-1.0 mapped to C-B)
                        const char* notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                        int noteIdx = juce::jlimit(0, 11, static_cast<int>(value * 12));
                        valueText = notes[noteIdx];
                        break;
                    }
                    case 1: { // Scale - show scale name (0.0-1.0 mapped to 0-6)
                        const char* scales[7] = {"Major", "Minor", "Pent", "Blues", "Dorian", "Whole", "Chrom"};
                        int scaleIdx = juce::jlimit(0, 6, static_cast<int>(value * 7));
                        valueText = scales[scaleIdx];
                        break;
                    }
                    case 2: // Chord Size - show as integer (0.0-1.0 mapped to 1-8)
                        valueText = juce::String(1 + static_cast<int>(value * 7));
                        break;
                    case 3: // Shift
                        valueText = juce::String(value, 2);
                        break;
                    case 4: // Color
                        valueText = juce::String(value, 1) + " dB";
                        break;
                    case 5: // Motion
                    case 6: // Resynth
                    case 7: // Mix
                        valueText = juce::String(value, 2);
                        break;
                }
                
                form2ValueLabels[i]->setText(valueText, juce::dontSendNotification);
                
                // Update indicator bar
                form2IndicatorBars[i]->setValue(value);
                
                // Update current step snapshot with new value
                processorRef.updateForm2CurrentStepSnapshot(i, value);
                
                // If All Steps toggle is active, update all step snapshots
                if (form2AllStepsEnabled) {
                    DBG("[All Steps] Form 2 knob " << i << " changed, form2AllStepsEnabled=true");
                    
                    // Convert knob value to actual parameter value using APVTS parameter ranges
                    float actualValue = value;
                    switch (i) {
                    case 0: { // Root Note
                        auto* param = processorRef.getAPVTS().getParameter("form2RootNote");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 1: { // Scale
                        auto* param = processorRef.getAPVTS().getParameter("form2Scale");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 2: { // Chord Size
                        auto* param = processorRef.getAPVTS().getParameter("form2ChordSize");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 3: { // Shift
                        auto* param = processorRef.getAPVTS().getParameter("form2Shift");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 4: { // Color
                        auto* param = processorRef.getAPVTS().getParameter("form2Brightness");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 5: { // Motion
                        auto* param = processorRef.getAPVTS().getParameter("form2Motion");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 6: { // Resynth
                        auto* param = processorRef.getAPVTS().getParameter("form2Air");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    case 7: { // Mix
                        auto* param = processorRef.getAPVTS().getParameter("form2Mix");
                        if (param) actualValue = param->convertFrom0to1(value);
                        break;
                    }
                    }
                    
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getForm2SafeSnapshot(step);
                        switch (i) {
                            case 0: snapshot.form2.rootNote = static_cast<int>(actualValue); break;
                            case 1: snapshot.form2.scale = static_cast<int>(actualValue); break;
                            case 2: snapshot.form2.chordSize = static_cast<int>(actualValue); break;
                            case 3: snapshot.form2.shift = actualValue; break;
                            case 4: snapshot.form2.color = actualValue; break;
                            case 5: snapshot.form2.motion = actualValue; break;
                            case 6: snapshot.form2.resynth = actualValue; break;
                            case 7: snapshot.form2.mix = actualValue; break;
                        }
                        processorRef.setForm2StepSnapshot(step, snapshot);
                    }
                } else {
                    DBG("[All Steps] Form 2 knob " << i << " changed, form2AllStepsEnabled=false, skipping All Steps update");
                }
            }
        };
    }
    
    DBG("[UI] Form 2 knobs setup complete");
}

void PluginEditor::setupForm2EffectsArea()
{
    DBG("[UI] Setting up Form 2 effects area...");
    
    // Effect area bounds (match other pages exactly)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title label (ALWAYS "EFFECT", NOT the effect name!)
    form2EffectsTitle = std::make_unique<juce::Label>();
    form2EffectsTitle->setText("EFFECT", juce::dontSendNotification);
    form2EffectsTitle->setFont(juce::Font(27.648f, juce::Font::bold));
    form2EffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    form2EffectsTitle->setJustificationType(juce::Justification::centredLeft);
    form2EffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    addAndMakeVisible(form2EffectsTitle.get());
    form2EffectsTitle->setVisible(false);
    
    // Create FX power button
    form2FxPowerButton = std::make_unique<juce::DrawableButton>("Form2EffectPower", juce::DrawableButton::ImageFitted);
    form2FxPowerButton->setClickingTogglesState(true);
    form2FxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    form2FxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    if (assets.fxPowerOn) {
        form2FxPowerButton->setImages(assets.fxPowerOn.get());
    }
    const int buttonSize = 46;
    form2FxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                 effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    addAndMakeVisible(form2FxPowerButton.get());
    form2FxPowerButton->setVisible(false);
    
    // Create dice button
    form2DiceButton = std::make_unique<CustomDiceButton>();
    const int diceSize = 32;
    form2DiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    if (assets.diceLarge != nullptr) {
        form2DiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    addAndMakeVisible(form2DiceButton.get());
    form2DiceButton->setVisible(false);
    
    // Set up FX power button callback
    form2FxPowerButton->onClick = [this]() {
        form2FxAreaEnabled = form2FxPowerButton->getToggleState();
        updateForm2FxAreaVisibility();
        
        // Update the actual parameter that the processor checks
        auto* form2EnabledParam = processorRef.getAPVTS().getParameter("form2Enabled");
        if (form2EnabledParam) {
            form2EnabledParam->setValueNotifyingHost(form2FxAreaEnabled ? 1.0f : 0.0f);
        }
    };
    
    // Set up dice button callback
    form2DiceButton->onClick = [this]() {
        randomizeForm2KnobValues();
    };
    
    DBG("[UI] Form 2 effects area setup complete");
}

void PluginEditor::setupForm2SequencerArea()
{
    DBG("[UI] Setting up Form 2 sequencer area...");
    
    // Sequencer area bounds (EXACT same as other pages)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    form2StepTitle = std::make_unique<juce::Label>();
    form2StepTitle->setText("STEP", juce::dontSendNotification);
    form2StepTitle->setFont(juce::Font(22.118f, juce::Font::bold));
    form2StepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    form2StepTitle->setJustificationType(juce::Justification::centredLeft);
    form2StepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    addAndMakeVisible(form2StepTitle.get());
    form2StepTitle->setVisible(false);
    
    // Create step buttons (2x8 grid)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        auto button = std::make_unique<StepButton>(i);
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        button->setBounds(x, y, buttonSize, buttonSize);
        button->onClick = [this, i]() { onForm2StepButtonClicked(i); };
        
        // Set step button images before adding
        if (assets.stepActive && assets.stepInactive) {
            button->setActiveImage(assets.stepActive->createCopy());
            button->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        addAndMakeVisible(button.get());
        button->setVisible(false);
        form2StepButtons[i] = std::move(button);
    }
    
    // Create step amount label (TextEditor)
    form2StepAmountLabel = std::make_unique<juce::TextEditor>();
    form2StepAmountLabel->setText("16");
    form2StepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    form2StepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    form2StepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    form2StepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    form2StepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    form2StepAmountLabel->setJustification(juce::Justification::centred);
    form2StepAmountLabel->setBorder(juce::BorderSize<int>(2));
    form2StepAmountLabel->setIndents(0, 0);
    form2StepAmountLabel->setInputRestrictions(2, "0123456789");
    form2StepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    addAndMakeVisible(form2StepAmountLabel.get());
    form2StepAmountLabel->setVisible(false);
    
    // Add callback for step amount changes
    form2StepAmountLabel->onTextChange = [this]() {
        int newStepsUsed = form2StepAmountLabel->getText().getIntValue();
        newStepsUsed = juce::jlimit(1, 16, newStepsUsed);
        processorRef.setForm2StepsUsed(newStepsUsed);
        updateForm2SequencerUI();
    };
    
    // Create rate dropdown
    form2RateDropdown = std::make_unique<juce::ComboBox>();
    form2RateDropdown->addItem("4", 1);
    form2RateDropdown->addItem("2", 2);
    form2RateDropdown->addItem("1", 3);
    form2RateDropdown->addItem("1/2", 4);
    form2RateDropdown->addItem("1/4", 5);
    form2RateDropdown->addItem("1/8", 6);
    form2RateDropdown->addItem("1/16", 7);
    form2RateDropdown->addItem("1/32", 8);
    form2RateDropdown->setSelectedId(5); // Default to 1/4
    form2RateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    form2RateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    form2RateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    form2RateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    form2RateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    form2RateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    addAndMakeVisible(form2RateDropdown.get());
    form2RateDropdown->setVisible(false);
    
    // Add callback for rate dropdown changes
    form2RateDropdown->onChange = [this]() {
        int selectedId = form2RateDropdown->getSelectedId();
        int divisionIndex = selectedId - 1; // Convert 1-based to 0-based
        processorRef.setForm2DivisionIndex(divisionIndex);
    };
    
    // Create STD toggle
    form2StdToggle = std::make_unique<CircularToggleButton>();
    form2StdToggle->setButtonText("-");
    form2StdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    addAndMakeVisible(form2StdToggle.get());
    form2StdToggle->setVisible(false);
    
    // Create step dice button
    form2StepDiceButton = std::make_unique<CustomDiceButton>();
    int stepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller
    form2StepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, stepDiceSize, stepDiceSize);
    if (assets.diceLarge != nullptr) {
        form2StepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    addAndMakeVisible(form2StepDiceButton.get());
    form2StepDiceButton->setVisible(false);
    
    // Create step power button
    form2StepPowerButton = std::make_unique<juce::DrawableButton>("Form2StepPower", juce::DrawableButton::ImageFitted);
    form2StepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    form2StepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    if (assets.stepPowerOn) {
        form2StepPowerButton->setImages(assets.stepPowerOn.get());
    }
    form2StepPowerButton->setClickingTogglesState(true);
    const int stepPowerSize = 40;
    form2StepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - stepPowerSize - 5 + 15 - 5 - 1, 
                                     sequencerArea.getY() - 5 - stepPowerSize + 25 + 5, stepPowerSize, stepPowerSize);
    addAndMakeVisible(form2StepPowerButton.get());
    form2StepPowerButton->setVisible(false);
    
    // Add callback for step power button
    form2StepPowerButton->onClick = [this]() {
        form2StepAreaEnabled = form2StepPowerButton->getToggleState();
        processorRef.setForm2SequencerEnabled(form2StepAreaEnabled);
        updateForm2StepAreaVisibility();
        DBG("[UI] Form 2 step power: " << (form2StepAreaEnabled ? "ON" : "OFF"));
    };
    
    // Add callback for step dice button
    form2StepDiceButton->onClick = [this]() {
        DBG("[FORM2] Step dice button clicked - randomizing all step snapshots");
        
        // Randomize step sequencer
        for (int i = 0; i < 16; ++i) {
            auto snapshot = processorRef.getForm2SafeSnapshot(i);
            // Randomize all parameters
            snapshot.form2.rootNote = juce::Random::getSystemRandom().nextInt(juce::Range(0, 12)); // 0-11
            snapshot.form2.scale = juce::Random::getSystemRandom().nextInt(juce::Range(0, 7)); // 0-6
            snapshot.form2.chordSize = juce::Random::getSystemRandom().nextInt(juce::Range(1, 9)); // 1-8
            snapshot.form2.shift = juce::Random::getSystemRandom().nextFloat() * 1.5f + 0.5f; // 0.5-2.0
            snapshot.form2.color = juce::Random::getSystemRandom().nextFloat() * 24.0f - 12.0f; // -12 to +12
            snapshot.form2.motion = juce::Random::getSystemRandom().nextFloat();
            snapshot.form2.resynth = juce::Random::getSystemRandom().nextFloat();
            snapshot.form2.mix = juce::Random::getSystemRandom().nextFloat();
            processorRef.setForm2StepSnapshot(i, snapshot);
        }
        
        // Update the UI to show the new random values
        updateForm2SequencerUI();
        
        // Update knob values to reflect the current step's randomized data
        if (form2UiSelectedStep >= 0 && form2UiSelectedStep < 16) {
            auto currentSnapshot = processorRef.getForm2SafeSnapshot(form2UiSelectedStep);
            
            // Update all knob values from the current step's snapshot
            if (form2Knobs[0]) form2Knobs[0]->setValue(currentSnapshot.form2.rootNote, juce::dontSendNotification);
            if (form2Knobs[1]) form2Knobs[1]->setValue(currentSnapshot.form2.scale, juce::dontSendNotification);
            if (form2Knobs[2]) form2Knobs[2]->setValue(currentSnapshot.form2.chordSize, juce::dontSendNotification);
            if (form2Knobs[3]) form2Knobs[3]->setValue(currentSnapshot.form2.shift, juce::dontSendNotification);
            if (form2Knobs[4]) form2Knobs[4]->setValue(currentSnapshot.form2.color, juce::dontSendNotification);
            if (form2Knobs[5]) form2Knobs[5]->setValue(currentSnapshot.form2.motion, juce::dontSendNotification);
            if (form2Knobs[6]) form2Knobs[6]->setValue(currentSnapshot.form2.resynth, juce::dontSendNotification);
            if (form2Knobs[7]) form2Knobs[7]->setValue(currentSnapshot.form2.mix, juce::dontSendNotification);
            
            // Update value labels to reflect the new knob values
            for (int j = 0; j < 8; ++j) {
                if (form2Knobs[j] && form2ValueLabels[j]) {
                    float value = form2Knobs[j]->getValue();
                    juce::String valueText;
                    
                    switch (j) {
                        case 0: // Morph X
                        case 1: // Morph Y
                            valueText = juce::String(value, 2); break;
                        case 2: // Sharpness
                            valueText = juce::String(value, 1); break;
                        case 3: valueText = juce::String(value, 1) + " dB"; break; // Emphasis
                        case 4: // Shift
                        case 5: // Motion
                        case 6: // Air
                        case 7: // Mix
                            valueText = juce::String(value, 2); break;
                    }
                    
                    form2ValueLabels[j]->setText(valueText, juce::dontSendNotification);
                }
            }
        }
        
        DBG("[FORM2] Step randomization complete - UI updated");
    };
    
    DBG("[UI] Form 2 sequencer area setup complete");
}

void PluginEditor::setupForm2AllStepsToggle()
{
    DBG("[UI] Setting up Form 2 all steps toggle...");
    
    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    form2AllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(form2AllStepsToggle.get());
    form2AllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    form2AllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                     effectArea.getY() - 1, buttonSize, buttonSize);
    
    if (assets.stepTopInactive && assets.stepTopActive) {
        static_cast<AllStepsToggleButton*>(form2AllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    form2AllStepsToggle->setToggleState(false, juce::dontSendNotification);
    form2AllStepsToggle->onClick = [this]() {
        form2AllStepsEnabled = form2AllStepsToggle->getToggleState();
        DBG("[UI] Form 2 All Steps toggle: " << (form2AllStepsEnabled ? "ON" : "OFF"));
        form2AllStepsLabel->setAlpha(form2AllStepsEnabled ? 1.0f : 0.5f);
    };
    
    form2AllStepsLabel = std::make_unique<juce::Label>();
    form2AllStepsLabel->setText("All Steps", juce::dontSendNotification);
    form2AllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    form2AllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    form2AllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(form2AllStepsLabel.get());
    form2AllStepsLabel->setVisible(false);
    form2AllStepsLabel->setAlpha(1.0f);
    form2AllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                   effectArea.getY() + 1, 80, 24);
    
    form2Group.push_back(form2AllStepsToggle.get());
    form2Group.push_back(form2AllStepsLabel.get());
    
    DBG("[UI] Form 2 all steps toggle setup complete");
    
    // Populate Form 2 group
    form2Group.clear();
    for (int i = 0; i < 8; ++i) {
        if (form2Knobs[i]) form2Group.push_back(form2Knobs[i].get());
        if (form2KnobLabels[i]) form2Group.push_back(form2KnobLabels[i].get());
        if (form2ValueLabels[i]) form2Group.push_back(form2ValueLabels[i].get());
        if (form2IndicatorBars[i]) form2Group.push_back(form2IndicatorBars[i].get());
        if (form2LockButtons[i]) form2Group.push_back(form2LockButtons[i].get());
    }
    if (form2EffectsTitle) form2Group.push_back(form2EffectsTitle.get());
    if (form2DiceButton) form2Group.push_back(form2DiceButton.get());
    if (form2FxPowerButton) form2Group.push_back(form2FxPowerButton.get());
    if (form2StepTitle) form2Group.push_back(form2StepTitle.get());
    if (form2StepDiceButton) form2Group.push_back(form2StepDiceButton.get());
    if (form2StepAmountLabel) form2Group.push_back(form2StepAmountLabel.get());
    if (form2RateDropdown) form2Group.push_back(form2RateDropdown.get());
    if (form2StdToggle) form2Group.push_back(form2StdToggle.get());
    if (form2StepPowerButton) form2Group.push_back(form2StepPowerButton.get());
    if (form2AllStepsToggle) form2Group.push_back(form2AllStepsToggle.get());
    if (form2AllStepsLabel) form2Group.push_back(form2AllStepsLabel.get());
    for (int i = 0; i < 16; ++i) {
        if (form2StepButtons[i]) form2Group.push_back(form2StepButtons[i].get());
    }
    
    DBG("[UI] Form 2 page setup complete");
}

void PluginEditor::updateDubDelayTimeLabel()
{
    if (!dubdelayValueLabels[0]) return;
    
    auto* syncParam = processorRef.getAPVTS().getRawParameterValue("dubSync");
    bool syncEnabled = syncParam ? (*syncParam > 0.5f) : false;
    
    if (syncEnabled) {
        // Show division label
        auto* divParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("dubTimeDiv"));
        auto* gridParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter("dubTimeGrid"));
        
        int divIdx = divParam ? divParam->getIndex() : 4; // Default 1/4
        int gridIdx = gridParam ? gridParam->getIndex() : 0; // Default Straight
        
        divIdx = juce::jlimit(0, 8, divIdx);
        gridIdx = juce::jlimit(0, 2, gridIdx);
        
        static const char* divStrings[] = {"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
        juce::String label = divStrings[divIdx];
        
        if (gridIdx == 1) label += "."; // Dotted
        else if (gridIdx == 2) label += "T"; // Triplet
        
        dubdelayValueLabels[0]->setText(label, juce::dontSendNotification);
    } else {
        // Show ms
        if (dubdelayKnobs[0]) {
            float timeMs = dubdelayKnobs[0]->getValue();
            dubdelayValueLabels[0]->setText(juce::String(int(timeMs)) + "ms", juce::dontSendNotification);
        }
    }
}

void PluginEditor::updateDubDelayParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8) return;
    if (!dubdelayKnobs[knobIndex]) return;
    
    float value = dubdelayKnobs[knobIndex]->getValue();
    
    // Update current step snapshot (All Steps logic is now handled in knob change handler)
    updateDubDelayCurrentStepSnapshot(knobIndex, value);
}

void PluginEditor::updateDubDelayCurrentStepSnapshot(int knobIndex, float value)
{
    // This calls the processor method to update the current step snapshot
    processorRef.updateDubDelayCurrentStepSnapshot(knobIndex, value);
}

void PluginEditor::onDubDelayStepButtonClicked(int stepIndex)
{
    dubdelayUiSelectedStep = stepIndex;
    processorRef.setDubDelaySelectedStep(stepIndex);
    
    // Update button states
    for (int i = 0; i < 16; ++i) {
        if (dubdelayStepButtons[i]) {
            dubdelayStepButtons[i]->setSelected(i == stepIndex);
        }
    }
    
    // Load snapshot into knobs
    auto snapshot = processorRef.getDubDelaySafeSnapshot(stepIndex);
    
    // Temporarily disable All Steps to prevent overwriting all steps
    bool wasAllSteps = dubdelayAllStepsEnabled;
    dubdelayAllStepsEnabled = false;
    
    if (dubdelayKnobs[0]) dubdelayKnobs[0]->setValue(snapshot.dubdelay.timeMs, juce::dontSendNotification);
    if (dubdelayKnobs[1]) dubdelayKnobs[1]->setValue(snapshot.dubdelay.feedback, juce::dontSendNotification);
    if (dubdelayKnobs[2]) dubdelayKnobs[2]->setValue(snapshot.dubdelay.toneHz, juce::dontSendNotification);
    if (dubdelayKnobs[3]) dubdelayKnobs[3]->setValue(snapshot.dubdelay.drive, juce::dontSendNotification);
    if (dubdelayKnobs[4]) dubdelayKnobs[4]->setValue(snapshot.dubdelay.pingPong ? 1.0f : 0.0f, juce::dontSendNotification);
    if (dubdelayKnobs[5]) dubdelayKnobs[5]->setValue(snapshot.dubdelay.wowFlutter, juce::dontSendNotification);
    if (dubdelayKnobs[6]) dubdelayKnobs[6]->setValue(snapshot.dubdelay.regenDamp, juce::dontSendNotification);
    // Mix (knob 7) is global - load from APVTS, not snapshot
    
    // Manually update value labels since dontSendNotification prevents onValueChange callbacks
    for (int i = 0; i < 7; ++i) { // Only update first 7 knobs (Mix is global)
        if (dubdelayKnobs[i] && dubdelayValueLabels[i]) {
            float value = dubdelayKnobs[i]->getValue();
            juce::String valueText;
            switch (i) {
                case 0: valueText = juce::String(int(value)) + "ms"; break; // Time
                case 1: valueText = juce::String(int(value * 100)) + "%"; break; // Feedback
                case 2: valueText = juce::String(int(value)) + "Hz"; break; // Tone
                case 3: valueText = juce::String(int(value * 100)) + "%"; break; // Drive
                case 4: valueText = snapshot.dubdelay.pingPong ? "On" : "Off"; break; // PingPong
                case 5: valueText = juce::String(int(value * 100)) + "%"; break; // WowFlutter
                case 6: valueText = juce::String(int(value * 100)) + "%"; break; // RegenDamp
            }
            dubdelayValueLabels[i]->setText(valueText, juce::dontSendNotification);
        }
    }
    
    dubdelayAllStepsEnabled = wasAllSteps;
}

void PluginEditor::updateDubDelaySequencerUI()
{
    int selectedStep = dubdelayUiSelectedStep;
    int playingStep = processorRef.getDubDelayCurrentStep();
    const int stepsUsed = processorRef.getDubDelaySeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (dubdelayStepButtons[i] != nullptr) {
            dubdelayStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getDubDelaySeqState().enabled.load();
            dubdelayStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            bool shouldBeEnabled = i < stepsUsed;
            dubdelayStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't overwrite if user is editing)
    if (dubdelayStepAmountLabel != nullptr && !dubdelayStepAmountLabel->hasKeyboardFocus(true)) {                                                               
        juce::String currentText = dubdelayStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            dubdelayStepAmountLabel->setText(newText, false);
        }
    }
    
    repaint();
}


//==============================================================================
// Redux Page Setup Methods
//==============================================================================

void PluginEditor::setupReduxKnobs()
{
    // Redux knob titles for bitcrusher controls
    const juce::StringArray reduxKnobTitles = {
        "Bit Depth",
        "Rate",
        "Jitter",
        "Pre Filter",
        "Post Filter",
        "Drive",
        "Emphasis",
        "Mix"
    };
    
    // Redux parameter IDs (must match APVTS order)
    const juce::StringArray reduxParamIDs = {
        "reduxBitDepth",
        "reduxSampleRateReduction",
        "reduxJitter",
        "reduxPreFilter",
        "reduxPostFilter",
        "reduxDrive",
        "reduxEmphasis",
        "reduxMix"
    };
    
    DBG("[UI] Setting up Redux knobs...");

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    // Create and setup knobs
    for (int i = 0; i < 8; ++i)
    {
        // Position 8 knobs in 2 rows of 4 (EXACT same as other effects)
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px (EXACT same as other effects)
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1; // Moved up 6px from +5 to -1
        
        // Create knob
        reduxKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(reduxKnobs[i].get());
        reduxKnobs[i]->setVisible(false);
        reduxKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        reduxKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        // Set knob ranges based on parameter (UI order)
        switch (i) {
            case 0: // Bit Depth (1-12 range)
                reduxKnobs[i]->setRange(1.0, 12.0, 1.0);
                reduxKnobs[i]->setValue(8.0, juce::dontSendNotification); // Default to 8 bits
                break;
            case 1: // Sample Rate (1-32)
                reduxKnobs[i]->setRange(1.0, 32.0, 1.0);
                reduxKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 2: // Jitter (0-1)
                reduxKnobs[i]->setRange(0.0, 1.0, 0.01);
                reduxKnobs[i]->setValue(0.0, juce::dontSendNotification);
                break;
            case 3: // Pre Filter (20-20000 Hz, log scale)
                reduxKnobs[i]->setRange(20.0, 20000.0, 1.0);
                reduxKnobs[i]->setSkewFactorFromMidPoint(1000.0);
                reduxKnobs[i]->setValue(20000.0, juce::dontSendNotification);
                break;
            case 4: // Post Filter (20-20000 Hz, log scale)
                reduxKnobs[i]->setRange(20.0, 20000.0, 1.0);
                reduxKnobs[i]->setSkewFactorFromMidPoint(1000.0);
                reduxKnobs[i]->setValue(20000.0, juce::dontSendNotification);
                break;
            case 5: // Drive (0-10)
                reduxKnobs[i]->setRange(0.0, 10.0, 0.01);
                reduxKnobs[i]->setValue(1.0, juce::dontSendNotification);
                break;
            case 6: // Emphasis (0-1)
                reduxKnobs[i]->setRange(0.0, 1.0, 0.01);
                reduxKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                reduxKnobs[i]->setRange(0.0, 1.0, 0.01);
                reduxKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
        }
        
        // Set knob images (CRITICAL - this makes them look like proper knobs!)
        if (assets.knobRing != nullptr)
            reduxKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            reduxKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        // Position knob
        reduxKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label
        reduxKnobLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(reduxKnobLabels[i].get());
        reduxKnobLabels[i]->setVisible(false);
        reduxKnobLabels[i]->setText(reduxKnobTitles[i], juce::dontSendNotification);
        reduxKnobLabels[i]->setJustificationType(juce::Justification::centred);
        reduxKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        reduxKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        reduxKnobLabels[i]->setBounds(x, y - 15, knobSize, 20); // Moved down 5px from -20 to -15 (same as other effects)
        
        // Create value label
        reduxValueLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(reduxValueLabels[i].get());
        reduxValueLabels[i]->setVisible(false);
        reduxValueLabels[i]->setText("0", juce::dontSendNotification);
        reduxValueLabels[i]->setJustificationType(juce::Justification::centred);
        reduxValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        reduxValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        reduxValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15); // Same as other effects
        
        // Create indicator bar
        reduxIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(reduxIndicatorBars[i].get());
        reduxIndicatorBars[i]->setVisible(false);
        reduxIndicatorBars[i]->setValue(0.5f);
        reduxIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13); // Same as other effects
        
        // Create dice button
        reduxDiceButtons[i] = std::make_unique<CustomDiceButton>();
        addAndMakeVisible(reduxDiceButtons[i].get());
        reduxDiceButtons[i]->setVisible(false);
        
        // Create lock button (positioned at end of title text like other effects)
        reduxLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(reduxLockButtons[i].get());
        reduxLockButtons[i]->setVisible(false);
        
        // Position lock button at end of title text (same as other effects)
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(reduxKnobTitles[i]);
        const int lockSize = 10; // Same size as other effects
        const int lockSpacing = 5; // Fixed distance from end of title text
        int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
        int lockY = y - 10; // Same position as other effects
        
        reduxLockButtons[i]->setBounds(lockX, lockY, lockSize, lockSize);
        
        // Set lock button images
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            reduxLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        reduxLockButtons[i]->setToggleState(reduxKnobLocked[i], juce::dontSendNotification);
        reduxLockButtons[i]->onClick = [this, i]() {
            reduxKnobLocked[i] = reduxLockButtons[i]->getToggleState();
            // DBG("[UI] Redux knob " << i << " lock: " << (reduxKnobLocked[i] ? "LOCKED" : "UNLOCKED"));
        };
        
        // Add value change callback to update value label
        reduxKnobs[i]->onValueChange = [this, i]() {
            if (reduxKnobs[i] && reduxValueLabels[i]) {
                float value = reduxKnobs[i]->getValue();
                juce::String valueText;
                
                switch (i) {
                    case 0: valueText = juce::String((int)value + 3); break; // Bit Depth (UI 1-12 maps to 4-16)
                    case 1: valueText = juce::String((int)value); break; // Sample Rate
                    case 2: valueText = juce::String(value, 2); break; // Jitter
                    case 3: valueText = juce::String((int)value) + " Hz"; break; // Pre Filter
                    case 4: valueText = juce::String((int)value) + " Hz"; break; // Post Filter
                    case 5: valueText = juce::String(value, 2); break; // Drive
                    case 6: valueText = juce::String(value, 2); break; // Emphasis
                    case 7: valueText = juce::String(value, 2); break; // Mix
                }
                
                reduxValueLabels[i]->setText(valueText, juce::dontSendNotification);
                
                // Update parameter in processor
                if (i == 0) {
                    // Use setValueNotifyingHost to avoid clicks for bit depth
                    auto* param = processorRef.getAPVTS().getParameter("reduxBitDepth");
                    if (param != nullptr) {
                        param->setValueNotifyingHost(value / 12.0f);
                    }
                    processorRef.updateReduxCurrentStepSnapshot(i, value);
                } else {
                    updateReduxParameterFromKnob(i);
                }
                
                // Update all steps if enabled
                if (reduxAllStepsEnabled) {
                    for (int step = 0; step < 16; ++step) {
                        StepSnapshot snapshot = processorRef.getReduxSafeSnapshot(step);
                        switch (i) {
                            case 0: {
                                // Convert UI value (1-12) to internal value (4-16)
                                int internalBitDepth = (int)value + 3;
                                snapshot.redux.bitDepth = internalBitDepth;
                                break;
                            }
                            case 1: snapshot.redux.sampleRateReduction = (int)value; break;
                            case 2: snapshot.redux.jitter = value; break;
                            case 3: snapshot.redux.preFilter = value; break;
                            case 4: snapshot.redux.postFilter = value; break;
                            case 5: snapshot.redux.drive = value; break;
                            case 6: snapshot.redux.emphasis = value; break;
                            case 7: snapshot.redux.mix = value; break;
                        }
                        processorRef.setReduxStepSnapshot(step, snapshot);
                    }
                    DBG("[UI] Redux All Steps: Updated all 16 steps with knob " << i << " value " << value);
                }
                
                if (reduxIndicatorBars[i]) {
                    float normValue = 0.5f;
                    switch (i) {
                        case 0: normValue = (value - 1.0f) / 11.0f; break; // Bit depth UI 1-12
                        case 1: normValue = (value - 1.0f) / 31.0f; break; // Sample rate 1-32
                        case 2: case 6: case 7: 
                            normValue = value; break; // 0-1 params (Jitter, Emphasis, Mix)
                        case 3: case 4: 
                            normValue = (value - 20.0f) / 19980.0f; break; // Filters 20-20000
                        case 5: normValue = value / 10.0f; break; // Drive 0-10
                    }
                    reduxIndicatorBars[i]->setValue(normValue);
                }
            }
        };
        
        // Attach to parameter (skip bit depth to avoid range conflicts)
        if (i == 0) {
            // Bit depth handled manually to avoid APVTS range conflicts
            reduxAttachments[i] = nullptr;
            DBG("[Redux] Bit depth knob handled manually (no APVTS attachment)");
        } else {
            auto* param = processorRef.getAPVTS().getParameter(reduxParamIDs[i]);
            if (param != nullptr) {
                reduxAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processorRef.getAPVTS(), reduxParamIDs[i], *reduxKnobs[i]);
                DBG("[Redux] Successfully attached knob " << i << " to parameter " << reduxParamIDs[i]);
            } else {
                DBG("[Redux] ERROR: Parameter " << reduxParamIDs[i] << " not found!");
                reduxAttachments[i] = nullptr;
            }
        }
        
        DBG("[Redux] Created knob " << i << ": " << reduxKnobTitles[i] << " -> " << reduxParamIDs[i]);
    }
    
    DBG("[Redux] Redux knobs setup complete");
}

void PluginEditor::setupReduxEffectsArea()
{
    DBG("[UI] Setting up Redux effects area...");
    
    // Effect area bounds (match other pages exactly)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title label (ALWAYS "EFFECT", NOT the effect name!)
    reduxEffectsTitle = std::make_unique<juce::Label>();
    reduxEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    reduxEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    reduxEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    reduxEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(reduxEffectsTitle.get());
    reduxEffectsTitle->setVisible(false);
    reduxEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    
    // FX Power button
    reduxFxPowerButton = std::make_unique<juce::DrawableButton>("reduxPower", juce::DrawableButton::ImageFitted);
    addAndMakeVisible(reduxFxPowerButton.get());
    reduxFxPowerButton->setVisible(false);
    reduxFxPowerButton->setClickingTogglesState(true);
    
    // Make button background transparent (match other pages)
    reduxFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    reduxFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.fxPowerOn) {
        reduxFxPowerButton->setImages(assets.fxPowerOn.get());
    }
    
    const int buttonSize = 46;
    reduxFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                 effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    
    reduxFxPowerButton->onClick = [this]() {
        reduxFxAreaEnabled = reduxFxPowerButton->getToggleState();
        
        // Update APVTS parameter
        auto* param = processorRef.getAPVTS().getParameter("reduxEnabled");
        if (param) {
            param->setValueNotifyingHost(reduxFxAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateReduxFxAreaVisibility();
        DBG("[UI] Redux FX power: " << (reduxFxAreaEnabled ? "ON" : "OFF"));
    };
    
    // Main dice button (randomize all unlocked knobs)
    reduxDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(reduxDiceButton.get());
    reduxDiceButton->setVisible(false);
    
    const int diceSize = 32;
    reduxDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    if (assets.diceLarge != nullptr) {
        reduxDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    reduxDiceButton->onClick = [this]() {
        randomizeReduxKnobValues();
    };
    
    DBG("[UI] Redux effects area setup complete");
}

void PluginEditor::setupCompressSliders()
{
    DBG("[UI] Setting up COMPRESS+ sliders...");
    
    // Setup gain reduction meter - horizontal above sliders
    gainReductionMeter = std::make_unique<GainReductionMeter>();
    gainReductionMeter->setBounds(460 + 20 + 15 + 10 - 4 + 1, 282 + 20 + 40 + 20 + 110 + 10, 410, 15); // Full width above sliders, moved down 40px + 20px + 110px + 10px, 100px longer, 5px shorter, 10px narrower, 22px right
    addChildComponent(gainReductionMeter.get());
    gainReductionMeter->setVisible(false); // Initially hidden
    
    
    // COMPRESS+ slider titles (Top row: Compressor controls, Bottom row: Drive/Lofi/Makeup Gain/Wet)
    const juce::StringArray compressSliderTitles = {
        "Threshold",
        "Attack", 
        "Release",
        "Ratio",
        "Drive",
        "Lofi",
        "Makeup", 
        "Wet"
    };
    
    // COMPRESS+ parameter IDs
    const juce::StringArray compressParamIDs = {
        "compressThreshold",
        "compressAttack",
        "compressRelease", 
        "compressRatio",
        "compressDrive",
        "compressLofi",
        "compressMakeupGain",
        "compressWet"
    };
    
    // Position sliders in the overlay area (2 rows of 4)
    auto overlayBounds = juce::Rectangle<int>(460, 282, 495, 460); // Master area bounds
    const int sliderWidth = 100;
    const int sliderHeight = 20;
    const int sliderSpacing = 20;
    const int rowSpacing = 60;
    const int startX = overlayBounds.getX() + 20 - 3; // Moved 30px left from 50, then 3px more left
    const int startY = overlayBounds.getY() + 100 + 40 - 10 - 50 + 20; // Moved down 40px, then up 10px, then up 50px more, then down 20px
    
    // Create sliders and labels
    juce::Slider* sliders[] = {
        compressThresholdSlider.get(),
        compressAttackSlider.get(), 
        compressReleaseSlider.get(),
        compressRatioSlider.get(),
        compressDriveSlider.get(),
        compressLofiSlider.get(),
        compressMakeupGainSlider.get(),
        compressWetSlider.get()
    };
    
    juce::Label* labels[] = {
        compressThresholdLabel.get(),
        compressAttackLabel.get(),
        compressReleaseLabel.get(),
        compressRatioLabel.get(),
        compressDriveLabel.get(),
        compressLofiLabel.get(),
        compressMakeupGainLabel.get(),
        compressWetLabel.get()
    };
    
    juce::Label* valueLabels[] = {
        compressThresholdValueLabel.get(),
        compressAttackValueLabel.get(),
        compressReleaseValueLabel.get(),
        compressRatioValueLabel.get(),
        compressDriveValueLabel.get(),
        compressLofiValueLabel.get(),
        compressMakeupGainValueLabel.get(),
        compressWetValueLabel.get()
    };
    
    // Create and setup all sliders
    for (int i = 0; i < 8; ++i)
    {
        // Calculate position (2 rows of 4)
        int x = startX + (i % 4) * (sliderWidth + sliderSpacing);
        int y = startY + (i / 4) * rowSpacing + (i < 4 ? 12 : 0); // Top 4 sliders moved down 12px
        
        // Create slider
        auto slider = std::make_unique<CompressSlider>();
        
        // Set slider ranges based on parameter type
        if (compressParamIDs[i] == "compressThreshold") {
            slider->setRange(-60.0, 0.0, 0.1); // -60dB to 0dB threshold
        } else if (compressParamIDs[i] == "compressAttack") {
            slider->setRange(0.1, 100.0, 0.1); // 0.1ms to 100ms attack
        } else if (compressParamIDs[i] == "compressRelease") {
            slider->setRange(10.0, 1000.0, 1.0); // 10ms to 1000ms release
        } else if (compressParamIDs[i] == "compressRatio") {
            slider->setRange(1.0, 20.0, 0.1); // 1:1 to 20:1 ratio
        } else if (compressParamIDs[i] == "compressDrive") {
            slider->setRange(0.0, 30.0, 0.1); // 0-30dB drive
        } else if (compressParamIDs[i] == "compressMakeupGain") {
            slider->setRange(-24.0, 24.0, 0.1); // -24dB to +24dB makeup gain
        } else {
            slider->setRange(0.0, 1.0, 0.01); // 0-1 for other parameters
        }
        
        slider->setBounds(x, y, sliderWidth, sliderHeight);
        slider->setVisible(false); // Initially hidden
        slider->setEnabled(false); // Initially disabled
        addChildComponent(slider.get()); // Use addChildComponent instead of addAndMakeVisible
        
        // Create parameter attachment and store it
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        switch(i) {
            case 0: compressThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 1: compressAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 2: compressReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 3: compressRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 4: compressDriveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 5: compressLofiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 6: compressMakeupGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
            case 7: compressWetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.getAPVTS(), compressParamIDs[i], *slider); break;
        }
            
        // Create title label
        auto label = std::make_unique<juce::Label>();
        label->setText(compressSliderTitles[i], juce::dontSendNotification);
        label->setFont(juce::Font(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack); // Remove black background
        label->setJustificationType(juce::Justification::centredLeft);
        label->setBounds(x, y - 20, sliderWidth, 16);
        label->setVisible(false); // Initially hidden
        label->setEnabled(false); // Initially disabled
        addChildComponent(label.get()); // Use addChildComponent instead of addAndMakeVisible
        
        // Create value label
        auto valueLabel = std::make_unique<juce::Label>();
        valueLabel->setText("0.0", juce::dontSendNotification);
        valueLabel->setFont(juce::Font(10.0f, juce::Font::bold));
        valueLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        valueLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack); // Remove black background
        valueLabel->setJustificationType(juce::Justification::centred);
        valueLabel->setBounds(x + sliderWidth - 40 + 6, y - 20, 40, 16); // Moved 6px right
        valueLabel->setVisible(false); // Initially hidden
        valueLabel->setEnabled(false); // Initially disabled
        addChildComponent(valueLabel.get()); // Use addChildComponent instead of addAndMakeVisible
        
        // Add value change listener to update the value label
        slider->onValueChange = [this, i, slider = slider.get(), valueLabel = valueLabel.get()]() {
            if (valueLabel) {
                float value = slider->getValue();
                juce::String valueText;
                
                // Format value based on parameter type
                const juce::StringArray paramIDs = {
                    "compressThreshold", "compressAttack", "compressRelease", "compressRatio",
                    "compressDrive", "compressLofi", "compressMakeupGain", "compressWet"
                };
                
                if (paramIDs[i] == "compressThreshold") {
                    valueText = juce::String(value, 1) + " dB";
                } else if (paramIDs[i] == "compressAttack") {
                    valueText = juce::String(value, 1) + " ms";
                } else if (paramIDs[i] == "compressRelease") {
                    valueText = juce::String(value, 0) + " ms";
                } else if (paramIDs[i] == "compressRatio") {
                    valueText = juce::String(value, 1) + ":1";
                } else if (paramIDs[i] == "compressDrive") {
                    valueText = juce::String(value, 1) + " dB";
                } else if (paramIDs[i] == "compressMakeupGain") {
                    valueText = juce::String(value, 1) + " dB";
                } else {
                    valueText = juce::String(value, 2);
                }
                
                valueLabel->setText(valueText, juce::dontSendNotification);
            }
        };
        
        // Store components
        sliders[i] = slider.get();
        labels[i] = label.get();
        valueLabels[i] = valueLabel.get();
        
        // Store unique_ptrs
        switch(i) {
            case 0: compressDriveSlider = std::move(slider); compressDriveLabel = std::move(label); compressDriveValueLabel = std::move(valueLabel); break;
            case 1: compressThresholdSlider = std::move(slider); compressThresholdLabel = std::move(label); compressThresholdValueLabel = std::move(valueLabel); break;
            case 2: compressAttackSlider = std::move(slider); compressAttackLabel = std::move(label); compressAttackValueLabel = std::move(valueLabel); break;
            case 3: compressReleaseSlider = std::move(slider); compressReleaseLabel = std::move(label); compressReleaseValueLabel = std::move(valueLabel); break;
            case 4: compressRatioSlider = std::move(slider); compressRatioLabel = std::move(label); compressRatioValueLabel = std::move(valueLabel); break;
            case 5: compressLofiSlider = std::move(slider); compressLofiLabel = std::move(label); compressLofiValueLabel = std::move(valueLabel); break;
            case 6: compressMakeupGainSlider = std::move(slider); compressMakeupGainLabel = std::move(label); compressMakeupGainValueLabel = std::move(valueLabel); break;
            case 7: compressWetSlider = std::move(slider); compressWetLabel = std::move(label); compressWetValueLabel = std::move(valueLabel); break;
        }
    }
    
    DBG("[UI] COMPRESS+ sliders setup complete");
    
    // Set initial values for all labels
    updateCompressValueLabels();
}

void PluginEditor::updateCompressValueLabels()
{
    // Update all COMPRESS+ value labels with current slider values
    const juce::StringArray compressParamIDs = {
        "compressThreshold", "compressAttack", "compressRelease", "compressRatio",
        "compressDrive", "compressLofi", "compressMakeupGain", "compressWet"
    };
    
    const std::vector<juce::Slider*> sliders = {
        compressThresholdSlider.get(), compressAttackSlider.get(), compressReleaseSlider.get(), compressRatioSlider.get(),
        compressDriveSlider.get(), compressLofiSlider.get(), compressMakeupGainSlider.get(), compressWetSlider.get()
    };
    
    const std::vector<juce::Label*> labels = {
        compressThresholdValueLabel.get(), compressAttackValueLabel.get(), compressReleaseValueLabel.get(), compressRatioValueLabel.get(),
        compressDriveValueLabel.get(), compressLofiValueLabel.get(), compressMakeupGainValueLabel.get(), compressWetValueLabel.get()
    };
    
    for (int i = 0; i < 8; ++i) {
        if (sliders[i] && labels[i]) {
            float value = sliders[i]->getValue();
            juce::String valueText;
            
            // Format value based on parameter type
            if (compressParamIDs[i] == "compressThreshold") {
                valueText = juce::String(value, 1) + " dB";
            } else if (compressParamIDs[i] == "compressAttack") {
                valueText = juce::String(value, 1) + " ms";
            } else if (compressParamIDs[i] == "compressRelease") {
                valueText = juce::String(value, 0) + " ms";
            } else if (compressParamIDs[i] == "compressRatio") {
                valueText = juce::String(value, 1) + ":1";
            } else if (compressParamIDs[i] == "compressDrive") {
                valueText = juce::String(value, 1) + " dB";
            } else if (compressParamIDs[i] == "compressMakeupGain") {
                valueText = juce::String(value, 1) + " dB";
            } else {
                valueText = juce::String(value, 2);
            }
            
            labels[i]->setText(valueText, juce::dontSendNotification);
        }
    }
}

void PluginEditor::setupReduxSequencerArea()
{
    DBG("[UI] Setting up Redux sequencer area...");
    
    // Sequencer area bounds (EXACT same as other pages)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    reduxStepTitle = std::make_unique<juce::Label>();
    reduxStepTitle->setText("STEP", juce::dontSendNotification);
    reduxStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    reduxStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    reduxStepTitle->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(reduxStepTitle.get());
    reduxStepTitle->setVisible(false);
    reduxStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    
    // Create step buttons (2 rows of 8)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
        reduxStepButtons[i] = std::make_unique<StepButton>(i);
        addAndMakeVisible(reduxStepButtons[i].get());
        reduxStepButtons[i]->setVisible(false);
        
        int x = startX + (i % 8) * (buttonSize + buttonSpacing);
        int y = startY + (i / 8) * (buttonSize + buttonSpacing);
        
        reduxStepButtons[i]->setBounds(x, y, buttonSize, buttonSize);
        
        if (assets.stepActive) {
            reduxStepButtons[i]->setActiveImage(assets.stepActive->createCopy());
        }
        if (assets.stepInactive) {
            reduxStepButtons[i]->setInactiveImage(assets.stepInactive->createCopy());
        }
        
        // Wire up step button click handler
        reduxStepButtons[i]->onClick = [this, i]() {
            onReduxStepButtonClicked(i);
        };
    }
    
    // Create step amount editor
    reduxStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setReduxStepsUsed(16);
    reduxStepAmountLabel->setText("16");
    reduxStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    reduxStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    reduxStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    reduxStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    reduxStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    reduxStepAmountLabel->setJustification(juce::Justification::centred);
    reduxStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    reduxStepAmountLabel->setIndents(0, 0);
    reduxStepAmountLabel->setInputRestrictions(2, "0123456789");
    reduxStepAmountLabel->setReadOnly(true); // Read-only for now as Redux uses fixed 16-step patterns
    addAndMakeVisible(reduxStepAmountLabel.get());
    reduxStepAmountLabel->setVisible(false);
    reduxStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    
    // Create rate dropdown
    reduxRateDropdown = std::make_unique<juce::ComboBox>();
    reduxRateDropdown->addItem("4", 1);
    reduxRateDropdown->addItem("2", 2);
    reduxRateDropdown->addItem("1", 3);
    reduxRateDropdown->addItem("1/2", 4);
    reduxRateDropdown->addItem("1/4", 5);
    reduxRateDropdown->addItem("1/8", 6);
    reduxRateDropdown->addItem("1/16", 7);
    reduxRateDropdown->addItem("1/32", 8);
    reduxRateDropdown->setSelectedId(4); // Default to 1/8
    reduxRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    reduxRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    reduxRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    reduxRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    reduxRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    reduxRateDropdown->onChange = [this]() {
        if (reduxRateDropdown) {
            int selectedId = reduxRateDropdown->getSelectedId();
            DBG("[UI] Redux rate changed to: " << selectedId);
            // TODO: Update processor with new rate
        }
    };
    addAndMakeVisible(reduxRateDropdown.get());
    reduxRateDropdown->setVisible(false);
    reduxRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 60, 25);
    
    // Create std toggle (EXACT same as other effects)
    reduxStdToggle = std::make_unique<CircularToggleButton>();
    reduxStdToggle->setButtonText("-");
    addAndMakeVisible(reduxStdToggle.get());
    reduxStdToggle->setVisible(false);
    reduxStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    
    // Set up toggle handler (EXACT same as other effects)
    reduxStdToggle->onClick = [this]() {
        // Cycle through -/t/. states
        static int stdState = 0; // 0=-, 1=t, 2=.
        stdState = (stdState + 1) % 3;
        
        switch (stdState) {
            case 0: reduxStdToggle->setButtonText("-"); break;
            case 1: reduxStdToggle->setButtonText("t"); break;
            case 2: reduxStdToggle->setButtonText("."); break;
        }
    };
    
    // Create step dice button (EXACT same positioning as other effects)
    reduxStepDiceButton = std::make_unique<CustomDiceButton>();
    addAndMakeVisible(reduxStepDiceButton.get());
    reduxStepDiceButton->setVisible(false);
    int reduxStepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller than 35px = ~24px
    reduxStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, reduxStepDiceSize, reduxStepDiceSize);
    
    // Set up step dice button SVG (EXACT same as other effects)
    if (assets.diceLarge != nullptr)
    {
        reduxStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    reduxStepDiceButton->onClick = [this]() {
        DBG("[UI] Redux step dice clicked - randomizing all steps");
        
        // Randomize all 16 steps for Redux
        for (int step = 0; step < 16; ++step) {
            auto snapshot = processorRef.getReduxSafeSnapshot(step);
            
            // Randomize Redux parameters (respecting locked knobs)
            if (!reduxKnobLocked[0]) {
                // Generate UI value (1-12) then convert to internal value (4-16)
                int uiBitDepth = 1 + juce::Random::getSystemRandom().nextInt(12); // 1-12
                snapshot.redux.bitDepth = uiBitDepth + 3; // Convert to 4-16
            }
            if (!reduxKnobLocked[1]) snapshot.redux.sampleRateReduction = 1 + juce::Random::getSystemRandom().nextInt(32); // 1-32
            if (!reduxKnobLocked[2]) snapshot.redux.jitter = juce::Random::getSystemRandom().nextFloat();
            if (!reduxKnobLocked[3]) snapshot.redux.preFilter = 20.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 20.0f);
            if (!reduxKnobLocked[4]) snapshot.redux.postFilter = 20.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 20.0f);
            if (!reduxKnobLocked[5]) snapshot.redux.drive = juce::Random::getSystemRandom().nextFloat() * 10.0f;
            if (!reduxKnobLocked[6]) snapshot.redux.emphasis = juce::Random::getSystemRandom().nextFloat();
            if (!reduxKnobLocked[7]) snapshot.redux.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setReduxStepSnapshot(step, snapshot);
        }
        
        // Update UI to show the randomized values for the current step
        updateReduxSequencerUI();
        loadSelectedStepIntoKnobs(FxPageID::Redux);
        
        DBG("[UI] Redux all steps randomized");
    };
    
    // Create step power button
    reduxStepPowerButton = std::make_unique<juce::DrawableButton>("ReduxStepPower", juce::DrawableButton::ImageFitted);
    addAndMakeVisible(reduxStepPowerButton.get());
    reduxStepPowerButton->setVisible(false);
    reduxStepPowerButton->setClickingTogglesState(true);
    
    // Make button background transparent (match other pages)
    reduxStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    reduxStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    if (assets.stepPowerOn) {
        reduxStepPowerButton->setImages(assets.stepPowerOn.get());
    }
    
    // Position at top right corner of step area, 20% smaller than 50px and adjusted position (EXACT same as other effects)
    const int powerButtonSize = 40; // 50 * 0.8 = 40 (20% smaller)
    reduxStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - powerButtonSize - 5 + 15 - 5 - 1, sequencerArea.getY() - 5 - 40 + 25 + 5, powerButtonSize, powerButtonSize); // 1px right, 5px up
    
    // Remove background colors (EXACT same as other effects)
    reduxStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    reduxStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    reduxStepPowerButton->onClick = [this]() {
        reduxStepAreaEnabled = reduxStepPowerButton->getToggleState();
        
        // Update sequencer state
        processorRef.setReduxSequencerEnabled(reduxStepAreaEnabled);
        
        // Update APVTS parameter if it exists
        auto* param = processorRef.getAPVTS().getParameter("reduxStepEnabled");
        if (param) {
            param->setValueNotifyingHost(reduxStepAreaEnabled ? 1.0f : 0.0f);
        }
        
        updateReduxStepAreaVisibility();
        DBG("[UI] Redux step power: " << (reduxStepAreaEnabled ? "ON" : "OFF"));
    };
    
    DBG("[UI] Redux sequencer area setup complete");
}

void PluginEditor::setupReduxAllStepsToggle()
{
    DBG("[UI] Setting up Redux all steps toggle...");
    
    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create all steps toggle (EXACT same positioning as other effects)
    reduxAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(reduxAllStepsToggle.get());
    reduxAllStepsToggle->setVisible(false);
    
    // Position button in EXACT same location as other effects
    const int buttonSize = 29; // 24 * 1.2 = 28.8, rounded to 29
    reduxAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, effectArea.getY() - 1, buttonSize, buttonSize);
    
    // Set up images (EXACT same as other effects)
    if (assets.stepTopInactive != nullptr && assets.stepTopActive != nullptr)
    {
        static_cast<AllStepsToggleButton*>(reduxAllStepsToggle.get())->setImages(
            assets.stepTopInactive->createCopy(),
            assets.stepTopActive->createCopy()
        );
    }
    
    reduxAllStepsToggle->setToggleState(false, juce::dontSendNotification);
    reduxAllStepsToggle->onClick = [this]() {
        reduxAllStepsEnabled = reduxAllStepsToggle->getToggleState();
        DBG("[UI] Redux All Steps toggle: " + juce::String(reduxAllStepsEnabled ? "ON" : "OFF") + " toggleState=" + juce::String(reduxAllStepsToggle->getToggleState() ? 1 : 0));
    };
    
    // Create all steps label (EXACT same positioning as other effects)
    reduxAllStepsLabel = std::make_unique<juce::Label>();
    reduxAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    reduxAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    reduxAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    reduxAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(reduxAllStepsLabel.get());
    reduxAllStepsLabel->setVisible(false);
    reduxAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, effectArea.getY() + 1, 80, 24);
    
    DBG("[UI] Redux all steps toggle setup complete");
}

void PluginEditor::updateReduxFxAreaVisibility() 
{
    float alpha = reduxFxAreaEnabled ? 1.0f : 0.3f;

    // Grey title, dice
    if (reduxEffectsTitle) reduxEffectsTitle->setAlpha(alpha);
    if (reduxDiceButton) { reduxDiceButton->setAlpha(alpha); reduxDiceButton->setEnabled(reduxFxAreaEnabled); }

    // Grey knobs, labels, values, indicators, locks
    for (int i = 0; i < 8; ++i) {
        if (reduxKnobs[i]) { reduxKnobs[i]->setAlpha(alpha); reduxKnobs[i]->setEnabled(reduxFxAreaEnabled); }
        if (reduxKnobLabels[i]) reduxKnobLabels[i]->setAlpha(alpha);
        if (reduxValueLabels[i]) reduxValueLabels[i]->setAlpha(alpha);
        if (reduxIndicatorBars[i]) reduxIndicatorBars[i]->setAlpha(alpha);
        if (reduxDiceButtons[i]) { 
            reduxDiceButtons[i]->setEnabled(reduxFxAreaEnabled);
            reduxDiceButtons[i]->setAlpha(alpha);
        }
        if (reduxLockButtons[i]) { 
            reduxLockButtons[i]->setEnabled(reduxFxAreaEnabled);
            reduxLockButtons[i]->setAlpha(alpha);
        }
    }
    
    // Power button always visible
    if (reduxFxPowerButton)
        reduxFxPowerButton->setVisible(true);
    
    DBG("[Redux] Redux FX area visibility updated");
}
void PluginEditor::updateReduxStepAreaVisibility()
{
    float alpha = reduxStepAreaEnabled ? 1.0f : 0.3f;

    // Grey step area components
    if (reduxStepTitle) { reduxStepTitle->setAlpha(alpha); reduxStepTitle->setVisible(true); }
    if (reduxStepAmountLabel) { reduxStepAmountLabel->setAlpha(alpha); reduxStepAmountLabel->setVisible(true); }
    if (reduxRateDropdown) { reduxRateDropdown->setAlpha(alpha); reduxRateDropdown->setEnabled(reduxStepAreaEnabled); }
    if (reduxStdToggle) { reduxStdToggle->setAlpha(alpha); reduxStdToggle->setEnabled(reduxStepAreaEnabled); }
    if (reduxStepDiceButton) { 
        reduxStepDiceButton->setAlpha(alpha); 
        reduxStepDiceButton->setEnabled(reduxStepAreaEnabled);
        reduxStepDiceButton->setVisible(true);
    }
    
    // Grey step buttons
    for (int i = 0; i < 16; ++i) {
        if (reduxStepButtons[i]) {
            reduxStepButtons[i]->setAlpha(alpha);
            reduxStepButtons[i]->setEnabled(reduxStepAreaEnabled);
            reduxStepButtons[i]->setVisible(true);
        }
    }
    
    // Power button always visible
    if (reduxStepPowerButton) reduxStepPowerButton->setVisible(true);
}
void PluginEditor::randomizeReduxKnobValues()
{
    DBG("[UI] Randomizing Redux knob values...");
    
    for (int i = 0; i < 8; ++i)
    {
        if (reduxKnobLocked[i]) {
            continue; // Skip locked knobs
        }
        
        randomizeIndividualReduxKnob(i);
    }
}

void PluginEditor::randomizeIndividualReduxKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || reduxKnobs[knobIndex] == nullptr) return;
    if (reduxKnobLocked[knobIndex]) return;
    
    float randomValue = 0.0f;
    switch (knobIndex) {
        case 0: // Bit Depth (UI 1-12, internal 4-16)
            randomValue = 1.0f + juce::Random::getSystemRandom().nextFloat() * 11.0f;
            break;
        case 1: // Sample Rate (1-32)
            randomValue = 1.0f + juce::Random::getSystemRandom().nextFloat() * 31.0f;
            break;
        case 2: // Jitter (0-1)
            randomValue = juce::Random::getSystemRandom().nextFloat();
            break;
        case 3: // Pre Filter (20-20000 Hz, log scale)
            randomValue = 20.0f + juce::Random::getSystemRandom().nextFloat() * 19980.0f;
            break;
        case 4: // Post Filter (20-20000 Hz, log scale)
            randomValue = 20.0f + juce::Random::getSystemRandom().nextFloat() * 19980.0f;
            break;
        case 5: // Drive (0-10)
            randomValue = juce::Random::getSystemRandom().nextFloat() * 10.0f;
            break;
        case 6: // Emphasis (0-1)
            randomValue = juce::Random::getSystemRandom().nextFloat();
            break;
        case 7: // Mix (0-1)
            randomValue = juce::Random::getSystemRandom().nextFloat();
            break;
    }
    
    reduxKnobs[knobIndex]->setValue(randomValue, juce::sendNotification);
    DBG("[UI] Redux knob " << knobIndex << " randomized to " << randomValue);
}
void PluginEditor::updateReduxParameterFromKnob(int knobIndex)
{
    if (knobIndex < 0 || knobIndex >= 8 || reduxKnobs[knobIndex] == nullptr) return;
    
    float value = reduxKnobs[knobIndex]->getValue();
    processorRef.updateReduxCurrentStepSnapshot(knobIndex, value);
}
void PluginEditor::updateReduxSequencerUI()
{
    int selectedStep = reduxUiSelectedStep;
    int playingStep = processorRef.getReduxCurrentStep();
    const int stepsUsed = processorRef.getReduxSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (reduxStepButtons[i] != nullptr) {
            reduxStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getReduxSeqState().enabled.load();
            reduxStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            bool shouldBeEnabled = i < stepsUsed;
            reduxStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display (don't overwrite if user is editing)
    if (reduxStepAmountLabel != nullptr && !reduxStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = reduxStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            reduxStepAmountLabel->setText(newText, false);
        }
    }
}

void PluginEditor::onReduxStepButtonClicked(int stepIndex)
{
    DBG("[UI] Redux step button " << stepIndex << " clicked");
    
    // Save current step's snapshot before switching
    int currentStep = reduxUiSelectedStep;
    if (currentStep >= 0 && currentStep < 16) {
        StepSnapshot currentSnapshot;
        // Read current Redux knob values and save to snapshot (CORRECT ORDER)
        if (reduxKnobs[0]) {
            // Convert UI bit depth (1-12) to internal value (4-16)
            float uiBitDepth = reduxKnobs[0]->getValue();
            currentSnapshot.redux.bitDepth = (int)(uiBitDepth + 3.0f);
        }
        if (reduxKnobs[1]) currentSnapshot.redux.sampleRateReduction = (int)reduxKnobs[1]->getValue();
        if (reduxKnobs[2]) currentSnapshot.redux.jitter = reduxKnobs[2]->getValue();
        if (reduxKnobs[3]) currentSnapshot.redux.preFilter = reduxKnobs[3]->getValue();
        if (reduxKnobs[4]) currentSnapshot.redux.postFilter = reduxKnobs[4]->getValue();
        if (reduxKnobs[5]) currentSnapshot.redux.drive = reduxKnobs[5]->getValue();
        if (reduxKnobs[6]) currentSnapshot.redux.emphasis = reduxKnobs[6]->getValue();
        if (reduxKnobs[7]) currentSnapshot.redux.mix = reduxKnobs[7]->getValue();
        
        processorRef.setReduxStepSnapshot(currentStep, currentSnapshot);
        DBG("[UI] Saved Redux snapshot for step " << currentStep);
    }
    
    // Switch to new step (both UI and processor tracking)
    reduxUiSelectedStep = stepIndex;
    processorRef.setReduxSelectedStep(stepIndex);
    
    // Load new step's snapshot into knobs (CRITICAL: Use dontSendNotification to prevent All Steps trigger)
    StepSnapshot newSnapshot = processorRef.getReduxSafeSnapshot(stepIndex);
    if (reduxKnobs[0]) {
        // Convert internal bit depth (4-16) to UI value (1-12)
        float uiBitDepth = (float)newSnapshot.redux.bitDepth - 3.0f;
        reduxKnobs[0]->setValue(uiBitDepth, juce::dontSendNotification);
    }
    if (reduxKnobs[1]) reduxKnobs[1]->setValue((float)newSnapshot.redux.sampleRateReduction, juce::dontSendNotification);
    if (reduxKnobs[2]) reduxKnobs[2]->setValue(newSnapshot.redux.jitter, juce::dontSendNotification);
    if (reduxKnobs[3]) reduxKnobs[3]->setValue(newSnapshot.redux.preFilter, juce::dontSendNotification);
    if (reduxKnobs[4]) reduxKnobs[4]->setValue(newSnapshot.redux.postFilter, juce::dontSendNotification);
    if (reduxKnobs[5]) reduxKnobs[5]->setValue(newSnapshot.redux.drive, juce::dontSendNotification);
    if (reduxKnobs[6]) reduxKnobs[6]->setValue(newSnapshot.redux.emphasis, juce::dontSendNotification);
    if (reduxKnobs[7]) reduxKnobs[7]->setValue(newSnapshot.redux.mix, juce::dontSendNotification);
    
    updateReduxSequencerUI();
    
    DBG("[UI] Switched to Redux step " << stepIndex);
}
void PluginEditor::ensureReduxAttachments() {}
void PluginEditor::rebindReduxAttachments() {}

//==============================================================================
// PhaseBloom Page Setup Methods
//==============================================================================

void PluginEditor::setupPhaseBloomKnobs()
{
    // PhaseBloom knob titles for phaser controls
    const juce::StringArray phaseBloomKnobTitles = {
        "Depth",
        "Rate",
        "Feedback",
        "Center",
        "Bloom",
        "Spread",
        "Resonance",
        "Mix"
    };
    
    // PhaseBloom parameter IDs (must match APVTS order)
    const juce::StringArray phaseBloomParamIDs = {
        "phasebloomDepth",
        "phasebloomRate",
        "phasebloomFeedback", 
        "phasebloomCenter",
        "phasebloomBloom",
        "phasebloomSpread",
        "phasebloomResonance",
        "phasebloomMix"
    };
    
    DBG("[UI] Setting up PhaseBloom knobs...");

    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    const int knobSize = 80;
    const int knobSpacing = 20;
    const int startX = effectArea.getX() + 15;
    const int startY = effectArea.getY() + effectArea.getHeight() - 210;

    // Create and setup knobs
    for (int i = 0; i < 8; ++i)
    {
        // Position 8 knobs in 2 rows of 4 (EXACT same as other effects)
        int x = startX + (i % 4) * (knobSize + knobSpacing);
        int y = startY + (i / 4) * (knobSize + 20);
        
        // Move all knob groups up 6px from current position, then top 4 down 8px (EXACT same as other effects)
        if (i < 4)
            y -= 23; // Moved up 6px from -25 to -31, then down 8px to -23
        else
            y -= 1; // Moved up 6px from +5 to -1
        
        // Create knob
        phaseBloomKnobs[i] = std::make_unique<CustomKnob>();
        addAndMakeVisible(phaseBloomKnobs[i].get());
        phaseBloomKnobs[i]->setVisible(false);
        phaseBloomKnobs[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        phaseBloomKnobs[i]->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        // Set knob ranges based on parameter (UI order)
        switch (i) {
            case 0: // Depth (0-1)
                phaseBloomKnobs[i]->setRange(0.0, 1.0, 0.01);
                phaseBloomKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 1: // Rate (0-1, tempo sync divisions)
                phaseBloomKnobs[i]->setRange(0.0, 1.0, 0.01);
                phaseBloomKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 2: // Feedback (-0.8 to +0.8)
                phaseBloomKnobs[i]->setRange(-0.8, 0.8, 0.01);
                phaseBloomKnobs[i]->setValue(0.3, juce::dontSendNotification);
                break;
            case 3: // Center (200-8000 Hz, log scale)
                phaseBloomKnobs[i]->setRange(200.0, 8000.0, 1.0);
                phaseBloomKnobs[i]->setSkewFactorFromMidPoint(2000.0);
                phaseBloomKnobs[i]->setValue(2000.0, juce::dontSendNotification);
                break;
            case 4: // Bloom (0-1)
                phaseBloomKnobs[i]->setRange(0.0, 1.0, 0.01);
                phaseBloomKnobs[i]->setValue(0.2, juce::dontSendNotification);
                break;
            case 5: // Spread (0-1)
                phaseBloomKnobs[i]->setRange(0.0, 1.0, 0.01);
                phaseBloomKnobs[i]->setValue(0.8, juce::dontSendNotification);
                break;
            case 6: // Resonance (0-1)
                phaseBloomKnobs[i]->setRange(0.0, 1.0, 0.01);
                phaseBloomKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
            case 7: // Mix (0-1)
                phaseBloomKnobs[i]->setRange(0.0, 1.0, 0.01);
                phaseBloomKnobs[i]->setValue(0.5, juce::dontSendNotification);
                break;
        }
        
        // Set knob images (CRITICAL - this makes them look like proper knobs!)
        if (assets.knobRing != nullptr)
            phaseBloomKnobs[i]->setRingImage(assets.knobRing->createCopy());
        if (assets.knobInside != nullptr)
            phaseBloomKnobs[i]->setInnerImage(assets.knobInside->createCopy());

        // Position knob
        phaseBloomKnobs[i]->setBounds(x, y, knobSize, knobSize);
        
        // Create knob label
        phaseBloomKnobLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(phaseBloomKnobLabels[i].get());
        phaseBloomKnobLabels[i]->setVisible(false);
        phaseBloomKnobLabels[i]->setText(phaseBloomKnobTitles[i], juce::dontSendNotification);
        phaseBloomKnobLabels[i]->setJustificationType(juce::Justification::centred);
        phaseBloomKnobLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        phaseBloomKnobLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold));
        phaseBloomKnobLabels[i]->setBounds(x, y - 15, knobSize, 20); // Moved down 5px from -20 to -15 (same as other effects)
        
        // Create value label
        phaseBloomValueLabels[i] = std::make_unique<juce::Label>();
        addAndMakeVisible(phaseBloomValueLabels[i].get());
        phaseBloomValueLabels[i]->setVisible(false);
        phaseBloomValueLabels[i]->setText("0", juce::dontSendNotification);
        phaseBloomValueLabels[i]->setJustificationType(juce::Justification::centred);
        phaseBloomValueLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        phaseBloomValueLabels[i]->setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 10.0f, juce::Font::plain));
        phaseBloomValueLabels[i]->setBounds(x, y + knobSize - 10, knobSize, 15); // Same as other effects
        
        // Create indicator bar
        phaseBloomIndicatorBars[i] = std::make_unique<IndicatorBar>();
        addAndMakeVisible(phaseBloomIndicatorBars[i].get());
        phaseBloomIndicatorBars[i]->setVisible(false);
        phaseBloomIndicatorBars[i]->setValue(0.5f);
        phaseBloomIndicatorBars[i]->setBounds(x + 10, y + knobSize + 8, knobSize - 20, 13); // Same as other effects
        
        // Create dice button
        phaseBloomDiceButtons[i] = std::make_unique<CustomDiceButton>();
        addAndMakeVisible(phaseBloomDiceButtons[i].get());
        phaseBloomDiceButtons[i]->setVisible(false);
        
        // Create lock button (positioned at end of title text like other effects)
        phaseBloomLockButtons[i] = std::make_unique<LockButton>();
        addAndMakeVisible(phaseBloomLockButtons[i].get());
        phaseBloomLockButtons[i]->setVisible(false);
        
        // Position lock button at end of title text (same as other effects)
        juce::Font labelFont(12.0f, juce::Font::bold);
        int textWidth = labelFont.getStringWidth(phaseBloomKnobTitles[i]);
        const int lockSize = 10; // Same size as other effects
        const int lockSpacing = 5; // Fixed distance from end of title text
        int lockX = x + (knobSize / 2) + (textWidth / 2) + lockSpacing;
        int lockY = y - 10; // Same position as other effects
        
        phaseBloomLockButtons[i]->setBounds(lockX, lockY, lockSize, lockSize);
        
        // Set lock button images
        if (assets.unlockedIcon && assets.lockedIcon) {
            auto imgUnlocked = assets.unlockedIcon->createCopy();
            auto imgLocked = assets.lockedIcon->createCopy();
            phaseBloomLockButtons[i]->setImages(std::move(imgUnlocked), std::move(imgLocked));
        }
        phaseBloomLockButtons[i]->setToggleState(phaseBloomKnobLocked[i], juce::dontSendNotification);
        phaseBloomLockButtons[i]->onClick = [this, i]() {
            phaseBloomKnobLocked[i] = phaseBloomLockButtons[i]->getToggleState();
        };
        
        // Create APVTS attachment
        phaseBloomAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), phaseBloomParamIDs[i], *phaseBloomKnobs[i]);
        
        // Add value change callback to update value label and indicator bar
        phaseBloomKnobs[i]->onValueChange = [this, i]() {
            // Skip if loading from snapshot (prevents circular updates during randomization)
            if (isLoadingFromSnapshot.load())
                return;
            
            if (phaseBloomKnobs[i] && phaseBloomValueLabels[i] && phaseBloomIndicatorBars[i]) {
                float value = phaseBloomKnobs[i]->getValue();
                juce::String valueText;
                
                switch (i) {
                    case 0: valueText = juce::String(value, 2); break; // Depth
                    case 1: valueText = PhaseBloomEngine::getRateLabel(value); break; // Rate (tempo sync)
                    case 2: valueText = juce::String(value, 2); break; // Feedback
                    case 3: valueText = juce::String((int)value) + " Hz"; break; // Center
                    case 4: valueText = juce::String(value, 2); break; // Bloom
                    case 5: valueText = juce::String(value, 2); break; // Spread
                    case 6: valueText = juce::String(value, 2); break; // Resonance
                    case 7: valueText = juce::String(value, 2); break; // Mix
                }
                
                phaseBloomValueLabels[i]->setText(valueText, juce::dontSendNotification);
                
                // Update indicator bar
                phaseBloomIndicatorBars[i]->setValue(value);
                
                // Update current step snapshot with new value
                processorRef.updatePhaseBloomCurrentStepSnapshot(i, value);
                
                // If All Steps toggle is active, update all step snapshots
                if (phaseBloomAllStepsEnabled) {
                    DBG("[All Steps] PhaseBloom knob " << i << " changed, phaseBloomAllStepsEnabled=true");
                    
                    // Convert knob value to actual parameter value using APVTS parameter ranges
                    float actualValue = value;
                    switch (i) {
                        case 0: { // Depth
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomDepth");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 1: { // Rate
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomRate");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 2: { // Feedback
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomFeedback");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 3: { // Center
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomCenter");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 4: { // Bloom
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomBloom");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 5: { // Spread
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomSpread");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 6: { // Resonance
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomResonance");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                        case 7: { // Mix
                            auto* param = processorRef.getAPVTS().getParameter("phasebloomMix");
                            if (param) actualValue = param->convertFrom0to1(value);
                            break;
                        }
                    }
                    
                    for (int step = 0; step < 16; ++step) {
                        auto snapshot = processorRef.getPhaseBloomSafeSnapshot(step);
                        switch (i) {
                            case 0: snapshot.phasebloom.depth = actualValue; break;
                            case 1: snapshot.phasebloom.rate = actualValue; break;
                            case 2: snapshot.phasebloom.feedback = actualValue; break;
                            case 3: snapshot.phasebloom.center = actualValue; break;
                            case 4: snapshot.phasebloom.bloom = actualValue; break;
                            case 5: snapshot.phasebloom.spread = actualValue; break;
                            case 6: snapshot.phasebloom.resonance = actualValue; break;
                            case 7: snapshot.phasebloom.mix = actualValue; break;
                        }
                        processorRef.setPhaseBloomStepSnapshot(step, snapshot);
                    }
                } else {
                    DBG("[All Steps] PhaseBloom knob " << i << " changed, phaseBloomAllStepsEnabled=false, skipping All Steps update");
                }
            }
        };
    }
    
    DBG("[UI] PhaseBloom knobs setup complete");
}

void PluginEditor::setupPhaseBloomEffectsArea()
{
    DBG("[UI] Setting up PhaseBloom effects area...");
    
    // Effect area bounds (match other pages exactly)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create "EFFECT" title label (ALWAYS "EFFECT", NOT the effect name!)
    phaseBloomEffectsTitle = std::make_unique<juce::Label>();
    phaseBloomEffectsTitle->setText("EFFECT", juce::dontSendNotification);
    phaseBloomEffectsTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 18.6624f, juce::Font::bold).withExtraKerningFactor(0.09f));
    phaseBloomEffectsTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    phaseBloomEffectsTitle->setJustificationType(juce::Justification::centredLeft);
    phaseBloomEffectsTitle->setBounds(effectArea.getX() + 10, effectArea.getY() + 5, 100, 30);
    addAndMakeVisible(phaseBloomEffectsTitle.get());
    phaseBloomEffectsTitle->setVisible(false);
    
    // Create FX power button
    phaseBloomFxPowerButton = std::make_unique<juce::DrawableButton>("PhaseBloomEffectPower", juce::DrawableButton::ImageFitted);
    phaseBloomFxPowerButton->setClickingTogglesState(true);
    phaseBloomFxPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    phaseBloomFxPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    if (assets.fxPowerOn) {
        phaseBloomFxPowerButton->setImages(assets.fxPowerOn.get());
    }
    const int buttonSize = 46;
    phaseBloomFxPowerButton->setBounds(effectArea.getX() + effectArea.getWidth() - buttonSize - 8 + 8 + 3, 
                                       effectArea.getY() + 6 - 20 + 4, buttonSize, buttonSize);
    addAndMakeVisible(phaseBloomFxPowerButton.get());
    phaseBloomFxPowerButton->setVisible(false);
    
    // Create dice button
    phaseBloomDiceButton = std::make_unique<CustomDiceButton>();
    const int diceSize = 32;
    phaseBloomDiceButton->setBounds(effectArea.getX() + 130, effectArea.getY() + 5, diceSize, diceSize);
    if (assets.diceLarge != nullptr) {
        phaseBloomDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    addAndMakeVisible(phaseBloomDiceButton.get());
    phaseBloomDiceButton->setVisible(false);
    
    // Set up FX power button callback
    phaseBloomFxPowerButton->onClick = [this]() {
        phaseBloomFxAreaEnabled = phaseBloomFxPowerButton->getToggleState();
        updatePhaseBloomFxAreaVisibility();
        
        // Update the actual parameter that the processor checks
        auto* phasebloomEnabledParam = processorRef.getAPVTS().getParameter("phasebloomEnabled");
        if (phasebloomEnabledParam) {
            phasebloomEnabledParam->setValueNotifyingHost(phaseBloomFxAreaEnabled ? 1.0f : 0.0f);
        }
    };
    
    // Set up dice button callback
    phaseBloomDiceButton->onClick = [this]() {
        randomizePhaseBloomKnobValues();
    };
    
    DBG("[UI] PhaseBloom effects area setup complete");
}

void PluginEditor::setupPhaseBloomSequencerArea()
{
    DBG("[UI] Setting up PhaseBloom sequencer area...");
    
    // Sequencer area bounds (EXACT same as other pages)
    auto sequencerArea = juce::Rectangle<int>(25, 374, 413, 140);
    
    // Create step title
    phaseBloomStepTitle = std::make_unique<juce::Label>();
    phaseBloomStepTitle->setText("STEP", juce::dontSendNotification);
    phaseBloomStepTitle->setFont(FontManager::getInstance().getFont("Akira Expanded", 13.436685f, juce::Font::bold).withExtraKerningFactor(0.09f));
    phaseBloomStepTitle->setColour(juce::Label::textColourId, juce::Colours::white);
    phaseBloomStepTitle->setJustificationType(juce::Justification::centredLeft);
    phaseBloomStepTitle->setBounds(sequencerArea.getX() + 10, sequencerArea.getY(), 80, 30);
    addAndMakeVisible(phaseBloomStepTitle.get());
    phaseBloomStepTitle->setVisible(false);
    
    // Create step buttons (2x8 grid)
    const int buttonSize = 40;
    const int buttonSpacing = 8;
    const int startX = sequencerArea.getX() + 20;
    const int startY = sequencerArea.getY() + 35;
    
    for (int i = 0; i < 16; ++i) {
            auto button = std::make_unique<StepButton>(i);
            int x = startX + (i % 8) * (buttonSize + buttonSpacing);
            int y = startY + (i / 8) * (buttonSize + buttonSpacing);
            button->setBounds(x, y, buttonSize, buttonSize);
            button->onClick = [this, i]() { onPhaseBloomStepButtonClicked(i); };
            
            // Set step button images before adding
            if (assets.stepActive && assets.stepInactive) {
                button->setActiveImage(assets.stepActive->createCopy());
                button->setInactiveImage(assets.stepInactive->createCopy());
            }
            
            addAndMakeVisible(button.get());
            button->setVisible(false);
            phaseBloomStepButtons[i] = std::move(button);
    }
    
    // Create step amount label (TextEditor)
    phaseBloomStepAmountLabel = std::make_unique<juce::TextEditor>();
    // Force to 16 by default, then sync with processor state
    processorRef.setPhaseBloomStepsUsed(16);
    phaseBloomStepAmountLabel->setText("16");
    phaseBloomStepAmountLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    phaseBloomStepAmountLabel->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    phaseBloomStepAmountLabel->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    phaseBloomStepAmountLabel->setColour(juce::TextEditor::outlineColourId, juce::Colours::white);
    phaseBloomStepAmountLabel->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    phaseBloomStepAmountLabel->setJustification(juce::Justification::centred);
    phaseBloomStepAmountLabel->setBorder(juce::BorderSize<int>(2));
    phaseBloomStepAmountLabel->setIndents(0, 0);
    phaseBloomStepAmountLabel->setInputRestrictions(2, "0123456789");
    phaseBloomStepAmountLabel->setBounds(sequencerArea.getX() + 180, sequencerArea.getY() - 10, 30, 25);
    addAndMakeVisible(phaseBloomStepAmountLabel.get());
    phaseBloomStepAmountLabel->setVisible(false);
    
    // Add callback for step amount changes
    phaseBloomStepAmountLabel->onTextChange = [this]() {
        int newStepsUsed = phaseBloomStepAmountLabel->getText().getIntValue();
        newStepsUsed = juce::jlimit(1, 16, newStepsUsed);
        processorRef.setPhaseBloomStepsUsed(newStepsUsed);
        updatePhaseBloomSequencerUI();
    };
    
    // Create rate dropdown
    phaseBloomRateDropdown = std::make_unique<juce::ComboBox>();
    phaseBloomRateDropdown->addItem("4", 1);
    phaseBloomRateDropdown->addItem("2", 2);
    phaseBloomRateDropdown->addItem("1", 3);
    phaseBloomRateDropdown->addItem("1/2", 4);
    phaseBloomRateDropdown->addItem("1/4", 5);
    phaseBloomRateDropdown->addItem("1/8", 6);
    phaseBloomRateDropdown->addItem("1/16", 7);
    phaseBloomRateDropdown->addItem("1/32", 8);
    phaseBloomRateDropdown->setSelectedId(6); // Default to 1/8
    phaseBloomRateDropdown->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    phaseBloomRateDropdown->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    phaseBloomRateDropdown->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    phaseBloomRateDropdown->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    phaseBloomRateDropdown->setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    phaseBloomRateDropdown->setBounds(sequencerArea.getX() + 220, sequencerArea.getY() - 10, 74, 25);
    addAndMakeVisible(phaseBloomRateDropdown.get());
    phaseBloomRateDropdown->setVisible(false);
    
    // Add callback for rate dropdown changes
    phaseBloomRateDropdown->onChange = [this]() {
        int selectedId = phaseBloomRateDropdown->getSelectedId();
        int divisionIndex = selectedId - 1; // Convert 1-based to 0-based
        processorRef.setPhaseBloomDivisionIndex(divisionIndex);
    };
    
    // Create STD toggle
    phaseBloomStdToggle = std::make_unique<CircularToggleButton>();
    phaseBloomStdToggle->setButtonText("-");
    phaseBloomStdToggle->setBounds(sequencerArea.getX() + 288, sequencerArea.getY() - 14, 30, 30);
    addAndMakeVisible(phaseBloomStdToggle.get());
    phaseBloomStdToggle->setVisible(false);
    
    // Create step dice button
    phaseBloomStepDiceButton = std::make_unique<CustomDiceButton>();
    int stepDiceSize = static_cast<int>(35 * 0.7); // 30% smaller
    phaseBloomStepDiceButton->setBounds(sequencerArea.getX() + 75, sequencerArea.getY() + 5, stepDiceSize, stepDiceSize);
    if (assets.diceLarge != nullptr) {
        phaseBloomStepDiceButton->setDiceImage(assets.diceLarge->createCopy());
    }
    addAndMakeVisible(phaseBloomStepDiceButton.get());
    phaseBloomStepDiceButton->setVisible(false);
    
    // Create step power button
    phaseBloomStepPowerButton = std::make_unique<juce::DrawableButton>("PhaseBloomStepPower", juce::DrawableButton::ImageFitted);
    phaseBloomStepPowerButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    phaseBloomStepPowerButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    if (assets.stepPowerOn) {
        phaseBloomStepPowerButton->setImages(assets.stepPowerOn.get());
    }
    phaseBloomStepPowerButton->setClickingTogglesState(true);
    const int stepPowerSize = 40;
    phaseBloomStepPowerButton->setBounds(sequencerArea.getX() + sequencerArea.getWidth() - stepPowerSize - 5 + 15 - 5 - 1, 
                                         sequencerArea.getY() - 5 - stepPowerSize + 25 + 5, stepPowerSize, stepPowerSize);
    addAndMakeVisible(phaseBloomStepPowerButton.get());
    phaseBloomStepPowerButton->setVisible(false);
    
    // Add callback for step power button
    phaseBloomStepPowerButton->onClick = [this]() {
        phaseBloomStepAreaEnabled = phaseBloomStepPowerButton->getToggleState();
        processorRef.setPhaseBloomSequencerEnabled(phaseBloomStepAreaEnabled);
        updatePhaseBloomStepAreaVisibility();
    };
    
    
    phaseBloomStepDiceButton->onClick = [this]() {
        DBG("[PHASEBLOOM] Step dice button clicked - randomizing all step snapshots");
        
        // Randomize step sequencer
        for (int i = 0; i < 16; ++i) {
            auto snapshot = processorRef.getPhaseBloomSafeSnapshot(i);
            // Randomize all parameters
            snapshot.phasebloom.depth = juce::Random::getSystemRandom().nextFloat();
            snapshot.phasebloom.rate = juce::Random::getSystemRandom().nextFloat();
            snapshot.phasebloom.feedback = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            snapshot.phasebloom.center = juce::Random::getSystemRandom().nextFloat() * 3900.0f + 100.0f;
            snapshot.phasebloom.bloom = juce::Random::getSystemRandom().nextFloat();
            snapshot.phasebloom.spread = juce::Random::getSystemRandom().nextFloat();
            snapshot.phasebloom.resonance = juce::Random::getSystemRandom().nextFloat();
            snapshot.phasebloom.mix = juce::Random::getSystemRandom().nextFloat();
            processorRef.setPhaseBloomStepSnapshot(i, snapshot);
        }
        
        // Update the UI to show the new random values
        updatePhaseBloomSequencerUI();
        
        // Update knob values to reflect the current step's randomized data
        if (phaseBloomUiSelectedStep >= 0 && phaseBloomUiSelectedStep < 16) {
            auto currentSnapshot = processorRef.getPhaseBloomSafeSnapshot(phaseBloomUiSelectedStep);
            
            // Update all knob values from the current step's snapshot
            if (phaseBloomKnobs[0]) phaseBloomKnobs[0]->setValue(currentSnapshot.phasebloom.depth, juce::dontSendNotification);
            if (phaseBloomKnobs[1]) phaseBloomKnobs[1]->setValue(currentSnapshot.phasebloom.rate, juce::dontSendNotification);
            if (phaseBloomKnobs[2]) phaseBloomKnobs[2]->setValue(currentSnapshot.phasebloom.feedback, juce::dontSendNotification);
            if (phaseBloomKnobs[3]) phaseBloomKnobs[3]->setValue(currentSnapshot.phasebloom.center, juce::dontSendNotification);
            if (phaseBloomKnobs[4]) phaseBloomKnobs[4]->setValue(currentSnapshot.phasebloom.bloom, juce::dontSendNotification);
            if (phaseBloomKnobs[5]) phaseBloomKnobs[5]->setValue(currentSnapshot.phasebloom.spread, juce::dontSendNotification);
            if (phaseBloomKnobs[6]) phaseBloomKnobs[6]->setValue(currentSnapshot.phasebloom.resonance, juce::dontSendNotification);
            if (phaseBloomKnobs[7]) phaseBloomKnobs[7]->setValue(currentSnapshot.phasebloom.mix, juce::dontSendNotification);
            
            // Update value labels to reflect the new knob values
            for (int i = 0; i < 8; ++i) {
                if (phaseBloomKnobs[i] && phaseBloomValueLabels[i]) {
                    float value = phaseBloomKnobs[i]->getValue();
                    juce::String valueText;
                    
                    switch (i) {
                        case 0: valueText = juce::String(value, 2); break; // Depth
                        case 1: valueText = PhaseBloomEngine::getRateLabel(value); break; // Rate (tempo sync)
                        case 2: valueText = juce::String(value, 2); break; // Feedback
                        case 3: valueText = juce::String((int)value) + " Hz"; break; // Center
                        case 4: valueText = juce::String(value, 2); break; // Bloom
                        case 5: valueText = juce::String(value, 2); break; // Spread
                        case 6: valueText = juce::String(value, 2); break; // Resonance
                        case 7: valueText = juce::String(value, 2); break; // Mix
                    }
                    
                    phaseBloomValueLabels[i]->setText(valueText, juce::dontSendNotification);
                }
            }
        }
        
        DBG("[PHASEBLOOM] Step randomization complete - UI updated");
    };
    
    DBG("[UI] PhaseBloom sequencer area setup complete");
}

void PluginEditor::setupPhaseBloomAllStepsToggle()
{
    DBG("[UI] Setting up PhaseBloom all steps toggle...");
    
    // Effect area bounds (same as other pages)
    auto effectArea = juce::Rectangle<int>(25, 54, 413, 296);
    
    // Create all steps toggle (EXACT same positioning as other effects)
    phaseBloomAllStepsToggle = std::make_unique<AllStepsToggleButton>();
    addAndMakeVisible(phaseBloomAllStepsToggle.get());
    phaseBloomAllStepsToggle->setVisible(false);
    
    const int buttonSize = 29;
    phaseBloomAllStepsToggle->setBounds(effectArea.getX() + effectArea.getWidth()/2 - buttonSize/2 + 30, 
                                        effectArea.getY() - 1, buttonSize, buttonSize);
    
    if (assets.stepTopInactive && assets.stepTopActive) {
        phaseBloomAllStepsToggle->setImages(assets.stepTopInactive->createCopy(), assets.stepTopActive->createCopy());
    }
    
    // Create all steps label
    phaseBloomAllStepsLabel = std::make_unique<juce::Label>();
    phaseBloomAllStepsLabel->setText("All Steps", juce::dontSendNotification);
    phaseBloomAllStepsLabel->setFont(juce::Font(14.4f, juce::Font::bold));
    phaseBloomAllStepsLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    phaseBloomAllStepsLabel->setJustificationType(juce::Justification::centredLeft);
    phaseBloomAllStepsLabel->setBounds(effectArea.getX() + effectArea.getWidth()/2 + buttonSize/2 + 5 + 30, 
                                       effectArea.getY() + 1, 80, 24);
    addAndMakeVisible(phaseBloomAllStepsLabel.get());
    phaseBloomAllStepsLabel->setVisible(false);
    
    // Set up callback
    phaseBloomAllStepsToggle->onClick = [this]() {
        phaseBloomAllStepsEnabled = phaseBloomAllStepsToggle->getToggleState();
        DBG("[UI] PhaseBloom All Steps toggle: " << (phaseBloomAllStepsEnabled ? "ON" : "OFF"));
        phaseBloomAllStepsLabel->setAlpha(phaseBloomAllStepsEnabled ? 1.0f : 0.5f);
    };
    
    DBG("[UI] PhaseBloom all steps toggle setup complete");
}

void PluginEditor::updatePhaseBloomFxAreaVisibility()
{
    float alpha = phaseBloomFxAreaEnabled ? 1.0f : 0.3f;
    for (int i = 0; i < 8; ++i) {
        if (phaseBloomKnobs[i]) { 
            phaseBloomKnobs[i]->setAlpha(alpha);
            phaseBloomKnobs[i]->setEnabled(phaseBloomFxAreaEnabled);
        }
        if (phaseBloomKnobLabels[i]) phaseBloomKnobLabels[i]->setAlpha(alpha);
        if (phaseBloomValueLabels[i]) phaseBloomValueLabels[i]->setAlpha(alpha);
        if (phaseBloomIndicatorBars[i]) phaseBloomIndicatorBars[i]->setAlpha(alpha);
        if (phaseBloomDiceButtons[i]) {
            phaseBloomDiceButtons[i]->setAlpha(alpha);
            phaseBloomDiceButtons[i]->setEnabled(phaseBloomFxAreaEnabled);
        }
        if (phaseBloomLockButtons[i]) phaseBloomLockButtons[i]->setAlpha(alpha);
    }
    if (phaseBloomDiceButton) {
        phaseBloomDiceButton->setAlpha(alpha);
        phaseBloomDiceButton->setEnabled(phaseBloomFxAreaEnabled);
    }
    if (phaseBloomEffectsTitle) phaseBloomEffectsTitle->setAlpha(alpha);
    repaint();
}

void PluginEditor::updatePhaseBloomStepAreaVisibility()
{
    float alpha = phaseBloomStepAreaEnabled ? 1.0f : 0.3f;
    for (int i = 0; i < 16; ++i) {
        if (phaseBloomStepButtons[i]) {
            phaseBloomStepButtons[i]->setAlpha(alpha);
            phaseBloomStepButtons[i]->setEnabled(phaseBloomStepAreaEnabled);
        }
    }
    if (phaseBloomStepAmountLabel) {
        phaseBloomStepAmountLabel->setAlpha(alpha);
        phaseBloomStepAmountLabel->setEnabled(phaseBloomStepAreaEnabled);
    }
    if (phaseBloomRateDropdown) {
        phaseBloomRateDropdown->setAlpha(alpha);
        phaseBloomRateDropdown->setEnabled(phaseBloomStepAreaEnabled);
    }
    if (phaseBloomStdToggle) {
        phaseBloomStdToggle->setAlpha(alpha);
        phaseBloomStdToggle->setEnabled(phaseBloomStepAreaEnabled);
    }
    if (phaseBloomStepTitle) phaseBloomStepTitle->setAlpha(alpha);
    if (phaseBloomStepDiceButton) {
        phaseBloomStepDiceButton->setAlpha(alpha);
        phaseBloomStepDiceButton->setEnabled(phaseBloomStepAreaEnabled);
    }
    repaint();
}

void PluginEditor::randomizePhaseBloomKnobValues()
{
    for (int i = 0; i < 8; ++i) {
        if (phaseBloomKnobs[i] && !phaseBloomKnobLocked[i]) {
            randomizeIndividualPhaseBloomKnob(i);
        }
    }
}

void PluginEditor::randomizeFormantKnobValues()
{
    DBG("[UI] Randomizing Formant knob values");
    
    for (int i = 0; i < 8; ++i) {
        if (formantKnobs[i] != nullptr) {
            float min = formantKnobs[i]->getMinimum();
            float max = formantKnobs[i]->getMaximum();
            float randomValue = min + (max - min) * juce::Random::getSystemRandom().nextFloat();
            formantKnobs[i]->setValue(randomValue);
        }
    }
}

void PluginEditor::updateFormantOverlay()
{
    if (!outputSpectrumView) return;
    
    // Check if Formant page is currently active
    auto& router = processorRef.getEffectRouter();
    bool formantActive = false;
    
    // Check all 4 slots to see if any has Formant effect
    for (int slot = 0; slot < 4; ++slot) {
        if (router.getEffectInSlot(static_cast<SlotID>(slot)) == EffectID::Formant) {
            formantActive = true;
            break;
        }
    }
    
    // Disable formant overlay - not using gradient line with 3 dots
    outputSpectrumView->setFormantOverlayEnabled(false);
}

// Form 2 helper methods
void PluginEditor::randomizeForm2KnobValues()
{
    DBG("[UI] Randomizing Form 2 knob values");
    
    for (int i = 0; i < 8; ++i) {
        if (form2Knobs[i] != nullptr) {
            float min = form2Knobs[i]->getMinimum();
            float max = form2Knobs[i]->getMaximum();
            float randomValue = min + (max - min) * juce::Random::getSystemRandom().nextFloat();
            form2Knobs[i]->setValue(randomValue);
        }
    }
}

void PluginEditor::randomizeIndividualForm2Knob(int knobIndex)
{
    if (form2Knobs[knobIndex]) {
        float min = form2Knobs[knobIndex]->getMinimum();
        float max = form2Knobs[knobIndex]->getMaximum();
        float randomValue = juce::Random::getSystemRandom().nextFloat() * (max - min) + min;
        form2Knobs[knobIndex]->setValue(randomValue);
    }
}

void PluginEditor::updateForm2FxAreaVisibility()
{
    float alpha = form2FxAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 8; ++i) {
        if (form2Knobs[i]) { 
            form2Knobs[i]->setAlpha(alpha); 
            form2Knobs[i]->setEnabled(form2FxAreaEnabled); 
        }
        if (form2KnobLabels[i]) form2KnobLabels[i]->setAlpha(alpha);
        if (form2ValueLabels[i]) form2ValueLabels[i]->setAlpha(alpha);
        if (form2IndicatorBars[i]) form2IndicatorBars[i]->setAlpha(alpha);
        if (form2LockButtons[i]) { 
            form2LockButtons[i]->setEnabled(form2FxAreaEnabled);
            form2LockButtons[i]->setAlpha(alpha);
        }
    }
}

void PluginEditor::updateForm2StepAreaVisibility()
{
    float alpha = form2StepAreaEnabled ? 1.0f : 0.3f;
    
    for (int i = 0; i < 16; ++i) {
        if (form2StepButtons[i]) {
            form2StepButtons[i]->setAlpha(alpha);
            form2StepButtons[i]->setEnabled(form2StepAreaEnabled);
            form2StepButtons[i]->setVisible(true);
        }
    }
}

void PluginEditor::updateForm2ParameterFromKnob(int knobIndex)
{
    if (form2Knobs[knobIndex]) {
        float value = form2Knobs[knobIndex]->getValue();
        // TODO: Add processor method to update Form2 step snapshot
    }
}

void PluginEditor::randomizeIndividualPhaseBloomKnob(int knobIndex)
{
    if (phaseBloomKnobs[knobIndex]) {
        float min = phaseBloomKnobs[knobIndex]->getMinimum();
        float max = phaseBloomKnobs[knobIndex]->getMaximum();
        float randomValue = juce::Random::getSystemRandom().nextFloat() * (max - min) + min;
        phaseBloomKnobs[knobIndex]->setValue(randomValue);
    }
}

void PluginEditor::updatePhaseBloomParameterFromKnob(int knobIndex)
{
    if (phaseBloomKnobs[knobIndex]) {
        float value = phaseBloomKnobs[knobIndex]->getValue();
        processorRef.updatePhaseBloomCurrentStepSnapshot(knobIndex, value);
    }
}

void PluginEditor::updatePhaseBloomSequencerUI()
{
    int selectedStep = phaseBloomUiSelectedStep;
    int playingStep = processorRef.getPhaseBloomCurrentStep();
    const int stepsUsed = processorRef.getPhaseBloomSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (phaseBloomStepButtons[i] != nullptr) {
            phaseBloomStepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getPhaseBloomSeqState().enabled.load();
            phaseBloomStepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            bool shouldBeEnabled = i < stepsUsed;
            phaseBloomStepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display
    if (phaseBloomStepAmountLabel != nullptr && !phaseBloomStepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = phaseBloomStepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            phaseBloomStepAmountLabel->setText(newText, false);
        }
    }
    
    repaint();
}

void PluginEditor::onPhaseBloomStepButtonClicked(int stepIndex)
{
    phaseBloomUiSelectedStep = stepIndex;
    processorRef.setPhaseBloomSelectedStep(stepIndex);
    
    // Load step snapshot into knobs
    auto snapshot = processorRef.getPhaseBloomSafeSnapshot(stepIndex);
    if (phaseBloomKnobs[0]) phaseBloomKnobs[0]->setValue(snapshot.phasebloom.depth, juce::dontSendNotification);
    if (phaseBloomKnobs[1]) phaseBloomKnobs[1]->setValue(snapshot.phasebloom.rate, juce::dontSendNotification);
    if (phaseBloomKnobs[2]) phaseBloomKnobs[2]->setValue(snapshot.phasebloom.feedback, juce::dontSendNotification);
    if (phaseBloomKnobs[3]) phaseBloomKnobs[3]->setValue(snapshot.phasebloom.center, juce::dontSendNotification);
    if (phaseBloomKnobs[4]) phaseBloomKnobs[4]->setValue(snapshot.phasebloom.bloom, juce::dontSendNotification);
    if (phaseBloomKnobs[5]) phaseBloomKnobs[5]->setValue(snapshot.phasebloom.spread, juce::dontSendNotification);
    if (phaseBloomKnobs[6]) phaseBloomKnobs[6]->setValue(snapshot.phasebloom.resonance, juce::dontSendNotification);
    if (phaseBloomKnobs[7]) phaseBloomKnobs[7]->setValue(snapshot.phasebloom.mix, juce::dontSendNotification);
    
    updatePhaseBloomSequencerUI();
    
    DBG("[UI] Switched to PhaseBloom step " << stepIndex);
}

void PluginEditor::ensurePhaseBloomAttachments() {}
void PluginEditor::rebindPhaseBloomAttachments() {}

void PluginEditor::onForm2StepButtonClicked(int stepIndex)
{
    form2UiSelectedStep = stepIndex;
    
    // Load step snapshot into knobs
    auto snapshot = processorRef.getForm2SafeSnapshot(stepIndex);
    // Map actual values to 0.0-1.0 range for knobs
    if (form2Knobs[0]) form2Knobs[0]->setValue(snapshot.form2.rootNote / 12.0f, juce::dontSendNotification);
    if (form2Knobs[1]) form2Knobs[1]->setValue(snapshot.form2.scale / 7.0f, juce::dontSendNotification);
    if (form2Knobs[2]) form2Knobs[2]->setValue((snapshot.form2.chordSize - 1) / 7.0f, juce::dontSendNotification);
    if (form2Knobs[3]) form2Knobs[3]->setValue(snapshot.form2.shift, juce::dontSendNotification);
    if (form2Knobs[4]) form2Knobs[4]->setValue(snapshot.form2.color, juce::dontSendNotification);
    if (form2Knobs[5]) form2Knobs[5]->setValue(snapshot.form2.motion, juce::dontSendNotification);
    if (form2Knobs[6]) form2Knobs[6]->setValue(snapshot.form2.resynth, juce::dontSendNotification);
    if (form2Knobs[7]) form2Knobs[7]->setValue(snapshot.form2.mix, juce::dontSendNotification);
    
    updateForm2SequencerUI();
    
    DBG("[UI] Switched to Form 2 step " << stepIndex);
}

void PluginEditor::updateForm2SequencerUI()
{
    int selectedStep = form2UiSelectedStep;
    int playingStep = processorRef.getForm2PlayingStep();
    auto& seq = processorRef.getForm2SeqState();
    const int stepsUsed = seq.stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (form2StepButtons[i] != nullptr) {
            form2StepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = seq.enabled.load();
            form2StepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep) && (i != selectedStep));
            bool shouldBeEnabled = i < stepsUsed;
            form2StepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display
    if (form2StepAmountLabel != nullptr && !form2StepAmountLabel->hasKeyboardFocus(true)) {
        juce::String currentText = form2StepAmountLabel->getText();
        juce::String newText = juce::String(stepsUsed);
        // Only update if the value has actually changed to avoid text doubling
        if (currentText != newText) {
            form2StepAmountLabel->setText(newText, false);
        }
    }
    
    if (form2RateDropdown != nullptr) {
        int divisionIndex = seq.divisionIndex.load();
        form2RateDropdown->setSelectedId(divisionIndex + 1);
    }
}

//==============================================================================
// Unified Effect Handling Methods
//==============================================================================

void PluginEditor::randomizeEffectStepSnapshot(FxPageID effect, int step)
{
    DBG("[UI] Randomizing step " << step << " for effect " << static_cast<int>(effect));
    
    switch (effect) {
        case FxPageID::SpaceDelay: {
            auto snapshot = processorRef.getSpaceDelaySafeSnapshot(step);
            
            // Randomize only unlocked parameters with correct ranges
            if (!knobLocked[0]) snapshot.delay.timeMs = 10.0f + juce::Random::getSystemRandom().nextFloat() * (2000.0f - 10.0f);
            if (!knobLocked[1]) snapshot.delay.feedback = juce::Random::getSystemRandom().nextFloat() * 0.95f;
            if (!knobLocked[2]) snapshot.delay.wowDepth = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[3]) snapshot.delay.wowRate = 0.1f + juce::Random::getSystemRandom().nextFloat() * (8.0f - 0.1f);
            if (!knobLocked[4]) snapshot.delay.saturation = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[5]) snapshot.delay.highCut = 1000.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 1000.0f);
            if (!knobLocked[6]) snapshot.delay.lowCut = 20.0f + juce::Random::getSystemRandom().nextFloat() * (2000.0f - 20.0f);
            if (!knobLocked[7]) snapshot.delay.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setSpaceDelayStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Panner: {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Randomize AutoPan parameters
            if (!knobLocked[0]) snapshot.autopan.rate = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[1]) snapshot.autopan.phase = juce::Random::getSystemRandom().nextFloat() * 360.0f;
            if (!knobLocked[2]) snapshot.autopan.waveType = juce::Random::getSystemRandom().nextInt(5);
            if (!knobLocked[3]) snapshot.autopan.waveShape = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[4]) snapshot.autopan.inverted = juce::Random::getSystemRandom().nextBool();
            if (!knobLocked[5]) snapshot.autopan.amount = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Formant: {
            auto snapshot = processorRef.getFormantSafeSnapshot(step);
            
            // Randomize Formant parameters (now only 4)
            if (!knobLocked[0]) snapshot.formant.vowel = juce::Random::getSystemRandom().nextInt(5); // 0-4
            if (!knobLocked[1]) snapshot.formant.resonance = 0.5f + juce::Random::getSystemRandom().nextFloat() * (20.0f - 0.5f); // 0.5-20
            if (!knobLocked[2]) snapshot.formant.intensity = juce::Random::getSystemRandom().nextFloat() * 12.0f; // 0-12
            if (!knobLocked[3]) snapshot.formant.mix = juce::Random::getSystemRandom().nextFloat(); // 0-1
            
            processorRef.setFormantStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Form2: {
            auto snapshot = processorRef.getForm2SafeSnapshot(step);
            
            // Randomize Form 2 parameters (8 knobs)
            if (!knobLocked[0]) snapshot.form2.rootNote = juce::Random::getSystemRandom().nextInt(juce::Range(0, 12)); // 0-11
            if (!knobLocked[1]) snapshot.form2.scale = juce::Random::getSystemRandom().nextInt(juce::Range(0, 7)); // 0-6
            if (!knobLocked[2]) snapshot.form2.chordSize = juce::Random::getSystemRandom().nextInt(juce::Range(1, 9)); // 1-8
            if (!knobLocked[3]) snapshot.form2.shift = 0.5f + juce::Random::getSystemRandom().nextFloat() * (2.0f - 0.5f); // 0.5-2.0
            if (!knobLocked[4]) snapshot.form2.color = -12.0f + juce::Random::getSystemRandom().nextFloat() * (12.0f + 12.0f); // -12 to +12
            if (!knobLocked[5]) snapshot.form2.motion = juce::Random::getSystemRandom().nextFloat(); // 0-1
            if (!knobLocked[6]) snapshot.form2.resynth = juce::Random::getSystemRandom().nextFloat(); // 0-1
            if (!knobLocked[7]) snapshot.form2.mix = juce::Random::getSystemRandom().nextFloat(); // 0-1
            
            processorRef.setForm2StepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Chorus: {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Randomize Chorus parameters
            if (!knobLocked[0]) snapshot.chorus.delayTime = 5.0f + juce::Random::getSystemRandom().nextFloat() * (50.0f - 5.0f);
            if (!knobLocked[1]) snapshot.chorus.rate = 0.02f + juce::Random::getSystemRandom().nextFloat() * (8.0f - 0.02f);
            if (!knobLocked[2]) snapshot.chorus.depth = juce::Random::getSystemRandom().nextFloat() * 12.0f;
            if (!knobLocked[3]) snapshot.chorus.feedback = juce::Random::getSystemRandom().nextFloat() * 0.9f;
            if (!knobLocked[4]) snapshot.chorus.voices = 2.0f + juce::Random::getSystemRandom().nextFloat() * (8.0f - 2.0f);
            if (!knobLocked[5]) snapshot.chorus.width = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[6]) snapshot.chorus.tone = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[7]) snapshot.chorus.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Reverb: {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Randomize Reverb parameters
            if (!knobLocked[0]) snapshot.reverb.type = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[1]) snapshot.reverb.size = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[2]) snapshot.reverb.predelayMs = juce::Random::getSystemRandom().nextFloat() * 200.0f;
            if (!knobLocked[3]) snapshot.reverb.dampHz = 1000.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 1000.0f);
            if (!knobLocked[4]) snapshot.reverb.diffusion = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[5]) snapshot.reverb.early = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[6]) snapshot.reverb.decaySec = 0.1f + juce::Random::getSystemRandom().nextFloat() * (10.0f - 0.1f);
            if (!knobLocked[7]) snapshot.reverb.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Granular: {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Randomize Granular parameters
            if (!knobLocked[0]) snapshot.granular.sizeMs = 1.0f + juce::Random::getSystemRandom().nextFloat() * (100.0f - 1.0f);
            if (!knobLocked[1]) snapshot.granular.densityHz = 0.1f + juce::Random::getSystemRandom().nextFloat() * (50.0f - 0.1f);
            if (!knobLocked[2]) snapshot.granular.position = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[3]) snapshot.granular.sprayMs = juce::Random::getSystemRandom().nextFloat() * 50.0f;
            if (!knobLocked[4]) snapshot.granular.pitchSemi = -24.0f + juce::Random::getSystemRandom().nextFloat() * (24.0f - (-24.0f));
            if (!knobLocked[5]) snapshot.granular.random = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[6]) snapshot.granular.texture = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[7]) snapshot.granular.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Slicer: {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Randomize Slicer parameters
            if (!knobLocked[0]) snapshot.slicer.pattern = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[1]) snapshot.slicer.division = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[2]) snapshot.slicer.offset = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[3]) snapshot.slicer.shape = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[4]) snapshot.slicer.releaseMs = 1.0f + juce::Random::getSystemRandom().nextFloat() * (1000.0f - 1.0f);
            if (!knobLocked[5]) snapshot.slicer.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setStepSnapshot(step, snapshot);
            break;
        }
        
        case FxPageID::Redux: {
            auto snapshot = processorRef.getReduxSafeSnapshot(step);
            
            // Randomize Redux parameters
            if (!knobLocked[0]) {
                // Generate UI value (1-12) then convert to internal value (4-16)
                int uiBitDepth = 1 + juce::Random::getSystemRandom().nextInt(12); // 1-12
                snapshot.redux.bitDepth = uiBitDepth + 3; // Convert to 4-16
            }
            if (!knobLocked[1]) snapshot.redux.sampleRateReduction = 1 + juce::Random::getSystemRandom().nextInt(32); // 1-32
            if (!knobLocked[2]) snapshot.redux.jitter = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[3]) snapshot.redux.preFilter = 20.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 20.0f);
            if (!knobLocked[4]) snapshot.redux.postFilter = 20.0f + juce::Random::getSystemRandom().nextFloat() * (20000.0f - 20.0f);
            if (!knobLocked[5]) snapshot.redux.drive = juce::Random::getSystemRandom().nextFloat() * 10.0f;
            if (!knobLocked[6]) snapshot.redux.emphasis = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[7]) snapshot.redux.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setReduxStepSnapshot(step, snapshot);
            
            // Update UI to show the randomized values for the current step
            if (step == reduxUiSelectedStep) {
                loadSelectedStepIntoKnobs(FxPageID::Redux);
                updateReduxSequencerUI();
            }
            break;
        }
        
        case FxPageID::PhaseBloom: {
            auto snapshot = processorRef.getSafeSnapshot(step);
            
            // Randomize PhaseBloom parameters
            if (!knobLocked[0]) snapshot.phasebloom.depth = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[1]) snapshot.phasebloom.rate = 0.1f + juce::Random::getSystemRandom().nextFloat() * (8.0f - 0.1f);
            if (!knobLocked[2]) snapshot.phasebloom.feedback = juce::Random::getSystemRandom().nextFloat() * 0.95f;
            if (!knobLocked[3]) snapshot.phasebloom.center = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[4]) snapshot.phasebloom.bloom = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[5]) snapshot.phasebloom.spread = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[6]) snapshot.phasebloom.resonance = juce::Random::getSystemRandom().nextFloat();
            if (!knobLocked[7]) snapshot.phasebloom.mix = juce::Random::getSystemRandom().nextFloat();
            
            processorRef.setStepSnapshot(step, snapshot);
            break;
        }
        
        default:
            DBG("[UI] Unknown effect type for randomization: " << static_cast<int>(effect));
            break;
    }
}

void PluginEditor::loadSelectedStepIntoKnobs(FxPageID effect)
{
    DBG("[UI] Loading selected step into knobs for effect " << static_cast<int>(effect));
    
    // CRITICAL: Temporarily disable All Steps to prevent it from triggering during step loading
    bool wasAllStepsEnabled = allStepsEnabled;
    allStepsEnabled = false;
    
    switch (effect) {
        case FxPageID::SpaceDelay: {
            int selectedStep = processorRef.getSpaceDelayUiSelectedStep();
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getSpaceDelaySafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs using APVTS parameter conversion
            processorRef.getAPVTS().getParameter("timeMs")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("timeMs")->convertTo0to1(snapshot.delay.timeMs));
            processorRef.getAPVTS().getParameter("feedback")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("feedback")->convertTo0to1(snapshot.delay.feedback));
            processorRef.getAPVTS().getParameter("wowDepth")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("wowDepth")->convertTo0to1(snapshot.delay.wowDepth));
            processorRef.getAPVTS().getParameter("wowRate")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("wowRate")->convertTo0to1(snapshot.delay.wowRate));
            processorRef.getAPVTS().getParameter("drive")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("drive")->convertTo0to1(snapshot.delay.saturation));
            processorRef.getAPVTS().getParameter("hiCut")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("hiCut")->convertTo0to1(snapshot.delay.highCut));
            processorRef.getAPVTS().getParameter("lowCut")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("lowCut")->convertTo0to1(snapshot.delay.lowCut));
            processorRef.getAPVTS().getParameter("mix")->setValueNotifyingHost(processorRef.getAPVTS().getParameter("mix")->convertTo0to1(snapshot.delay.mix));
                break;
        }
        
        case FxPageID::Panner: {
            int selectedStep = autopanUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getAutoPanSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (autopanKnobs[0]) autopanKnobs[0]->setValue(snapshot.autopan.rate, juce::dontSendNotification);
            if (autopanKnobs[1]) autopanKnobs[1]->setValue(snapshot.autopan.phase, juce::dontSendNotification);
            if (autopanKnobs[2]) autopanKnobs[2]->setValue((float)snapshot.autopan.waveType, juce::dontSendNotification);
            if (autopanKnobs[3]) autopanKnobs[3]->setValue(snapshot.autopan.waveShape, juce::dontSendNotification);
            if (autopanKnobs[4]) autopanKnobs[4]->setValue(snapshot.autopan.inverted ? 1.0f : 0.0f, juce::dontSendNotification);
            if (autopanKnobs[5]) autopanKnobs[5]->setValue(snapshot.autopan.amount, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Dirt: {
            int selectedStep = dirtUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getDirtSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (dirtKnobs[0]) dirtKnobs[0]->setValue(snapshot.dirt.drive, juce::dontSendNotification);
            if (dirtKnobs[1]) dirtKnobs[1]->setValue(snapshot.dirt.color, juce::dontSendNotification);
            if (dirtKnobs[2]) dirtKnobs[2]->setValue(snapshot.dirt.asym, juce::dontSendNotification);
            if (dirtKnobs[3]) dirtKnobs[3]->setValue(snapshot.dirt.texture, juce::dontSendNotification);
            if (dirtKnobs[4]) dirtKnobs[4]->setValue(snapshot.dirt.lowCut, juce::dontSendNotification);
            if (dirtKnobs[5]) dirtKnobs[5]->setValue(snapshot.dirt.highCut, juce::dontSendNotification);
            if (dirtKnobs[6]) dirtKnobs[6]->setValue(snapshot.dirt.tone, juce::dontSendNotification);
            if (dirtKnobs[7]) dirtKnobs[7]->setValue(snapshot.dirt.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Chorus: {
            int selectedStep = chorusUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getChorusSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (chorusKnobs[0]) chorusKnobs[0]->setValue(snapshot.chorus.delayTime, juce::dontSendNotification);
            if (chorusKnobs[1]) chorusKnobs[1]->setValue(snapshot.chorus.rate, juce::dontSendNotification);
            if (chorusKnobs[2]) chorusKnobs[2]->setValue(snapshot.chorus.depth, juce::dontSendNotification);
            if (chorusKnobs[3]) chorusKnobs[3]->setValue(snapshot.chorus.feedback, juce::dontSendNotification);
            if (chorusKnobs[4]) chorusKnobs[4]->setValue(snapshot.chorus.voices, juce::dontSendNotification);
            if (chorusKnobs[5]) chorusKnobs[5]->setValue(snapshot.chorus.width, juce::dontSendNotification);
            if (chorusKnobs[6]) chorusKnobs[6]->setValue(snapshot.chorus.tone, juce::dontSendNotification);
            if (chorusKnobs[7]) chorusKnobs[7]->setValue(snapshot.chorus.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Reverb: {
            int selectedStep = reverbUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getReverbSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (reverbKnobs[0]) reverbKnobs[0]->setValue(snapshot.reverb.type, juce::dontSendNotification);
            if (reverbKnobs[1]) reverbKnobs[1]->setValue(snapshot.reverb.size, juce::dontSendNotification);
            if (reverbKnobs[2]) reverbKnobs[2]->setValue(snapshot.reverb.predelayMs, juce::dontSendNotification);
            if (reverbKnobs[3]) reverbKnobs[3]->setValue(snapshot.reverb.dampHz, juce::dontSendNotification);
            if (reverbKnobs[4]) reverbKnobs[4]->setValue(snapshot.reverb.diffusion, juce::dontSendNotification);
            if (reverbKnobs[5]) reverbKnobs[5]->setValue(snapshot.reverb.early, juce::dontSendNotification);
            if (reverbKnobs[6]) reverbKnobs[6]->setValue(snapshot.reverb.decaySec, juce::dontSendNotification);
            if (reverbKnobs[7]) reverbKnobs[7]->setValue(snapshot.reverb.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Granular: {
            int selectedStep = granularUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getGranularSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (granularKnobs[0]) granularKnobs[0]->setValue(snapshot.granular.sizeMs, juce::dontSendNotification);
            if (granularKnobs[1]) granularKnobs[1]->setValue(snapshot.granular.densityHz, juce::dontSendNotification);
            if (granularKnobs[2]) granularKnobs[2]->setValue(snapshot.granular.position, juce::dontSendNotification);
            if (granularKnobs[3]) granularKnobs[3]->setValue(snapshot.granular.sprayMs, juce::dontSendNotification);
            if (granularKnobs[4]) granularKnobs[4]->setValue(snapshot.granular.pitchSemi, juce::dontSendNotification);
            if (granularKnobs[5]) granularKnobs[5]->setValue(snapshot.granular.random, juce::dontSendNotification);
            if (granularKnobs[6]) granularKnobs[6]->setValue(snapshot.granular.texture, juce::dontSendNotification);
            if (granularKnobs[7]) granularKnobs[7]->setValue(snapshot.granular.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Slicer: {
            int selectedStep = slicerUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getSlicerSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (slicerKnobs[0]) slicerKnobs[0]->setValue(snapshot.slicer.pattern, juce::dontSendNotification);
            if (slicerKnobs[1]) slicerKnobs[1]->setValue(snapshot.slicer.division, juce::dontSendNotification);
            if (slicerKnobs[2]) slicerKnobs[2]->setValue(snapshot.slicer.offset, juce::dontSendNotification);
            if (slicerKnobs[3]) slicerKnobs[3]->setValue(snapshot.slicer.shape, juce::dontSendNotification);
            if (slicerKnobs[4]) slicerKnobs[4]->setValue(snapshot.slicer.releaseMs, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Redux: {
            int selectedStep = reduxUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getReduxSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (reduxKnobs[0]) {
                float uiBitDepth = (float)snapshot.redux.bitDepth - 3.0f;
                reduxKnobs[0]->setValue(uiBitDepth, juce::dontSendNotification);
            }
            if (reduxKnobs[1]) reduxKnobs[1]->setValue((float)snapshot.redux.sampleRateReduction, juce::dontSendNotification);
            if (reduxKnobs[2]) reduxKnobs[2]->setValue(snapshot.redux.jitter, juce::dontSendNotification);
            if (reduxKnobs[3]) reduxKnobs[3]->setValue(snapshot.redux.preFilter, juce::dontSendNotification);
            if (reduxKnobs[4]) reduxKnobs[4]->setValue(snapshot.redux.postFilter, juce::dontSendNotification);
            if (reduxKnobs[5]) reduxKnobs[5]->setValue(snapshot.redux.drive, juce::dontSendNotification);
            if (reduxKnobs[6]) reduxKnobs[6]->setValue(snapshot.redux.emphasis, juce::dontSendNotification);
            if (reduxKnobs[7]) reduxKnobs[7]->setValue(snapshot.redux.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::PhaseBloom: {
            int selectedStep = phaseBloomUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getPhaseBloomSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (phaseBloomKnobs[0]) phaseBloomKnobs[0]->setValue(snapshot.phasebloom.depth, juce::dontSendNotification);
            if (phaseBloomKnobs[1]) phaseBloomKnobs[1]->setValue(snapshot.phasebloom.rate, juce::dontSendNotification);
            if (phaseBloomKnobs[2]) phaseBloomKnobs[2]->setValue(snapshot.phasebloom.feedback, juce::dontSendNotification);
            if (phaseBloomKnobs[3]) phaseBloomKnobs[3]->setValue(snapshot.phasebloom.center, juce::dontSendNotification);
            if (phaseBloomKnobs[4]) phaseBloomKnobs[4]->setValue(snapshot.phasebloom.bloom, juce::dontSendNotification);
            if (phaseBloomKnobs[5]) phaseBloomKnobs[5]->setValue(snapshot.phasebloom.spread, juce::dontSendNotification);
            if (phaseBloomKnobs[6]) phaseBloomKnobs[6]->setValue(snapshot.phasebloom.resonance, juce::dontSendNotification);
            if (phaseBloomKnobs[7]) phaseBloomKnobs[7]->setValue(snapshot.phasebloom.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Form2: {
            int selectedStep = form2UiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getForm2SafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs
            if (form2Knobs[0]) form2Knobs[0]->setValue(snapshot.form2.rootNote / 12.0f, juce::dontSendNotification);
            if (form2Knobs[1]) form2Knobs[1]->setValue(snapshot.form2.scale / 7.0f, juce::dontSendNotification);
            if (form2Knobs[2]) form2Knobs[2]->setValue((snapshot.form2.chordSize - 1) / 7.0f, juce::dontSendNotification);
            if (form2Knobs[3]) form2Knobs[3]->setValue(snapshot.form2.shift, juce::dontSendNotification);
            if (form2Knobs[4]) form2Knobs[4]->setValue(snapshot.form2.color, juce::dontSendNotification);
            if (form2Knobs[5]) form2Knobs[5]->setValue(snapshot.form2.motion, juce::dontSendNotification);
            if (form2Knobs[6]) form2Knobs[6]->setValue(snapshot.form2.resynth, juce::dontSendNotification);
            if (form2Knobs[7]) form2Knobs[7]->setValue(snapshot.form2.mix, juce::dontSendNotification);
                break;
        }
        
        case FxPageID::Formant: {
            int selectedStep = formantUiSelectedStep;
            selectedStep = juce::jlimit(0, 15, selectedStep);
            const auto snapshot = processorRef.getFormantSafeSnapshot(selectedStep);
            
            // Load snapshot values into knobs (now only 4)
            if (formantKnobs[0]) formantKnobs[0]->setValue((float)snapshot.formant.vowel, juce::dontSendNotification);
            if (formantKnobs[1]) formantKnobs[1]->setValue(snapshot.formant.resonance, juce::dontSendNotification);
            if (formantKnobs[2]) formantKnobs[2]->setValue(snapshot.formant.intensity, juce::dontSendNotification);
            if (formantKnobs[3]) formantKnobs[3]->setValue(snapshot.formant.mix, juce::dontSendNotification);
                break;
        }
        
        default:
            DBG("[UI] loadSelectedStepIntoKnobs not implemented for effect: " << static_cast<int>(effect));
            break;
    }
    
    // CRITICAL: Restore All Steps state
    allStepsEnabled = wasAllStepsEnabled;
    
    DBG("[UI] Loaded step values into knobs, All Steps restored to: " << (allStepsEnabled ? "ON" : "OFF"));
}

void PluginEditor::onUnifiedStepButtonClicked(int stepIndex)
{
    // Handle Space Delay specifically since it uses global knobs and All Steps toggle
    if (currentPage == FxPageID::SpaceDelay) {
        // Save current step's snapshot before switching (if All Steps is OFF)
        if (!allStepsEnabled) {
            saveCurrentStepSnapshot();
        }
        
        // Update selected step in processor
        processorRef.setSpaceDelaySelectedStep(stepIndex);
        
        // Update UI to show which step is selected
        updateSequencerUI();
        
        // Load the snapshot for this step into the knobs (without triggering All Steps)
        loadSelectedStepIntoKnobs(FxPageID::SpaceDelay);
        return;
    }
    
    // For other effects, use the generic approach
    // Save current step's snapshot before switching (if All Steps is OFF)
    if (!allStepsEnabled) {
        saveCurrentStepSnapshot();
    }
    
    // Update selected step in processor
    updateSelectedStepInProcessor(stepIndex);
    
    // Update UI to show which step is selected
    updateSequencerUI();
    
    // Load the snapshot for this step into the knobs (without triggering All Steps)
    loadSelectedStepIntoKnobs(currentPage);
}

void PluginEditor::updateUnifiedAllStepSnapshots(int knobIndex)
{
    DBG("[UI] Unified All Steps update for knob " << knobIndex << " on effect " << static_cast<int>(currentPage));
    
    // TODO: Implement unified All Steps logic for all effects
    // This will replace the current updateAllStepSnapshots method
}

void PluginEditor::saveCurrentStepSnapshot()
{
    DBG("[UI] Saving current step snapshot for effect " << static_cast<int>(currentPage));
    
    switch (currentPage) {
        case FxPageID::SpaceDelay: {
            int currentStep = processorRef.getSpaceDelayUiSelectedStep();
            if (currentStep >= 0 && currentStep < 16) {
                StepSnapshot currentSnapshot;
                // Read current knob values and save to snapshot
                for (int i = 0; i < 8; ++i) {
                    if (knobs[i] != nullptr) {
                        auto* param = processorRef.getAPVTS().getParameter(getParameterIdForKnob(i));
                        if (param != nullptr) {
                            float actualValue = param->convertFrom0to1(knobs[i]->getValue());
                            updateSnapshotValue(currentSnapshot, i, actualValue);
                        }
                    }
                }
                processorRef.setSpaceDelayStepSnapshot(currentStep, currentSnapshot);
                DBG("[UI] Saved Space Delay snapshot for step " << currentStep);
            }
                break;
        }
        
        case FxPageID::Formant: {
            int currentStep = processorRef.getFormantUiSelectedStep();
            if (currentStep >= 0 && currentStep < 16) {
                StepSnapshot currentSnapshot;
                // Read current knob values and save to snapshot (now only 4 knobs)
                for (int i = 0; i < 4; ++i) {
                    if (knobs[i] != nullptr) {
                        auto* param = processorRef.getAPVTS().getParameter(getParameterIdForKnob(i));
                        if (param != nullptr) {
                            float actualValue = param->convertFrom0to1(knobs[i]->getValue());
                            updateSnapshotValue(currentSnapshot, i, actualValue);
                        }
                    }
                }
                processorRef.setFormantStepSnapshot(currentStep, currentSnapshot);
                DBG("[UI] Saved Formant snapshot for step " << currentStep);
            }
                break;
        }
        
        // TODO: Implement for other effects
        case FxPageID::Panner:
        case FxPageID::Dirt:
        case FxPageID::Chorus:
        case FxPageID::Reverb:
        case FxPageID::Granular:
        case FxPageID::Slicer:
        case FxPageID::Redux:
        case FxPageID::PhaseBloom:
        default:
            DBG("[UI] saveCurrentStepSnapshot not implemented for effect: " << static_cast<int>(currentPage));
                break;
    }
}

void PluginEditor::updateSelectedStepInProcessor(int stepIndex)
{
    DBG("[UI] Updating selected step in processor to " << stepIndex << " for effect " << static_cast<int>(currentPage));
    
    switch (currentPage) {
        case FxPageID::SpaceDelay:
            processorRef.setSpaceDelaySelectedStep(stepIndex);
                break;
            
        case FxPageID::Formant:
            processorRef.setFormantSelectedStep(stepIndex);
                break;
            
        // TODO: Implement for other effects
        case FxPageID::Panner:
        case FxPageID::Dirt:
        case FxPageID::Chorus:
        case FxPageID::Reverb:
        case FxPageID::Granular:
        case FxPageID::Slicer:
        case FxPageID::Redux:
        case FxPageID::PhaseBloom:
        default:
            DBG("[UI] updateSelectedStepInProcessor not implemented for effect: " << static_cast<int>(currentPage));
                break;
    }
}

void PluginEditor::updateSnapshotValue(StepSnapshot& snapshot, int knobIndex, float value)
{
    switch (currentPage) {
        case FxPageID::SpaceDelay:
            switch (knobIndex) {
                case 0: snapshot.delay.timeMs = value; break;
                case 1: snapshot.delay.feedback = value; break;
                case 2: snapshot.delay.wowDepth = value; break;
                case 3: snapshot.delay.wowRate = value; break;
                case 4: snapshot.delay.saturation = value; break;
                case 5: snapshot.delay.highCut = value; break;
                case 6: snapshot.delay.lowCut = value; break;
                case 7: snapshot.delay.mix = value; break;
            }
                break;
            
        case FxPageID::Formant:
            switch (knobIndex) {
                case 0: snapshot.formant.vowel = (int)value; break;
                case 1: snapshot.formant.resonance = value; break;
                case 2: snapshot.formant.intensity = value; break;
                case 3: snapshot.formant.mix = value; break;
            }
                break;
            
        // TODO: Implement for other effects
        case FxPageID::Panner:
        case FxPageID::Dirt:
        case FxPageID::Chorus:
        case FxPageID::Reverb:
        case FxPageID::Granular:
        case FxPageID::Slicer:
        case FxPageID::Redux:
        case FxPageID::PhaseBloom:
        default:
                break;
    }
}

juce::String PluginEditor::getParameterIdForKnob(int knobIndex)
{
    switch (currentPage) {
        case FxPageID::SpaceDelay:
            switch (knobIndex) {
                case 0: return "timeMs";
                case 1: return "feedback";
                case 2: return "wowDepth";
                case 3: return "wowRate";
                case 4: return "drive";
                case 5: return "hiCut";
                case 6: return "lowCut";
                case 7: return "mix";
            }
            break;
            
        case FxPageID::Formant:
            switch (knobIndex) {
                case 0: return "vowel";
                case 1: return "resonance";
                case 2: return "intensity";
                case 3: return "mix";
            }
            break;
            
        // TODO: Implement for other effects
        case FxPageID::Panner:
        case FxPageID::Dirt:
        case FxPageID::Chorus:
        case FxPageID::Reverb:
        case FxPageID::Granular:
        case FxPageID::Slicer:
        case FxPageID::Redux:
        case FxPageID::PhaseBloom:
        default:
            break;
    }
    return "";
}

//==============================================================================
// License Management Methods
//==============================================================================

void PluginEditor::checkLicenseOnStartup()
{
    if (!licenseManager)
        return;
    
    // Load saved license state
    licenseManager->loadLicenseState();
    
    // Check if we already have a valid license saved
    if (licenseManager->isLicenseValid())
    {
        DBG("[LICENSE] Valid license found in saved state - no dialog needed");
        repaint(); // Update UI
        return;
    }
    
    auto licenseInfo = licenseManager->getCurrentLicense();
    
    // If we have a saved license key, verify it
    if (!licenseInfo.licenseKey.isEmpty())
    {
        DBG("[LICENSE] Verifying saved license key on startup...");
        licenseManager->verifyLicenseAsync(licenseInfo.licenseKey, false, [this](const GumroadLicenseInfo& info) {
            if (info.isValid())
            {
                DBG("[LICENSE] License verified successfully on startup");
                repaint(); // Update UI
            }
            else
            {
                DBG("[LICENSE] Saved license key is invalid");
                // Show dialog if invalid
                juce::MessageManager::callAsync([this]() {
                    showLicenseDialog();
                });
            }
        });
    }
    else
    {
        // No license key saved - show dialog
        DBG("[LICENSE] No license key found - showing dialog");
        showLicenseDialog();
    }
}

void PluginEditor::showLicenseDialog()
{
    if (!licenseManager)
        return;
    
    DBG("[LICENSE] Showing license dialog");
    GumroadLicenseDialog::showDialog(this, licenseManager.get(), [this]() {
        // User dismissed the dialog - set flag to prevent immediate reopening
        licenseDialogDismissed = true;
        lastLicenseDialogShowTime = juce::Time::getCurrentTime();
        DBG("[LICENSE] Dialog dismissed by user");
    });
}

