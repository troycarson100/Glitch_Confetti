#pragma once
#include <juce_dsp/juce_dsp.h>

//===================== Small helpers =====================
namespace dspx
{
    struct OnePoleLPF {
        void prepare (double sr) { sampleRate = sr; z = 0.0f; setCutoff (cutoffHz); }
        void setCutoff (float hz) {
            cutoffHz = juce::jlimit (20.0f, 20000.0f, hz);
            const float x = std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
            a = x; b = 1.0f - x;
        }
        inline float process (float x) { z = a * z + b * x; return z; }

        double sampleRate { 44100.0 };
        float cutoffHz { 8000.0f }, a { 0.0f }, b { 1.0f }, z { 0.0f };
    };

    // Simple predelay line (stereo)
    struct PreDelay {
        void prepare (double sr, int maxMs) {
            sampleRate = sr; const int n = (int) std::ceil (sr * maxMs / 1000.0) + 4;
            buf.setSize (2, n); buf.clear(); widx = 0; delaySamps = 0.0f;
        }
        void setMs (float ms) { delaySamps = juce::jlimit (0.0f, (float) buf.getNumSamples()-4.0f, ms * (float) sampleRate * 0.001f); }
        inline void push (float l, float r) { buf.setSample (0, widx, l); buf.setSample (1, widx, r); if (++widx>=buf.getNumSamples()) widx=0; }
        inline float readCh (int ch) const {
            float rp = (float) widx - delaySamps; const int N = buf.getNumSamples();
            while (rp < 0.0f) rp += (float) N; int i = (int) rp; float f = rp - (float) i;
            auto at = [&](int k){ return buf.getSample (ch, (k + N) % N); };
            float y0 = at (i-1), y1 = at (i), y2 = at (i+1), y3 = at (i+2);
            // 4-pt Hermite
            float c0=y1, c1=0.5f*(y2-y0), c2=y0-2.5f*y1+2.0f*y2-0.5f*y3, c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            return ((c3*f + c2)*f + c1)*f + c0;
        }
        juce::AudioBuffer<float> buf; int widx{0}; float delaySamps{0.0f}; double sampleRate{44100.0};
    };

    // Gentle safety soft clip
    inline float softSat (float x) { return std::tanh (x * 0.8f); }

    // Tri-weighted crossfade weights for verbType in [0..2]
    struct TypeWeights {
        float wH{1}, wR{0}, wS{0};
        void set (float t) {
            auto tri = [](float x, float c){ return juce::jmax (0.0f, 1.0f - std::abs (x - c)); };
            float h = tri (t, 0.0f), r = tri (t, 1.0f), s = tri (t, 2.0f);
            float sum = h + r + s; if (sum <= 1e-6f) { wH=1; wR=0; wS=0; } else { wH=h/sum; wR=r/sum; wS=s/sum; }
        }
    };
}

//===================== Hall: FDN with modulated diffusers =====================
struct HallReverb
{
    static constexpr int N = 4; // 4-line FDN keeps CPU low but sounds lush

    void prepare (double sr, int maxMs)
    {
        sampleRate = sr;
        const int maxSamps = (int) std::ceil (sr * maxMs / 1000.0) + 8;
        
        // Prepare IIR filters with ProcessSpec first
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        for (int i=0; i<N; ++i)
        {
            lines[i].setSize (1, maxSamps); lines[i].clear(); widx[i]=0;
            
            // Prepare allpass filter with ProcessSpec
            ap[i].prepare(spec);
            ap[i].reset();
            *ap[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 600.0f, 0.5f);
            
            lfoPhase[i] = juce::Random::getSystemRandom().nextFloat();
        }
        dampLpf.prepare (sr); dampLpf.setCutoff (8000.0f);
        setSize (0.7f); setDiffusion (0.7f);
    }

    void setSize (float s)      { size = juce::jlimit (0.1f, 1.5f, s); }
    void setDiffusion (float d) { diffusion = juce::jlimit (0.0f, 1.0f, d); }
    void setDampHz (float hz)   { dampLpf.setCutoff (hz); }

    // rt60-ish mapping (simple): feedback = exp(-3 ln(10) * T / delay)
    void setDecayFromSize()
    {
        // base delays ~ [35..75] ms scaled by size
        const float baseMs[N] = { 37.1f, 53.3f, 61.7f, 73.0f };
        for (int i=0; i<N; ++i)
            delayMs[i] = baseMs[i] * size;
        const float rtSec = juce::jmap (size, 0.1f, 1.5f, 0.6f, 8.0f); // musical mapping
        for (int i=0; i<N; ++i)
        {
            const float dS = delayMs[i] * 0.001f * (float) sampleRate;
            fb[i] = std::exp (-6.9078f * (dS / (rtSec * (float) sampleRate))); // ~e^(-ln(1000)*delay/RT)
            fb[i] = juce::jlimit (0.1f, 0.98f, fb[i]);
        }
    }

    void updateParams (float size_, float diffusion_, float dampHz_)
    {
        setSize (size_); setDiffusion (diffusion_); setDampHz (dampHz_); setDecayFromSize();
    }

    inline void processSample (float inL, float inR, float& outL, float& outR)
    {
        // Hadamard 4×4 feedback mixing, with modulated allpass per line for diffusion
        float in = 0.5f * (inL + inR);

        float y[N];
        for (int i=0; i<N; ++i)
        {
            // Read line with cubic interp
            float dSamp = delayMs[i] * 0.001f * (float) sampleRate;
            float rp = (float) widx[i] - dSamp; const int Nbuf = lines[i].getNumSamples();
            while (rp < 0.0f) rp += (float) Nbuf;
            int idx = (int) rp; float frac = rp - (float) idx;
            auto at=[&](int k){ return lines[i].getSample (0, (k + Nbuf) % Nbuf); };
            float y0 = at(idx-1), y1 = at(idx), y2 = at(idx+1), y3 = at(idx+2);
            float c0=y1, c1=0.5f*(y2-y0), c2=y0-2.5f*y1+2.0f*y2-0.5f*y3, c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            float del = ((c3*frac + c2)*frac + c1)*frac + c0;

            // Diffusion via allpass with slight LFO mod
            float lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase[i]); // slow sine
            lfoPhase[i] += (0.15f + 0.05f * (i+1)) / (float) sampleRate; if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
            float apIn = del;
            y[i] = ap[i].processSample (apIn + 0.0005f * lfo * apIn); // tiny mod

            y[i] = dampLpf.process (y[i]); // HF damping
        }

        // 4×4 Hadamard mix (orthogonal)
        float h0 =  0.5f*( y[0] + y[1] + y[2] + y[3]);
        float h1 =  0.5f*( y[0] - y[1] + y[2] - y[3]);
        float h2 =  0.5f*( y[0] + y[1] - y[2] - y[3]);
        float h3 =  0.5f*( y[0] - y[1] - y[2] + y[3]);

        // Feedback write with diffusion amount shaping
        float d[N] { h0, h1, h2, h3 };
        for (int i=0; i<N; ++i)
        {
            float xin = in + fb[i] * (juce::jmap (diffusion, 0.0f, 1.0f, 0.7f, 1.0f) * d[i]);
            lines[i].setSample (0, widx[i], dspx::softSat (xin));
            if (++widx[i] >= lines[i].getNumSamples()) widx[i] = 0;
        }

        // Stereo out: simple pairings for width
        outL = 0.5f * (y[0] + y[2]);
        outR = 0.5f * (y[1] + y[3]);
    }

    double sampleRate { 44100.0 };
    juce::AudioBuffer<float> lines[N]; int widx[N] {0,0,0,0};
    float delayMs[N] {}; float fb[N] {}; float size{0.7f}, diffusion{0.7f};
    juce::dsp::IIR::Filter<float> ap[N]; float lfoPhase[N] {};
    dspx::OnePoleLPF dampLpf;
};

//===================== Room: Early reflections + compact tail =====================
struct RoomReverb
{
    void prepare (double sr)
    {
        sampleRate = sr;
        // Early taps (ms, gains) — musical small/med room sketch
        taps = {
            { 6.7f,  0.72f }, { 9.9f, 0.65f }, { 15.2f, 0.58f },
            { 21.7f, 0.49f }, { 32.1f,0.42f }, { 47.3f, 0.35f }
        };
        int maxMs = 120;
        earlyBuf.setSize (2, (int) std::ceil (sr * maxMs / 1000.0) + 4); earlyBuf.clear(); widx = 0;

        // Tail: reuse a compact Hall tank with smaller sizes
        hall.prepare (sr, 120);
        setParams (0.7f, 8000.0f, 0.7f, 0.35f, 20.0f);
    }

    void setParams (float size, float dampHz, float diffusion, float earlyLevel, float predelayMs)
    {
        size_ = juce::jlimit (0.1f, 1.5f, size);
        hall.updateParams (0.5f * size_, diffusion, dampHz);
        earlyLevel_ = juce::jlimit (0.0f, 1.0f, earlyLevel);
        predelay.setMs (juce::jlimit (0.0f, 200.0f, predelayMs));
    }

    inline void processSample (float inL, float inR, float& outL, float& outR)
    {
        // write predelay input
        predelay.push (inL, inR);
        const float pdL = predelay.readCh (0);
        const float pdR = predelay.readCh (1);

        // early reflections: sum fixed taps from stereo buffer
        earlyBuf.setSample (0, widx, pdL);
        earlyBuf.setSample (1, widx, pdR);

        float erL = 0.0f, erR = 0.0f;
        for (auto& t : taps)
        {
            float dS = t.first * 0.001f * (float) sampleRate;
            float rp = (float) widx - dS; const int N = earlyBuf.getNumSamples();
            while (rp < 0.0f) rp += (float) N; int i = (int) rp; float f = rp - (float) i;
            auto at=[&](int ch,int k){ return earlyBuf.getSample (ch,(k+N)%N); };
            auto interp=[&](int ch){
                float y0=at(ch,i-1),y1=at(ch,i),y2=at(ch,i+1),y3=at(ch,i+2);
                float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                return ((c3*f + c2)*f + c1)*f + c0;
            };
            erL += t.second * interp(0);
            erR += t.second * interp(1);
        }
        if (++widx >= earlyBuf.getNumSamples()) widx = 0;

        // compact tail
        float hallL=0, hallR=0;
        hall.processSample (pdL, pdR, hallL, hallR);

        outL = earlyLevel_ * erL + (1.0f - earlyLevel_) * hallL;
        outR = earlyLevel_ * erR + (1.0f - earlyLevel_) * hallR;
    }

    double sampleRate { 44100.0 };
    HallReverb hall;
    dspx::PreDelay predelay;
    std::vector<std::pair<float,float>> taps; // {ms, gain}
    juce::AudioBuffer<float> earlyBuf; int widx { 0 };
    float size_ { 0.7f }, earlyLevel_ { 0.35f };
};

//===================== Shimmer: Hall tail + pitch-shifted feedback =====================
struct SimpleOctaveUp // lightweight time-domain shifter for tails
{
    // dual crossfaded delay grains, upshift by +12 semitones with sliding read
    void prepare (double sr, int maxMs)
    {
        sampleRate = sr; const int N = (int) std::ceil (sr * maxMs / 1000.0) + 8;
        buf.setSize (1, N); buf.clear(); widx = 0;
        grainLen = (int) std::round (0.040 * sr); // 40 ms grains
    }
    void setShift (float semitones) {
        // fixed +12 for now, but keep interface
        shiftRatio = std::pow (2.0f, semitones / 12.0f);
    }
    inline float process (float x)
    {
        buf.setSample (0, widx, x);
        // two grains with opposite crossfades
        float y=0.0f; for (int g=0; g<2; ++g)
        {
            float ph = phases[g]; ph += (shiftRatio - 1.0f) * (grainLen / (float) sampleRate);
            if (ph >= 1.0f) ph -= 1.0f; phases[g] = ph;

            float readBack = ph * (float) grainLen;
            float rp = (float) widx - readBack; const int N = buf.getNumSamples();
            while (rp < 0.0f) rp += (float) N; int i = (int) rp; float f = rp - (float) i;

            auto at=[&](int k){ return buf.getSample (0, (k + N) % N); };
            float y0=at(i-1), y1=at(i), y2=at(i+1), y3=at(i+2);
            float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            float s = ((c3*f + c2)*f + c1)*f + c0;

            float win = (g==0 ? (1.0f - ph) : ph); // crossfade window
            y += win * s;
        }
        if (++widx >= buf.getNumSamples()) widx = 0;
        return y * 0.7f; // tame gain
    }
    juce::AudioBuffer<float> buf; int widx{0}, grainLen{2048}; double sampleRate{44100.0}; float phases[2]{0,0}, shiftRatio{2.0f};
};

struct ShimmerReverb
{
    void prepare (double sr, int maxMs)
    {
        sampleRate = sr;
        hall.prepare (sr, maxMs);
        shifterL.prepare (sr, 1200);
        shifterR.prepare (sr, 1200);
        shifterL.setShift (+12.0f); shifterR.setShift (+12.0f);
        predelay.prepare (sr, 200);
        damp.setCutoff (8000.0f); damp.prepare (sr);
    }

    void setParams (float size, float dampHz, float diffusion, float predelayMs, float shimmerAmt)
    {
        hall.updateParams (size, diffusion, dampHz);
        predelay.setMs (predelayMs);
        shimmer = juce::jlimit (0.0f, 1.0f, shimmerAmt);
        damp.setCutoff (dampHz);
    }

    inline void processSample (float inL, float inR, float& outL, float& outR)
    {
        // predelay into hall
        predelay.push (inL, inR);
        const float pdL = predelay.readCh (0);
        const float pdR = predelay.readCh (1);

        float hL=0, hR=0;
        hall.processSample (pdL + fbL, pdR + fbR, hL, hR); // inject feedback into hall input

        // pitch-shifted feedback from hall out
        float sL = shifterL.process (hL);
        float sR = shifterR.process (hR);

        // filter & scale feedback
        fbL = shimmer * damp.process (sL);
        fbR = shimmer * damp.process (sR);

        outL = hL; outR = hR;
    }

    double sampleRate{44100.0};
    HallReverb hall; dspx::PreDelay predelay; SimpleOctaveUp shifterL, shifterR; dspx::OnePoleLPF damp;
    float shimmer { 0.0f }, fbL{0.0f}, fbR{0.0f};
};

//===================== Mixer wrapper for all three =====================
struct MultiReverb
{
    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        hall.prepare    (sr, 3000);
        room.prepare    (sr);
        shimmer.prepare (sr, 3000);

        // smoothed crossfade weights & params
        wH.reset (sr, 0.12); wR.reset (sr, 0.12); wS.reset (sr, 0.12);
        mix.reset (sr, 0.04);
    }

    void setParams (float type, float size, float predelayMs, float dampHz, float diffusion, float early, float shimmerAmt, float mix01)
    {
        // update engines
        hall.updateParams (size, diffusion, dampHz);
        room.setParams     (size, dampHz, diffusion, early, predelayMs);
        shimmer.setParams  (size, dampHz, diffusion, predelayMs, shimmerAmt);

        // crossfade weights from type
        dspx::TypeWeights tw; tw.set (type);
        wH.setTargetValue (tw.wH); wR.setTargetValue (tw.wR); wS.setTargetValue (tw.wS);
        mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mix01));
    }

    inline void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int N = buffer.getNumSamples();
        auto L = buffer.getWritePointer (0);
        auto R = buffer.getNumChannels()>1 ? buffer.getWritePointer (1) : nullptr;

        for (int n=0; n<N; ++n)
        {
            const float inL = L[n];
            const float inR = R ? R[n] : inL;

            float hL=0,hR=0, rL=0,rR=0, sL=0,sR=0;

            hall.processSample    (inL, inR, hL, hR);
            room.processSample    (inL, inR, rL, rR);
            shimmer.processSample (inL, inR, sL, sR);

            const float a = wH.getNextValue(), b = wR.getNextValue(), c = wS.getNextValue();
            float wetL = a*hL + b*rL + c*sL;
            float wetR = a*hR + b*rR + c*sR;

            // true wet/dry crossfade
            const float m = mix.getNextValue();
            L[n] = juce::jmap (m, inL, wetL);
            if (R) R[n] = juce::jmap (m, inR, wetR);
        }
    }

    double sampleRate{44100.0};
    HallReverb hall; RoomReverb room; ShimmerReverb shimmer;
    juce::SmoothedValue<float> wH, wR, wS, mix;
};

