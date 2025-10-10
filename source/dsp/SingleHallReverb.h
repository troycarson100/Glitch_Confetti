#pragma once
#include <juce_dsp/juce_dsp.h>

namespace hallx
{
    inline float softSat(float x) { return std::tanh(0.7f * x); }
    static constexpr float LN1000 = 6.90775527898f;

    struct OnePoleLPF {
        void prepare(double sr){ sampleRate=sr; setCutoff(hz); z=0; }
        void setCutoff(float f){
            hz = juce::jlimit(20.0f, 20000.0f, f);
            const float a1 = std::exp(-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
            a = a1; b = 1.0f - a1;
        }
        inline float process(float x){ z = a*z + b*x; return z; }
        double sampleRate{44100.0}; float hz{8000.0f}, a{0}, b{1}, z{0};
    };

    struct PreDelay {
        void prepare(double sr, int maxMs) {
            sampleRate = sr;
            const int N = (int) std::ceil(sr * maxMs / 1000.0) + 16;
            buf.setSize(2, N); buf.clear(); w=0; dSamps=0.0f;
        }
        void setMs(float ms){
            dSamps = juce::jlimit(0.0f, (float) buf.getNumSamples()-4.0f, ms * (float) sampleRate * 0.001f);
        }
        inline void push(float L, float R){
            buf.setSample(0, w, L); buf.setSample(1, w, R);
            if (++w >= buf.getNumSamples()) w = 0;
        }
        inline float readCh(int ch) const {
            float rp = (float) w - dSamps;
            const int N = buf.getNumSamples();
            while (rp < 0.0f) rp += (float) N;
            int i = (int) rp; float f = rp - (float) i;
            auto at=[&](int k){ return buf.getSample(ch, (k+N)%N); };
            float y0=at(i-1),y1=at(i),y2=at(i+1),y3=at(i+2);
            float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            return ((c3*f + c2)*f + c1)*f + c0;
        }
        juce::AudioBuffer<float> buf; int w{0}; float dSamps{0}; double sampleRate{44100.0};
    };
}

//==================== Single, bold Hall ====================
struct SingleHallReverb
{
    static constexpr int N = 8; // 8-line FDN

    void prepare (double sr, int maxDelayMs)
    {
        sampleRate = sr;

        // Long buffers so feedback can be very close to 1.0 without risk
        const int maxSamps = (int) std::ceil (sr * maxDelayMs / 1000.0) + 32;

        for (int i=0; i<N; ++i)
        {
            line[i].setSize (1, maxSamps);
            line[i].clear();
            widx[i] = 0;

            baseMs[i] = baseTemplate[i];
            targetMs[i] = baseMs[i];
            smMs[i].reset (sr, 0.08); // 80 ms size morph
            smMs[i].setCurrentAndTargetValue (targetMs[i]);

            ap[i].reset();
            juce::dsp::ProcessSpec spec{sr, (juce::uint32)maxDelayMs, 1};
            ap[i].prepare(spec);
            *ap[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 900.0f, 0.7f);
        }

        // Early reflections buffer
        earlyBuf.setSize (2, (int) std::ceil(sr * 160 / 1000.0) + 32);
        earlyBuf.clear(); ew = 0;

        pd.prepare (sr, 240);
        damp.prepare (sr);
        
        // Smooth predelay parameter changes
        predelaySmooth.reset(sr, 0.08); // 80ms smooth
        predelaySmooth.setCurrentAndTargetValue(20.0f);
        targetPredelayMs = 20.0f;
        
        setParams (/*size*/ 0.8f, /*decaySec*/ 4.0f, /*dampingHz*/ 9000.0f, /*diffuse*/ 0.8f, /*early*/ 0.5f, /*width*/ 1.0f, /*predelayMs*/20.0f);

        wetTrim = 4.0f; // ≈ +12 dB wet makeup for BOLD, obvious reverb
    }

    // Public setter called once per block
    void setParams (float size01, float decaySec, float dampingHz, float diffusion01, float earlyLevel01, float width01, float predelayMs)
    {
        size = juce::jlimit (0.1f, 1.5f, size01);
        decay = juce::jlimit (0.2f, 20.0f, decaySec);
        diffusion = juce::jlimit (0.0f, 1.0f, diffusion01);
        earlyLevel = juce::jlimit (0.0f, 1.0f, earlyLevel01);
        width = juce::jlimit (0.0f, 1.0f, width01);

        // Smooth predelay changes to avoid scratching
        targetPredelayMs = juce::jlimit (0.0f, 200.0f, predelayMs);
        
        damp.setCutoff (juce::jlimit (1000.0f, 20000.0f, dampingHz));

        apGain = juce::jmap (diffusion, 0.55f, 0.92f);

        // Update delay targets and per-line feedback from true RT60:
        // g = exp(-ln(1000) * D / T), where D is the loop delay in seconds.
        for (int i=0; i<N; ++i)
        {
            targetMs[i] = baseTemplate[i] * size;
            smMs[i].setTargetValue (targetMs[i]);

            const float D = targetMs[i] * 0.001f;
            float g = std::exp (-hallx::LN1000 * (D / juce::jmax (0.2f, decay)));
            fb[i] = juce::jlimit (0.2f, 0.99995f, g); // allow *very* long tails
        }
    }

    inline void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int nsmps = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = (buffer.getNumChannels()>1) ? buffer.getWritePointer (1) : nullptr;

        for (int n=0; n<nsmps; ++n)
        {
            float inL = L[n];
            float inR = R ? R[n] : inL;

            // 1) Predelay (with smoothing to avoid scratching)
            predelaySmooth.setTargetValue(targetPredelayMs);
            float smoothPredelayMs = predelaySmooth.getNextValue();
            pd.setMs(smoothPredelayMs);
            
            pd.push (inL, inR);
            const float pdL = pd.readCh (0);
            const float pdR = pd.readCh (1);

            // 2) Early reflections (strong, musical)
            earlyBuf.setSample (0, ew, pdL);
            earlyBuf.setSample (1, ew, pdR);
            float erL=0.0f, erR=0.0f;
            for (auto& t : earlyTaps)
            {
                const float dS = t.first * 0.001f * (float) sampleRate;
                float rp = (float) ew - dS; const int N = earlyBuf.getNumSamples();
                while (rp < 0.0f) rp += (float) N;
                int i = (int) rp; float f = rp - (float) i;
                auto at=[&](int ch,int k){ return earlyBuf.getSample (ch, (k+N)%N); };
                auto interp=[&](int ch){
                    float y0=at(ch,i-1),y1=at(ch,i),y2=at(ch,i+1),y3=at(ch,i+2);
                    float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                    return ((c3*f+c2)*f+c1)*f+c0;
                };
                erL += t.second * interp(0);
                erR += t.second * interp(1);
            }
            if (++ew >= earlyBuf.getNumSamples()) ew = 0;

            // 3) Late FDN read → diffuse → damp
            const float inM = 0.5f * (pdL + pdR);
            float y[N];
            for (int i=0; i<N; ++i)
            {
                // Size morph (no-glitch)
                const float msNow = smMs[i].getNextValue();

                auto readInterp = [&](float ms){
                    float dS = ms * 0.001f * (float) sampleRate;
                    float rp = (float) widx[i] - dS; const int Nb = line[i].getNumSamples();
                    while (rp < 0.0f) rp += (float) Nb; int idx=(int)rp; float f = rp - (float) idx;
                    auto at=[&](int k){ return line[i].getSample (0, (k+Nb)%Nb); };
                    float y0=at(idx-1),y1=at(idx),y2=at(idx+1),y3=at(idx+2);
                    float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                    return ((c3*f+c2)*f+c1)*f+c0;
                };
                const float del = readInterp (msNow);

                float apOut = ap[i].processSample (del);
                y[i] = apGain * apOut; // NO damping in feedback loop - apply at output only
            }

            // 4) Orthogonal mix (two 4x4 Hadamards)
            auto hmix4=[&](float a0,float a1,float a2,float a3,float& o0,float& o1,float& o2,float& o3){
                o0=0.5f*(a0+a1+a2+a3);
                o1=0.5f*(a0-a1+a2-a3);
                o2=0.5f*(a0+a1-a2-a3);
                o3=0.5f*(a0-a1-a2+a3);
            };
            float h0,h1,h2,h3,h4,h5,h6,h7;
            hmix4(y[0],y[1],y[2],y[3], h0,h1,h2,h3);
            hmix4(y[4],y[5],y[6],y[7], h4,h5,h6,h7);
            float d[8]{h0,h1,h2,h3,h4,h5,h6,h7};

            // 5) Feedback write
            for (int i=0; i<N; ++i)
            {
                const float xin = inM + fb[i] * d[i];
                line[i].setSample (0, widx[i], hallx::softSat (xin));
            }
            if (++widx[0] >= line[0].getNumSamples())
            {
                for (int i=0; i<N; ++i)
                    if (++widx[i] >= line[i].getNumSamples()) widx[i] = 0;
            }

            // 6) Stereo late sum (width controls M/S spread)
            float lateL = 0.25f * (y[0]+y[2]+y[4]+y[6]);
            float lateR = 0.25f * (y[1]+y[3]+y[5]+y[7]);
            
            // Apply VERY gentle damping (blend, not full filtering) to preserve energy
            float dampedL = damp.process(lateL);
            float dampedR = damp.process(lateR);
            // Scale damping based on decay time: long decay = less damping
            float dampBlend = juce::jmap(decay, 0.2f, 20.0f, 1.0f, 0.15f); // 100% to 15% damping
            lateL = lateL + dampBlend * (dampedL - lateL);
            lateR = lateR + dampBlend * (dampedR - lateR);

            // Convert to mid/side, apply width, back to L/R
            float mid  = 0.5f * (lateL + lateR);
            float side = 0.5f * (lateL - lateR);
            side *= width; // width control
            float wideL = mid + side;
            float wideR = mid - side;

            // 7) Early/Late blend + wet makeup
            float wetL = hallx::softSat (wetTrim * (earlyLevel * erL + (1.0f - earlyLevel) * wideL));
            float wetR = hallx::softSat (wetTrim * (earlyLevel * erR + (1.0f - earlyLevel) * wideR));

            // 8) Mix
            const float m = mix;
            L[n] = juce::jmap (m, inL, wetL);
            if (R) R[n] = juce::jmap (m, inR, wetR);
        }
    }

    // Call once per block
    void setMix (float m01) { mix = juce::jlimit (0.0f, 1.0f, m01); }

    // === members ===
    double sampleRate{44100.0};

    // FDN lines
    juce::AudioBuffer<float> line[N]; int widx[N]{};
    float baseMs[N]{}, targetMs[N]{}, fb[N]{};
    juce::SmoothedValue<float> smMs[N];
    juce::dsp::IIR::Filter<float> ap[N]; // diffusers
    hallx::OnePoleLPF damp;

    // Early reflections
    juce::AudioBuffer<float> earlyBuf; int ew{0};
    // (ms, gain): boosted & musical
    std::array<std::pair<float,float>,6> earlyTaps {{
        {6.5f,0.95f}, {10.0f,0.88f}, {15.5f,0.80f},
        {22.0f,0.72f}, {31.0f,0.64f}, {46.0f,0.56f}
    }};

    hallx::PreDelay pd;

    // Params/state
    float size{0.8f}, decay{4.0f}, diffusion{0.8f}, earlyLevel{0.5f}, width{1.0f}, apGain{0.8f};
    float wetTrim{4.0f}; // wet makeup so the hall is present at 100% mix
    float mix{0.3f};
    float targetPredelayMs{20.0f};
    juce::SmoothedValue<float> predelaySmooth; // Smooth predelay to avoid scratching

    // Incommensurate base delays (ms)
    const float baseTemplate[N] = { 31.7f, 37.1f, 43.8f, 51.2f, 58.9f, 67.3f, 73.5f, 81.0f };
};

