#pragma once
#include <juce_dsp/juce_dsp.h>

// ===== ChorusEngine - Best-in-class multi-voice chorus =====
struct ChorusEngine
{
    // Max settings
    static constexpr int   kMaxVoices      = 8;
    static constexpr float kMaxDelayMs     = 60.0f;  // safety headroom
    static constexpr int   kMaxChannels    = 2;

    void prepare (double sr, int blockSize)
    {
        sampleRate = (sr > 0.0 ? sr : 44100.0);
        const int maxSamps = (int) std::ceil(sampleRate * (kMaxDelayMs/1000.0f)) + 8;
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            delayBuf[ch].setSize(1, maxSamps + blockSize + 8);
            delayBuf[ch].clear();
            writeIndex[ch] = 0;
        }

        // smoothing (~80–150 ms for silk)
        const double glideSec = 0.12;
        rateHz .reset(sampleRate, glideSec);
        depthMs.reset(sampleRate, glideSec);
        baseMs .reset(sampleRate, glideSec);
        width  .reset(sampleRate, 0.08);
        feedback.reset(sampleRate, 0.08);
        mix    .reset(sampleRate, 0.04);
        shape  .reset(sampleRate, 0.08);

        // per-voice phases & random drift (slow analog wobble)
        for (int v = 0; v < kMaxVoices; ++v)
        {
            voicePhase[v] = juce::Random::getSystemRandom().nextFloat(); // 0..1
            driftPhase[v] = juce::Random::getSystemRandom().nextFloat();
        }

        // feedback LP sweetener
        for (int ch = 0; ch < kMaxChannels; ++ch)
            fbLpState[ch] = 0.0f;
    }

    // Call every block to set targets (no hard jumps)
    void setParams (int voices,
                    float baseDelayMs, float rateHzTarget, float depthMsTarget,
                    float width01, float feedback01, float shape01, float mix01)
    {
        numVoices = juce::jlimit(2, kMaxVoices, voices);
        baseMs   .setTargetValue(juce::jlimit(5.0f, kMaxDelayMs-5.0f, baseDelayMs));
        rateHz   .setTargetValue(juce::jlimit(0.01f, 12.0f, rateHzTarget));
        depthMs  .setTargetValue(juce::jlimit(0.0f,  15.0f, depthMsTarget));
        width    .setTargetValue(juce::jlimit(0.0f,   1.0f, width01));
        feedback .setTargetValue(juce::jlimit(0.0f,   0.9f, feedback01));
        mix      .setTargetValue(juce::jlimit(0.0f,   1.0f, mix01));
        shape    .setTargetValue(juce::jlimit(0.0f,   1.0f, shape01));
    }

    void process (juce::AudioBuffer<float>& io)
    {
        const int N = io.getNumSamples();
        const int C = std::min(io.getNumChannels(), kMaxChannels);

        auto* L = io.getWritePointer(0);
        auto* R = (C > 1 ? io.getWritePointer(1) : nullptr);

        for (int n = 0; n < N; ++n)
        {
            const float inL = L[n];
            const float inR = (R ? R[n] : inL);

            // write input + last feedback
            for (int ch = 0; ch < C; ++ch)
            {
                const float fb = feedback.getCurrentValue();
                // gentle 1-pole LP in feedback path (sweetness)
                fbLpState[ch] = fbLpState[ch] + 0.1f * (fbAccum[ch] - fbLpState[ch]); // ~1k-ish
                const float write = (ch==0 ? inL : inR) + fb * fbLpState[ch];
                delayBuf[ch].setSample(0, writeIndex[ch], write);
            }

            // accumulate wet per channel
            float wetL = 0.0f, wetR = 0.0f;

            // smoothed params per sample
            const float base   = baseMs.getNextValue();
            const float rate   = rateHz.getNextValue();
            const float depth  = depthMs.getNextValue();
            const float wid    = width .getNextValue();
            const float shp    = shape .getNextValue();

            for (int v = 0; v < numVoices; ++v)
            {
                // LFO with soft shape: sin -> tri -> soft-square
                lfoPhaseAdvance(v, rate);
                const float lfo = shapedLFO(voicePhase[v], shp); // [-1,1]

                // small random drift (very slow) for analog feel
                driftAdvance(v);
                const float drift = 0.02f * (driftValue[v] - 0.5f); // ±1% rate drift implied

                // delay time in samples for this voice (base ± depth)
                const float dMs   = base + depth * (lfo + drift);
                const float dSamp = juce::jlimit(1.0f, (float)delayBuf[0].getNumSamples()-4.0f, dMs * 0.001f * (float)sampleRate);

                // stereo spread: equal-power pan across left/right
                // distribute voices evenly across stereo with 'wid'
                const float pan   = (numVoices == 1 ? 0.5f : (float)v / (float)(numVoices - 1)); // 0..1
                const float panW  = 0.5f + (pan - 0.5f) * wid; // collapse toward center when width<1
                const float gL    = std::cos(panW * juce::MathConstants<float>::halfPi);
                const float gR    = std::sin(panW * juce::MathConstants<float>::halfPi);

                // read same delay for both channels (chorus "voices" are images), pan by gL/gR
                const float vL = readHermite(delayBuf[0], writeIndex[0], dSamp);
                const float vR = readHermite(delayBuf[1], writeIndex[1], dSamp);

                // sum: take mono voice content then distribute by pan gains (stable)
                const float monoV = 0.5f * (vL + vR);
                wetL += monoV * gL;
                wetR += monoV * gR;
            }

            // average voices to keep level sensible
            if (numVoices > 0) { wetL /= (float) numVoices; wetR /= (float) numVoices; }

            // store for feedback sweetener
            fbAccum[0] = wetL; fbAccum[1] = wetR;

            // crossfade dry/wet (true mix)
            const float mx = mix.getNextValue();
            const float outL = juce::jmap(mx, inL, wetL);
            const float outR = juce::jmap(mx, inR, wetR);

            L[n] = outL;
            if (R) R[n] = outR;

            // advance write index
            for (int ch = 0; ch < C; ++ch)
            {
                writeIndex[ch]++;
                if (writeIndex[ch] >= delayBuf[ch].getNumSamples())
                    writeIndex[ch] = 0;
            }
        }
    }

private:
    // ===== helpers =====
    inline void lfoPhaseAdvance (int v, float rateHz)
    {
        // smooth phase increment via sampleRate, no hard jumps
        const double inc = (double)rateHz / sampleRate; // cycles per sample
        voicePhase[v] += (float)inc;
        if (voicePhase[v] >= 1.0f) voicePhase[v] -= 1.0f;
    }
    inline void driftAdvance (int v)
    {
        // very slow drift ~0.1 Hz * 0.02 depth equivalent
        driftPhase[v] += (float)(0.1 / sampleRate);
        if (driftPhase[v] >= 1.0f) driftPhase[v] -= 1.0f;
        driftValue[v] = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * driftPhase[v]);
    }

    // sin -> tri -> soft-square morph, shape 0..1
    static inline float shapedLFO (float phase01, float shape01)
    {
        const float s = juce::jlimit(0.0f, 1.0f, shape01);
        const float ang = juce::MathConstants<float>::twoPi * phase01;
        const float sinv = std::sin(ang);
        const float tri  = (2.0f / juce::MathConstants<float>::pi) * std::asin(sinv);
        const float sq   = std::tanh(juce::jmap(s, 0.0f, 1.0f, 0.0f, 3.0f) * sinv);
        return (s < 0.5f) ? juce::jmap(s * 2.0f, sinv, tri)
                          : juce::jmap((s - 0.5f) * 2.0f, tri,  sq);
    }

    // 4-point Hermite (cubic) interpolation for fractional delay taps
    static inline float hermite4 (float y0, float y1, float y2, float y3, float frac)
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f*(y1 - y2);
        return ((c3*frac + c2)*frac + c1)*frac + c0;
    }
    static inline float readHermite (juce::AudioBuffer<float>& buf, int wIdx, float delaySamps)
    {
        const int size = buf.getNumSamples();
        float readPos = (float)wIdx - delaySamps;
        while (readPos < 0.0f) readPos += (float)size;
        int idx1 = (int) readPos;
        float frac = readPos - (float) idx1;

        auto at = [&](int i){ return buf.getSample(0, (i + size) % size); };
        float y0 = at(idx1 - 1), y1 = at(idx1), y2 = at(idx1 + 1), y3 = at(idx1 + 2);
        return hermite4(y0, y1, y2, y3, frac);
    }

    // state
    double sampleRate { 44100.0 };

    juce::AudioBuffer<float> delayBuf[kMaxChannels];
    int writeIndex[kMaxChannels] { 0, 0 };
    float fbAccum[kMaxChannels]  { 0.0f, 0.0f };
    float fbLpState[kMaxChannels]{ 0.0f, 0.0f };

    int   numVoices { 4 };
    float voicePhase[kMaxVoices] {};
    float driftPhase[kMaxVoices] {};
    float driftValue[kMaxVoices] {};

    // smoothed params
    juce::SmoothedValue<float> baseMs, rateHz, depthMs, width, feedback, mix, shape;
};