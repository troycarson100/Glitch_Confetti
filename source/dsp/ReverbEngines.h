#pragma once
#include <juce_dsp/juce_dsp.h>

/*** Small helpers ***/
namespace rvb {
    inline float satSoft(float x){ return std::tanh(0.7f*x); }
    inline float lerp(float a,float b,float t){ return a+(b-a)*t; }
    static constexpr float LN1000 = 6.90775527898f;

    struct OnePoleLPF {
        void prepare(double sr){ sampleRate=sr; setCutoff(cutHz); z=0; }
        void setCutoff(float hz){
            cutHz = juce::jlimit(20.0f, 20000.0f, hz);
            const float a1 = std::exp(-juce::MathConstants<float>::twoPi*cutHz/(float)sampleRate);
            a=a1; b=1.0f-a1;
        }
        inline float process(float x){ z = a*z + b*x; return z; }
        double sampleRate{44100.0}; float cutHz{8000.0f}, a{0}, b{1}, z{0};
    };
    struct OnePoleHPF {
        void prepare(double sr){ sampleRate=sr; setCutoff(cutHz); xz=yz=0; }
        void setCutoff(float hz){
            cutHz = juce::jlimit(20.0f, 1000.0f, hz);
            const float c = std::exp(-juce::MathConstants<float>::twoPi*cutHz/(float)sampleRate);
            a=c; b=(1.0f+c)*0.5f; d=(1.0f-c)*0.5f;
        }
        inline float process(float x){ float y=b*x + d*xz - a*yz; xz=x; yz=y; return y; }
        double sampleRate{44100.0}; float cutHz{150.0f}, a{0}, b{0}, d{0}, xz{0}, yz{0};
    };
    struct PreDelay {
        void prepare(double sr,int maxMs){
            sampleRate=sr; const int N=(int)std::ceil(sr*maxMs/1000.0)+8;
            buf.setSize(2,N); buf.clear(); w=0; dSamps=0;
        }
        void setMs(float ms){
            dSamps = juce::jlimit(0.0f,(float)buf.getNumSamples()-4.0f, ms*(float)sampleRate*0.001f);
        }
        inline void push(float L,float R){ buf.setSample(0,w,L); buf.setSample(1,w,R); if(++w>=buf.getNumSamples()) w=0; }
        inline float readCh(int ch) const {
            float rp=(float)w - dSamps; const int N=buf.getNumSamples();
            while(rp<0) rp += (float)N; int i=(int)rp; float f=rp-(float)i;
            auto at=[&](int k){ return buf.getSample(ch, (k+N)%N); };
            float y0=at(i-1), y1=at(i), y2=at(i+1), y3=at(i+2);
            float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            return ((c3*f+c2)*f+c1)*f+c0;
        }
        juce::AudioBuffer<float> buf; int w{0}; float dSamps{0}; double sampleRate{44100.0};
    };
}

/*** Hall: 8-line FDN with proper RT60 and no-glitch size ***/
struct HallReverb {
    static constexpr int N=8;
    void prepare(double sr,int maxMs){
        sampleRate=sr;
        const int maxSamps=(int)std::ceil(sr*maxMs/1000.0)+16;
        for(int i=0;i<N;++i){
            line[i].setSize(1,maxSamps); line[i].clear(); widx[i]=0;
            baseMs[i]=baseTemplate[i];
            delayMs[i]=baseMs[i];
            smoothMs[i].reset(sr,0.06); smoothMs[i].setCurrentAndTargetValue(delayMs[i]);
            ap[i].reset();
            juce::dsp::ProcessSpec spec{sr, (juce::uint32)maxMs, 1};
            ap[i].prepare(spec);
            *ap[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 900.0f, 0.7f);
        }
        dampL.prepare(sr); dampL.setCutoff(8000.0f);
        dampR.prepare(sr); dampR.setCutoff(8000.0f);
        setParams(0.7f, 4.0f, 0.8f); // size, decaySec, diffusion
        wetTrim=1.7f; // ~+4.6 dB (more balanced)
    }

    // size01 ∈ [0.1..1.5], decaySec ∈ [0.2..20], diffusion ∈ [0..1]
    void setParams(float size01,float decaySec,float diffusion){
        size = juce::jlimit(0.1f,1.5f,size01);
        decay= juce::jlimit(0.2f,20.0f,decaySec);
        diff = juce::jlimit(0.0f,1.0f,diffusion);
        apGain = juce::jmap(diff, 0.0f,1.0f, 0.6f,0.9f);

        for(int i=0;i<N;++i){
            delayMs[i] = baseTemplate[i]*size;
            smoothMs[i].setTargetValue(delayMs[i]);

            // Proper RT60 per-line feedback: g = exp( -ln(1000) * D / T )
            const float D = delayMs[i]*0.001f; // seconds
            float g = std::exp(-rvb::LN1000 * (D / decay));
            fb[i] = juce::jlimit(0.2f, 0.9999f, g);
        }
    }
    void setDampHz(float hz){ dampL.setCutoff(hz); dampR.setCutoff(hz); }

    inline void processSample(float inL,float inR,float& outL,float& outR){
        const float in = 0.5f*(inL+inR);

        // read with smoothed delay (size morph)
        float y[N];
        for(int i=0;i<N;++i){
            const float msTarget = delayMs[i];
            smoothMs[i].setTargetValue(msTarget);
            const float msNow = smoothMs[i].getNextValue();

            auto readInterp = [&](float ms){
                float dS = ms*0.001f*(float)sampleRate;
                float rp = (float)widx[i] - dS; const int Nb=line[i].getNumSamples();
                while(rp<0) rp += (float)Nb; int idx=(int)rp; float f=rp-(float)idx;
                auto at=[&](int k){ return line[i].getSample(0,(k+Nb)%Nb); };
                float y0=at(idx-1),y1=at(idx),y2=at(idx+1),y3=at(idx+2);
                float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                return ((c3*f+c2)*f+c1)*f+c0;
            };
            const float delNow = readInterp(msNow);
            const float delOld = readInterp(msTarget);
            float xf = juce::jlimit(0.0f,1.0f, std::abs(msTarget-msNow)/10.0f);
            float del = rvb::lerp(delOld, delNow, 1.0f - xf);

            float apOut = ap[i].processSample(del); // diffuser
            y[i] = apGain * apOut; // Don't apply damping here - will apply after mixing
        }

        // Orthogonal mix (two 4x4 Hadamards)
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

        // write with feedback + input
        for(int i=0;i<N;++i){
            const float xin = in + fb[i]*d[i];
            line[i].setSample(0, widx[i], rvb::satSoft(xin));
        }
        // advance all widx equally
        if(++widx[0] >= line[0].getNumSamples()){
            for(int i=0;i<N;++i) if(++widx[i] >= line[i].getNumSamples()) widx[i]=0;
        }

        // wide stereo sum with damping applied after mixing (preserves energy in feedback)
        float L = 0.25f*(y[0]+y[2]+y[4]+y[6]);
        float R = 0.25f*(y[1]+y[3]+y[5]+y[7]);
        
        // Apply damping to output only, not in feedback loop
        L = dampL.process(L);
        R = dampR.process(R);
        
        outL = rvb::satSoft(wetTrim * L);
        outR = rvb::satSoft(wetTrim * R);
    }

    double sampleRate{44100.0};
    juce::AudioBuffer<float> line[N]; int widx[N]{};
    float baseMs[N]{}, delayMs[N]{}, fb[N]{}, size{0.7f}, decay{4.0f}, diff{0.8f}, apGain{0.8f}, wetTrim{1.7f};
    juce::SmoothedValue<float> smoothMs[N];
    juce::dsp::IIR::Filter<float> ap[N];
    rvb::OnePoleLPF dampL, dampR; // Stereo damping filters
    // incommensurate template delays (ms) - longer for proper RT60
    const float baseTemplate[N] = { 79.3f, 103.7f, 127.1f, 149.8f, 173.2f, 191.5f, 223.7f, 251.0f };
};

/*** Room: strong early reflections + compact late tail ***/
struct RoomReverb {
    void prepare(double sr){
        sampleRate=sr;
        // Strong ER taps (ms, gain) - reduced for better balance
        taps = {{6.3f,0.65f},{9.5f,0.58f},{14.7f,0.52f},{21.0f,0.46f},{30.0f,0.38f},{44.0f,0.32f}};
        earlyBuf.setSize(2, (int)std::ceil(sr*140/1000.0)+8); earlyBuf.clear(); ew=0;
        predelay.prepare(sr, 200);
        hall.prepare(sr, 1200);            // compact late tail reuse
        hall.setParams(0.55f, 2.0f, 0.7f); // size, decay, diffusion defaults
        earlyGain = 0.95f;                 // balanced ER level
        wetTrim   = 1.6f;                  // ~+4 dB
    }
    void setParams(float size01,float decaySec,float dampHz,float diffusion,float earlyLevel, float predelayMs){
        size = juce::jlimit(0.1f,1.5f,size01);
        ER   = juce::jlimit(0.0f,1.0f,earlyLevel);
        predelay.setMs(predelayMs);
        hall.setParams(0.55f*size, juce::jlimit(0.2f,20.0f,decaySec), juce::jlimit(0.0f,1.0f,diffusion));
        hall.setDampHz(dampHz);
    }
    inline void processSample(float inL,float inR,float& outL,float& outR){
        predelay.push(inL,inR);
        const float pdL=predelay.readCh(0), pdR=predelay.readCh(1);

        // ER write
        earlyBuf.setSample(0,ew,pdL);
        earlyBuf.setSample(1,ew,pdR);

        float erL=0, erR=0;
        for(auto& t : taps){
            const float dS = t.first*0.001f*(float)sampleRate;
            float rp=(float)ew - dS; const int N=earlyBuf.getNumSamples();
            while(rp<0) rp += (float)N; int i=(int)rp; float f=rp-(float)i;
            auto at=[&](int ch,int k){ return earlyBuf.getSample(ch,(k+N)%N); };
            auto interp=[&](int ch){
                float y0=at(ch,i-1),y1=at(ch,i),y2=at(ch,i+1),y3=at(ch,i+2);
                float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
                return ((c3*f+c2)*f+c1)*f+c0;
            };
            erL += t.second * interp(0);
            erR += t.second * interp(1);
        }
        if(++ew >= earlyBuf.getNumSamples()) ew=0;

        float tailL=0, tailR=0;
        hall.processSample(pdL, pdR, tailL, tailR);

        outL = rvb::satSoft(wetTrim * (earlyGain*ER*erL + (1.0f-ER)*tailL));
        outR = rvb::satSoft(wetTrim * (earlyGain*ER*erR + (1.0f-ER)*tailR));
    }

    double sampleRate{44100.0};
    std::vector<std::pair<float,float>> taps; // {ms,gain}
    juce::AudioBuffer<float> earlyBuf; int ew{0};
    rvb::PreDelay predelay;
    HallReverb hall;
    float earlyGain{0.95f}, wetTrim{1.6f}, size{0.55f}, ER{0.6f};
};

/*** Shimmer: parallel octave tail in feedback (4-grain OLA), exclusive engine ***/
struct OctaveUpGrains4 {
    void prepare(double sr,int maxMs){
        sampleRate=sr; const int N=(int)std::ceil(sr*maxMs/1000.0)+16;
        buf.setSize(1,N); buf.clear(); w=0;
        setGrainMs(90.0f);
        for(int g=0; g<4; ++g) phase[g] = g/4.0f;
        shiftRatio = 2.0f; // +12 sem
    }
    void setGrainMs(float ms){
        grain = juce::jlimit(64,(int)std::round(ms*sampleRate*0.001f),16384);
    }
    inline float process(float x){
        buf.setSample(0,w,x);
        float y=0;
        for(int g=0; g<4; ++g){
            // 75% overlap
            phase[g] += (shiftRatio - 1.0f) * (grain/(float)sampleRate) * 1.33f;
            if(phase[g]>=1.0f) phase[g]-=1.0f;
            float win = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase[g]);
            float readBack = phase[g] * (float)grain;

            float rp=(float)w - readBack; const int N=buf.getNumSamples();
            while(rp<0) rp += (float)N; int i=(int)rp; float f=rp-(float)i;

            auto at=[&](int k){ return buf.getSample(0,(k+N)%N); };
            float y0=at(i-1),y1=at(i),y2=at(i+1),y3=at(i+2);
            float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            y += win * (((c3*f+c2)*f+c1)*f+c0);
        }
        if(++w >= buf.getNumSamples()) w=0;
        return y * 0.5f; // normalize
    }
    juce::AudioBuffer<float> buf; int w{0}, grain{4096}; double sampleRate{44100.0}; float phase[4]{0,0,0,0}, shiftRatio{2.0f};
};

struct ShimmerReverb {
    void prepare(double sr,int maxMs){
        sampleRate=sr;
        hall.prepare(sr, maxMs);
        predelay.prepare(sr, 300);
        shL.prepare(sr, 2000); shR.prepare(sr, 2000);
        lp.prepare(sr); lp.setCutoff(12000.0f); // wider for less resonance
        hp.prepare(sr); hp.setCutoff(120.0f);   // lower for more sub content
        fbAmt.reset(sr, 0.12); fbAmt.setCurrentAndTargetValue(0.25f); // reduced for stability
        wetTrim=1.7f; // ~+4.6 dB
    }
    void setParams(float size01,float decaySec,float dampHz,float diffusion,float predelayMs){
        hall.setParams(size01, juce::jlimit(0.2f,20.0f,decaySec), juce::jlimit(0.0f,1.0f,diffusion));
        hall.setDampHz(dampHz);
        predelay.setMs(predelayMs);
        lp.setCutoff(dampHz);
    }
    inline void processSample(float inL,float inR,float& outL,float& outR){
        // Shimmer is an exclusive engine: does not mix Hall/Room externally
        predelay.push(inL,inR);
        const float pdL=predelay.readCh(0), pdR=predelay.readCh(1);

        float hL=0,hR=0;
        // Feed shimmer feedback into hall input for infinite angelic bloom
        hall.processSample(pdL + fbL, pdR + fbR, hL, hR);

        float sL = shL.process(hL);
        float sR = shR.process(hR);

        // tone-shape pitched layer and feed a portion back
        float fL = hp.process( lp.process(sL) );
        float fR = hp.process( lp.process(sR) );
        const float g = fbAmt.getNextValue();
        fbL = g * fL;
        fbR = g * fR;

        // parallel mix: 70% hall core + 30% octave layer (more natural)
        const float mL = 0.7f * hL + 0.3f * fL;
        const float mR = 0.7f * hR + 0.3f * fR;
        outL = rvb::satSoft(wetTrim * mL);
        outR = rvb::satSoft(wetTrim * mR);
    }

    double sampleRate{44100.0};
    HallReverb hall; rvb::PreDelay predelay;
    OctaveUpGrains4 shL, shR; rvb::OnePoleLPF lp; rvb::OnePoleHPF hp;
    juce::SmoothedValue<float> fbAmt; float wetTrim{1.7f};
    float fbL{0.0f}, fbR{0.0f};
};

/*** Mixer wrapper with EXCLUSIVE modes (no crossfade) ***/
struct MultiReverb {
    void prepare(double sr,int /*block*/){
        sampleRate=sr;
        hall.prepare(sr, 3200);
        room.prepare(sr);
        shimmer.prepare(sr, 3200);
        mix.reset(sr, 0.05);
    }
    // type: 0=Hall, 1=Room, 2=Shimmer
    void setParams(float type, float size, float predelayMs, float dampHz, float diffusion, float early, float decaySec, float mix01)
    {
        mode = (int) juce::jlimit(0.0f, 2.0f, type + 0.0001f); // bias to int
        mix.setTargetValue(juce::jlimit(0.0f,1.0f,mix01));

        // Update engines
        hall.setParams(size, decaySec, diffusion);
        hall.setDampHz(dampHz);

        room.setParams(size, decaySec, dampHz, diffusion, early, predelayMs);

        shimmer.setParams(size, decaySec, dampHz, diffusion, predelayMs);
    }
    inline void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int N=buffer.getNumSamples();
        auto* L = buffer.getWritePointer(0);
        auto* R = buffer.getNumChannels()>1 ? buffer.getWritePointer(1) : nullptr;

        for(int n=0;n<N;++n){
            const float inL = L[n];
            const float inR = R ? R[n] : inL;
            float wetL=0, wetR=0;

            switch(mode){
                case 0: { float hL,hR; hall.processSample(inL,inR,hL,hR); wetL=hL; wetR=hR; } break;
                case 1: { float rL,rR; room.processSample(inL,inR,rL,rR); wetL=rL; wetR=rR; } break;
                case 2: { float sL,sR; shimmer.processSample(inL,inR,sL,sR); wetL=sL; wetR=sR; } break;
            }

            const float m = mix.getNextValue();
            L[n] = juce::jmap(m, inL, wetL);
            if(R) R[n] = juce::jmap(m, inR, wetR);
        }
    }

    double sampleRate{44100.0};
    int mode{0};
    HallReverb hall; RoomReverb room; ShimmerReverb shimmer;
    juce::SmoothedValue<float> mix;
};
