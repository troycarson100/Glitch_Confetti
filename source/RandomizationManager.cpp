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
    
    DBG("[RAND] ═══════════════════════════════════════════");
    DBG("[RAND] Starting randomization on message thread");
    randomizeAll();
    
    // Refresh UI to show new effects after randomization
    if (editor) {
        // Update tab button images to reflect new router assignments
        editor->updateTabButtonImages();
        // Refresh effect selector dropdowns
        auto& router = processor.getEffectRouter();
        if (editor->effectSelector1) editor->effectSelector1->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot1)) + 1, juce::dontSendNotification);
        if (editor->effectSelector2) editor->effectSelector2->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot2)) + 1, juce::dontSendNotification);
        if (editor->effectSelector3) editor->effectSelector3->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot3)) + 1, juce::dontSendNotification);
        if (editor->effectSelector4) editor->effectSelector4->setSelectedId(static_cast<int>(router.getEffectInSlot(SlotID::Slot4)) + 1, juce::dontSendNotification);
        
        // Check if Filter is assigned to any slot and switch to that page
        // Map slots to FxPageID: Slot1→SpaceDelay, Slot2→Panner, Slot3→Dirt, Slot4→Chorus
        if (router.getEffectInSlot(SlotID::Slot1) == EffectID::Filter) {
            editor->showPage(FxPageID::SpaceDelay);
        } else if (router.getEffectInSlot(SlotID::Slot2) == EffectID::Filter) {
            editor->showPage(FxPageID::Panner);
        } else if (router.getEffectInSlot(SlotID::Slot3) == EffectID::Filter) {
            editor->showPage(FxPageID::Dirt);
        } else if (router.getEffectInSlot(SlotID::Slot4) == EffectID::Filter) {
            editor->showPage(FxPageID::Chorus);
        } else {
            // Filter not assigned, just refresh current page
            editor->showPage(editor->currentPage);
        }
        editor->repaint();
    }
    
    busy.store(false);
    DBG("[RAND] ═══════════════════════════════════════════");
}

void RandomizationManager::randomizeAll()
{
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
    
    // Resume processing
    processor.suspendProcessing(false);
}

void RandomizationManager::randomizeEffectRouter()
{
    DBG("[RAND] Randomizing effect router assignments...");
    
    // Get all available effects (excluding master/compressor)
    std::vector<EffectID> availableEffects;
    for (int i = 0; i <= 13; ++i) { // EffectID::SpaceDelay (0) to EffectID::Filter (13)
        availableEffects.push_back(static_cast<EffectID>(i));
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
                    // Load into knobs (which triggers APVTS update via attachments)
                    if (editor->knobs[0]) editor->knobs[0]->setValue((s.delay.timeMs - 10.0f) / (2000.0f - 10.0f), juce::sendNotification);
                    if (editor->knobs[1]) editor->knobs[1]->setValue(s.delay.feedback / 100.0f, juce::sendNotification);
                    if (editor->knobs[2]) editor->knobs[2]->setValue(s.delay.wowDepth / 100.0f, juce::sendNotification);
                    if (editor->knobs[3]) editor->knobs[3]->setValue((s.delay.wowRate - 0.1f) / (8.0f - 0.1f), juce::sendNotification);
                    if (editor->knobs[4]) editor->knobs[4]->setValue(s.delay.saturation / 100.0f, juce::sendNotification);
                    if (editor->knobs[5]) editor->knobs[5]->setValue((s.delay.highCut - 1000.0f) / (20000.0f - 1000.0f), juce::sendNotification);
                    if (editor->knobs[6]) editor->knobs[6]->setValue((s.delay.lowCut - 20.0f) / (2000.0f - 20.0f), juce::sendNotification);
                    if (editor->knobs[7]) editor->knobs[7]->setValue(s.delay.mix / 100.0f, juce::sendNotification);
                    DBG("[RAND]   SpaceDelay step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::AutoPan:
            {
                int step = editor->autopanUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getAutoPanSafeSnapshot(step);
                    // Load into knobs (CRITICAL: Use dontSendNotification to prevent All Steps trigger)
                    if (editor->autopanKnobs[0]) editor->autopanKnobs[0]->setValue(s.autopan.rate, juce::dontSendNotification);
                    if (editor->autopanKnobs[1]) editor->autopanKnobs[1]->setValue(s.autopan.phase, juce::dontSendNotification);
                    if (editor->autopanKnobs[2]) editor->autopanKnobs[2]->setValue((float)s.autopan.waveType, juce::dontSendNotification);
                    if (editor->autopanKnobs[3]) editor->autopanKnobs[3]->setValue(s.autopan.waveShape, juce::dontSendNotification);
                    if (editor->autopanKnobs[4]) editor->autopanKnobs[4]->setValue(s.autopan.inverted ? 1.0f : 0.0f, juce::dontSendNotification);
                    if (editor->autopanKnobs[5]) editor->autopanKnobs[5]->setValue(s.autopan.amount, juce::dontSendNotification);
                    DBG("[RAND]   AutoPan step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Dirt:
            {
                int step = editor->dirtUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getDirtSafeSnapshot(step);
                    // Load into knobs
                    if (editor->dirtKnobs[0]) editor->dirtKnobs[0]->setValue(s.dirt.drive, juce::sendNotification);
                    if (editor->dirtKnobs[1]) editor->dirtKnobs[1]->setValue(s.dirt.color, juce::sendNotification);
                    if (editor->dirtKnobs[2]) editor->dirtKnobs[2]->setValue(s.dirt.asym, juce::sendNotification);
                    if (editor->dirtKnobs[3]) editor->dirtKnobs[3]->setValue(s.dirt.texture, juce::sendNotification);
                    if (editor->dirtKnobs[4]) editor->dirtKnobs[4]->setValue(s.dirt.lowCut, juce::sendNotification);
                    if (editor->dirtKnobs[5]) editor->dirtKnobs[5]->setValue(s.dirt.highCut, juce::sendNotification);
                    if (editor->dirtKnobs[6]) editor->dirtKnobs[6]->setValue(s.dirt.tone, juce::sendNotification);
                    if (editor->dirtKnobs[7]) editor->dirtKnobs[7]->setValue(s.dirt.mix, juce::sendNotification);
                    DBG("[RAND]   Dirt step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Chorus:
            {
                int step = editor->chorusUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getChorusSafeSnapshot(step);
                    // Load into knobs
                    if (editor->chorusKnobs[0]) editor->chorusKnobs[0]->setValue(s.chorus.delayTime, juce::sendNotification);
                    if (editor->chorusKnobs[1]) editor->chorusKnobs[1]->setValue(s.chorus.rate, juce::sendNotification);
                    if (editor->chorusKnobs[2]) editor->chorusKnobs[2]->setValue(s.chorus.depth, juce::sendNotification);
                    if (editor->chorusKnobs[3]) editor->chorusKnobs[3]->setValue(s.chorus.feedback, juce::sendNotification);
                    if (editor->chorusKnobs[4]) editor->chorusKnobs[4]->setValue(s.chorus.voices, juce::sendNotification);
                    if (editor->chorusKnobs[5]) editor->chorusKnobs[5]->setValue(s.chorus.width, juce::sendNotification);
                    if (editor->chorusKnobs[6]) editor->chorusKnobs[6]->setValue(s.chorus.tone, juce::sendNotification);
                    if (editor->chorusKnobs[7]) editor->chorusKnobs[7]->setValue(s.chorus.mix, juce::sendNotification);
                    DBG("[RAND]   Chorus step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Reverb:
            {
                int step = editor->reverbUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getReverbSafeSnapshot(step);
                    // Load into knobs
                    if (editor->reverbKnobs[0]) editor->reverbKnobs[0]->setValue(s.reverb.type, juce::sendNotification); // Width
                    if (editor->reverbKnobs[1]) editor->reverbKnobs[1]->setValue(s.reverb.size, juce::sendNotification);
                    if (editor->reverbKnobs[2]) editor->reverbKnobs[2]->setValue(s.reverb.predelayMs, juce::sendNotification);
                    if (editor->reverbKnobs[3]) editor->reverbKnobs[3]->setValue(s.reverb.dampHz, juce::sendNotification);
                    if (editor->reverbKnobs[4]) editor->reverbKnobs[4]->setValue(s.reverb.diffusion, juce::sendNotification);
                    if (editor->reverbKnobs[5]) editor->reverbKnobs[5]->setValue(s.reverb.early, juce::sendNotification);
                    if (editor->reverbKnobs[6]) editor->reverbKnobs[6]->setValue(s.reverb.decaySec, juce::sendNotification);
                    if (editor->reverbKnobs[7]) editor->reverbKnobs[7]->setValue(s.reverb.mix, juce::sendNotification);
                    DBG("[RAND]   Reverb step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Granular:
            {
                int step = editor->granularUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getGranularSafeSnapshot(step);
                    // Load into knobs
                    if (editor->granularKnobs[0]) editor->granularKnobs[0]->setValue(s.granular.sizeMs, juce::sendNotification);
                    if (editor->granularKnobs[1]) editor->granularKnobs[1]->setValue(s.granular.densityHz, juce::sendNotification);
                    if (editor->granularKnobs[2]) editor->granularKnobs[2]->setValue(s.granular.position, juce::sendNotification);
                    if (editor->granularKnobs[3]) editor->granularKnobs[3]->setValue(s.granular.sprayMs, juce::sendNotification);
                    if (editor->granularKnobs[4]) editor->granularKnobs[4]->setValue(s.granular.pitchSemi, juce::sendNotification);
                    if (editor->granularKnobs[5]) editor->granularKnobs[5]->setValue(s.granular.random, juce::sendNotification);
                    if (editor->granularKnobs[6]) editor->granularKnobs[6]->setValue(s.granular.texture, juce::sendNotification);
                    if (editor->granularKnobs[7]) editor->granularKnobs[7]->setValue(s.granular.mix, juce::sendNotification);
                    DBG("[RAND]   Granular step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Slicer:
            {
                int step = editor->slicerUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getSlicerSafeSnapshot(step);
                    // Load into knobs (6 knobs, not 8)
                    if (editor->slicerKnobs[0]) editor->slicerKnobs[0]->setValue(s.slicer.pattern, juce::sendNotification);
                    if (editor->slicerKnobs[1]) editor->slicerKnobs[1]->setValue(s.slicer.division, juce::sendNotification);
                    if (editor->slicerKnobs[2]) editor->slicerKnobs[2]->setValue(s.slicer.offset, juce::sendNotification);
                    if (editor->slicerKnobs[3]) editor->slicerKnobs[3]->setValue(s.slicer.shape, juce::sendNotification);
                    if (editor->slicerKnobs[4]) editor->slicerKnobs[4]->setValue(s.slicer.releaseMs, juce::sendNotification);
                    if (editor->slicerKnobs[5]) editor->slicerKnobs[5]->setValue(s.slicer.mix, juce::sendNotification);
                    DBG("[RAND]   Slicer step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::DubDelay:
            {
                int step = editor->dubdelayUiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getDubDelaySafeSnapshot(step);
                    // Load into knobs (8 knobs)
                    if (editor->dubdelayKnobs[0]) editor->dubdelayKnobs[0]->setValue(s.dubdelay.timeMs, juce::sendNotification);
                    if (editor->dubdelayKnobs[1]) editor->dubdelayKnobs[1]->setValue(s.dubdelay.feedback, juce::sendNotification);
                    if (editor->dubdelayKnobs[2]) editor->dubdelayKnobs[2]->setValue(s.dubdelay.toneHz, juce::sendNotification);
                    if (editor->dubdelayKnobs[3]) editor->dubdelayKnobs[3]->setValue(s.dubdelay.drive, juce::sendNotification);
                    // Knob 4 is PingPong toggle - represented as 0 or 1
                    if (editor->dubdelayKnobs[4]) editor->dubdelayKnobs[4]->setValue(s.dubdelay.pingPong ? 1.0f : 0.0f, juce::sendNotification);
                    if (editor->dubdelayKnobs[5]) editor->dubdelayKnobs[5]->setValue(s.dubdelay.wowFlutter, juce::sendNotification);
                    if (editor->dubdelayKnobs[6]) editor->dubdelayKnobs[6]->setValue(s.dubdelay.regenDamp, juce::sendNotification);
                    if (editor->dubdelayKnobs[7]) editor->dubdelayKnobs[7]->setValue(s.dubdelay.mix, juce::sendNotification);
                    DBG("[RAND]   DubDelay step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Form2:
            {
                int step = editor->form2UiSelectedStep;
                if (step >= 0 && step < 16) {
                    auto s = processor.getForm2SafeSnapshot(step);
                    // Load into knobs (8 knobs) - map to 0.0-1.0 range
                    if (editor->form2Knobs[0]) editor->form2Knobs[0]->setValue(s.form2.rootNote / 12.0f, juce::sendNotification);
                    if (editor->form2Knobs[1]) editor->form2Knobs[1]->setValue(s.form2.scale / 7.0f, juce::sendNotification);
                    if (editor->form2Knobs[2]) editor->form2Knobs[2]->setValue((s.form2.chordSize - 1) / 7.0f, juce::sendNotification);
                    if (editor->form2Knobs[3]) editor->form2Knobs[3]->setValue(s.form2.shift, juce::sendNotification);
                    if (editor->form2Knobs[4]) editor->form2Knobs[4]->setValue(s.form2.color, juce::sendNotification);
                    if (editor->form2Knobs[5]) editor->form2Knobs[5]->setValue(s.form2.motion, juce::sendNotification);
                    if (editor->form2Knobs[6]) editor->form2Knobs[6]->setValue(s.form2.resynth, juce::sendNotification);
                    if (editor->form2Knobs[7]) editor->form2Knobs[7]->setValue(s.form2.mix, juce::sendNotification);
                    DBG("[RAND]   Form2 step " + juce::String(step) + " reloaded");
                }
                break;
            }
            
            case EffectID::Filter:
            {
                int step = processor.getFilterSelectedStep();
                if (step >= 0 && step < 16) {
                    auto s = processor.getFilterSafeSnapshot(step);
                    // Load into filter knobs
                    if (editor->filterTypeKnob) editor->filterTypeKnob->setValue(s.filter.type, juce::sendNotification);
                    if (editor->filterKnobs[0]) { // Cutoff - convert frequency to normalized
                        float normalized = s.filter.cutoff <= 5000.0f
                            ? (s.filter.cutoff - 20.0f) / (5000.0f - 20.0f) * 0.75f
                            : 0.75f + 0.25f * (std::log(s.filter.cutoff / 5000.0f) / std::log(4.0f));
                        normalized = juce::jlimit(0.0f, 1.0f, normalized);
                        editor->filterKnobs[0]->setValue(normalized, juce::sendNotification);
                    }
                    if (editor->filterKnobs[1]) editor->filterKnobs[1]->setValue(s.filter.resonance, juce::sendNotification);
                    if (editor->filterSlopeKnob) editor->filterSlopeKnob->setValue(s.filter.slope, juce::sendNotification);
                    if (editor->filterKnobs[2]) editor->filterKnobs[2]->setValue(s.filter.drive, juce::sendNotification);
                    if (editor->filterKnobs[3]) editor->filterKnobs[3]->setValue(s.filter.keytrack, juce::sendNotification);
                    DBG("[RAND]   Filter step " + juce::String(step) + " reloaded");
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
            
            case EffectID::Form2:
            {
                auto snapshot = processor.getForm2SafeSnapshot(target.stepIndex);
                snapshot.form2.rootNote = static_cast<int>(rand01() * 12); // 0-11
                snapshot.form2.scale = static_cast<int>(rand01() * 7); // 0-6
                snapshot.form2.chordSize = 1 + static_cast<int>(rand01() * 8); // 1-8
                snapshot.form2.shift = 0.5f + rand01() * 1.5f; // 0.5-2.0
                snapshot.form2.color = -12.0f + rand01() * 24.0f; // -12 to +12
                snapshot.form2.motion = rand01(); // 0-1
                snapshot.form2.resynth = rand01(); // 0-1
                snapshot.form2.mix = rand01(); // 0-1
                processor.setForm2StepSnapshot(target.stepIndex, snapshot);
                break;
            }
            
            case EffectID::Filter:
            {
                auto snapshot = processor.getFilterSafeSnapshot(target.stepIndex);
                snapshot.filter.type = static_cast<float>(static_cast<int>(rand01() * 5)); // 0-4
                snapshot.filter.cutoff = 20.0f + rand01() * 19980.0f; // 20-20000 Hz
                snapshot.filter.resonance = rand01(); // 0-1
                snapshot.filter.slope = static_cast<float>(static_cast<int>(rand01() * 2)); // 0 or 1
                snapshot.filter.drive = rand01() * 36.0f; // 0-36 dB
                snapshot.filter.spread = 0.0f; // Spread removed - always 0
                snapshot.filter.keytrack = rand01(); // 0-1
                snapshot.filter.mix = 1.0f; // Mix is global, not per-step
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
        
        // Randomize division index (1-7: 2 bars to 1/32)
        int divisionIndex = 1 + static_cast<int>(rand01() * 7); // Range: 1-7 (2 bars to 1/32)
        
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
                break;
                
            case EffectID::Granular:
                processor.setGranularSequencerEnabled(sequencerEnabled);
                processor.setGranularStepsUsed(stepsUsed);
                processor.setGranularDivisionIndex(divisionIndex);
                break;
                
            case EffectID::Slicer:
                processor.setSlicerSequencerEnabled(sequencerEnabled);
                processor.setSlicerStepsUsed(stepsUsed);
                processor.setSlicerDivisionIndex(divisionIndex);
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
                
            case EffectID::Form2:
                processor.setForm2SequencerEnabled(sequencerEnabled);
                processor.setForm2StepsUsed(stepsUsed);
                processor.setForm2DivisionIndex(divisionIndex);
                break;
                
            case EffectID::Filter:
                processor.setFilterSequencerEnabled(sequencerEnabled);
                processor.setFilterStepAmount(stepsUsed);
                processor.setFilterDivisionIndex(divisionIndex);
                break;
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
