#include "FxFilter.h"

//==============================================================================
// FxFilter Implementation
//==============================================================================

FxFilter::FxFilter() : combMinus(-1), combPlus(+1)
{
    // Use very fast smoothing for cutoff (1ms) since frequency changes need to be responsive
    cutoffSm.reset(48000.0, 0.001);  // 1ms smoothing for fast response
    cutoffSm.setCurrentAndTargetValue(1200.0f);
    resSm.reset(48000.0, 0.005);  // 5ms for resonance
    resSm.setCurrentAndTargetValue(0.35f);
    driveSm.reset(48000.0, 0.005);  // 5ms for drive
    driveSm.setCurrentAndTargetValue(6.0f);
    
    // Initialize filter type to LP (0)
    currentType = 0;
    targetType = 0;
    
    // Initialize default parameters
    currentCutoff = 1200.0f;
    currentRes = 0.35f;
    currentDrive = 6.0f;
    currentSpread = 0.0f;
    mix = 1.0f;
    slope = 1;
}

void FxFilter::prepare(double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    block = maxBlockSize;
    channels = 2;
    
    // Reset smoothing - use fast smoothing for cutoff for responsive knob control
    cutoffSm.reset(sampleRate, 0.001);  // 1ms smoothing for fast response
    cutoffSm.setCurrentAndTargetValue(1200.0f);
    resSm.reset(sampleRate, 0.005);  // 5ms for resonance
    resSm.setCurrentAndTargetValue(0.35f);
    driveSm.reset(sampleRate, 0.005);  // 5ms for drive
    driveSm.setCurrentAndTargetValue(6.0f);
    
    // Prepare JUCE filters
    processSpec.sampleRate = sampleRate;
    processSpec.maximumBlockSize = maxBlockSize;
    processSpec.numChannels = channels;
    
    // Prepare all filter instances (separate for L/R channels for spread support)
    svfLP_L.prepare(processSpec);
    svfLP_R.prepare(processSpec);
    svfHP_L.prepare(processSpec);
    svfHP_R.prepare(processSpec);
    svfBP_L.prepare(processSpec);
    svfBP_R.prepare(processSpec);
    svfLP2_L.prepare(processSpec);
    svfLP2_R.prepare(processSpec);
    svfHP2_L.prepare(processSpec);
    svfHP2_R.prepare(processSpec);
    svfBP2_L.prepare(processSpec);
    svfBP2_R.prepare(processSpec);
    
    // Set initial filter types
    svfLP_L.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    svfLP_R.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    svfHP_L.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    svfHP_R.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    svfBP_L.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    svfBP_R.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    svfLP2_L.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    svfLP2_R.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    svfHP2_L.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    svfHP2_R.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    svfBP2_L.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    svfBP2_R.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    
    // Prepare comb filters
    combMinus.prepare(sampleRate, maxBlockSize, channels);
    combPlus.prepare(sampleRate, maxBlockSize, channels);
    
    // Prepare crossfade switcher
    switcher.prepare(sampleRate, maxBlockSize, channels);
    
    // Allocate scratch buffers
    tmpA.setSize(channels, maxBlockSize);
    tmpB.setSize(channels, maxBlockSize);
    dryBuffer.setSize(channels, maxBlockSize);
}

float FxFilter::applyKeyTracking(float baseCutoff, float keytrackVal, int midiNote)
{
    if (keytrackVal < 0.001f) return baseCutoff;
    
    // Key tracking: cutoff *= 2^((note-60)*0.01*keytrack)
    float semitoneOffset = (float)(midiNote - 60) * 0.01f * keytrackVal;
    return baseCutoff * std::pow(2.0f, semitoneOffset / 12.0f);
}

float FxFilter::mapResToQ(float res, int filterType)
{
    // Clamp res to 0-1.0 and map to Q (allows up to 100% resonance)
    res = juce::jlimit(0.0f, 1.0f, res);
    
    // BP mode (type == 2) uses a gentler, smoother Q curve to prevent harsh resonance
    if (filterType == 2) // BP
    {
        // Use exponential curve for smoother resonance: Q from 0.5 to 6.0 (lower max than LP/HP)
        // The curve is gentler: res^1.5 gives smoother response at high resonance
        float qMax = 6.0f; // Lower max Q for smoother BP sound
        float qMin = 0.5f;
        float qRange = qMax - qMin;
        // Exponential curve: res^1.5 makes it gentler at high values
        float curved = std::pow(res, 1.5f);
        return qMin + (qRange * curved);
    }
    else
    {
        // LP and HP use linear mapping for predictable, responsive control
        // Q ranges from 0.5 (min) to 12.0 (max) for full resonance control
        return 0.5f + (res * 11.5f); // Linear: 0.5 at res=0, 12.0 at res=1.0
    }
}

float FxFilter::mapResToCombFeedback(float res)
{
    // Clamp res to 0-1.0 and map to feedback for comb filters (0-0.9)
    // Use quadratic curve to make resonance audible at lower values
    // res^2 means: at 25% resonance, we get ~6% feedback; at 50%, ~25% feedback
    res = juce::jlimit(0.0f, 1.0f, res);
    float feedbackMax = 0.9f;
    // Quadratic curve: res^2 makes lower values more audible
    float curved = res * res;
    return curved * feedbackMax;
}

float FxFilter::applyDrive(float sample, float driveDb)
{
    // Only apply drive if > 0.01dB
    if (driveDb <= 0.01f)
        return sample;
    
    // Convert dB to linear gain - make drive more aggressive
    float gain = juce::Decibels::decibelsToGain(driveDb);
    
    // Apply gain
    float driven = sample * gain;
    
    // Apply aggressive saturation to make drive very apparent
    float driveAmount = driveDb / 36.0f; // 0-1 normalized
    
    // Use tanh saturation with variable strength based on drive amount
    // At low drive: subtle saturation, at high drive: heavy saturation
    float saturationStrength = 0.3f + (driveAmount * 0.7f); // 0.3 to 1.0
    float saturationAmount = 1.5f + (driveAmount * 3.0f); // 1.5 to 4.5
    
    // Apply saturation: blend between linear and tanh based on drive
    driven = driven * (1.0f - saturationStrength) + saturationStrength * std::tanh(driven * saturationAmount);
    
    return driven;
}

void FxFilter::switchFilterType(int newType)
{
    if (currentType != newType && !switcher.isActive())
    {
        // Start crossfade with longer duration for smoother transitions
        // Don't reset filters - let the crossfade handle the transition smoothly
        switcher.start(50.0f);  // 50ms crossfade for better smoothing and click prevention
        // Don't set currentType yet - wait until crossfade completes
        // This preserves the old filter state for the crossfade
    }
}

void FxFilter::setTargets(const FilterTargets& targets)
{
    targetType = juce::jlimit(0, 4, targets.type);
    slope = targets.slope;
    spreadCents = targets.spread;
    keytrack = targets.keytrack;
    mix = juce::jlimit(0.0f, 1.0f, targets.mix);
    
    // Store current values
    currentCutoff = targets.cutoff;
    currentRes = targets.res;
    currentDrive = targets.drive;
    currentSpread = targets.spread;
    
    // Apply key tracking to cutoff
    float effectiveCutoff = applyKeyTracking(targets.cutoff, targets.keytrack, currentMIDINote);
    
    // Set target values immediately (no smoothing delay on first call)
    if (cutoffSm.getCurrentValue() == cutoffSm.getTargetValue() && cutoffSm.getCurrentValue() == 1200.0f)
    {
        // First time - set current and target to same value
        cutoffSm.setCurrentAndTargetValue(effectiveCutoff);
        resSm.setCurrentAndTargetValue(targets.res);
        driveSm.setCurrentAndTargetValue(targets.drive);
    }
    else
    {
        cutoffSm.setTargetValue(effectiveCutoff);
        resSm.setTargetValue(targets.res);
        driveSm.setTargetValue(targets.drive);
    }
    
    // Check if type changed (only if not already crossfading)
    if (targetType != currentType && !switcher.isActive())
    {
        switchFilterType(targetType);
    }
}

void FxFilter::processSVFMode(juce::AudioBuffer<float>& buffer, int type, float cutoff, float q, int slopeSel, float driveDb, float spreadCents)
{
    // Spread removed - no stereo detuning
    // Use same cutoff for both channels
    const float cutoffL = cutoff;
    const float cutoffR = cutoff;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Apply drive pre-filter (before filtering) - modify buffer in place
    // This is safe because during crossfade, we work on copies (tmpA/tmpB)
    if (driveDb > 0.01f)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto* x = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i)
            {
                x[i] = applyDrive(x[i], driveDb);
            }
        }
    }
    
    // Select appropriate filter instances based on type
    juce::dsp::StateVariableTPTFilter<float>* filterL = nullptr;
    juce::dsp::StateVariableTPTFilter<float>* filterR = nullptr;
    juce::dsp::StateVariableTPTFilter<float>* filter2L = nullptr;
    juce::dsp::StateVariableTPTFilter<float>* filter2R = nullptr;
    
    if (type == 0) // LP
    {
        filterL = &svfLP_L;
        filterR = &svfLP_R;
        filter2L = &svfLP2_L;
        filter2R = &svfLP2_R;
    }
    else if (type == 1) // HP
    {
        filterL = &svfHP_L;
        filterR = &svfHP_R;
        filter2L = &svfHP2_L;
        filter2R = &svfHP2_R;
    }
    else // BP (type == 2)
    {
        filterL = &svfBP_L;
        filterR = &svfBP_R;
        filter2L = &svfBP2_L;
        filter2R = &svfBP2_R;
    }
    
    // Set filter parameters for left channel
    filterL->setCutoffFrequency(cutoffL);
    filterL->setResonance(q);
    if (slopeSel > 0)
    {
        filter2L->setCutoffFrequency(cutoffL);
        filter2L->setResonance(q);
    }
    
    // Set filter parameters for right channel (if stereo)
    if (numChannels > 1)
    {
        filterR->setCutoffFrequency(cutoffR);
        filterR->setResonance(q);
        if (slopeSel > 0)
        {
            filter2R->setCutoffFrequency(cutoffR);
            filter2R->setResonance(q);
        }
    }
    
    // Create audio block and process
    juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(), numChannels, numSamples);
    
    if (slopeSel > 0) // 24dB cascade
    {
        // Process left channel
        juce::dsp::AudioBlock<float> blockL = block.getSingleChannelBlock(0);
        juce::dsp::ProcessContextReplacing<float> contextL(blockL);
        filterL->process(contextL);
        filter2L->process(contextL);
        
        // Process right channel if present
        if (numChannels > 1)
        {
            juce::dsp::AudioBlock<float> blockR = block.getSingleChannelBlock(1);
            juce::dsp::ProcessContextReplacing<float> contextR(blockR);
            filterR->process(contextR);
            filter2R->process(contextR);
        }
    }
    else // 12dB single stage
    {
        // Process left channel
        juce::dsp::AudioBlock<float> blockL = block.getSingleChannelBlock(0);
        juce::dsp::ProcessContextReplacing<float> contextL(blockL);
        filterL->process(contextL);
        
        // Process right channel if present
        if (numChannels > 1)
        {
            juce::dsp::AudioBlock<float> blockR = block.getSingleChannelBlock(1);
            juce::dsp::ProcessContextReplacing<float> contextR(blockR);
            filterR->process(contextR);
        }
    }
}

void FxFilter::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::ScopedNoDenormals nd;
    
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples <= 0) return;
    
    // Store dry signal for mix (before any processing)
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int c = 0; c < numChannels; ++c)
    {
        dryBuffer.copyFrom(c, 0, buffer, c, 0, numSamples);
    }
    
    // Use target values directly for instant response (no smoothing delay)
    // UI already smooths via SliderAttachment
    float cut = cutoffSm.getTargetValue();
    float r = resSm.getTargetValue();
    float drv = driveSm.getTargetValue();
    
    // Ensure we have valid values
    cut = juce::jlimit(20.0f, 20000.0f, cut);
    r = juce::jlimit(0.0f, 1.0f, r);
    drv = juce::jlimit(0.0f, 36.0f, drv);
    
    // Update smoothing to match targets (for consistency)
    cutoffSm.setCurrentAndTargetValue(cut);
    resSm.setCurrentAndTargetValue(r);
    driveSm.setCurrentAndTargetValue(drv);
    
    // Map res to Q for SVF (musical mapping) - BP uses gentler curve for smoother resonance
    float qMapped = mapResToQ(r, currentType);
    
    // Process based on filter type
    if (switcher.isActive())
    {
        // Crossfade between old and new filter types
        tmpA.setSize(numChannels, numSamples, false, false, true);
        tmpB.setSize(numChannels, numSamples, false, false, true);
        
        // Copy input to temp buffers (before processing)
        // This ensures both old and new filters process from the same input signal
        for (int c = 0; c < numChannels; ++c)
        {
            tmpA.copyFrom(c, 0, buffer, c, 0, numSamples);
            tmpB.copyFrom(c, 0, buffer, c, 0, numSamples);
        }
        
        // Process old filter type (use currentType - it hasn't changed yet)
        int oldType = currentType; // This is the old type (before switchFilterType was called)
        float qOld = mapResToQ(r, oldType);
        
        // Apply drive to tmpA before processing old filter (if needed)
        if (oldType <= 2 && drv > 0.01f)
        {
            // Drive will be applied inside processSVFMode
            processSVFMode(tmpA, oldType, cut, qOld, slope, drv, spreadCents);
        }
        else if (oldType <= 2)
        {
            processSVFMode(tmpA, oldType, cut, qOld, slope, 0.0f, spreadCents);
        }
        else if (oldType == 3)
        {
            // Comb-
            float combFeedbackOld = mapResToCombFeedback(r);
            combMinus.processBlock(tmpA, cut, combFeedbackOld, (slope == 1) ? 1.0f : 0.0f, spreadCents, drv);
        }
        else
        {
            // Comb+
            float combFeedbackOld = mapResToCombFeedback(r);
            combPlus.processBlock(tmpA, cut, combFeedbackOld, (slope == 1) ? 1.0f : 0.0f, spreadCents, drv);
        }
        
        // Process new filter type
        float qNew = mapResToQ(r, targetType);
        
        // Apply drive to tmpB before processing new filter (if needed)
        if (targetType <= 2 && drv > 0.01f)
        {
            // Drive will be applied inside processSVFMode
            processSVFMode(tmpB, targetType, cut, qNew, slope, drv, spreadCents);
        }
        else if (targetType <= 2)
        {
            processSVFMode(tmpB, targetType, cut, qNew, slope, 0.0f, spreadCents);
        }
        else if (targetType == 3)
        {
            // Comb-
            float combFeedbackNew = mapResToCombFeedback(r);
            combMinus.processBlock(tmpB, cut, combFeedbackNew, (slope == 1) ? 1.0f : 0.0f, spreadCents, drv);
        }
        else
        {
            // Comb+
            float combFeedbackNew = mapResToCombFeedback(r);
            combPlus.processBlock(tmpB, cut, combFeedbackNew, (slope == 1) ? 1.0f : 0.0f, spreadCents, drv);
        }
        
        // Crossfade
        switcher.render(buffer, tmpA, tmpB);
        
        // Update currentType after crossfade completes
        if (!switcher.isActive())
        {
            currentType = targetType;
        }
    }
    else
    {
        // No crossfade, process directly
        // Ensure we have a valid filter type (default to LP if somehow invalid)
        int typeToUse = (currentType >= 0 && currentType <= 4) ? currentType : 0;
        
        if (typeToUse <= 2)
        {
            // SVF mode (LP/HP/BP) using JUCE filters
            processSVFMode(buffer, typeToUse, cut, qMapped, slope, drv, spreadCents);
        }
        else if (typeToUse == 3)
        {
            // Comb-
            float combFeedback = mapResToCombFeedback(r);
            combMinus.processBlock(buffer, cut, combFeedback, (slope == 1) ? 1.0f : 0.0f, spreadCents, drv);
        }
        else if (typeToUse == 4)
        {
            // Comb+
            float combFeedback = mapResToCombFeedback(r);
            combPlus.processBlock(buffer, cut, combFeedback, (slope == 1) ? 1.0f : 0.0f, spreadCents, drv);
        }
    }
    
    // Soft limit: y = x / (1 + 0.5|x|) - only for Comb modes (JUCE filters are stable)
    // Only apply soft limiting for Comb modes to prevent extreme values
    if (currentType >= 3) // Comb- or Comb+
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto* p = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = p[i];
                p[i] = x / (1.0f + 0.5f * std::abs(x));
            }
        }
    }
    
    // Apply dry/wet mix
    float wetMix = mix;
    float dryMix = 1.0f - mix;
    
    for (int c = 0; c < numChannels; ++c)
    {
        auto* out = buffer.getWritePointer(c);
        auto* dry = dryBuffer.getReadPointer(c);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float result = dryMix * dry[i] + wetMix * out[i];
            
            // Safety check for NaN/infinity
            if (!std::isfinite(result))
                result = dry[i]; // Fallback to dry signal
            
            // Clamp to prevent extreme values
            result = juce::jlimit(-2.0f, 2.0f, result);
            
            out[i] = result;
        }
    }
}
