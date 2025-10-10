#include "RandomizationManager.h"
#include "PluginEditor.h"

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
    
    // Collect all targets
    collectTargets();
    
    // Apply changes
    applyParamChanges();
    applyStepChanges();
    
    // Verify and report
    verifyAndReport();
    
    // Resume processing
    processor.suspendProcessing(false);
}

void RandomizationManager::collectTargets()
{
    DBG("[RAND] Collecting targets...");
    
    // Get the 4 active pages
    auto activePages = registry.getActivePages(processor, apvts);
    
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto& page = activePages[slot];
        DBG("[RAND] Page " + juce::String(slot) + ": " + page.pageId);
        
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
    }
    
    DBG("[RAND] Collected " + juce::String(paramTargets.size()) + " params, " 
        + juce::String(stepTargets.size()) + " steps");
}

void RandomizationManager::applyParamChanges()
{
    // After randomizing all step snapshots, we need to reload the current step
    // into APVTS for each effect so the knobs update immediately
    
    if (!editor)
        return;
    
    DBG("[RAND] Reloading current steps into knobs...");
    
    // Trigger a reload for each effect's current step by simulating a re-click
    // This loads the randomized snapshot into the knobs
    
    auto& router = processor.getEffectRouter();
    
    for (int slot = 0; slot < 4; ++slot)
    {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slot));
        
        switch (effect)
        {
            case EffectID::SpaceDelay:
            {
                int currentStep = processor.getSelectedStep();
                if (currentStep >= 0 && currentStep < 16) {
                    // Re-trigger the step button click to reload snapshot into knobs
                    editor->onStepButtonClicked(currentStep);
                    DBG("[RAND]   SpaceDelay step " + juce::String(currentStep) + " reloaded");
                }
                break;
            }
            
            case EffectID::AutoPan:
            {
                int currentStep = editor->autopanUiSelectedStep;
                if (currentStep >= 0 && currentStep < 16) {
                    editor->onAutoPanStepButtonClicked(currentStep);
                    DBG("[RAND]   AutoPan step " + juce::String(currentStep) + " reloaded");
                }
                break;
            }
            
            case EffectID::Dirt:
            {
                int currentStep = editor->dirtUiSelectedStep;
                if (currentStep >= 0 && currentStep < 16) {
                    editor->onDirtStepButtonClicked(currentStep);
                    DBG("[RAND]   Dirt step " + juce::String(currentStep) + " reloaded");
                }
                break;
            }
            
            case EffectID::Chorus:
            {
                int currentStep = editor->chorusUiSelectedStep;
                if (currentStep >= 0 && currentStep < 16) {
                    editor->onChorusStepButtonClicked(currentStep);
                    DBG("[RAND]   Chorus step " + juce::String(currentStep) + " reloaded");
                }
                break;
            }
            
            case EffectID::Reverb:
            {
                int currentStep = editor->reverbUiSelectedStep;
                if (currentStep >= 0 && currentStep < 16) {
                    editor->onReverbStepButtonClicked(currentStep);
                    DBG("[RAND]   Reverb step " + juce::String(currentStep) + " reloaded");
                }
                break;
            }
            
            default:
                break;
        }
    }
    
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
            
            default:
                break;
        }
        
        stats.stepsRandomized++;
    }
    
    DBG("[RAND] Randomized " + juce::String(stats.stepsRandomized) + " steps");
}

void RandomizationManager::verifyAndReport()
{
    DBG("[RAND] ══════════ RANDOMIZATION REPORT ══════════");
    DBG("[RAND] Pages: 4 active");
    DBG("[RAND] Knobs: " + juce::String(stats.paramsRandomized) + "/" + juce::String(stats.paramsExpected) 
        + " (" + juce::String(stats.paramsLocked) + " locked)");
    DBG("[RAND] Steps: " + juce::String(stats.stepsRandomized) + "/" + juce::String(stats.stepsExpected)
        + " (" + juce::String(stats.stepsLocked) + " locked)");
    
    // Verify non-zero coverage
    if (stats.paramsRandomized == 0 && stats.paramsExpected > 0)
        DBG("[RAND] ERROR: No parameters randomized!");
    if (stats.stepsRandomized == 0 && stats.stepsExpected > 0)
        DBG("[RAND] ERROR: No steps randomized!");
    
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
