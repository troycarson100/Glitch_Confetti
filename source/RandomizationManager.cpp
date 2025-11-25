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
    // Debounce: ignore if already busy
    if (busy.exchange(true)) {
        DBG("[RAND] Already randomizing, ignoring request");
        return;
    }
    
    DBG("[RAND] Randomization requested - triggering async update");
    triggerAsyncUpdate();
}

void RandomizationManager::handleAsyncUpdate()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    // Fix: Check global shutdown flag FIRST to prevent any operations during shutdown
    if (processor.globalShutdownFlag.load()) {
        busy.store(false);
        return;
    }
    
    // Fix: bail immediately if the editor has already been destroyed
    if (editor == nullptr) {
        busy.store(false);
        return;
    }
    
    DBG("[RAND] ═══════════════════════════════════════════");
    DBG("[RAND] Starting randomization on message thread");
    randomizeAll();
    busy.store(false);
    DBG("[RAND] ═══════════════════════════════════════════");
}

void RandomizationManager::randomizeAll()
{
    // Fix: Check global shutdown flag FIRST to prevent any operations during shutdown
    if (processor.globalShutdownFlag.load()) {
        return;
    }
    
    // Fix: randomization is skipped if shutdown has already begun
    if (editor == nullptr) {
        return;
    }
    
    // Suspend processing briefly
    processor.suspendProcessing(true);
    
    // Clear previous stats
    stats = Stats();
    paramTargets.clear();
    stepTargets.clear();
    sequencerTargets.clear();
    
    // FIRST: Change the effect router to select 4 random effects
    randomizeEffectRouter();
    
    // THEN: Collect targets for the newly assigned effects
    collectTargets();
    
    // Apply changes
    applyParamChanges();
    applyStepChanges();
    applySequencerChanges();
    
    // Verify and report
    verifyAndReport();
    
    // Update effect selector dropdowns to reflect new router assignments
    if (editor) {
        editor->updateAllEffectSelectors();
    }
    
    // Resume processing
    processor.suspendProcessing(false);
}

void RandomizationManager::randomizeEffectRouter()
{
    DBG("[RAND] Randomizing effect router assignments...");
    
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
        DBG("[RAND] Assigned " + juce::String(static_cast<int>(selectedEffects[slot])) + " to slot " + juce::String(slot));
    }
}

void RandomizationManager::collectTargets()
{
    DBG("[RAND] Collecting targets...");
    
    // Get the currently active pages (after router randomization)
    auto activePages = registry.getActivePages(processor, apvts);
    
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto& page = activePages[slot];
        DBG("[RAND] Active Effect " + juce::String(slot) + ": " + page.pageId);
        
        // Collect knob parameters
        for (const auto& paramId : page.knobParamIds)
        {
            auto* param = apvts.getParameter(paramId);
            if (!param) {
                DBG("[RAND]   WARNING: Parameter '" + paramId + "' not found!");
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
    
    DBG("[RAND] Collected " + juce::String(paramTargets.size()) + " params, " 
        + juce::String(stepTargets.size()) + " steps, " 
        + juce::String(sequencerTargets.size()) + " sequencers");
}

// Helper function to safely set parameter value directly (bypassing slider to avoid snapToLegalValue crash)
// This sets the parameter in APVTS directly, and the SliderAttachment will automatically sync the slider
static void safeSetParameterValue(juce::RangedAudioParameter* param, float denormalizedValue)
{
    if (!param)
        return;
    
    // Check if value is finite
    if (!std::isfinite(denormalizedValue)) {
        DBG("[RAND] ERROR: Non-finite value for parameter " + param->getName());
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
        DBG("[RAND] ERROR: Exception setting parameter value for " + param->getName() + ": " + juce::String(e.what()));
    } catch (...) {
        DBG("[RAND] ERROR: Unknown exception setting parameter value for " + param->getName());
    }
}

void RandomizationManager::applyParamChanges()
{
    // After randomizing all step snapshots, reload current step into knobs for each effect
    // IMPORTANT: Don't call onStepButtonClicked() - it saves current knobs FIRST, overwriting our randomization!
    // Instead, directly load snapshots into knobs
    
    if (!editor)
        return;
    
    DBG("[RAND] Reloading current steps into knobs...");
    
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
                int step = processor.getSelectedStep();
                if (step >= 0 && step < 16) {
                    auto s = processor.getSafeSnapshot(step);
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
                // Enable filter effect
                auto* filterEnabledParam = apvts.getParameter("filterEnabled");
                if (filterEnabledParam) {
                    safeSetParameterValue(filterEnabledParam, 1.0f);
                }
                
                int step = editor->filterUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getFilterSafeSnapshot(step);
                    // Filter uses special knobs: filterTypeKnob (0), filterKnobs[0-2] (Cutoff, Res, Drive), filterSlopeKnob (3), filterKnobs[3-4] (Key Track, Mix)
                    // Parameter IDs: "fType", "cutoff", "res", "slope", "filterDrive", "keytrack", "filterMix"
                    // Note: keytrack might not be a real parameter, it's stored in snapshot but may not have APVTS param
                    auto* typeParam = apvts.getParameter("fType");
                    auto* cutoffParam = apvts.getParameter("cutoff");
                    auto* resParam = apvts.getParameter("res");
                    auto* slopeParam = apvts.getParameter("slope");
                    auto* driveParam = apvts.getParameter("filterDrive");
                    auto* keytrackParam = apvts.getParameter("keytrack");
                    auto* mixParam = apvts.getParameter("filterMix");
                    
                    // Type knob (special knob, not in filterKnobs array)
                    safeSetParameterValue(typeParam, s.filter.type);
                    // Cutoff (filterKnobs[0])
                    safeSetParameterValue(cutoffParam, s.filter.cutoff);
                    // Resonance (filterKnobs[1])
                    safeSetParameterValue(resParam, s.filter.resonance);
                    // Slope knob (special knob, not in filterKnobs array)
                    safeSetParameterValue(slopeParam, s.filter.slope);
                    // Drive (filterKnobs[2])
                    safeSetParameterValue(driveParam, s.filter.drive);
                    // Key Track (filterKnobs[3]) - only set if parameter exists
                    if (keytrackParam) {
                        safeSetParameterValue(keytrackParam, s.filter.keytrack);
                    }
                    // Mix (filterKnobs[4])
                    safeSetParameterValue(mixParam, s.filter.mix);
                    DBG("[RAND]   Filter step " + juce::String(step) + " reloaded");
                }
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
    DBG("[RAND] isLoadingFromSnapshot flag cleared");
    
    stats.paramsRandomized = paramTargets.size() - stats.paramsLocked;
}

void RandomizationManager::applyStepChanges()
{
    if (stepTargets.empty())
        return;
    
    DBG("[RAND] Applying step changes...");
    
    // Randomize each step's snapshot
    for (const auto& target : stepTargets)
    {
        if (target.locked)
            continue;
        
        // Randomize based on effect type
        switch (target.effect)
        {
            case EffectID::SpaceDelay:
            {
                auto snapshot = processor.getSafeSnapshot(target.stepIndex);
                snapshot.delay.timeMs = 50.0f + rand01() * 950.0f;
                snapshot.delay.feedback = rand01() * 95.0f;
                snapshot.delay.wowDepth = rand01() * 100.0f;
                snapshot.delay.wowRate = 0.1f + rand01() * 7.9f;
                snapshot.delay.saturation = rand01() * 100.0f;
                snapshot.delay.highCut = 1000.0f + rand01() * 19000.0f;
                snapshot.delay.lowCut = 20.0f + rand01() * 1980.0f;
                snapshot.delay.mix = rand01() * 100.0f;
                processor.setStepSnapshot(target.stepIndex, snapshot);
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
                snapshot.saturate.type = std::floor(rand01() * 8.0f); // 0-7
                snapshot.saturate.drive = rand01() * 36.0f; // 0-36 dB
                snapshot.saturate.color = rand01(); // 0-1 (dynamic based on model)
                snapshot.saturate.shape = rand01(); // 0-1 (dynamic based on model)
                snapshot.saturate.bias = -0.2f + rand01() * 0.4f; // -0.2 to 0.2
                snapshot.saturate.output = -24.0f + rand01() * 36.0f; // -24 to +12 dB
                snapshot.saturate.mix = rand01(); // 0-1
                processor.setSaturateStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Filter:
            {
                auto snapshot = processor.getFilterSafeSnapshot(target.stepIndex);
                snapshot.filter.type = std::floor(rand01() * 5.0f); // 0-4 (LP, HP, BP, Comb-, Comb+)
                snapshot.filter.cutoff = 20.0f + rand01() * 19980.0f; // 20-20000 Hz
                snapshot.filter.resonance = rand01(); // 0-1
                snapshot.filter.slope = rand01(); // 0-1 (12dB to 24dB)
                snapshot.filter.drive = rand01() * 18.0f; // 0-18 dB (50% of 36 dB, displayed as 0-100%)
                snapshot.filter.spread = -50.0f + rand01() * 100.0f; // -50 to +50 cents
                snapshot.filter.keytrack = rand01(); // 0-1
                snapshot.filter.mix = rand01(); // 0-1
                processor.setFilterStepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            default:
                break;
        }
        
        stats.stepsRandomized++;
    }
    
    DBG("[RAND] Randomized " + juce::String(stats.stepsRandomized) + " steps");
}

void RandomizationManager::applySequencerChanges()
{
    if (sequencerTargets.empty())
        return;
    
    DBG("[RAND] Applying sequencer changes...");
    
    for (const auto& target : sequencerTargets)
    {
        if (target.locked)
        {
            stats.sequencersLocked++;
            continue;
        }
        
        // Randomize sequencer enabled state (70% chance to be enabled)
        bool sequencerEnabled = (rand01() < 0.7f);
        
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
                break;
                
            case EffectID::AutoPan:
                processor.setAutoPanSequencerEnabled(sequencerEnabled);
                processor.setAutoPanStepsUsed(stepsUsed);
                processor.setAutoPanDivisionIndex(divisionIndex);
                break;
                
            case EffectID::Dirt:
                processor.setDirtSequencerEnabled(sequencerEnabled);
                processor.setDirtStepsUsed(stepsUsed);
                processor.setDirtDivisionIndex(divisionIndex);
                break;
                
            case EffectID::Chorus:
                processor.setChorusSequencerEnabled(sequencerEnabled);
                processor.setChorusStepsUsed(stepsUsed);
                processor.setChorusDivisionIndex(divisionIndex);
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
                break;
                
            case EffectID::Redux:
                processor.setReduxSequencerEnabled(sequencerEnabled);
                processor.setReduxStepsUsed(stepsUsed);
                processor.setReduxDivisionIndex(divisionIndex);
                break;
                
            case EffectID::PhaseBloom:
                processor.setPhaseBloomSequencerEnabled(sequencerEnabled);
                processor.setPhaseBloomStepsUsed(stepsUsed);
                processor.setPhaseBloomDivisionIndex(divisionIndex);
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
                break;
                
            case EffectID::Filter:
                processor.setFilterSequencerEnabled(sequencerEnabled);
                processor.setFilterStepsUsed(stepsUsed);
                processor.setFilterDivisionIndex(divisionIndex);
                break;
        }
        
        // If sequencer was enabled, immediately lock it in to current transport position
        // This ensures it starts running right away if transport is already playing
        if (sequencerEnabled) {
            processor.forceSequencerLockIn(target.effect);
        }
        
        DBG("[RAND] " + target.pageId + ": enabled=" + (sequencerEnabled ? "ON" : "OFF") 
            + ", steps=" + juce::String(stepsUsed) + ", division=" + juce::String(divisionIndex));
        
        stats.sequencersRandomized++;
    }
    
    DBG("[RAND] Randomized " + juce::String(stats.sequencersRandomized) + " sequencers");
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
