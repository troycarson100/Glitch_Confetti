#include "FilterProcessor.h"

FilterProcessor::FilterProcessor()
{
    try {
        cutoffSm.reset(48000.0, 0.001); // 1ms smoothing for fast response
        resSm.reset(48000.0, 0.001);
        cutoffSm.setCurrentAndTargetValue(1200.0f);
        resSm.setCurrentAndTargetValue(0.35f);
        DBG("[FilterProcessor] Constructor completed successfully");
    } catch (const std::exception& e) {
        DBG("[FilterProcessor] ERROR in constructor: " << e.what());
    } catch (...) {
        DBG("[FilterProcessor] UNKNOWN ERROR in constructor");
    }
}

void FilterProcessor::prepare(double sampleRate, int maxBlockSize)
{
    if (sampleRate <= 0.0 || maxBlockSize <= 0) {
        DBG("[FilterProcessor] Invalid prepare parameters: sampleRate=" << sampleRate << " maxBlockSize=" << maxBlockSize);
        return;
    }
    
    fs = sampleRate;
    block = maxBlockSize;
    
    specCached.sampleRate = sampleRate;
    specCached.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
    specCached.numChannels = 2;
    
    cutoffSm.reset(sampleRate, 0.001); // 1ms smoothing for fast response
    resSm.reset(sampleRate, 0.001);
    cutoffSm.setCurrentAndTargetValue(1200.0f);
    resSm.setCurrentAndTargetValue(0.35f);
    
    tmpA.setSize(2, maxBlockSize);
    tmpB.setSize(2, maxBlockSize);
    
    // Create initial filter
    makeFilter(0); // LP default
    if (cur) {
        cur->prepare(specCached);
        currentType = 0;
        targetType = 0;
    } else {
        DBG("[FilterProcessor] Failed to create initial filter!");
    }
    
    isPrepared = true;
}

void FilterProcessor::setTargets(const Targets& t)
{
    targetType = juce::jlimit(0, 4, t.type);
    
    // Clamp cutoff to valid range and ensure it's finite
    // CRITICAL: Non-finite cutoff will cause division by zero in comb filter delay calculation
    float newCutoff = t.cutoff;
    if (!std::isfinite(newCutoff) || newCutoff <= 0.0f) {
        newCutoff = (targetType >= 3) ? 40.0f : 20.0f; // Safe default based on filter type
    }
    newCutoff = juce::jlimit(20.0f, 20000.0f, newCutoff);
    
    // Detect large cutoff changes (>20% change) and use instant update
    float currentCutoff = cutoffSm.getCurrentValue();
    if (!std::isfinite(currentCutoff) || currentCutoff <= 0.0f) {
        currentCutoff = newCutoff; // Reset if current value is invalid
    }
    float cutoffChange = std::abs(newCutoff - currentCutoff) / juce::jmax(1.0f, currentCutoff);
    
    if (cutoffChange > 0.2f) {
        // Large change - instant update for responsive knob movement
        cutoffSm.setCurrentAndTargetValue(newCutoff);
    } else {
        // Small change - use smoothing
        cutoffSm.setTargetValue(newCutoff);
    }
    
    // Ensure resonance is finite and valid
    float newRes = t.res;
    if (!std::isfinite(newRes) || newRes < 0.0f) {
        newRes = 0.35f; // Safe default
    }
    resSm.setTargetValue(juce::jlimit(0.0f, 0.95f, newRes));
    
    slope = juce::jlimit(0, 1, t.slope);
    drive = std::isfinite(t.drive) ? juce::jlimit(0.0f, 36.0f, t.drive) : 6.0f;
    spreadCents = std::isfinite(t.spread) ? t.spread : 0.0f;
    kt = std::isfinite(t.keytrack) ? juce::jlimit(0.0f, 1.0f, t.keytrack) : 0.0f;
    mix = std::isfinite(t.mix) ? juce::jlimit(0.0f, 1.0f, t.mix) : 1.0f;
}

void FilterProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 2) return;
    
    // Safety check: must be prepared
    if (!isPrepared || fs <= 0.0) {
        DBG("[FilterProcessor] Not prepared, skipping process");
        return;
    }
    
    juce::ScopedNoDenormals _nd;
    
    // Ensure we have a filter
    if (!cur) {
        DBG("[FilterProcessor] No filter, creating default");
        makeFilter(0);
        if (cur) {
            cur->prepare(specCached);
            currentType = 0;
            targetType = 0;
        } else {
            DBG("[FilterProcessor] Failed to create filter, returning");
            return; // Can't process without filter
        }
    }
    
    // Handle type change with crossfade
    if (currentType != targetType) {
        // Reset current filter if it's a comb filter (clear delay lines)
        // This prevents delay line state from persisting when switching away from comb
        if (cur) {
            auto* combFilterOld = dynamic_cast<CombProc*>(cur.get());
            if (combFilterOld) {
                // Recover old comb filter state before switching
                combFilterOld->recover();
            }
        }
        
        // Create new filter with current params
        makeFilter(targetType);
        if (newF) {
            newF->prepare(specCached);
            
            // If switching to comb filter, ensure it's in clean state
            auto* combFilterNew = dynamic_cast<CombProc*>(newF.get());
            if (combFilterNew) {
                combFilterNew->recover(); // Ensure clean state
            }
            
            ramp.start(fs, 20.0); // 20ms crossfade
        }
        currentType = targetType;
    }
    
    // Drive pre-filter
    float gPre = juce::Decibels::decibelsToGain(drive);
    buffer.applyGain(gPre);
    
    // Convert buffer to AudioBlock
    juce::dsp::AudioBlock<float> ioBlock(buffer);
    auto blockA = juce::dsp::AudioBlock<float>(tmpA);
    auto blockB = juce::dsp::AudioBlock<float>(tmpB);
    
    // Ensure tmp buffers are sized correctly
    if (tmpA.getNumSamples() < numSamples || tmpA.getNumChannels() < 2) {
        tmpA.setSize(2, numSamples, false, false, true);
        tmpB.setSize(2, numSamples, false, false, true);
        blockA = juce::dsp::AudioBlock<float>(tmpA);
        blockB = juce::dsp::AudioBlock<float>(tmpB);
    }
    
    // Limit to actual buffer size
    blockA = blockA.getSubBlock(0, juce::jmin(numSamples, (int)tmpA.getNumSamples()));
    blockB = blockB.getSubBlock(0, juce::jmin(numSamples, (int)tmpB.getNumSamples()));
    auto ioBlockSub = ioBlock.getSubBlock(0, numSamples);
    
    blockA.copyFrom(ioBlockSub);
    
    // Process current filter with responsive cutoff updates
    // Advance smoothing through the entire block to get the final smoothed value
    float cut = cutoffSm.getCurrentValue();
    float r = resSm.getCurrentValue();
    
    // Advance smoothing through the block (update every 4 samples for balance)
    const int updateInterval = 4;
    for (int i = 0; i < numSamples; i += updateInterval) {
        cut = cutoffSm.getNextValue();
        r = resSm.getNextValue();
        // Advance remaining samples in this interval by calling getNextValue multiple times
        for (int j = 1; j < updateInterval && (i + j) < numSamples; ++j) {
            cut = cutoffSm.getNextValue();
            r = resSm.getNextValue();
        }
    }
    
    // Apply key track
    applyKeyTrack(cut);
    
    // For comb filters (type >= 3), use Q (r) as depth parameter for feedforward
    // Scale it to make comb effect more pronounced: map 0-1 resonance to 0-0.8 depth
    // For regular filters, depth is 0.0f (not used)
    float depthValue = (targetType >= 3) ? (r * 0.8f) : 0.0f;
    
    // Process current filter
    if (cur) {
        // Health check: if current filter is a CombProc, validate its state before processing
        // This catches corruption before it causes channel failure
        auto* combFilter = dynamic_cast<CombProc*>(cur.get());
        if (combFilter) {
            if (!combFilter->isValidState()) {
                // State is invalid - try recovery (safe to do in audio thread)
                combFilter->recover();
                // If still invalid after recovery, skip processing this block (pass through)
                // Can't call prepare() from audio thread, so just skip if recovery didn't work
                if (!combFilter->isValidState()) {
                    // Just copy input to output and skip filter processing
                    ioBlockSub.copyFrom(blockA);
                    return;
                }
            }
        }
        
        cur->set(cut, r, depthValue, slope, drive, spreadCents, (float)fs);
        cur->process(blockA, blockA);
    }
    
    if (ramp.isActive() && newF) {
        // Health check: if new filter is a CombProc, validate its state before processing
        auto* combFilterNew = dynamic_cast<CombProc*>(newF.get());
        if (combFilterNew) {
            if (!combFilterNew->isValidState()) {
                // State is invalid - try recovery (safe to do in audio thread)
                combFilterNew->recover();
                // If still invalid after recovery, skip crossfade and just use current filter
                if (!combFilterNew->isValidState()) {
                    // Skip crossfade, just use current filter output
                    ioBlockSub.copyFrom(blockA);
                    return;
                }
            }
        }
        
        // Process new filter
        blockB.copyFrom(ioBlockSub);
        newF->set(cut, r, depthValue, slope, drive, spreadCents, (float)fs);
        newF->process(blockB, blockB);
        
        // Crossfade
        auto* aL = blockA.getChannelPointer(0);
        auto* aR = blockA.getChannelPointer(1);
        auto* bL = blockB.getChannelPointer(0);
        auto* bR = blockB.getChannelPointer(1);
        auto* outL = ioBlockSub.getChannelPointer(0);
        auto* outR = ioBlockSub.getChannelPointer(1);
        
        for (int i = 0; i < numSamples; ++i) {
            float g = ramp.next();
            outL[i] = (1.0f - g) * aL[i] + g * bL[i];
            outR[i] = (1.0f - g) * aR[i] + g * bR[i];
        }
        
        // Swap in new filter when crossfade completes
        if (!ramp.isActive()) {
            cur.swap(newF);
        }
    } else {
        // Just copy A -> io
        ioBlockSub.copyFrom(blockA);
    }
    
    // Soft limit
    auto* outL = ioBlockSub.getChannelPointer(0);
    auto* outR = ioBlockSub.getChannelPointer(1);
    
    for (int i = 0; i < numSamples; ++i) {
        float xL = outL[i];
        float xR = outR[i];
        outL[i] = xL / (1.0f + 0.5f * std::abs(xL));
        outR[i] = xR / (1.0f + 0.5f * std::abs(xR));
    }
    
    // Note: Mix is handled at page mixer level (dry*(1-mix) + wet*mix)
}

void FilterProcessor::makeFilter(int type)
{
    if (type <= 2) {
        // LP/HP/BP
        newF = std::make_unique<SVFProc>(type);
    } else {
        // Comb- or Comb+
        newF = std::make_unique<CombProc>(type == 3 ? -1 : +1);
    }
    
    if (!cur) {
        cur = std::make_unique<SVFProc>(0);
    }
}

void FilterProcessor::applyKeyTrack(float& cutoffHz)
{
    if (kt > 0.01f && lastMidiNote > 0.0f) {
        // Key track: 1 semitone per 10 units (musical default)
        float semitones = (lastMidiNote - 60.0f) * 0.1f * kt;
        cutoffHz *= std::pow(2.0f, semitones / 12.0f);
        cutoffHz = juce::jlimit(20.0f, 20000.0f, cutoffHz);
    }
}

