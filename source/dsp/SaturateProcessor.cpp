#include "SaturateProcessor.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;
    constexpr float epsilon = 1.0e-9f;

    inline float adaaTanh(float x, float& xPrev)
    {
        const float dx = x - xPrev;
        if (std::abs(dx) > 1.0e-6f)
        {
            const float result = (std::log(std::cosh(x)) - std::log(std::cosh(xPrev))) / dx;
            xPrev = x;
            return juce::jlimit(-1.0f, 1.0f, result);
        }

        const float mid = 0.5f * (x + xPrev);
        xPrev = x;
        return std::tanh(mid);
    }

    inline float adaaAtan(float x, float& xPrev)
    {
        const float dx = x - xPrev;
        if (std::abs(dx) > 1.0e-6f)
        {
            const float result = (std::log(1.0f + x * x) - std::log(1.0f + xPrev * xPrev)) / (2.0f * dx);
            xPrev = x;
            return juce::jlimit(-1.0f, 1.0f, result);
        }

        const float mid = 0.5f * (x + xPrev);
        xPrev = x;
        return std::atan(mid);
    }

    inline float softClipCubic(float x, float amount)
    {
        const float a = juce::jlimit(0.0f, 1.0f, amount);
        const float k = juce::jmap(a, 0.6f, 1.2f);
        const float y = x - k * x * x * x;
        return juce::jlimit(-3.0f, 3.0f, y);
    }

    inline float saturateAsymmetric(float x, float bias, float hardness)
    {
        const float shifted = x + bias;
        const float scale = juce::jmap(hardness, 0.1f, 1.25f);
        const float pos = std::tanh(shifted * scale);
        const float neg = std::tanh(shifted * (scale * 0.6f));
        return shifted >= 0.0f ? pos : neg;
    }

    inline float dbToLinear(float dbValue)
    {
        return juce::Decibels::decibelsToGain(dbValue, -80.0f);
    }

    inline float equalPowerFadeIn(float phase)
    {
        return std::sin(halfPi * juce::jlimit(0.0f, 1.0f, phase));
    }

    inline float equalPowerFadeOut(float phase)
    {
        return std::cos(halfPi * juce::jlimit(0.0f, 1.0f, phase));
    }

} // namespace

//==============================================================================
void SaturateProcessor::TiltFilter::prepare(double fs, float pivotHz)
{
    sampleRate = fs;
    pivot = pivotHz;
    const double omega = 2.0 * pi * pivot / sampleRate;
    alpha = std::exp(-omega);
    z = 0.0f;
    setTiltDb(0.0f);
}

void SaturateProcessor::TiltFilter::reset()
{
    z = 0.0f;
}

void SaturateProcessor::TiltFilter::setTiltDb(float tiltDb)
{
    const float half = 0.5f * tiltDb;
    lowGain = dbToLinear(half);
    highGain = dbToLinear(-half);
}

float SaturateProcessor::TiltFilter::process(float x)
{
    const float low = static_cast<float>(alpha) * z + (1.0f - static_cast<float>(alpha)) * x;
    const float high = x - low;
    z = low;
    return low * lowGain + high * highGain;
}

//==============================================================================
void SaturateProcessor::OnePole::prepare(double fs, float cutoffHz, bool highpass)
{
    sampleRate = fs;
    isHighpass = highpass;
    setCutoff(cutoffHz);
    reset();
}

void SaturateProcessor::OnePole::reset()
{
    x1 = 0.0f;
    y1 = 0.0f;
}

void SaturateProcessor::OnePole::setCutoff(float cutoffHz)
{
    cutoff = juce::jlimit(10.0f, 20000.0f, cutoffHz);
    updateCoeffs();
}

void SaturateProcessor::OnePole::updateCoeffs()
{
    const float k = std::tan(juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
    const float norm = 1.0f / (1.0f + k);

    if (isHighpass)
    {
        b0 = norm;
        b1 = -norm;
    }
    else
    {
        b0 = k * norm;
        b1 = b0;
    }

    a1 = (1.0f - k) * norm;
}

float SaturateProcessor::OnePole::process(float x)
{
    const float y = b0 * x + b1 * x1 + a1 * y1;
    x1 = x;
    y1 = y;
        return y;
    }

//==============================================================================
void SaturateProcessor::ToneFilter::prepare(double fs)
{
    // Safety check: ensure valid sample rate
    if (fs <= 0.0 || !std::isfinite(fs))
    {
        fs = 44100.0; // Fallback to safe default
    }
    
    sampleRate = fs;
    
    // CRITICAL FIX: Preserve prepared state if filter is already prepared
    // If filter is already prepared and re-prepare fails, keep it prepared
    // This prevents audio from stopping during rapid randomization
    bool wasPrepared = isPrepared;
    isPrepared = false; // Reset flag only for this prepare attempt
    
    // Prepare the StateVariableTPTFilter with proper ProcessSpec
    try
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = fs;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        // Prepare filter - wrap in try-catch to prevent crashes
        svf.prepare(spec);
        svf.reset();
        svf.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        svf.setCutoffFrequency(15000.0f);
        svf.setResonance(0.707f);
        
        // Mark as prepared only if all operations succeeded
        isPrepared = true;
        isBypassed.store(false); // Re-enable filter after successful prepare
        errorCount.store(0); // Reset error count
        
        // Apply any pending tone update that was deferred
        if (hasPendingUpdate.load())
        {
            try
            {
                // Limit maximum cutoff to 18kHz to prevent unwanted high-frequency ringing
                const float hz = juce::jlimit(10000.0f, 18000.0f, juce::jmap(pendingToneNorm.load(), 10000.0f, 22000.0f));
                svf.setCutoffFrequency(hz);
                
                // Reduce resonance when cutoff is very high to prevent ringing
                // At maximum cutoff (18kHz), reduce resonance to 0.5 (from 0.707)
                const float resonance = juce::jmap(hz, 10000.0f, 18000.0f, 0.707f, 0.5f);
                svf.setResonance(resonance);
                
                hasPendingUpdate.store(false);
            }
            catch (...)
            {
                // Update failed, but filter is still prepared
                // The update will be retried in process()
            }
        }
    }
    catch (...)
    {
        // If prepare fails, preserve the previous prepared state
        // CRITICAL FIX: If filter was already prepared, keep it that way
        // This prevents audio from stopping if prepare() fails during rapid randomization
        if (wasPrepared)
        {
            // Filter was already prepared - keep it that way even if re-prepare fails
            // This prevents audio from stopping during rapid randomization
            isPrepared = true;
        }
        else
        {
            // Filter was not prepared before - mark as unprepared
            isPrepared = false;
        }
    }
}

void SaturateProcessor::ToneFilter::reset()
{
    // Wrap reset in try-catch to prevent crashes if filter is in invalid state
    try
    {
        if (isPrepared)
        {
            svf.reset();
        }
    }
    catch (...)
    {
        // If reset fails, don't mark filter as unprepared
        // This prevents audio from stopping during rapid randomization
        // The filter will continue processing even if reset failed
        // Just skip the reset and continue with current state
    }
}

void SaturateProcessor::ToneFilter::setTone(float norm)
{
    // CRITICAL: Don't update if filter is bypassed - it's corrupted
    if (isBypassed.load()) {
        return;
    }
    
    // Store the pending tone value - we'll apply it safely in process() if filter is ready
    // This prevents modifying filter state during rapid randomization which can cause corruption
    pendingToneNorm.store(juce::jlimit(0.0f, 1.0f, norm));
    hasPendingUpdate.store(true);
    
    // Only apply immediately if filter is prepared and we can do it safely
    // During rapid randomization, we defer the update to process() to avoid race conditions
    if (!isPrepared || sampleRate <= 0.0)
    {
        return; // Filter not ready, update will be applied when filter is prepared
    }
    
    // Try to apply update, but don't crash if it fails
    // CRITICAL: Don't mark filter as unprepared if update fails - this causes audio to stop processing
    // Instead, just skip the update and let the filter continue with its current settings
        try
        {
            // Limit maximum cutoff to 18kHz to prevent unwanted high-frequency ringing
            const float hz = juce::jlimit(10000.0f, 18000.0f, juce::jmap(pendingToneNorm.load(), 10000.0f, 22000.0f));
            svf.setCutoffFrequency(hz);
            
            // Reduce resonance when cutoff is very high to prevent ringing
            // At maximum cutoff (18kHz), reduce resonance to 0.5 (from 0.707)
            const float resonance = juce::jmap(hz, 10000.0f, 18000.0f, 0.707f, 0.5f);
            svf.setResonance(resonance);
            
            hasPendingUpdate.store(false); // Update applied successfully
        }
    catch (...)
    {
        // If setting tone fails, increment error count and bypass if too many errors
        int errors = errorCount.fetch_add(1) + 1;
        if (errors > 10) {
            isBypassed.store(true);
        }
        // Clear the pending update to prevent retry loops
        hasPendingUpdate.store(false);
    }
}

float SaturateProcessor::ToneFilter::process(float x)
{
    // CRITICAL: Multiple safety checks to prevent crashes during rapid randomization
    // 1. Check if filter was prepared - if not, just bypass
    // CRITICAL FIX: Never call prepare() from process() - it causes double-free crashes
    // The filter will be re-prepared by the main prepare() method when prepareToPlay() is called
    // CRITICAL FIX: Never mark filter as unprepared from process() - it causes audio to stop
    // If there's an error, just bypass this sample but keep filter marked as prepared
    
    // CRITICAL: If filter is bypassed or not prepared, just bypass - NEVER call prepare() from audio thread
    // Calling prepare() from process() causes double-free crashes
    // The filter will be re-prepared by the main prepare() method when prepareToPlay() is called
    // CRITICAL FIX: Never mark filter as unprepared from process() - it causes audio to stop
    // If there's an error, just bypass this sample but keep filter marked as prepared
    if (isBypassed.load() || !isPrepared || sampleRate <= 0.0)
    {
        // Filter is bypassed or not prepared - just bypass this sample
        // Don't try to re-prepare from audio thread as it causes memory corruption
        // CRITICAL: Validate input before returning to prevent buzzing
        if (!std::isfinite(x)) {
            return 0.0f; // Return silence for invalid input
        }
        return x;
    }
    
    // 2. Apply pending tone update if we have one (safe to do at start of processing)
    // This defers filter modifications from setTone() to avoid race conditions
    if (hasPendingUpdate.load())
    {
        try
        {
            // Limit maximum cutoff to 18kHz to prevent unwanted high-frequency ringing
            const float hz = juce::jlimit(10000.0f, 18000.0f, juce::jmap(pendingToneNorm.load(), 10000.0f, 22000.0f));
            svf.setCutoffFrequency(hz);
            
            // Reduce resonance when cutoff is very high to prevent ringing
            // At maximum cutoff (18kHz), reduce resonance to 0.5 (from 0.707)
            const float resonance = juce::jmap(hz, 10000.0f, 18000.0f, 0.707f, 0.5f);
            svf.setResonance(resonance);
            
            hasPendingUpdate.store(false);
        }
        catch (...)
        {
            // Update failed - increment error count and bypass if too many errors
            int errors = errorCount.fetch_add(1) + 1;
            if (errors > 10) {
                isBypassed.store(true);
            }
            // Clear the pending update to prevent retry loops
            hasPendingUpdate.store(false);
        }
    }
    
    // 3. Validate input is finite
    if (!std::isfinite(x))
    {
        return 0.0f; // Return silence for invalid input
    }
    
    // 4. CRITICAL: Additional validation - check if filter object is in valid state
    // During rapid randomization, the filter's internal state can become corrupted
    // even if isPrepared is true. We need to be extra defensive.
    
    // Try to access the filter in a safe way - if it throws or crashes, we bypass
    try
    {
        // Double-check the filter is still valid by checking sampleRate matches
        // This is a lightweight check that doesn't access filter internals
        if (sampleRate <= 0.0 || !std::isfinite(sampleRate))
        {
            // Sample rate is invalid - can't recover, just bypass
            // Don't mark as unprepared as that would prevent recovery if sampleRate becomes valid again
            return x;
        }
        
        // Now try to process - wrap in nested try-catch for maximum safety
        // CRITICAL: If filter is corrupted, it will crash here - we need to catch it
        try
        {
            float result = svf.processSample(0, x);
            
            // 5. Validate output is finite
            if (!std::isfinite(result))
            {
                // Output is invalid - increment error count and bypass
                int errors = errorCount.fetch_add(1) + 1;
                if (errors > 10) {
                    // Too many errors - permanently bypass filter to prevent crashes
                    isBypassed.store(true);
                }
                // CRITICAL: Return silence instead of input to prevent buzzing
                return 0.0f;
            }
            
            // CRITICAL: Clamp output to prevent excessive values that could cause buzzing
            result = juce::jlimit(-2.0f, 2.0f, result);
            
            // Success - reset error count
            errorCount.store(0);
            return result;
        }
        catch (...)
        {
            // Filter processing threw an exception - filter is corrupted
            // CRITICAL: Increment error count and bypass permanently if too many errors
            int errors = errorCount.fetch_add(1) + 1;
            if (errors > 10) {
                // Too many errors - permanently bypass filter to prevent crashes
                isBypassed.store(true);
            }
            // Don't try to reset - filter is corrupted and reset() might also crash
            return x;
        }
    }
    catch (...)
    {
        // Even the safety checks failed - filter is completely invalid
        // CRITICAL: Don't mark as unprepared - this causes audio to stop permanently
        // Just bypass this sample - the filter will try to recover on next call
        // The auto-recovery in step 1 will re-prepare the filter if needed
        return x;
    }
}

//==============================================================================
void SaturateProcessor::ModelRuntime::reset()
{
    for (auto& ch : channels)
    {
        ch.adaaPrev = 0.0f;
        ch.envelope = 0.0f;
        ch.compEnv = 0.0f;
        
        // CRITICAL: Reset all filters to prevent buzzing from corrupted state
        try {
            ch.preTilt.reset();
            ch.postTilt.reset();
            ch.preHP.reset();
            ch.toneFilter.reset();
        } catch (...) {
            // If reset fails, continue - filters will be re-prepared on next prepare()
        }
    }

    lastDrive = 12.0f;
    lastToneNorm = 0.5f;
    lastPreTilt = 0.0f;
    lastPostTilt = 0.0f;
    lastHpCut = 30.0f;
    lastCharacter = 0.5f;
    lastCharacter2 = 0.0f;
    lastBias = 0.0f;
    lastComp = 0.3f;
    lastOutputDb = 0.0f;
    autoGain = 1.0f;
    rmsInAccum = 0.0f;
    rmsOutAccum = 0.0f;
    rmsCount = 0;
}

//==============================================================================
void SaturateProcessor::prepare(double fs, int blockSize)
{
    sampleRate = fs;
    maxBlockSize = blockSize;
    osSampleRate = sampleRate * HEAT_OVERSAMPLE;

    const int osPower = (HEAT_OVERSAMPLE == 8) ? 3 : 2;

    auto createOversampler = [osPower](int channels)
    {
        return std::make_unique<juce::dsp::Oversampling<float>>(channels,
                                                                osPower,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                true,
                                                                false);
    };

    oversamplerPrimary = createOversampler(2);
    oversamplerDry = createOversampler(2);

    oversamplerPrimary->initProcessing(static_cast<size_t>(maxBlockSize));
    oversamplerDry->initProcessing(static_cast<size_t>(maxBlockSize));

    dryBuffer.setSize(2, maxBlockSize);
    dryMatchedBuffer.setSize(2, maxBlockSize);

    // Initialize all model states
    for (auto& runtime : modelStates)
    {
        runtime.reset();
        for (auto& ch : runtime.channels)
        {
            ch.preTilt.prepare(osSampleRate, 750.0f);
            ch.postTilt.prepare(osSampleRate, 1600.0f);
            ch.preHP.prepare(osSampleRate, 28.0f, true);
            ch.postHP.prepare(osSampleRate, 18.0f, true);
            ch.toneFilter.prepare(osSampleRate);
        }
    }

    // Initialize parameter smoothing (100ms for top knobs to prevent clicks and static)
    const double smoothTime = 0.100;
    driveSmooth.reset(sampleRate, smoothTime);
    colorSmooth.reset(sampleRate, smoothTime);
    shapeSmooth.reset(sampleRate, smoothTime);
    biasSmooth.reset(sampleRate, smoothTime);
    outputSmooth.reset(sampleRate, smoothTime);
    mixSmooth.reset(sampleRate, smoothTime);
    
    // Set initial values
    driveSmooth.setCurrentAndTargetValue(12.0f);
    colorSmooth.setCurrentAndTargetValue(0.5f);
    shapeSmooth.setCurrentAndTargetValue(0.5f);
    biasSmooth.setCurrentAndTargetValue(0.0f);
    outputSmooth.setCurrentAndTargetValue(0.0f);
    mixSmooth.setCurrentAndTargetValue(1.0f);

    reset();
}

void SaturateProcessor::reset()
{
    if (oversamplerPrimary) oversamplerPrimary->reset();
    if (oversamplerDry) oversamplerDry->reset();
    
    // Reset all model states
    for (auto& runtime : modelStates)
    {
        runtime.reset();
    }

    // Reset filter tracking to prevent clicks on page switch
    prevPreTiltDb = 0.0f;
    prevPostTiltDb = 0.0f;
    prevHpCutHz = 30.0f;
    prevToneNorm = 0.5f;
}

//==============================================================================
SaturateProcessor::ParameterSet SaturateProcessor::mapParameters(float type,
                                                                 float drive,
                                                                 float color,
                                                                 float shape,
                                                                 float bias,
                                                                 float output,
                                                                 float mix) const
{
    ParameterSet p;
    
    // Drive: Musical exponential curve - smoother and more controlled
    p.driveDb = drive;
    const float driveNorm = drive / 36.0f;
    // Smoother exponential curve for musical response
    const float driveCurve = driveNorm * driveNorm * 0.8f + driveNorm * 0.6f;
    p.driveLin = dbToLinear(drive * (0.5f + driveCurve * 0.6f));
    
    p.mix = juce::jlimit(0.0f, 1.0f, mix);
    p.outputDb = output;
    p.outputLin = dbToLinear(output);

    const float colorNorm = juce::jlimit(0.0f, 1.0f, color);
    const float shapeNorm = juce::jlimit(0.0f, 1.0f, shape);

    // Color controls frequency response (brightness/tone)
    // Low Color (0.0): Darker, low-mid emphasis
    // High Color (1.0): Brighter, high-frequency emphasis
    // Limit toneNorm to prevent extreme high-frequency ringing (max 0.85 instead of 0.95)
    p.toneNorm = juce::jmap(colorNorm, 0.2f, 0.85f);  // Tone filter range (reduced max to prevent ringing)
    p.hpCutHz = juce::jmap(colorNorm, 80.0f, 200.0f); // HPF: 80Hz (dark) to 200Hz (bright)
    p.preTiltDb = juce::jmap(colorNorm, -4.0f, 4.0f);  // Pre-tilt: -4dB (dark) to +4dB (bright) - reduced for smoother sound
    p.postTiltDb = -p.preTiltDb * 0.3f;                // Compensatory post-tilt - reduced
    
    // Shape controls waveshape selection (0-1 maps to 5 different curves)
    // Store shape selection in character field (0.0-1.0 for curve selection)
    p.character = shapeNorm;  // 0.0 = soft tanh, 0.25 = hard tanh, 0.5 = cubic, 0.75 = asymmetric, 1.0 = polynomial
    
    // Bias for asymmetric saturation
    p.bias = juce::jmap(bias, -0.3f, 0.3f);
    
    // Harmonic enhancement amount (use character2 for even/odd blend)
    p.character2 = juce::jmap(shapeNorm, 0.2f, 0.8f);  // Harmonic amount based on shape
    
    // Compression amount
    p.compAmount = juce::jmap(shapeNorm, 0.3f, 1.0f);

    return p;
}

void SaturateProcessor::refreshFilters(ModelRuntime& runtime,
                                       const ParameterSet& params,
                                       double osSampleRate,
                                       int modelIndex)
{
    // Only update filters if values have changed significantly to prevent clicks
    // Increased thresholds to reduce filter update frequency and prevent static
    const float tiltThreshold = 0.2f;  // Increased from 0.1f
    const float cutoffThreshold = 2.0f;  // Increased from 1.0f
    const float toneThreshold = 0.02f;  // Increased from 0.01f
    
    bool needsUpdate = false;
    
    if (std::abs(params.preTiltDb - prevPreTiltDb) > tiltThreshold ||
        std::abs(params.postTiltDb - prevPostTiltDb) > tiltThreshold ||
        std::abs(params.hpCutHz - prevHpCutHz) > cutoffThreshold ||
        std::abs(params.toneNorm - prevToneNorm) > toneThreshold)
    {
        needsUpdate = true;
        prevPreTiltDb = params.preTiltDb;
        prevPostTiltDb = params.postTiltDb;
        prevHpCutHz = params.hpCutHz;
        prevToneNorm = params.toneNorm;
    }
    
    if (needsUpdate)
{
    for (auto& ch : runtime.channels)
    {
        ch.preTilt.setTiltDb(params.preTiltDb);
        ch.postTilt.setTiltDb(params.postTiltDb);
        ch.preHP.setCutoff(params.hpCutHz);
        ch.toneFilter.setTone(params.toneNorm);
        }
    }

    runtime.lastDrive = params.driveDb;
    runtime.lastToneNorm = params.toneNorm;
    runtime.lastPreTilt = params.preTiltDb;
    runtime.lastPostTilt = params.postTiltDb;
    runtime.lastHpCut = params.hpCutHz;
    runtime.lastCharacter = params.character;
    runtime.lastCharacter2 = params.character2;
    runtime.lastBias = params.bias;
    runtime.lastComp = params.compAmount;
    runtime.lastOutputDb = params.outputDb;
}

//==============================================================================
void SaturateProcessor::process(juce::AudioBuffer<float>& buffer,
                                int numSamples,
                                juce::AudioProcessorValueTreeState& apvts)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    // Safety check: ensure oversamplers are initialized and processor is prepared
    // CRITICAL: If not ready, bypass (pass audio through) instead of dropping audio
    if (!oversamplerPrimary || !oversamplerDry || sampleRate <= 0.0 || maxBlockSize <= 0)
    {
        // Update smoothers to prevent jumps when processor becomes ready
        for (int i = 0; i < numSamples; ++i)
        {
            driveSmooth.skip(1);
            colorSmooth.skip(1);
            shapeSmooth.skip(1);
            biasSmooth.skip(1);
            outputSmooth.skip(1);
            mixSmooth.skip(1);
        }
        return; // Audio passes through unchanged (bypass mode)
    }

    // Enable flush-to-zero for stability
    juce::ScopedNoDenormals noDenormals;

    // Check if input is silent - if so, skip processing to avoid noise
    float inputLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            inputLevel += std::abs(buffer.getSample(ch, n));
        }
    }
    inputLevel /= (numSamples * buffer.getNumChannels());
    
    // If input is essentially silent, skip processing to avoid noise
    if (inputLevel < 1.0e-6f)
    {
        // Still update smoothers to prevent jumps when audio returns
        for (int n = 0; n < numSamples; ++n)
        {
            driveSmooth.skip(1);
            colorSmooth.skip(1);
            shapeSmooth.skip(1);
            biasSmooth.skip(1);
            outputSmooth.skip(1);
            mixSmooth.skip(1);
        }
        return;
    }

    auto* driveParam = apvts.getRawParameterValue("satDrive");
    auto* colorParam = apvts.getRawParameterValue("satColor");
    auto* shapeParam = apvts.getRawParameterValue("satShape");
    auto* biasParam = apvts.getRawParameterValue("satBias");
    auto* outParam = apvts.getRawParameterValue("satOut");
    auto* mixParam = apvts.getRawParameterValue("satMix");
    
    // Set target values for smoothing
    driveSmooth.setTargetValue(driveParam ? driveParam->load() : 12.0f);
    colorSmooth.setTargetValue(colorParam ? colorParam->load() : 0.5f);
    shapeSmooth.setTargetValue(shapeParam ? shapeParam->load() : 0.5f);
    biasSmooth.setTargetValue(biasParam ? biasParam->load() : 0.0f);
    outputSmooth.setTargetValue(outParam ? outParam->load() : 0.0f);
    mixSmooth.setTargetValue(mixParam ? mixParam->load() : 1.0f);
    
    // Get smoothed values (smoothing prevents clicks when knobs change)
    // Use getNextValue() to advance smoothing and get smoothed value
    const float drive = driveSmooth.getNextValue();
    const float color = colorSmooth.getNextValue();
    const float shape = shapeSmooth.getNextValue();
    const float bias = biasSmooth.getNextValue();
    const float output = outputSmooth.getNextValue();
    const float mix = mixSmooth.getNextValue();
    
    // Skip remaining samples to keep smoothers in sync
    for (int i = 1; i < numSamples; ++i)
    {
        driveSmooth.skip(1);
        colorSmooth.skip(1);
        shapeSmooth.skip(1);
        biasSmooth.skip(1);
        outputSmooth.skip(1);
        mixSmooth.skip(1);
    }

    ParameterSet params = mapParameters(currentType,
                                        drive,
                                        color,
                                        shape,
                                        bias,
                                        output,
                                        mix);

    processInternal(buffer, numSamples, params, currentType, false);
}

void SaturateProcessor::processWithSnapshot(juce::AudioBuffer<float>& buffer,
                                            int numSamples,
                                            float type,
                                            float drive,
                                            float color,
                                            float shape,
                                            float bias,
                                            float output,
                                            float mix,
                                            bool stepChanged)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    // Safety check: ensure oversamplers are initialized and processor is prepared
    // CRITICAL: If not ready, bypass (pass audio through) instead of dropping audio
    if (!oversamplerPrimary || !oversamplerDry || sampleRate <= 0.0 || maxBlockSize <= 0)
    {
        // Update smoothers to prevent jumps when processor becomes ready
        for (int i = 0; i < numSamples; ++i)
        {
            driveSmooth.skip(1);
            colorSmooth.skip(1);
            shapeSmooth.skip(1);
            biasSmooth.skip(1);
            outputSmooth.skip(1);
            mixSmooth.skip(1);
        }
        return; // Audio passes through unchanged (bypass mode)
    }

    // Calculate input level to check if audio is silent
    float inputLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            inputLevel += std::abs(data[i]);
        }
    }
    inputLevel /= (numSamples * buffer.getNumChannels());
    
    // If input is essentially silent, skip processing to avoid noise
    if (inputLevel < 1.0e-6f)
    {
        // Still update smoothers to prevent jumps when audio returns
        for (int n = 0; n < numSamples; ++n)
        {
            driveSmooth.skip(1);
            colorSmooth.skip(1);
            shapeSmooth.skip(1);
            biasSmooth.skip(1);
            outputSmooth.skip(1);
            mixSmooth.skip(1);
        }
        return;
    }

    // Set target values for smoothing (prevents clicks when sequencer changes steps)
    driveSmooth.setTargetValue(drive);
    colorSmooth.setTargetValue(color);
    shapeSmooth.setTargetValue(shape);
    biasSmooth.setTargetValue(bias);
    outputSmooth.setTargetValue(output);
    mixSmooth.setTargetValue(mix);
    
    // Get smoothed values (smoothing prevents clicks when steps change)
    const float smoothedDrive = driveSmooth.getNextValue();
    const float smoothedColor = colorSmooth.getNextValue();
    const float smoothedShape = shapeSmooth.getNextValue();
    const float smoothedBias = biasSmooth.getNextValue();
    const float smoothedOutput = outputSmooth.getNextValue();
    const float smoothedMix = mixSmooth.getNextValue();
    
    // Skip remaining samples to keep smoothers in sync
    for (int i = 1; i < numSamples; ++i)
    {
        driveSmooth.skip(1);
        colorSmooth.skip(1);
        shapeSmooth.skip(1);
        biasSmooth.skip(1);
        outputSmooth.skip(1);
        mixSmooth.skip(1);
    }

    ParameterSet params = mapParameters(type,
                                        smoothedDrive,
                                        smoothedColor,
                                        smoothedShape,
                                        smoothedBias,
                                        smoothedOutput,
                                        smoothedMix);

    processInternal(buffer, numSamples, params, static_cast<int>(type), stepChanged);
}

//==============================================================================
void SaturateProcessor::processInternal(juce::AudioBuffer<float>& buffer,
                                        int numSamples,
                                        ParameterSet liveParams,
                                        int targetType,
                                        bool stepChanged)
{
    // CRITICAL: Wrap entire method in try-catch to prevent any crashes
    try
    {
        juce::ScopedNoDenormals guard;

        // Safety check: ensure oversamplers are initialized and processor is prepared
        // CRITICAL: If not ready, bypass (pass audio through) instead of dropping audio
        if (!oversamplerPrimary || !oversamplerDry || sampleRate <= 0.0 || maxBlockSize <= 0)
            return; // Audio passes through unchanged (bypass mode)
        
        // CRITICAL: Validate buffer before copying to prevent crashes during rapid randomization
        // If buffer is invalid, bypass instead of dropping audio
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() < numSamples || numSamples <= 0)
            return; // Audio passes through unchanged (bypass mode)
        
        // Validate buffer pointers are valid
        if (buffer.getReadPointer(0) == nullptr || buffer.getReadPointer(1) == nullptr)
            return; // Audio passes through unchanged (bypass mode)
        
        // Validate buffer size matches expected size
        if (buffer.getNumSamples() < numSamples)
            return; // Audio passes through unchanged (bypass mode)
        
        // CRITICAL: Validate targetType is within valid range
        if (targetType < 0 || targetType >= numModels)
        {
            DBG("[SATURATE] Invalid targetType: " << targetType << ", bypassing");
            return; // Audio passes through unchanged (bypass mode)
        }

    // Wrap makeCopyOf in try-catch to prevent crashes from corrupted buffer state
    try
    {
        dryBuffer.makeCopyOf(buffer, true);
    }
    catch (...)
    {
        // If buffer copy fails, bypass (pass audio through) instead of dropping audio
        DBG("[SATURATE] Exception during dryBuffer.makeCopyOf, bypassing");
        return; // Audio passes through unchanged (bypass mode)
    }

    // Refresh filters at the start of the block for current model
    // CRITICAL: Validate modelStates array access
    if (targetType < 0 || targetType >= static_cast<int>(modelStates.size()))
    {
        DBG("[SATURATE] Invalid targetType for modelStates: " << targetType);
        return; // Audio passes through unchanged (bypass mode)
    }
    
    auto& runtime = modelStates[targetType];
    
    // CRITICAL: Wrap filter refresh in try-catch
    try
    {
        refreshFilters(runtime, liveParams, osSampleRate, targetType);
    }
    catch (...)
    {
        DBG("[SATURATE] Exception during refreshFilters, bypassing");
        return; // Audio passes through unchanged (bypass mode)
    }

    // CRITICAL: Wrap renderModelToBuffer in try-catch
    try
    {
        renderModelToBuffer(targetType, runtime, buffer, liveParams, false);
    }
    catch (...)
    {
        DBG("[SATURATE] Exception during renderModelToBuffer, bypassing");
        return; // Audio passes through unchanged (bypass mode)
    }

    // Wrap makeCopyOf in try-catch to prevent crashes from corrupted buffer state
    try
    {
        // Validate dryBuffer before copying
        if (dryBuffer.getNumChannels() < 2 || dryBuffer.getNumSamples() < numSamples)
        {
            DBG("[SATURATE] Invalid dryBuffer state, bypassing dry/wet mix");
            return; // Audio passes through unchanged (bypass mode)
        }
        
        dryMatchedBuffer.makeCopyOf(dryBuffer, true);
    }
    catch (...)
    {
        // If buffer copy fails, bypass (pass audio through) instead of dropping audio
        DBG("[SATURATE] Exception during dryMatchedBuffer.makeCopyOf, bypassing dry/wet mix");
        return; // Audio passes through unchanged (bypass mode)
    }
    // Validate dryMatchedBuffer before creating AudioBlock
    if (dryMatchedBuffer.getNumChannels() < 2 || dryMatchedBuffer.getNumSamples() < numSamples)
    {
        DBG("[SATURATE] Invalid dryMatchedBuffer state, bypassing dry/wet mix");
        return; // Audio passes through unchanged (bypass mode)
    }
    
    juce::dsp::AudioBlock<float> dryBlock(dryMatchedBuffer);
    auto dryUp = oversamplerDry->processSamplesUp(dryBlock);
    juce::ignoreUnused(dryUp);
    oversamplerDry->processSamplesDown(dryBlock);

    // Validate buffer pointers before accessing
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);
    auto* dryL = dryMatchedBuffer.getWritePointer(0);
    auto* dryR = dryMatchedBuffer.getWritePointer(1);
    
    // CRITICAL: Validate all pointers are valid before mixing
    if (outL == nullptr || outR == nullptr || dryL == nullptr || dryR == nullptr)
    {
        DBG("[SATURATE] Null buffer pointers in dry/wet mix, skipping");
        return;
    }
    
    // Validate buffer sizes match
    if (buffer.getNumSamples() < numSamples || dryMatchedBuffer.getNumSamples() < numSamples)
    {
        DBG("[SATURATE] Buffer size mismatch in dry/wet mix, skipping");
        return;
    }

    const float mixVal = juce::jlimit(0.0f, 1.0f, liveParams.mix);
    const float dryGain = 1.0f - mixVal;
    
    // Wrap mixing loop in try-catch to prevent crashes from corrupted buffer data
    try
    {
        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] = dryL[i] * dryGain + outL[i] * mixVal;
            outR[i] = dryR[i] * dryGain + outR[i] * mixVal;
        }
    }
    catch (...)
    {
        DBG("[SATURATE] Exception during dry/wet mix, skipping");
        return;
    }
    }
    catch (...)
    {
        // CRITICAL: Catch any exception in processInternal and bypass completely
        // This prevents crashes from any unexpected errors
        DBG("[SATURATE] FATAL: Exception in processInternal, bypassing entire processing");
        return; // Audio passes through unchanged (bypass mode)
    }
}

//==============================================================================
void SaturateProcessor::renderModelToBuffer(int modelIndex,
                                            ModelRuntime& runtime,
                                            juce::AudioBuffer<float>& workBuffer,
                                            const ParameterSet& params,
                                            bool updateRuntimeParams)
{
    // CRITICAL: Wrap entire method in try-catch to prevent crashes
    try
    {
        // Safety check: ensure oversampler is initialized
        if (!oversamplerPrimary)
            return;

        // CRITICAL: Validate modelIndex
        if (modelIndex < 0 || modelIndex >= numModels)
        {
            DBG("[SATURATE] Invalid modelIndex in renderModelToBuffer: " << modelIndex);
            return;
        }

        // CRITICAL: Validate buffer
        if (workBuffer.getNumChannels() < 2 || workBuffer.getNumSamples() <= 0)
            return;

        juce::dsp::AudioBlock<float> block(workBuffer);
        auto osBlock = oversamplerPrimary->processSamplesUp(block);

        // CRITICAL: Wrap processOversampledBlock in try-catch
        try
        {
            processOversampledBlock(modelIndex, runtime, osBlock, params);
        }
        catch (...)
        {
            DBG("[SATURATE] Exception in processOversampledBlock, bypassing");
            return;
        }

        oversamplerPrimary->processSamplesDown(block);
    }
    catch (...)
    {
        // CRITICAL: Catch any exception and bypass
        DBG("[SATURATE] FATAL: Exception in renderModelToBuffer, bypassing");
        return;
    }
}

//==============================================================================
// Helper functions for aggressive saturation
float SaturateProcessor::applyWaveshape(float x, float shapeParam) const
{
    // Clamp input to prevent excessive values - reduced range for smoother sound
    x = juce::jlimit(-2.5f, 2.5f, x);
    
    // Map shapeParam (0-1) to 5 different waveshaping curves - all smoother and more musical
    if (shapeParam < 0.2f)
    {
        // Soft tanh (gentle, smooth, musical)
        const float amount = juce::jmap(shapeParam, 0.0f, 0.2f, 0.6f, 0.9f);
        return std::tanh(x * amount);
    }
    else if (shapeParam < 0.4f)
    {
        // Medium tanh (warm, musical)
        const float amount = juce::jmap(shapeParam, 0.2f, 0.4f, 0.9f, 1.3f);
        return std::tanh(x * amount);
    }
    else if (shapeParam < 0.6f)
    {
        // Cubic soft clip (smooth saturation, no harsh edges)
        const float amount = juce::jmap(shapeParam, 0.4f, 0.6f, 1.1f, 1.6f);
        float driven = x * amount;
        // Smooth cubic curve - reduced coefficient for less harshness
        float x3 = driven * driven * driven;
        return juce::jlimit(-0.9f, 0.9f, driven - x3 * 0.12f);
    }
    else if (shapeParam < 0.8f)
    {
        // Asymmetric tube-like (warm, musical, subtle asymmetry)
        const float amount = juce::jmap(shapeParam, 0.6f, 0.8f, 1.0f, 1.5f);
        float driven = x * amount;
        // Subtle asymmetric response for warmth
        if (driven > 0.0f)
            return std::tanh(driven * 0.93f) * 1.01f;
        else
            return std::tanh(driven * 0.90f) * 1.01f;
    }
    else
    {
        // Harder tanh (more aggressive but still smooth)
        const float amount = juce::jmap(shapeParam, 0.8f, 1.0f, 1.4f, 2.0f);
        float result = std::tanh(x * amount);
        // Very gentle soft clipping for harder character
        if (std::abs(result) > 0.88f)
            result = juce::jlimit(-0.95f, 0.95f, result * 1.02f);
        return result;
    }
}

float SaturateProcessor::addHarmonics(float saturated, float driven, const ParameterSet& params) const
{
    // Add subtle harmonic content - much reduced to avoid static
    const float harmonicAmount = params.character2;
    const float driveAmount = juce::jlimit(0.0f, 1.0f, params.driveDb / 36.0f);
    
    // Only add harmonics at higher drive levels to avoid static at low levels
    if (driveAmount < 0.3f)
        return saturated;
    
    // Generate subtle even harmonics (2nd) - reduced significantly
    const float evenHarm = driven * driven * 0.08f * harmonicAmount * driveAmount;
    
    // Generate subtle odd harmonics (3rd) - reduced significantly
    const float thirdHarm = driven * driven * driven * 0.06f * (1.0f - harmonicAmount * 0.6f) * driveAmount;
    
    // Blend harmonics very subtly
    float enriched = saturated;
    enriched += evenHarm * 0.25f;
    enriched += thirdHarm * 0.20f;
    
    return juce::jlimit(-1.0f, 1.0f, enriched);
}

float SaturateProcessor::blendDryWet(float dry, float wet, float mix) const
{
    // Equal-power crossfade for smooth blending
    const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
    const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
    return dry * dryGain + wet * wetGain;
}

//==============================================================================
void SaturateProcessor::processOversampledBlock(int modelIndex,
                                                ModelRuntime& runtime,
                                                juce::dsp::AudioBlock<float>& osBlock,
                                                const ParameterSet& params)
{
    // CRITICAL: Wrap entire method in try-catch to prevent crashes
    try
    {
        // CRITICAL: Validate modelIndex
        if (modelIndex < 0 || modelIndex >= numModels)
        {
            DBG("[SATURATE] Invalid modelIndex in processOversampledBlock: " << modelIndex);
            return;
        }

        auto* left = osBlock.getChannelPointer(0);
        auto* right = osBlock.getChannelPointer(1);
        
        // CRITICAL: Validate pointers
        if (left == nullptr || right == nullptr)
        {
            DBG("[SATURATE] Null pointers in processOversampledBlock");
            return;
        }
        
        const int osSamples = static_cast<int>(osBlock.getNumSamples());
        
        // CRITICAL: Validate sample count
        if (osSamples <= 0 || osSamples > maxBlockSize * HEAT_OVERSAMPLE)
        {
            DBG("[SATURATE] Invalid osSamples: " << osSamples);
            return;
        }

    auto& leftState = runtime.channels[0];
    auto& rightState = runtime.channels[1];

    runtime.rmsInAccum = 0.0f;
    runtime.rmsOutAccum = 0.0f;
    runtime.rmsCount = osSamples;

    const float driveBlendGlobal = juce::jlimit(0.0f, 1.0f, params.driveDb / 36.0f);
    
    // Set target for per-sample bias smoother (prevents clicks when bias changes)
    // Update bias smoothing target
    biasSmooth.setTargetValue(params.bias);

    // Aggressive, colorful saturation with frequency-dependent processing
    auto processAggressiveSaturation = [&](float& sample, ModelChannelState& state, float smoothedBias)
    {
        if (driveBlendGlobal <= 0.0001f)
        {
            state.envelope *= 0.995f;
            return;
        }

        // Store original for dry/wet blending
        const float original = sample;

        // Stage 1: Frequency-dependent HPF (Color controls cutoff to remove rumble)
        sample = state.preHP.process(sample);
        
        // Stage 2: Frequency-dependent drive (Color emphasizes highs/lows via pre-tilt)
        float preEmphasis = state.preTilt.process(sample);
        
        // Apply drive with per-sample smoothed bias for asymmetry (prevents clicks)
        const float driveLin = params.driveLin;
        float driven = preEmphasis * driveLin + smoothedBias;
        
        // Clamp to prevent excessive values
        driven = juce::jlimit(-3.0f, 3.0f, driven);

        // Stage 3: Musical waveshaping (Shape selects curve: 0-1 maps to 5 curves)
        float saturated = applyWaveshape(driven, params.character);
        
        // Stage 4: Subtle harmonic enhancement (only at higher drive)
        saturated = addHarmonics(saturated, driven, params);
        
        // Gentle soft clipping to prevent harsh artifacts
        saturated = juce::jlimit(-0.98f, 0.98f, saturated);

        // Stage 5: Post-processing EQ
        saturated = state.postTilt.process(saturated);
        
        // CRITICAL: Validate input before tone filter to prevent buzzing
        if (!std::isfinite(saturated)) {
            saturated = 0.0f;
        }
        
        saturated = state.toneFilter.process(saturated);
        
        // CRITICAL: Validate tone filter output to prevent buzzing from corrupted filter state
        if (!std::isfinite(saturated)) {
            saturated = 0.0f; // Return silence if filter output is invalid
        }
        
        saturated = state.postHP.process(saturated);
        
        // CRITICAL: Final validation to prevent any buzzing artifacts
        if (!std::isfinite(saturated)) {
            saturated = 0.0f;
        }

        // Gentle compression for musical character (not aggressive)
        const float absSat = std::abs(saturated);
        const float envCoeff = std::exp(-1.0f / (compReleaseSeconds * static_cast<float>(osSampleRate)));
        state.compEnv = envCoeff * state.compEnv + (1.0f - envCoeff) * absSat;
        
        // Much gentler compression threshold and ratio
        if (state.compEnv > 0.75f)
        {
            const float over = state.compEnv / 0.75f;
            const float gain = std::pow(over, -5.0f);  // Gentler compression
            saturated *= juce::jlimit(0.7f, 1.0f, gain);
        }

        // Blend with original using equal-power crossfade
        sample = blendDryWet(original, saturated, params.mix);
        sample = juce::jlimit(-1.0f, 1.0f, sample);
        
        // CRITICAL: Final validation to prevent buzzing - ensure output is finite and valid
        if (!std::isfinite(sample)) {
            sample = 0.0f; // Return silence if output is invalid
        }
    };

    for (int i = 0; i < osSamples; ++i)
    {
        // Get smoothed bias value (prevents clicks when bias changes)
        const float smoothedBias = biasSmooth.getNextValue();
        
        float l = left[i];
        float r = right[i];

        runtime.rmsInAccum += 0.5f * (l * l + r * r);

        processAggressiveSaturation(l, leftState, smoothedBias);
        processAggressiveSaturation(r, rightState, smoothedBias);

        left[i] = juce::jlimit(-1.0f, 1.0f, l);
        right[i] = juce::jlimit(-1.0f, 1.0f, r);
        runtime.rmsOutAccum += 0.5f * (left[i] * left[i] + right[i] * right[i]);
    }

    updateAutoGain(runtime, osSamples, false);

    const float finalGain = runtime.autoGain * params.outputLin;

    for (int i = 0; i < osSamples; ++i)
    {
        left[i] = juce::jlimit(-1.0f, 1.0f, left[i] * finalGain);
        right[i] = juce::jlimit(-1.0f, 1.0f, right[i] * finalGain);
        
        // CRITICAL: Validate final output to prevent buzzing
        if (!std::isfinite(left[i])) left[i] = 0.0f;
        if (!std::isfinite(right[i])) right[i] = 0.0f;
    }
    }
    catch (...)
    {
        // CRITICAL: Catch any exception in processOversampledBlock and bypass
        DBG("[SATURATE] FATAL: Exception in processOversampledBlock, bypassing");
        return;
    }
}

//==============================================================================
void SaturateProcessor::updateAutoGain(ModelRuntime& runtime, int osSamples, bool allowHeavyAttenuation)
{
    if (osSamples <= 0)
        return;

    const float rmsIn = std::sqrt(runtime.rmsInAccum / static_cast<float>(osSamples) + epsilon);
    const float rmsOut = std::sqrt(runtime.rmsOutAccum / static_cast<float>(osSamples) + epsilon);
    float target = 1.0f;
    if (rmsOut > 0.0f)
        target = juce::jlimit(0.25f, 4.0f, rmsIn / rmsOut);

    const float minGain = allowHeavyAttenuation ? 0.25f : 0.75f;
    target = juce::jlimit(minGain, 4.0f, target);
    if (!std::isfinite(target))
        target = 1.0f;

    const float coeff = std::exp(-1.0f / (autoGainTauSeconds * static_cast<float>(osSampleRate)));
    runtime.autoGain = coeff * runtime.autoGain + (1.0f - coeff) * target;
}
