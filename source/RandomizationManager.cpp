#include "RandomizationManager.h"

RandomizationManager::RandomizationManager(PluginProcessor& proc, juce::AudioProcessorValueTreeState& tree)
    : processor(proc), apvts(tree)
{
    // Reserve space to avoid allocations during randomization
    paramTargets.reserve(64); // 4 pages * 8 knobs + some headroom
    stepTargets.reserve(256); // 4 pages * 16 steps * 8 params worst case
    
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
    
    DBG("[RAND] Async update starting on message thread");
    randomizeAll();
    busy.store(false);
    DBG("[RAND] Async update complete");
}

void RandomizationManager::randomizeAll()
{
    // RAII guard to ensure processor state is restored even if exception
    struct ProcessGuard {
        PluginProcessor& p;
        ProcessGuard(PluginProcessor& proc) : p(proc) { p.suspendProcessing(true); }
        ~ProcessGuard() { p.suspendProcessing(false); }
    } guard(processor);
    
    // Collect all targets (parameters + step data)
    paramTargets.clear();
    stepTargets.clear();
    collectTargets();
    
    DBG("[RAND] Collected " << paramTargets.size() << " params and " << stepTargets.size() << " step edits");
    
    // Apply changes transactionally
    applyParamChanges();
    applyStepDataChanges();
    
    // Notify UI
    notifyUI();
}

void RandomizationManager::collectTargets()
{
    auto& router = processor.getEffectRouter();
    
    // Iterate through all 4 active effect slots
    for (int slot = 0; slot < 4; ++slot)
    {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slot));
        
        // Note: For now, we're only randomizing step data since parameter locking
        // is tracked per-page. We could extend this to randomize APVTS parameters too.
        
        // Collect step data for all 16 steps
        for (int step = 0; step < 16; ++step)
        {
            // Check if step is locked (implementation depends on your lock system)
            if (isStepLocked(step, effect))
                continue;
            
            // For each effect, randomize its step snapshot parameters
            // The actual randomization values will be computed in applyStepDataChanges
            // For now, just mark that this step needs randomization
            // (We'll do the actual randomization inline during apply)
        }
    }
}

void RandomizationManager::applyParamChanges()
{
    if (paramTargets.empty())
        return;
    
    // Start undo transaction
    auto* um = apvts.undoManager;
    if (um)
        um->beginNewTransaction("Dice Randomize");
    
    // Apply each parameter change with proper gestures
    for (const auto& target : paramTargets)
    {
        if (!target.p)
            continue;
        
        target.p->beginChangeGesture();
        target.p->setValueNotifyingHost(target.normTarget);
        target.p->endChangeGesture();
    }
}

void RandomizationManager::applyStepDataChanges()
{
    // For now, we directly update the processor's step snapshots
    // This avoids ValueTree complexity and uses the existing system
    
    auto& router = processor.getEffectRouter();
    
    for (int slot = 0; slot < 4; ++slot)
    {
        EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slot));
        
        for (int step = 0; step < 16; ++step)
        {
            // Skip locked steps
            if (isStepLocked(step, effect))
                continue;
            
            // Randomize based on effect type
            switch (effect)
            {
                case EffectID::SpaceDelay:
                {
                    auto snapshot = processor.getSafeSnapshot(step);
                    snapshot.delay.timeMs = 50.0f + rand01() * 950.0f;
                    snapshot.delay.feedback = rand01() * 95.0f;
                    snapshot.delay.wowDepth = rand01();
                    snapshot.delay.wowRate = rand01() * 10.0f;
                    snapshot.delay.saturation = rand01();
                    snapshot.delay.lowCut = 20.0f + rand01() * 19980.0f;
                    snapshot.delay.highCut = 20.0f + rand01() * 19980.0f;
                    snapshot.delay.mix = rand01() * 100.0f;
                    processor.setStepSnapshot(step, snapshot);
                    break;
                }
                
                case EffectID::AutoPan:
                {
                    auto snapshot = processor.getAutoPanSafeSnapshot(step);
                    snapshot.autopan.rate = 0.01f + rand01() * 9.99f;
                    snapshot.autopan.amount = rand01();
                    snapshot.autopan.waveShape = rand01();
                    snapshot.autopan.phase = rand01() * 360.0f;
                    snapshot.autopan.waveType = static_cast<int>(rand01() * 4.999f);
                    snapshot.autopan.inverted = rand01() > 0.5f;
                    processor.setAutoPanStepSnapshot(step, snapshot);
                    break;
                }
                
                case EffectID::Dirt:
                {
                    auto snapshot = processor.getDirtSafeSnapshot(step);
                    snapshot.dirt.drive = rand01() * 36.0f;
                    snapshot.dirt.color = -1.0f + rand01() * 2.0f;
                    snapshot.dirt.asym = -1.0f + rand01() * 2.0f;
                    snapshot.dirt.texture = rand01();
                    snapshot.dirt.lowCut = 20.0f + rand01() * 280.0f;
                    snapshot.dirt.highCut = 3000.0f + rand01() * 19000.0f;
                    snapshot.dirt.tone = -1.0f + rand01() * 2.0f;
                    snapshot.dirt.mix = rand01();
                    processor.setDirtStepSnapshot(step, snapshot);
                    break;
                }
                
                case EffectID::Chorus:
                {
                    auto snapshot = processor.getChorusSafeSnapshot(step);
                    snapshot.chorus.delayTime = 5.0f + rand01() * 45.0f;
                    snapshot.chorus.rate = 0.02f + rand01() * 7.98f;
                    snapshot.chorus.depth = rand01() * 12.0f;
                    snapshot.chorus.feedback = rand01() * 0.9f;
                    snapshot.chorus.voices = 2.0f + rand01() * 6.0f;
                    snapshot.chorus.width = rand01();
                    snapshot.chorus.tone = rand01();
                    snapshot.chorus.mix = rand01();
                    processor.setChorusStepSnapshot(step, snapshot);
                    break;
                }
                
                case EffectID::Reverb:
                {
                    auto snapshot = processor.getReverbSafeSnapshot(step);
                    snapshot.reverb.type = rand01();
                    snapshot.reverb.size = 0.1f + rand01() * 1.4f;
                    snapshot.reverb.predelayMs = rand01() * 200.0f;
                    snapshot.reverb.dampHz = 1000.0f + rand01() * 19000.0f;
                    snapshot.reverb.diffusion = rand01();
                    snapshot.reverb.early = rand01();
                    snapshot.reverb.decaySec = 0.2f + rand01() * 19.8f;
                    snapshot.reverb.mix = rand01();
                    processor.setReverbStepSnapshot(step, snapshot);
                    break;
                }
                
                default:
                    break;
            }
        }
    }
}

void RandomizationManager::notifyUI()
{
    // UI will update automatically via the existing timer callback
    // No explicit repaint needed
    DBG("[RAND] Randomization complete - UI will update via timer");
}

float RandomizationManager::rand01()
{
    // Fast xorshift PRNG
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(rngState) / static_cast<float>(0xFFFFFFFFu);
}

bool RandomizationManager::isParamLocked(const juce::String& paramID)
{
    // TODO: Implement based on your lock system
    // For now, return false (nothing locked)
    return false;
}

bool RandomizationManager::isStepLocked(int step, EffectID effect)
{
    // TODO: Implement based on your lock system  
    // For now, return false (nothing locked)
    return false;
}

