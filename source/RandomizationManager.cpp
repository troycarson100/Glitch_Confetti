#include "RandomizationManager.h"
#include "PluginEditor.h"
#include <random>
#include <algorithm>

RandomizationManager::RandomizationManager(PluginProcessor& proc, juce::AudioProcessorValueTreeState& tree, PluginEditor* ed)
    : processor(proc), apvts(tree), editor(ed)
{
    // Reserve space to avoid allocations during randomization
    paramTargets.reserve(40); // 4 pages * ~8 knobs + margin
    stepTargets.reserve(64); // 4 pages * 16 steps
    
    // Seed PRNG
    rngState = static_cast<uint32_t>(juce::Time::currentTimeMillis());
}

void RandomizationManager::requestRandomizeAllActivePages()
{
    // CRITICAL FIX 1: Debounce - prevent rapid-fire clicks
    const juce::int64 currentTime = juce::Time::currentTimeMillis();
    const juce::int64 timeSinceLastRandomization = currentTime - lastRandomizationTime;
    
    if (timeSinceLastRandomization < MIN_RANDOMIZATION_INTERVAL_MS) {
        DBG("[RAND] Request ignored - too soon after last randomization (" 
            << timeSinceLastRandomization << "ms < " << MIN_RANDOMIZATION_INTERVAL_MS << "ms)");
        return;
    }
    
    // CRITICAL FIX 2: Cancel any pending async updates to prevent queue buildup
    cancelPendingUpdate();
    
    // CRITICAL FIX 3: Check if already busy (prevents re-entrancy)
    bool expected = false;
    if (!busy.compare_exchange_strong(expected, true)) {
        DBG("[RAND] Already randomizing, ignoring request");
        return;
    }
    
    // Update debounce timestamp
    lastRandomizationTime = currentTime;
    
    DBG("[RAND] Randomization requested - triggering async update");
    triggerAsyncUpdate();
}

void RandomizationManager::handleAsyncUpdate()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    // CRITICAL FIX: Ensure busy flag is cleared even if we return early
    struct BusyGuard {
        std::atomic<bool>& flag;
        BusyGuard(std::atomic<bool>& f) : flag(f) {}
        ~BusyGuard() { flag.store(false); }
    };
    BusyGuard guard(busy);
    
    // Fix: Check global shutdown flag FIRST to prevent any operations during shutdown
    if (processor.globalShutdownFlag.load()) {
        DBG("[RAND] Shutdown in progress, aborting randomization");
        return;
    }
    
    // Fix: bail immediately if the editor has already been destroyed
    if (editor == nullptr) {
        DBG("[RAND] Editor destroyed, aborting randomization");
        return;
    }
    
    DBG("[RAND] ═══════════════════════════════════════════");
    DBG("[RAND] Starting randomization on message thread");
    randomizeAll();
    DBG("[RAND] ═══════════════════════════════════════════");
    // Busy flag will be cleared by BusyGuard destructor
}

void RandomizationManager::randomizeAll()
{
    // CRITICAL: Step snapshots are already protected by CriticalSection locks in setStepSnapshot methods
    // VST3's suspend/resume mechanism is unreliable and causes audio dropouts
    // We rely on the critical section locks instead, which are thread-safe and don't interrupt audio processing
    // This approach is safer and more reliable for VST3 hosts
    
    // Fix: Check global shutdown flag AFTER guard is created
    if (processor.globalShutdownFlag.load()) {
        return; // Guard will resume processing
    }
    
    // Fix: randomization is skipped if shutdown has already begun
    if (editor == nullptr) {
        return; // Guard will resume processing
    }
    
    // Clear previous stats
    stats = Stats();
    paramTargets.clear();
    stepTargets.clear();
    sequencerTargets.clear();
    
    try {
        // FIRST: Change the effect router to select 4 random effects
        DBG("[RAND] Step 1: Randomizing effect router...");
        randomizeEffectRouter();
        
        // THEN: Collect targets for the newly assigned effects
        DBG("[RAND] Step 2: Collecting targets...");
        collectTargets();
        
        // Apply changes with error handling for each step
        DBG("[RAND] Step 3: Applying step changes...");
        try {
            applyStepChanges();
        } catch (const std::exception& e) {
            DBG("[RAND] ERROR in applyStepChanges: " << e.what());
        } catch (...) {
            DBG("[RAND] ERROR in applyStepChanges: Unknown exception");
        }
        
        DBG("[RAND] Step 4: Applying sequencer changes...");
        try {
            applySequencerChanges();
        } catch (const std::exception& e) {
            DBG("[RAND] ERROR in applySequencerChanges: " << e.what());
        } catch (...) {
            DBG("[RAND] ERROR in applySequencerChanges: Unknown exception");
        }
        
        DBG("[RAND] Step 5: Applying parameter changes...");
        try {
            applyParamChanges();
        } catch (const std::exception& e) {
            DBG("[RAND] ERROR in applyParamChanges: " << e.what());
        } catch (...) {
            DBG("[RAND] ERROR in applyParamChanges: Unknown exception");
        }
        
        // Verify and report
        verifyAndReport();
        
        // Update effect selector dropdowns to reflect new router assignments
        if (editor) {
            DBG("[RAND] Step 6: Updating UI...");
            try {
                editor->updateAllEffectSelectors();
                
                // Refresh the current page to update backgrounds and visibility
                editor->showPage(editor->getCurrentPage());
                
                // Refresh the current page's UI to show updated knobs and sequencer
                editor->refreshCurrentPageUI();
                
                // Force update all sequencer UIs to ensure step indicators are correct
                // This is important because randomization might have changed sequencers on other pages
                // The timer will continuously update these, but we call them once immediately to sync state
                editor->updateSequencerUI();
                editor->updateAutoPanSequencerUI();
                editor->updateDirtSequencerUI();
                editor->updateChorusSequencerUI();
                editor->updateReduxSequencerUI();
                editor->updatePhaseBloomSequencerUI();
                editor->updateSaturateSequencerUI();
                editor->updateGranularSequencerUI();
                editor->updateReverbSequencerUI();
                editor->updateSlicerSequencerUI();
                editor->updateDubDelaySequencerUI();
                editor->updateFormantSequencerUI();
                editor->updateFilterSequencerUI();
                editor->repaint(); // Force repaint to show updated step indicators
            } catch (const std::exception& e) {
                DBG("[RAND] ERROR updating UI: " << e.what());
            } catch (...) {
                DBG("[RAND] ERROR updating UI: Unknown exception");
            }
        }
    } catch (const std::exception& e) {
        DBG("[RAND] FATAL ERROR in randomizeAll: " << e.what());
        // Processing will still be resumed by ProcessingGuard
    } catch (...) {
        DBG("[RAND] FATAL ERROR in randomizeAll: Unknown exception");
        // Processing will still be resumed by ProcessingGuard
    }
    
    // Processing will be resumed automatically by ProcessingGuard destructor
}

void RandomizationManager::randomizeEffectRouter()
{
    // Get all available effects (excluding master/compressor and Form2)
    std::vector<EffectID> availableEffects;
    for (int i = 0; i <= 13; ++i) { // EffectID::SpaceDelay (0) to EffectID::Filter (13)
        EffectID effect = static_cast<EffectID>(i);
        // Exclude Form2 (11) - using Formant (10) instead
        if (effect != EffectID::Form2) {
            availableEffects.push_back(effect);
        }
    }
    
    // Randomly select 4 effects
    std::vector<EffectID> selectedEffects;
    std::sample(availableEffects.begin(), availableEffects.end(),
                std::back_inserter(selectedEffects), 4,
                std::mt19937{std::random_device{}()});
    
    // Assign the selected effects to the 4 slots
    auto& router = processor.getEffectRouter();
    for (int slot = 0; slot < 4; ++slot) {
        router.assignEffectToSlot(selectedEffects[slot], static_cast<SlotID>(slot));
    }
}

void RandomizationManager::collectTargets()
{
    // Get the currently active pages (after router randomization)
    auto activePages = registry.getActivePages(processor, apvts);
    
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto& page = activePages[slot];
        
        // Collect knob parameters
        for (const auto& paramId : page.knobParamIds)
        {
            auto* param = apvts.getParameter(paramId);
            if (!param) {
                continue;
            }
            
            ParamTarget target;
            target.param = param;
            target.paramId = paramId;
            target.currentNorm = param->getValue();
            target.locked = isParamLocked(paramId);
            
            paramTargets.push_back(target);
            stats.paramsExpected++;
            
            if (target.locked)
                stats.paramsLocked++;
        }
        
        // Collect step targets (all 16 steps)
        EffectID effect = processor.getEffectRouter().getEffectInSlot(static_cast<SlotID>(slot));
        for (int step = 0; step < page.maxSteps; ++step)
        {
            StepTarget target;
            target.effect = effect;
            target.stepIndex = step;
            target.locked = isStepLocked(effect, step);
            
            stepTargets.push_back(target);
            stats.stepsExpected++;
            
            if (target.locked)
                stats.stepsLocked++;
        }
        
        // Collect sequencer settings
        SequencerTarget seqTarget;
        seqTarget.effect = effect;
        seqTarget.pageId = page.pageId;
        seqTarget.maxSteps = page.maxSteps;
        seqTarget.maxDivisionIndex = page.maxDivisionIndex;
        seqTarget.locked = false; // TODO: Add sequencer lock checking
        sequencerTargets.push_back(seqTarget);
        stats.sequencersExpected++;
    }
}

// Helper function to safely set parameter value directly (bypassing slider to avoid snapToLegalValue crash)
// This sets the parameter in APVTS directly, and the SliderAttachment will automatically sync the slider
static void safeSetParameterValue(juce::RangedAudioParameter* param, float denormalizedValue)
{
    if (!param)
        return;
    
    // Check if value is finite
    if (!std::isfinite(denormalizedValue)) {
        juce::String paramName = param->getName(100);
        DBG("[RAND] ERROR: Non-finite value for parameter " << paramName);
        return;
    }
    
    try {
        // Handle different parameter types safely
        auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param);
        auto* intParam = dynamic_cast<juce::AudioParameterInt*>(param);
        
        if (choiceParam) {
            // For Choice parameters, convert float to int index, then normalize
            int numChoices = choiceParam->choices.size();
            if (numChoices > 0) {
                int index = juce::roundToInt(juce::jlimit(0.0f, static_cast<float>(numChoices - 1), denormalizedValue));
                // Normalize: index / (numChoices - 1)
                float normalizedValue = numChoices > 1 ? static_cast<float>(index) / static_cast<float>(numChoices - 1) : 0.0f;
                normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
                // Set parameter directly - SliderAttachment will sync the slider automatically
                param->setValueNotifyingHost(normalizedValue);
            }
        } else if (intParam) {
            // For Int parameters, convert float to int, then normalize
            const auto& range = intParam->getNormalisableRange();
            int minValue = static_cast<int>(range.start);
            int maxValue = static_cast<int>(range.end);
            int intValue = juce::roundToInt(juce::jlimit(static_cast<float>(minValue), static_cast<float>(maxValue), denormalizedValue));
            // Normalize: (intValue - minValue) / (maxValue - minValue)
            float normalizedValue = (maxValue > minValue) ? static_cast<float>(intValue - minValue) / static_cast<float>(maxValue - minValue) : 0.0f;
            normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
            // Set parameter directly - SliderAttachment will sync the slider automatically
            param->setValueNotifyingHost(normalizedValue);
        } else {
            // For Float parameters, manually calculate normalized value to avoid snapToLegalValue
            const auto& range = param->getNormalisableRange();
            // Clamp denormalized value to parameter's range
            float clampedDenorm = juce::jlimit(range.start, range.end, denormalizedValue);
            
            // Manual conversion to avoid snap function issues
            // Normalize: (value - start) / (end - start)
            float normalizedValue = 0.0f;
            if (range.end > range.start) {
                normalizedValue = (clampedDenorm - range.start) / (range.end - range.start);
            }
            normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
            // Set parameter directly - SliderAttachment will sync the slider automatically
            param->setValueNotifyingHost(normalizedValue);
        }
    } catch (const std::exception& e) {
        juce::String paramName = param->getName(100);
        DBG("[RAND] ERROR: Exception setting parameter value for " << paramName << ": " << juce::String(e.what()));
    } catch (...) {
        juce::String paramName = param->getName(100);
        DBG("[RAND] ERROR: Unknown exception setting parameter value for " << paramName);
    }
}

void RandomizationManager::applyParamChanges()
{
    // After randomizing all step snapshots, reload current step into knobs for each effect
    // IMPORTANT: Don't call onStepButtonClicked() - it saves current knobs FIRST, overwriting our randomization!
    // Instead, directly load snapshots into knobs
    
    if (!editor)
        return;
    
    // Set flag in editor to block onValueChange callbacks during this reload
    editor->isLoadingFromSnapshot.store(true);
    
    auto& router = processor.getEffectRouter();
    
    for (int slot = 0; slot < 4; ++slot)
    {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slot));
        
        switch (effect)
        {
            case EffectID::SpaceDelay:
            {
                int step = processor.getSpaceDelayUiSelectedStep();
                if (step >= 0 && step < 16) {
                    auto s = processor.getSpaceDelaySafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"delayTimeMs", "delayFeedback", "delayWowDepth", "delayWowRate", 
                                                          "delaySaturation", "delayHighCut", "delayLowCut", "delayMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.delay.timeMs);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.delay.feedback * 100.0f);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.delay.wowDepth * 100.0f);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.delay.wowRate);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.delay.saturation * 100.0f);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.delay.highCut);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.delay.lowCut);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.delay.mix * 100.0f);
                    DBG("[RAND]   SpaceDelay step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::AutoPan:
            {
                // Enable autopan effect
                auto* autopanEnabledParam = apvts.getParameter("autopanEnabled");
                if (autopanEnabledParam) {
                    safeSetParameterValue(autopanEnabledParam, 1.0f);
                }
                
                int step = editor->autopanUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getAutoPanSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"autopanRate", "autopanPhase", "autopanWaveType", "autopanWaveShape", 
                                                          "autopanInverted", "autopanAmount"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.autopan.rate);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.autopan.phase);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), (float)s.autopan.waveType);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.autopan.waveShape);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.autopan.inverted ? 1.0f : 0.0f);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.autopan.amount);
                    DBG("[RAND]   AutoPan step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Dirt:
            {
                int step = editor->dirtUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getDirtSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"dirtDrive", "dirtColor", "dirtAsym", "dirtTexture", 
                                                          "dirtLowCut", "dirtHighCut", "dirtTone", "dirtMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.dirt.drive);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.dirt.color);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.dirt.asym);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.dirt.texture);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.dirt.lowCut);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.dirt.highCut);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.dirt.tone);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.dirt.mix);
                    DBG("[RAND]   Dirt step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Chorus:
            {
                int step = editor->chorusUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getChorusSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"chorusDelayMs", "chorusRateHz", "chorusDepthMs", "chorusFeedback", 
                                                          "chorusVoices", "chorusWidth", "chorusShape", "chorusMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.chorus.delayTime);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.chorus.rate);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.chorus.depth);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.chorus.feedback);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.chorus.voices);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.chorus.width);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.chorus.tone);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.chorus.mix);
                    DBG("[RAND]   Chorus step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Reverb:
            {
                // Enable reverb effect
                auto* verbEnabledParam = apvts.getParameter("verbEnabled");
                if (verbEnabledParam) {
                    safeSetParameterValue(verbEnabledParam, 1.0f);
                }
                
                int step = editor->reverbUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getReverbSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"verbWidth", "verbSize", "verbPredelayMs", "verbDampHz", 
                                                          "verbDiffusion", "verbEarlyLevel", "verbDecaySec", "verbMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.reverb.type);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.reverb.size);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.reverb.predelayMs);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.reverb.dampHz);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.reverb.diffusion);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.reverb.early);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.reverb.decaySec);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.reverb.mix);
                    DBG("[RAND]   Reverb step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Granular:
            {
                int step = editor->granularUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getGranularSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"granSizeMs", "granDensityHz", "granPosition", "granSprayMs",
                                                          "granPitchSemi", "granRandom", "granTexture", "granMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.granular.sizeMs);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.granular.densityHz);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.granular.position);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.granular.sprayMs);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.granular.pitchSemi);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.granular.random);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.granular.texture);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.granular.mix);
                    DBG("[RAND]   Granular step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Slicer:
            {
                int step = editor->slicerUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getSlicerSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"slicerPattern", "slicerDivision", "slicerOffset", "slicerShape", 
                                                          "slicerReleaseMs", "slicerMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.slicer.pattern);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.slicer.division);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.slicer.offset);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.slicer.shape);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.slicer.releaseMs);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.slicer.mix);
                    DBG("[RAND]   Slicer step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::DubDelay:
            {
                int step = editor->dubdelayUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getDubDelaySafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"dubTimeMs", "dubFeedback", "dubToneHz", "dubDrive", 
                                                          "dubPingPong", "dubWowFlutter", "dubRegenDamp", "dubMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.dubdelay.timeMs);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.dubdelay.feedback);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.dubdelay.toneHz);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.dubdelay.drive);
                    // Knob 4 is PingPong toggle - represented as 0 or 1
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.dubdelay.pingPong ? 1.0f : 0.0f);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.dubdelay.wowFlutter);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.dubdelay.regenDamp);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.dubdelay.mix);
                    DBG("[RAND]   DubDelay step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Formant:
            {
                int step = editor->formantUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getFormantSafeSnapshot(step);
                    // Formant has 4 snapshot parameters: vowel, resonance, intensity, mix
                    // Note: Other params (formantShift, formantBrightness, formantMotion, formantAir) are regular APVTS params
                    auto* vowelParam = apvts.getParameter("formantVowel");
                    auto* resonanceParam = apvts.getParameter("formantResonance");
                    auto* intensityParam = apvts.getParameter("formantIntensity");
                    auto* mixParam = apvts.getParameter("formantMix");
                    safeSetParameterValue(vowelParam, s.formant.vowel);
                    safeSetParameterValue(resonanceParam, s.formant.resonance);
                    safeSetParameterValue(intensityParam, s.formant.intensity);
                    safeSetParameterValue(mixParam, s.formant.mix);
                    
                    // Also randomize the bottom 4 knobs (Shift, Brightness, Motion, Air) which are regular APVTS params
                    auto* shiftParam = apvts.getParameter("formantShift");
                    auto* brightnessParam = apvts.getParameter("formantBrightness");
                    auto* motionParam = apvts.getParameter("formantMotion");
                    auto* airParam = apvts.getParameter("formantAir");
                    if (shiftParam) safeSetParameterValue(shiftParam, 0.5f + rand01() * 1.5f); // 0.5-2.0
                    if (brightnessParam) safeSetParameterValue(brightnessParam, -12.0f + rand01() * 24.0f); // -12 to +12 dB
                    if (motionParam) safeSetParameterValue(motionParam, rand01()); // 0-1
                    if (airParam) safeSetParameterValue(airParam, rand01()); // 0-1
                    
                    DBG("[RAND]   Formant step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Saturate:
            {
                int step = editor->saturateUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getSaturateSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"satType", "satDrive", "satColor", "satShape", 
                                                          "satBias", "satOut", "satMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.saturate.type);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.saturate.drive);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.saturate.color);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.saturate.shape);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.saturate.bias);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.saturate.output);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.saturate.mix);
                    DBG("[RAND]   Saturate step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Filter:
            {
                // CRITICAL: Filter effect requires extra validation to prevent crashes
                // CRITICAL FIX: Skip Filter parameter updates during rapid randomization
                // Filter parameters are very sensitive to rapid changes and cause audio dropouts
                // The step snapshots are still randomized, but we don't reload them into knobs
                // This prevents rapid parameter changes that cause crashes
                
                // Only enable the filter, but skip parameter updates
                auto* filterEnabledParam = apvts.getParameter("filterEnabled");
                if (filterEnabledParam) {
                    safeSetParameterValue(filterEnabledParam, 1.0f);
                }
                
                // Skip reloading Filter parameters to prevent rapid changes
                // The snapshots are still randomized, but won't be applied until user manually selects a step
                break;
            }
            
            case EffectID::Redux:
            {
                // Enable redux effect
                auto* reduxEnabledParam = apvts.getParameter("reduxEnabled");
                if (reduxEnabledParam) {
                    safeSetParameterValue(reduxEnabledParam, 1.0f);
                }
                
                int step = editor->reduxUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getReduxSafeSnapshot(step);
                    std::vector<juce::String> paramIds = {"reduxBitDepth", "reduxSampleRateReduction", "reduxJitter", "reduxPreFilter",
                                                          "reduxPostFilter", "reduxDrive", "reduxEmphasis", "reduxMix"};
                    safeSetParameterValue(apvts.getParameter(paramIds[0]), s.redux.bitDepth);
                    safeSetParameterValue(apvts.getParameter(paramIds[1]), s.redux.sampleRateReduction);
                    safeSetParameterValue(apvts.getParameter(paramIds[2]), s.redux.jitter);
                    safeSetParameterValue(apvts.getParameter(paramIds[3]), s.redux.preFilter);
                    safeSetParameterValue(apvts.getParameter(paramIds[4]), s.redux.postFilter);
                    safeSetParameterValue(apvts.getParameter(paramIds[5]), s.redux.drive);
                    safeSetParameterValue(apvts.getParameter(paramIds[6]), s.redux.emphasis);
                    safeSetParameterValue(apvts.getParameter(paramIds[7]), s.redux.mix);
                    DBG("[RAND]   Redux step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    // Clear the flag
    editor->isLoadingFromSnapshot.store(false);
    
    stats.paramsRandomized = paramTargets.size() - stats.paramsLocked;
}

void RandomizationManager::applyStepChanges()
{
    if (stepTargets.empty())
        return;
    
    DBG("[RAND] Applying step changes...");
    
    // Randomize each step's snapshot with per-effect error handling
    for (const auto& target : stepTargets)
    {
        if (target.locked)
            continue;
        
        // Randomize based on effect type
        switch (target.effect)
        {
            case EffectID::SpaceDelay:
            {
                auto snapshot = processor.getSpaceDelaySafeSnapshot(target.stepIndex);
                snapshot.delay.timeMs = 50.0f + rand01() * 950.0f;
                snapshot.delay.feedback = rand01() * 95.0f;
                snapshot.delay.wowDepth = rand01() * 100.0f;
                snapshot.delay.wowRate = 0.1f + rand01() * 7.9f;
                snapshot.delay.saturation = rand01() * 100.0f;
                snapshot.delay.highCut = 1000.0f + rand01() * 19000.0f;
                snapshot.delay.lowCut = 20.0f + rand01() * 1980.0f;
                snapshot.delay.mix = rand01() * 100.0f;
                processor.setSpaceDelayStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::AutoPan:
            {
                auto snapshot = processor.getAutoPanSafeSnapshot(target.stepIndex);
                snapshot.autopan.rate = 0.01f + rand01() * 9.99f;
                snapshot.autopan.phase = rand01() * 360.0f;
                snapshot.autopan.waveType = static_cast<int>(rand01() * 4.999f);
                snapshot.autopan.waveShape = rand01();
                snapshot.autopan.inverted = rand01() > 0.5f;
                snapshot.autopan.amount = rand01();
                processor.setAutoPanStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Dirt:
            {
                auto snapshot = processor.getDirtSafeSnapshot(target.stepIndex);
                snapshot.dirt.drive = rand01() * 36.0f;
                snapshot.dirt.color = -1.0f + rand01() * 2.0f;
                snapshot.dirt.asym = -1.0f + rand01() * 2.0f;
                snapshot.dirt.texture = rand01();
                snapshot.dirt.lowCut = 20.0f + rand01() * 280.0f;
                snapshot.dirt.highCut = 3000.0f + rand01() * 19000.0f;
                snapshot.dirt.tone = -1.0f + rand01() * 2.0f;
                snapshot.dirt.mix = rand01();
                processor.setDirtStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Chorus:
            {
                auto snapshot = processor.getChorusSafeSnapshot(target.stepIndex);
                snapshot.chorus.delayTime = 5.0f + rand01() * 45.0f;
                snapshot.chorus.rate = 0.02f + rand01() * 7.98f;
                snapshot.chorus.depth = rand01() * 12.0f;
                snapshot.chorus.feedback = rand01() * 0.9f;
                snapshot.chorus.voices = 2.0f + rand01() * 6.0f;
                snapshot.chorus.width = rand01();
                snapshot.chorus.tone = rand01();
                snapshot.chorus.mix = rand01();
                processor.setChorusStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Reverb:
            {
                auto snapshot = processor.getReverbSafeSnapshot(target.stepIndex);
                snapshot.reverb.type = rand01(); // Width
                snapshot.reverb.size = 0.1f + rand01() * 1.4f;
                snapshot.reverb.predelayMs = rand01() * 200.0f;
                snapshot.reverb.dampHz = 1000.0f + rand01() * 19000.0f;
                snapshot.reverb.diffusion = rand01();
                snapshot.reverb.early = rand01();
                snapshot.reverb.decaySec = 0.2f + rand01() * 19.8f;
                snapshot.reverb.mix = rand01();
                processor.setReverbStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Granular:
            {
                auto snapshot = processor.getGranularSafeSnapshot(target.stepIndex);
                snapshot.granular.sizeMs = 5.0f + rand01() * 195.0f; // 5-200ms
                snapshot.granular.densityHz = 1.0f + rand01() * 39.0f; // 1-40Hz
                snapshot.granular.position = rand01(); // 0-1
                snapshot.granular.sprayMs = rand01() * 200.0f; // 0-200ms
                snapshot.granular.pitchSemi = -24.0f + rand01() * 48.0f; // -24 to +24
                snapshot.granular.random = rand01(); // 0-1
                snapshot.granular.texture = rand01(); // 0-1
                snapshot.granular.mix = rand01(); // 0-1
                processor.setGranularStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Slicer:
            {
                auto snapshot = processor.getSlicerSafeSnapshot(target.stepIndex);
                snapshot.slicer.pattern = std::floor(rand01() * 8.0f); // 0-7
                snapshot.slicer.division = std::floor(rand01() * 6.0f); // 0-5
                snapshot.slicer.offset = rand01(); // 0-1 (bipolar in engine)
                snapshot.slicer.shape = 0.2f + rand01() * 0.6f; // 0.2-0.8 (musical range)
                snapshot.slicer.releaseMs = 10.0f + rand01() * 50.0f; // 10-60ms (musical range)
                snapshot.slicer.mix = 0.3f + rand01() * 0.7f; // 0.3-1.0 (audible range)
                processor.setSlicerStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::DubDelay:
            {
                auto snapshot = processor.getDubDelaySafeSnapshot(target.stepIndex);
                snapshot.dubdelay.timeMs = 100.0f + rand01() * 1400.0f; // 100-1500ms (musical range)
                snapshot.dubdelay.feedback = 0.2f + rand01() * 0.65f; // 0.2-0.85 (safe musical range)
                snapshot.dubdelay.toneHz = 1000.0f + rand01() * 14000.0f; // 1-15 kHz (musical range)
                snapshot.dubdelay.drive = rand01() * 0.6f; // 0-0.6 (gentle to moderate)
                snapshot.dubdelay.pingPong = (rand01() > 0.5f); // Random ping-pong
                snapshot.dubdelay.wowFlutter = rand01() * 0.5f; // 0-0.5 (subtle to moderate)
                snapshot.dubdelay.regenDamp = rand01() * 0.6f; // 0-0.6 (subtle to moderate damping)
                snapshot.dubdelay.mix = 0.2f + rand01() * 0.6f; // 0.2-0.8 (audible range)
                processor.setDubDelayStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Formant:
            {
                auto snapshot = processor.getFormantSafeSnapshot(target.stepIndex);
                snapshot.formant.vowel = rand01() * 4.0f; // 0-4 (A=0, E=1, I=2, O=3, U=4)
                snapshot.formant.resonance = 0.4f + rand01() * 17.6f; // 0.4-18
                snapshot.formant.intensity = -6.0f + rand01() * 24.0f; // -6 to +18
                snapshot.formant.mix = rand01(); // 0-1
                processor.setFormantStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Saturate:
            {
                auto snapshot = processor.getSaturateSafeSnapshot(target.stepIndex);
                
                // CRITICAL: Never randomize Saturate type - keep it completely stable
                // Rapid type changes (0-7) cause crashes during rapid randomization
                // Only randomize other parameters but keep type unchanged from existing snapshot
                // snapshot.saturate.type is NOT randomized - uses existing type
                
                // Conservative parameter randomization to prevent crashes
                // Drive: ±30% of current value (or default 12.0f if current is invalid)
                float currentDrive = snapshot.saturate.drive;
                if (currentDrive <= 0.0f || currentDrive > 36.0f || !std::isfinite(currentDrive)) {
                    currentDrive = 12.0f; // Safe default
                }
                float driveChange = (rand01() - 0.5f) * 0.6f; // ±30%
                snapshot.saturate.drive = juce::jlimit(0.0f, 36.0f, currentDrive * (1.0f + driveChange));
                
                // Color: ±20% of current value
                float currentColor = snapshot.saturate.color;
                if (currentColor < 0.0f || currentColor > 1.0f || !std::isfinite(currentColor)) {
                    currentColor = 0.5f; // Safe default
                }
                float colorChange = (rand01() - 0.5f) * 0.4f; // ±20%
                snapshot.saturate.color = juce::jlimit(0.0f, 1.0f, currentColor * (1.0f + colorChange));
                
                // Shape: ±20% of current value
                float currentShape = snapshot.saturate.shape;
                if (currentShape < 0.0f || currentShape > 1.0f || !std::isfinite(currentShape)) {
                    currentShape = 0.5f; // Safe default
                }
                float shapeChange = (rand01() - 0.5f) * 0.4f; // ±20%
                snapshot.saturate.shape = juce::jlimit(0.0f, 1.0f, currentShape * (1.0f + shapeChange));
                
                // Bias: ±50% of current value (small range anyway)
                float currentBias = snapshot.saturate.bias;
                if (!std::isfinite(currentBias)) {
                    currentBias = 0.0f; // Safe default
                }
                float biasChange = (rand01() - 0.5f) * 0.1f; // ±0.1
                snapshot.saturate.bias = juce::jlimit(-0.2f, 0.2f, currentBias + biasChange);
                
                // Output: ±30% of current value
                float currentOutput = snapshot.saturate.output;
                if (!std::isfinite(currentOutput)) {
                    currentOutput = 0.0f; // Safe default
                }
                float outputChange = (rand01() - 0.5f) * 0.6f; // ±30%
                snapshot.saturate.output = juce::jlimit(-24.0f, 12.0f, currentOutput * (1.0f + outputChange));
                
                // Mix: ±20% of current value
                float currentMix = snapshot.saturate.mix;
                if (currentMix < 0.0f || currentMix > 1.0f || !std::isfinite(currentMix)) {
                    currentMix = 1.0f; // Safe default
                }
                float mixChange = (rand01() - 0.5f) * 0.4f; // ±20%
                snapshot.saturate.mix = juce::jlimit(0.0f, 1.0f, currentMix * (1.0f + mixChange));
                
                // Validate all parameters are finite
                if (!std::isfinite(snapshot.saturate.drive)) snapshot.saturate.drive = 12.0f;
                if (!std::isfinite(snapshot.saturate.color)) snapshot.saturate.color = 0.5f;
                if (!std::isfinite(snapshot.saturate.shape)) snapshot.saturate.shape = 0.5f;
                if (!std::isfinite(snapshot.saturate.bias)) snapshot.saturate.bias = 0.0f;
                if (!std::isfinite(snapshot.saturate.output)) snapshot.saturate.output = 0.0f;
                if (!std::isfinite(snapshot.saturate.mix)) snapshot.saturate.mix = 1.0f;
                
                processor.setSaturateStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Filter:
            {
                // CRITICAL: Filter effect requires strict validation to prevent crashes
                // CRITICAL FIX: NEVER randomize Filter type - keep it completely stable
                // Rapid filter type changes cause audio dropouts and high-pitched sounds
                // Only randomize other parameters (cutoff, resonance, etc.) but keep type unchanged
                auto snapshot = processor.getFilterSafeSnapshot(target.stepIndex);
                
                // Keep the existing filter type - DO NOT randomize it
                int filterType = static_cast<int>(snapshot.filter.type);
                filterType = juce::jlimit(0, 4, filterType);
                snapshot.filter.type = static_cast<float>(filterType);
                
                // CRITICAL: Cutoff range depends on filter type
                // Comb filters (type 3,4) have range 40-8000 Hz, others have 20-20000 Hz
                // CRITICAL FIX: Use conservative ranges to prevent rapid changes that cause audio dropouts
                float minCutoff = (filterType >= 3) ? 40.0f : 20.0f;
                float maxCutoff = (filterType >= 3) ? 8000.0f : 20000.0f;
                
                // Get current cutoff and only randomize within a small range around it (±20%)
                // This prevents large jumps that cause audio dropouts
                float currentCutoff = snapshot.filter.cutoff;
                if (!std::isfinite(currentCutoff) || currentCutoff <= 0.0f) {
                    currentCutoff = (filterType >= 3) ? 2000.0f : 1200.0f; // Safe default
                }
                float cutoffRange = currentCutoff * 0.2f; // ±20% range
                float newCutoff = currentCutoff + (rand01() - 0.5f) * cutoffRange * 2.0f;
                
                // Ensure cutoff is finite and within valid range
                if (!std::isfinite(newCutoff) || newCutoff <= 0.0f) {
                    newCutoff = currentCutoff; // Keep current if invalid
                }
                snapshot.filter.cutoff = juce::jlimit(minCutoff, maxCutoff, newCutoff);
                
                // Resonance: Use conservative range around current value (±30%) to prevent rapid changes
                float currentRes = snapshot.filter.resonance;
                if (!std::isfinite(currentRes) || currentRes < 0.0f) {
                    currentRes = 0.35f; // Safe default
                }
                float resRange = currentRes * 0.3f; // ±30% range
                float newRes = currentRes + (rand01() - 0.5f) * resRange * 2.0f;
                if (!std::isfinite(newRes) || newRes < 0.0f) {
                    newRes = currentRes; // Keep current if invalid
                }
                snapshot.filter.resonance = juce::jlimit(0.0f, 0.95f, newRes);
                
                // Slope: 0-1 (12dB to 24dB)
                float slope = rand01();
                snapshot.filter.slope = juce::jlimit(0.0f, 1.0f, slope);
                
                // Drive: 0-18 dB (safe range)
                float drive = rand01() * 18.0f;
                if (!std::isfinite(drive) || drive < 0.0f) {
                    drive = 6.0f; // Safe default
                }
                snapshot.filter.drive = juce::jlimit(0.0f, 18.0f, drive);
                
                // Spread: -50 to +50 cents
                float spread = -50.0f + rand01() * 100.0f;
                if (!std::isfinite(spread)) {
                    spread = 0.0f;
                }
                snapshot.filter.spread = juce::jlimit(-50.0f, 50.0f, spread);
                
                // Keytrack: 0-1
                float keytrack = rand01();
                snapshot.filter.keytrack = juce::jlimit(0.0f, 1.0f, keytrack);
                
                // Mix: 0-1
                float mix = rand01();
                if (!std::isfinite(mix) || mix < 0.0f) {
                    mix = 1.0f; // Safe default
                }
                snapshot.filter.mix = juce::jlimit(0.0f, 1.0f, mix);
                
                processor.setFilterStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            default:
                break;
        }
        
        stats.stepsRandomized++;
    }
    
}

void RandomizationManager::applySequencerChanges()
{
    if (sequencerTargets.empty())
        return;
    
    for (const auto& target : sequencerTargets)
    {
        if (target.locked)
        {
            stats.sequencersLocked++;
            continue;
        }
        
        // Randomize sequencer enabled state (70% chance to be enabled)
        // EXCEPTION: DubDelay sequencer is always disabled
        bool sequencerEnabled = (target.effect == EffectID::DubDelay) 
            ? false 
            : (rand01() < 0.7f);
        
        // Randomize steps used (4-16, weighted toward higher values)
        int stepsUsed = 4 + static_cast<int>(rand01() * rand01() * 12); // Square for bias toward higher values
        
        // Randomize division index (0-7: 4 bars to 1/32)
        int divisionIndex = static_cast<int>(rand01() * 8); // Range: 0-7 (4 bars to 1/32)
        
        // Apply sequencer settings based on effect type
        switch (target.effect)
        {
            case EffectID::SpaceDelay:
                processor.setSpaceDelaySequencerEnabled(sequencerEnabled);
                processor.setSpaceDelayStepsUsed(stepsUsed);
                processor.setSpaceDelayDivisionIndex(divisionIndex);
                // Ensure sequencer is active (setSpaceDelaySequencerEnabled already sets active when enabled=true)
                // Just ensure it will lock in properly by clearing origin if it was previously locked in
                if (sequencerEnabled) {
                    auto& spacedelaySeq = const_cast<SeqState&>(processor.getSpaceDelaySeqState());
                    // Clear origin so it can lock in fresh on next play edge
                    spacedelaySeq.haveOrigin.store(false);
                    spacedelaySeq.originPPQ.store(0.0);
                }
                // Update main rate dropdown to match (SpaceDelay uses the main rateDropdown)
                // IMPORTANT: Also manually call setSpaceDelayDivisionIndex again to ensure sequencer uses the correct value
                if (editor && editor->rateDropdown) {
                    editor->rateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Manually call setSpaceDelayDivisionIndex again to ensure it's applied
                    processor.setSpaceDelayDivisionIndex(divisionIndex);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::AutoPan:
                processor.setAutoPanSequencerEnabled(sequencerEnabled);
                processor.setAutoPanStepsUsed(stepsUsed);
                processor.setAutoPanDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->autopanRateDropdown) {
                    editor->autopanRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::Dirt:
                processor.setDirtSequencerEnabled(sequencerEnabled);
                processor.setDirtStepsUsed(stepsUsed);
                processor.setDirtDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->dirtRateDropdown) {
                    editor->dirtRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::Chorus:
                processor.setChorusSequencerEnabled(sequencerEnabled);
                processor.setChorusStepsUsed(stepsUsed);
                processor.setChorusDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->chorusRateDropdown) {
                    editor->chorusRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::Reverb:
                processor.setReverbSequencerEnabled(sequencerEnabled);
                processor.setReverbStepsUsed(stepsUsed);
                processor.setReverbDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->reverbRateDropdown) {
                    editor->reverbRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                }
                break;
                
            case EffectID::Granular:
                processor.setGranularSequencerEnabled(sequencerEnabled);
                processor.setGranularStepsUsed(stepsUsed);
                processor.setGranularDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->granularRateDropdown) {
                    editor->granularRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                }
                break;
                
            case EffectID::Slicer:
                processor.setSlicerSequencerEnabled(sequencerEnabled);
                processor.setSlicerStepsUsed(stepsUsed);
                processor.setSlicerDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->slicerRateDropdown) {
                    editor->slicerRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                }
                break;
                
            case EffectID::DubDelay:
                processor.setDubDelaySequencerEnabled(sequencerEnabled);
                processor.setDubDelayStepsUsed(stepsUsed);
                processor.setDubDelayDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->dubdelayRateDropdown) {
                    editor->dubdelayRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                }
                break;
                
            case EffectID::Redux:
                processor.setReduxSequencerEnabled(sequencerEnabled);
                processor.setReduxStepsUsed(stepsUsed);
                processor.setReduxDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                // IMPORTANT: Also manually call setReduxDivisionIndex again to ensure sequencer uses the correct value
                if (editor && editor->reduxRateDropdown) {
                    editor->reduxRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Manually call setReduxDivisionIndex again to ensure it's applied
                    processor.setReduxDivisionIndex(divisionIndex);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::PhaseBloom:
                processor.setPhaseBloomSequencerEnabled(sequencerEnabled);
                processor.setPhaseBloomStepsUsed(stepsUsed);
                processor.setPhaseBloomDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                // IMPORTANT: Also manually call setPhaseBloomDivisionIndex again to ensure sequencer uses the correct value
                if (editor && editor->phaseBloomRateDropdown) {
                    editor->phaseBloomRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Manually call setPhaseBloomDivisionIndex again to ensure it's applied
                    processor.setPhaseBloomDivisionIndex(divisionIndex);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::Formant:
                processor.setFormantSequencerEnabled(sequencerEnabled);
                processor.setFormantStepsUsed(stepsUsed);
                processor.setFormantDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                // IMPORTANT: Also manually trigger the onChange callback to ensure sequencer uses the correct value
                if (editor && editor->formantRateDropdown) {
                    editor->formantRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Manually call setFormantDivisionIndex again to ensure it's applied
                    processor.setFormantDivisionIndex(divisionIndex);
                }
                break;
                
            case EffectID::Saturate:
                processor.setSaturateSequencerEnabled(sequencerEnabled);
                processor.setSaturateStepsUsed(stepsUsed);
                processor.setSaturateDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                // IMPORTANT: Also manually call setSaturateDivisionIndex again to ensure sequencer uses the correct value
                if (editor && editor->saturateRateDropdown) {
                    editor->saturateRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                    // Manually call setSaturateDivisionIndex again to ensure it's applied
                    processor.setSaturateDivisionIndex(divisionIndex);
                    // Note: UI update will happen via timerCallback and refreshCurrentPageUI()
                }
                break;
                
            case EffectID::Filter:
                processor.setFilterSequencerEnabled(sequencerEnabled);
                processor.setFilterStepsUsed(stepsUsed);
                processor.setFilterDivisionIndex(divisionIndex);
                // Update UI dropdown to match (dropdown IDs are 1-8, so add 1 to index)
                if (editor && editor->filterRateDropdown) {
                    editor->filterRateDropdown->setSelectedId(divisionIndex + 1, juce::dontSendNotification);
                }
                break;
        }
        
        // If sequencer was enabled, immediately lock it in to current transport position
        // This ensures it starts running right away if transport is already playing
        if (sequencerEnabled) {
            processor.forceSequencerLockIn(target.effect);
            // Also set forceAllSequencersLockIn flag to ensure lock-in happens in next processBlock
            // This handles the case where transport isn't playing yet or PPQ isn't valid
            processor.setForceAllSequencersLockIn(true);
        }
        
        stats.sequencersRandomized++;
    }
}

void RandomizationManager::verifyAndReport()
{
    DBG("[RAND] ══════════ RANDOMIZATION REPORT ══════════");
    DBG("[RAND] Pages: 4 random effects");
    DBG("[RAND] Knobs: " + juce::String(stats.paramsRandomized) + "/" + juce::String(stats.paramsExpected) 
        + " (" + juce::String(stats.paramsLocked) + " locked)");
    DBG("[RAND] Steps: " + juce::String(stats.stepsRandomized) + "/" + juce::String(stats.stepsExpected)
        + " (" + juce::String(stats.stepsLocked) + " locked)");
    DBG("[RAND] Sequencers: " + juce::String(stats.sequencersRandomized) + "/" + juce::String(stats.sequencersExpected)
        + " (" + juce::String(stats.sequencersLocked) + " locked)");
    
    // Verify non-zero coverage
    if (stats.paramsRandomized == 0 && stats.paramsExpected > 0)
        DBG("[RAND] ERROR: No parameters randomized!");
    if (stats.stepsRandomized == 0 && stats.stepsExpected > 0)
        DBG("[RAND] ERROR: No steps randomized!");
    if (stats.sequencersRandomized == 0 && stats.sequencersExpected > 0)
        DBG("[RAND] ERROR: No sequencers randomized!");
    
    DBG("[RAND] ═══════════════════════════════════════════");
}

float RandomizationManager::rand01()
{
    // Fast xorshift PRNG
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(rngState) / static_cast<float>(0xFFFFFFFFu);
}

bool RandomizationManager::isParamLocked(const juce::String& paramId) const
{
    // TODO: Check if parameter is locked
    // For now, return false (nothing locked)
    return false;
}

bool RandomizationManager::isStepLocked(EffectID effect, int step) const
{
    // TODO: Check if step is locked
    // For now, return false (nothing locked)
    return false;
}
