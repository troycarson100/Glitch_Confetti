#pragma once
#include <juce_dsp/juce_dsp.h>

// Minimal, reliable Hall based on JUCE's Reverb with extras:
// - dedicated pre-delay (DelayLine)
// - external Mix crossfade (no double-mix)
// - Width that never mutes a channel
// - Smoothed params to avoid zipper/clicks

struct SimpleHallReverb
{
    void prepare (double sampleRate, int samplesPerBlock, int maxPredelayMs = 300)
    {
        sr = sampleRate;

        // JUCE Reverb
        reverb.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });

        // Pre-delay: allocate enough for UI range
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
        predelay.prepare (spec);
        const int maxDelaySamps = (int) std::ceil (sampleRate * maxPredelayMs / 1000.0) + 8;
        predelay.setMaximumDelayInSamples (maxDelaySamps);
        predelay.setDelay (0.0);

        // Smoothing with initial values
        mix.reset   (sampleRate, 0.050); mix.setCurrentAndTargetValue(0.0f);
        room.reset  (sampleRate, 0.050); room.setCurrentAndTargetValue(0.90f);
        damp.reset  (sampleRate, 0.050); damp.setCurrentAndTargetValue(0.20f);
        width.reset (sampleRate, 0.050); width.setCurrentAndTargetValue(1.0f);
        erAmt.reset (sampleRate, 0.050); erAmt.setCurrentAndTargetValue(0.5f);

        // Make it present by default
        params.roomSize  = 0.90f;   // big space
        params.damping   = 0.20f;   // slower HF loss
        params.wetLevel  = 1.00f;   // we'll mix externally anyway
        params.dryLevel  = 0.00f;   // external mix handles dry
        params.width     = 1.00f;   // max stereo spread
        reverb.setParameters (params);

        // Early reflections (simple Haas taps)
        erBuffer.setSize (2, (int) std::ceil (sampleRate * 0.200) + 16); // 200ms ring
        erWrite = 0;
    }

    // Map your 8 UI knobs here (values already 0..1 or raw as noted)
    void setParameters (float verbSize01, float predelayMs, float dampHz,
                        float diffusion01, float early01, float decaySec,
                        float width01, float mix01)
    {
        // JUCE Reverb doesn't have true RT60 control; map Decay/Size/Diffusion onto roomSize/damping:
        // - roomSize ≈ "space / decay feel"
        // - damping  ≈ HF loss (lower cutoff = darker)
        // Keep it simple & audible:
        const float targetRoom = juce::jlimit (0.10f, 0.98f,
                                0.50f * verbSize01 + 0.45f * juce::jlimit (0.0f, 1.0f, decaySec / 20.0f)
                              + 0.05f * diffusion01);
        const float targetDamp = juce::jmap (juce::jlimit (1000.0f, 20000.0f, dampHz), 1000.0f, 20000.0f, 0.85f, 0.05f);

        room.setTargetValue  (targetRoom);
        damp.setTargetValue  (targetDamp);
        width.setTargetValue (juce::jlimit (0.0f, 1.0f, width01));
        mix.setTargetValue   (juce::jlimit (0.0f, 1.0f, mix01));
        erAmt.setTargetValue (juce::jlimit (0.0f, 1.0f, early01));

        // Pre-delay sets the delay line in samples (linear interp)
        const double dSamps = juce::jlimit (0.0, sr * 0.300, (double) predelayMs * sr * 0.001);
        predelay.setDelay (dSamps);

        // (diffusion01 is only used to nudge roomSize; JUCE Reverb has no explicit diffusion control)
        // (decaySec similarly influences roomSize here to keep UI identical without fragile custom DSP)
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        // SAVE DRY SIGNAL FIRST (before any processing)
        juce::AudioBuffer<float> dry (2, numSamples);
        dry.copyFrom (0, 0, L, numSamples);
        if (R) dry.copyFrom (1, 0, R, numSamples);
        else   dry.copyFrom (1, 0, L, numSamples);

        // Update reverb params smoothly per block (not every sample)
        params.roomSize = room.getNextValue();
        params.damping  = damp.getNextValue();
        params.width    = width.getNextValue(); // 0..1 inside JUCE
        params.wetLevel = 1.0f;                 // fixed; we crossfade externally
        params.dryLevel = 0.0f;                 // keep dry out of JUCE block
        reverb.setParameters (params);

        // 1) Copy input to a temp wet buffer, apply pre-delay, run reverb in-place on that wet buffer
        juce::AudioBuffer<float> wet (2, numSamples);
        wet.copyFrom (0, 0, L, numSamples);
        if (R) wet.copyFrom (1, 0, R, numSamples);
        else   wet.copyFrom (1, 0, L, numSamples);

        // Pre-delay (channel-wise)
        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = wet.getSample (0, n);
            const float inR = wet.getSample (1, n);
            predelay.pushSample (0, inL);
            predelay.pushSample (1, inR);
            const float pdL = (float) predelay.popSample (0);
            const float pdR = (float) predelay.popSample (1);
            wet.setSample (0, n, pdL);
            wet.setSample (1, n, pdR);
        }

        juce::dsp::AudioBlock<float> block (wet);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);

        // 2) Apply soft saturation to wet signal to prevent crackling
        for (int n = 0; n < numSamples; ++n)
        {
            float wL = wet.getSample (0, n);
            float wR = wet.getSample (1, n);
            
            // Soft saturation to prevent hard clipping
            wL = std::tanh(wL * 0.8f);
            wR = std::tanh(wR * 0.8f);
            
            wet.setSample (0, n, wL);
            wet.setSample (1, n, wR);
        }

        // 3) External mix crossfade (use saved dry signal)
        for (int n = 0; n < numSamples; ++n)
        {
            const float m = mix.getNextValue(); // Get smoothed mix value per sample
            const float dryL = dry.getSample (0, n);
            const float dryR = dry.getSample (1, n);
            const float wetL = wet.getSample (0, n);
            const float wetR = wet.getSample (1, n);
            L[n] = juce::jmap (m, dryL, wetL);
            if (R) R[n] = juce::jmap (m, dryR, wetR);
        }
    }

    // Call once per block with APVTS values
    void pushParams (float size01, float predelayMs, float dampHz,
                     float diffusion01, float early01, float decaySec,
                     float width01, float mix01)
    {
        setParameters (size01, predelayMs, dampHz, diffusion01, early01, decaySec, width01, mix01);
    }

private:
    double sr = 44100.0;

    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay;

    juce::SmoothedValue<float> mix, room, damp, width, erAmt;
    juce::dsp::Reverb::Parameters params;

    juce::AudioBuffer<float> erBuffer; int erWrite = 0;
};

