#pragma once
#include <juce_dsp/juce_dsp.h>

// ===================== Small helpers =====================
namespace rvb
{
    inline float safetySat (float x) { return std::tanh (0.7f * x); } // gentle
    inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

    struct OnePoleLPF {
        void prepare (double sr) { sampleRate = sr; z = 0.0f; setCutoff (cutoffHz); }
        void setCutoff (float hz) {
            cutoffHz = juce::jlimit (20.0f, 20000.0f, hz);
            const float a1 = std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
            a = a1; b = 1.0f - a1;
        }
        inline float process (float x) { z = a * z + b * x; return z; }
        double sampleRate{44100.0}; float cutoffHz{8000.0f}, a{0}, b{1}, z{0};
    };

    struct OnePoleHPF {
        void prepare (double sr) { sampleRate = sr; xz = 0.0f; yz = 0.0f; setCutoff (cutoffHz); }
        void setCutoff (float hz) {
            cutoffHz = juce::jlimit (20.0f, 1000.0f, hz);
            const float c = std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
            a = c; b = (1.0f + c) * 0.5f; d = (1.0f - c) * 0.5f;
        }
        inline float process (float x) { float y = b * x + d * xz - a * yz; xz = x; yz = y; return y; }
        double sampleRate{44100.0}; float cutoffHz{150.0f}, a{0}, b{0}, d{0}, xz{0}, yz{0};
    };

    // Predelay with 4-pt Hermite
    struct PreDelay {
        void prepare (double sr, int maxMs) {
            sampleRate = sr;
            const int n = (int) std::ceil (sr * maxMs / 1000.0) + 8;
            buf.setSize (2, n); buf.clear(); w = 0; dSamps = 0.0f;
        }
        void setMs (float ms) {
            dSamps = juce::jlimit (0.0f, (float) buf.getNumSamples()-4.0f, ms * (float) sampleRate * 0.001f);
        }
        inline void push (float L, float R) {
            buf.setSample (0, w, L); buf.setSample (1, w, R);
            if (++w >= buf.getNumSamples()) w = 0;
        }
        inline float readCh (int ch) const {
            float rp = (float) w - dSamps; const int N = buf.getNumSamples();
            while (rp < 0.0f) rp += (float) N; int i = (int) rp; float f = rp - (float) i;
            auto at = [&](int k){ return buf.getSample (ch, (k + N) % N); };
            float y0=at(i-1), y1=at(i), y2=at(i+1), y3=at(i+2);
            float c0=y1, c1=0.5f*(y2-y0), c2=y0-2.5f*y1+2.0f*y2-0.5f*y3, c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            return ((c3*f + c2)*f + c1)*f + c0;
        }
        juce::AudioBuffer<float> buf; int w{0}; float dSamps{0}; double sampleRate{44100.0};
    };

    // Tri weights for verbType in [0..2] (Hall, Room, Shimmer)
    struct TypeWeights {
        float wH{1}, wR{0}, wS{0};
        void set (float t) {
            auto tri = [](float x, float c){ return juce::jmax (0.0f, 1.0f - std::abs (x - c)); };
            float h = tri (t, 0.0f), r = tri (t, 1.0f), s = tri (t, 2.0f);
            float sum = h + r + s; if (sum <= 1e-6f) { wH=1; wR=0; wS=0; } else { wH=h/sum; wR=r/sum; wS=s/sum; }
        }
    };
}

// ===================== Hall: 8-line FDN with morphing taps =====================
struct HallReverb
{
    static constexpr int N = 8;

    void prepare (double sr, int maxMs)
    {
        sampleRate = sr;
        const int maxSamps = (int) std::ceil (sr * maxMs / 1000.0) + 16;
        for (int i=0; i<N; ++i) {
            lines[i].setSize (1, maxSamps); lines[i].clear(); widx[i] = 0;
            delayMs[i] = baseMs[i];
            delaySmooth[i].reset (sr, 0.06); // 60ms morph
            delaySmooth[i].setCurrentAndTargetValue (delayMs[i]);
            ap[i].reset();
            // allpass as diffuser
            juce::dsp::ProcessSpec spec { sr, (juce::uint32)maxMs, 1 };
            ap[i].prepare(spec);
            *ap[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 900.0f, 0.7f);
            lfoPhase[i] = juce::Random::getSystemRandom().nextFloat();
        }
        damp.prepare (sr); damp.setCutoff (8000.0f);
        wetTrim = 2.2f; // +6.8 dB
        setDecayAndSize (4.0f, 0.7f);
        setDiffusion (0.8f);
    }

    void setDecayAndSize (float decaySec, float size01)
    {
        decay = juce::jlimit (0.2f, 20.0f, decaySec);
        size  = juce::jlimit (0.1f, 1.5f, size01);
        for (int i=0; i<N; ++i) {
            delayMs[i] = baseMs[i] * size;
            delaySmooth[i].setTargetValue (delayMs[i]);
            const float D = delayMs[i] * 0.001f; // seconds
            float g = std::exp (-6.907755f * (D / juce::jmax (0.2f, decay)));
            fb[i] = juce::jlimit (0.2f, 0.9999f, g);
        }
    }

    void setDiffusion (float d)
    {
        diffusion = juce::jlimit (0.0f, 1.0f, d);
        apGain = juce::jmap (diffusion, 0.0f, 1.0f, 0.6f, 0.9f);
    }

    void setDampHz (float hz) { damp.setCutoff (hz); }

    inline void processSample (float inL, float inR, float& outL, float& outR)
    {
        const float in = 0.5f * (inL + inR);

        // Read with dual-tap morph + diffuser
        float y[N];
        for (int i=0; i<N; ++i)
        {
            const float msTarget = delayMs[i];
            delaySmooth[i].setTargetValue (msTarget);
            const float msNow = delaySmooth[i].getNextValue();

            auto readInterp = [&](float ms){
                float dS = ms * 0.001f * (float) sampleRate;
                float rp = (float) widx[i] - dS; const int Nb = lines[i].getNumSamples();
                while (rp < 0.0f) rp += (float) Nb;
                int idx = (int) rp; float f = rp - (float) idx;
                auto at=[&](int k){ return lines[i].getSample (0, (k + Nb) % Nb); };
                float y0=at(idx-1), y1=at(idx), y2=at(idx+1), y3=at(idx+2);
                float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                return ((c3*f + c2)*f + c1)*f + c0;
            };

            const float delNow = readInterp (msNow);
            const float delOld = readInterp (msTarget);
            // crossfade proportion shrinks as we arrive at target
            float xf = juce::jlimit (0.0f, 1.0f, std::abs (msTarget - msNow) / 10.0f);
            const float del = rvb::lerp (delOld, delNow, 1.0f - xf);

            // diffuser with slight LFO
            float lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase[i]);
            lfoPhase[i] += (0.12f + 0.03f*(i+1)) / (float) sampleRate; if (lfoPhase[i]>=1.0f) lfoPhase[i]-=1.0f;

            float apIn = del + 0.0007f * lfo * del;
            float apOut = ap[i].processSample (apIn);
            y[i] = damp.process (apGain * apOut);
        }

        // 8x8 Hadamard is separable; do two 4x4 mixes (simple orthogonal mix blocks)
        auto hmix4 = [](float a0,float a1,float a2,float a3, float& o0,float& o1,float& o2,float& o3){
            o0 = 0.5f*( a0 + a1 + a2 + a3);
            o1 = 0.5f*( a0 - a1 + a2 - a3);
            o2 = 0.5f*( a0 + a1 - a2 - a3);
            o3 = 0.5f*( a0 - a1 - a2 + a3);
        };

        float h0,h1,h2,h3, h4,h5,h6,h7;
        hmix4 (y[0],y[1],y[2],y[3], h0,h1,h2,h3);
        hmix4 (y[4],y[5],y[6],y[7], h4,h5,h6,h7);

        float d[8] { h0,h1,h2,h3,h4,h5,h6,h7 };

        // write with feedback + input
        for (int i=0; i<N; ++i) {
            const float xin = in + fb[i] * d[i];
            lines[i].setSample (0, widx[i], rvb::safetySat (xin));
        }
        if (++widx[0] >= lines[0].getNumSamples()) {
            for (int i=0; i<N; ++i) if (++widx[i] >= lines[i].getNumSamples()) widx[i] = 0;
        }

        // wide stereo sum
        const float L = (y[0] + y[2] + y[4] + y[6]) * 0.25f;
        const float R = (y[1] + y[3] + y[5] + y[7]) * 0.25f;

        outL = rvb::safetySat (wetTrim * L);
        outR = rvb::safetySat (wetTrim * R);
    }

    double sampleRate{44100.0};
    juce::AudioBuffer<float> lines[N]; int widx[N]{};
    // Base delays chosen to avoid common modes (ms)
    const float baseMs[N] = { 31.7f, 37.1f, 43.8f, 51.2f, 58.9f, 67.3f, 73.5f, 81.0f };
    float delayMs[N]{}; juce::SmoothedValue<float> delaySmooth[N];
    float fb[N]{}, size{0.7f}, decay{4.0f}, diffusion{0.8f}, apGain{0.8f}, wetTrim{2.2f};
    juce::dsp::IIR::Filter<float> ap[N]; float lfoPhase[N]{};
    rvb::OnePoleLPF damp;
};

// ===================== Room: ER taps + compact tail =====================
struct RoomReverb
{
    void prepare (double sr)
    {
        sampleRate = sr;
        // Early reflections (ms, gain) — boosted
        taps = { {6.7f,0.9f},{9.9f,0.82f},{15.2f,0.74f},{21.7f,0.64f},{32.1f,0.56f},{47.3f,0.5f} };
        int maxMs = 140;
        earlyBuf.setSize (2, (int) std::ceil (sr * maxMs / 1000.0) + 8); earlyBuf.clear(); ew = 0;

        predelay.prepare (sr, 200);

        hall.prepare (sr, 1200); // compact tank
        hall.setDecayAndSize (1.5f, 0.5f); // initial
        hall.setDiffusion (0.75f);

        earlyGain = 1.8f; // strong ERs
        wetTrim   = 2.0f; // +6 dB
    }

    void setParams (float decaySec, float size01, float dampHz, float diffusion, float earlyLevel, float predelayMs)
    {
        size = juce::jlimit (0.1f, 1.5f, size01);
        earlyLevel_ = juce::jlimit (0.0f, 1.0f, earlyLevel);
        predelay.setMs (predelayMs);

        hall.setDecayAndSize (decaySec, 0.55f * size);
        hall.setDampHz (dampHz);
        hall.setDiffusion (juce::jlimit (0.0f, 1.0f, diffusion));
    }

    inline void processSample (float inL, float inR, float& outL, float& outR)
    {
        predelay.push (inL, inR);
        const float pdL = predelay.readCh (0);
        const float pdR = predelay.readCh (1);

        // write ER buffer
        earlyBuf.setSample (0, ew, pdL);
        earlyBuf.setSample (1, ew, pdR);

        float erL=0.0f, erR=0.0f;
        for (auto& t : taps)
        {
            const float dS = t.first * 0.001f * (float) sampleRate;
            float rp = (float) ew - dS; const int N = earlyBuf.getNumSamples();
            while (rp < 0.0f) rp += (float) N; int i = (int) rp; float f = rp - (float) i;

            auto at=[&](int ch,int k){ return earlyBuf.getSample (ch, (k+N)%N); };
            auto interp=[&](int ch){
                float y0=at(ch,i-1),y1=at(ch,i),y2=at(ch,i+1),y3=at(ch,i+2);
                float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                return ((c3*f + c2)*f + c1)*f + c0;
            };
            erL += t.second * interp(0);
            erR += t.second * interp(1);
        }
        if (++ew >= earlyBuf.getNumSamples()) ew = 0;

        float tailL=0, tailR=0;
        hall.processSample (pdL, pdR, tailL, tailR);

        // ER prominence + early/late cross
        const float ER = earlyLevel_;
        outL = rvb::safetySat (wetTrim * (earlyGain * ER * erL + (1.0f - ER) * tailL));
        outR = rvb::safetySat (wetTrim * (earlyGain * ER * erR + (1.0f - ER) * tailR));
    }

    double sampleRate{44100.0};
    std::vector<std::pair<float,float>> taps; // {ms, gain}
    juce::AudioBuffer<float> earlyBuf; int ew{0};
    rvb::PreDelay predelay;
    HallReverb hall; float earlyGain{1.8f}, wetTrim{2.0f}, size{0.6f}, earlyLevel_{0.55f};
};

// ===================== Shimmer: Hall + parallel octave tail in feedback =====================
struct OctaveUpGrains4
{
    void prepare (double sr, int maxMs)
    {
        sampleRate = sr;
        int N = (int) std::ceil (sr * maxMs / 1000.0) + 16;
        buf.setSize (1, N); buf.clear(); w = 0;
        setGrainMs (90.0f); // long grains
        for (int g=0; g<4; ++g) phase[g] = g / 4.0f;
        shiftRatio = 2.0f; // +12 sem
    }
    void setGrainMs (float ms)
    {
        grainLen = juce::jlimit (64, (int) std::round (ms * sampleRate * 0.001f), 16384);
    }
    inline float process (float x)
    {
        buf.setSample (0, w, x);
        float y = 0.0f;
        for (int g=0; g<4; ++g)
        {
            // 75% overlap
            phase[g] += (shiftRatio - 1.0f) * (grainLen / (float) sampleRate) * 1.33f;
            if (phase[g] >= 1.0f) phase[g] -= 1.0f;

            float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase[g]);
            float readBack = phase[g] * (float) grainLen;

            float rp = (float) w - readBack; const int N = buf.getNumSamples();
            while (rp < 0.0f) rp += (float) N; int i = (int) rp; float f = rp - (float) i;

            auto at=[&](int k){ return buf.getSample (0, (k + N) % N); };
            float y0=at(i-1), y1=at(i), y2=at(i+1), y3=at(i+2);
            float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            float s = ((c3*f + c2)*f + c1)*f + c0;

            y += win * s;
        }
        if (++w >= buf.getNumSamples()) w = 0;
        return y * 0.5f; // normalize 4 grains
    }
    juce::AudioBuffer<float> buf; int w{0}, grainLen{4096}; double sampleRate{44100.0}; float phase[4]{0,0,0,0}, shiftRatio{2.0f};
};

struct ShimmerReverb
{
    void prepare (double sr, int maxMs)
    {
        sampleRate = sr;
        hall.prepare (sr, maxMs);
        predelay.prepare (sr, 300);
        shL.prepare (sr, 2000); shR.prepare (sr, 2000);
        lp.prepare (sr); lp.setCutoff (9500.0f);
        hp.prepare (sr); hp.setCutoff (180.0f);
        fbAmt.reset (sr, 0.12); fbAmt.setCurrentAndTargetValue (0.36f); // tuned
        wetTrim = 2.2f; // +6.8 dB
    }

    void setParams (float decaySec, float size01, float dampHz, float diffusion, float predelayMs)
    {
        hall.setDecayAndSize (decaySec, size01);
        hall.setDampHz (dampHz);
        hall.setDiffusion (diffusion);
        predelay.setMs (predelayMs);
        lp.setCutoff (dampHz);
    }

    inline void processSample (float inL, float inR, float& outL, float& outR)
    {
        predelay.push (inL, inR);
        const float pdL = predelay.readCh (0);
        const float pdR = predelay.readCh (1);

        float hL=0, hR=0;
        // inject feedback pre-hall to sustain shimmer
        hall.processSample (pdL + fbL, pdR + fbR, hL, hR);

        // octave-up the hall tail (parallel layer)
        float sL = shL.process (hL);
        float sR = shR.process (hR);

        // filter & feed a portion back
        const float g = fbAmt.getNextValue();
        float fL = hp.process (lp.process (sL));
        float fR = hp.process (lp.process (sR));
        fbL = g * fL;
        fbR = g * fR;

        // parallel mix of original hall + octave layer, then makeup
        const float mL = 0.5f * (hL + fL);
        const float mR = 0.5f * (hR + fR);
        outL = rvb::safetySat (wetTrim * mL);
        outR = rvb::safetySat (wetTrim * mR);
    }

    double sampleRate{44100.0};
    HallReverb hall; rvb::PreDelay predelay;
    OctaveUpGrains4 shL, shR; rvb::OnePoleLPF lp; rvb::OnePoleHPF hp;
    juce::SmoothedValue<float> fbAmt;
    float wetTrim{2.2f}, fbL{0.0f}, fbR{0.0f};
};

// ===================== Mixer wrapper for all three =====================
struct MultiReverb
{
    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;
        hall.prepare    (sr, 3200);
        room.prepare    (sr);
        shimmer.prepare (sr, 3200);
        wH.reset (sr, 0.12); wR.reset (sr, 0.12); wS.reset (sr, 0.12);
        mix.reset (sr, 0.05);
    }

    void setParams (float type, float size, float predelayMs, float dampHz, float diffusion, float early, float decaySec, float mix01)
    {
        hall.setDecayAndSize (decaySec, size);
        hall.setDampHz (dampHz);
        hall.setDiffusion (diffusion);

        room.setParams (decaySec, size, dampHz, diffusion, early, predelayMs);
        shimmer.setParams (decaySec, size, dampHz, diffusion, predelayMs);

        rvb::TypeWeights tw; tw.set (type);
        wH.setTargetValue (tw.wH); wR.setTargetValue (tw.wR); wS.setTargetValue (tw.wS);
        mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mix01));
    }

    inline void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int N = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels()>1 ? buffer.getWritePointer (1) : nullptr;

        for (int n=0; n<N; ++n)
        {
            const float inL = L[n];
            const float inR = R ? R[n] : inL;

            float hL=0,hR=0, rL=0,rR=0, sL=0,sR=0;
            hall.processSample    (inL, inR, hL, hR);
            room.processSample    (inL, inR, rL, rR);
            shimmer.processSample (inL, inR, sL, sR);

            const float a=wH.getNextValue(), b=wR.getNextValue(), c=wS.getNextValue();
            float wetL = a*hL + b*rL + c*sL;
            float wetR = a*hR + b*rR + c*sR;

            const float m = mix.getNextValue();
            L[n] = juce::jmap (m, inL, wetL);
            if (R) R[n] = juce::jmap (m, inR, wetR);
        }
    }

    double sampleRate{44100.0};
    HallReverb hall; RoomReverb room; ShimmerReverb shimmer;
    juce::SmoothedValue<float> wH, wR, wS, mix;
};
