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
        const int maxDelaySamps = (int) std::ceil (sampleRate * maxPredelayMs / 1000.0) + 8;
        predelay.reset();
        predelay.setMaximumDelayInSamples (maxDelaySamps);
        predelay.setDelay (0.0);

        // Smoothing
        mix.reset   (sampleRate, 0.050);
        room.reset  (sampleRate, 0.050);
        damp.reset  (sampleRate, 0.050);
        width.reset (sampleRate, 0.050);
        erAmt.reset (sampleRate, 0.050);

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

        // 2) Simple early reflections (audible presence) blended into wet
        //    Short stereo taps mixed in; scaled by erAmt
        const float erGain = erAmt.getNextValue();
        for (int n = 0; n < numSamples; ++n)
        {
            // write to ER ring
            erBuffer.setSample (0, erWrite, wet.getSample (0, n));
            erBuffer.setSample (1, erWrite, wet.getSample (1, n));

            auto tap = [&](int ch, float ms)
            {
                float d = ms * (float) sr * 0.001f;
                float rp = (float) erWrite - d; const int N = erBuffer.getNumSamples();
                while (rp < 0) rp += (float) N;
                int i = (int) rp; float f = rp - (float) i;
                auto at=[&](int k){ return erBuffer.getSample (ch, (k+N)%N); };
                float y0=at(i-1), y1=at(i), y2=at(i+1), y3=at(i+2);
                float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                return ((c3*f+c2)*f+c1)*f+c0;
            };

            float erL = 0.0f, erR = 0.0f;
            // tasteful audible taps
            erL += 0.95f * tap(0, 7.0f);
            erR += 0.95f * tap(1, 7.0f);
            erL += 0.80f * tap(0, 15.5f);
            erR += 0.80f * tap(1, 16.0f);
            erL += 0.66f * tap(0, 33.0f);
            erR += 0.66f * tap(1, 33.0f);

            // add ER presence into the wet tail
            float wL = wet.getSample (0, n) + erGain * erL;
            float wR = wet.getSample (1, n) + erGain * erR;
            wet.setSample (0, n, juce::jlimit (-1.2f, 1.2f, wL));
            wet.setSample (1, n, juce::jlimit (-1.2f, 1.2f, wR));

            if (++erWrite >= erBuffer.getNumSamples()) erWrite = 0;
        }

        // 3) External mix crossfade (prevents double mixing + fixes "width kills left" issues)
        const float m = mix.getNextValue();
        for (int n = 0; n < numSamples; ++n)
        {
            const float dryL = L[n], dryR = R ? R[n] : dryL;
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

