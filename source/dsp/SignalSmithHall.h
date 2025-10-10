#pragma once
#include <juce_dsp/juce_dsp.h>

namespace ss {
    inline float sat(float x){ return std::tanh(0.7f*x); }
    static constexpr float LN1000 = 6.90775527898f;

    struct OnePoleLPF {
        void prepare(double sr){ sampleRate=sr; z=0; setCutoff(9000.0f); }
        void setCutoff(float hz){
            cutHz = juce::jlimit(20.0f, 20000.0f, hz);
            const float a1 = std::exp(-juce::MathConstants<float>::twoPi*cutHz/(float)sampleRate);
            a=a1; b=1.0f-a1;
        }
        inline float process(float x){ z = a*z + b*x; return z; }
        double sampleRate{44100.0}; float cutHz{9000.0f}, a{0}, b{1}, z{0};
    };

    struct Householder8 {
        inline void process(float* v) const {
            float s=0.0f; for(int i=0;i<8;++i) s += v[i];
            float c = s * 0.25f;
            for(int i=0;i<8;++i) v[i] -= c;
        }
    };
}

struct SimpleFDN8Hall {
    static constexpr int N=8;
    
    void prepare(double sr, int maxMs){
        sampleRate=sr;
        const int maxSamps = (int)std::ceil(sr*maxMs/1000.0)+32;
        for(int i=0;i<N;++i){
            line[i].setSize(1, maxSamps); line[i].clear(); widx[i]=0;
            targetMs[i]=baseTemplate[i];
            smMs[i].reset(sr, 0.08);
            smMs[i].setCurrentAndTargetValue(targetMs[i]);
        }
        damp.prepare(sr);
        wetTrim = 4.0f;
    }

    void setParams(float size01, float decaySec, float dampHz)
    {
        size  = juce::jlimit(0.1f,1.5f,size01);
        decay = juce::jlimit(0.2f,20.0f,decaySec);
        damp.setCutoff(dampHz);

        for(int i=0;i<N;++i){
            targetMs[i] = baseTemplate[i]*size;
            smMs[i].setTargetValue(targetMs[i]);
            const float D = targetMs[i]*0.001f;
            float g = std::exp(-ss::LN1000 * (D / decay));
            fb[i] = juce::jlimit(0.2f, 0.99995f, g);
        }
    }

    inline void process(float in, float& outL, float& outR)
    {
        float v[N];
        for(int i=0;i<N;++i){
            const float ms = smMs[i].getNextValue();
            float dS = ms*0.001f*(float)sampleRate;
            float rp = (float)widx[i] - dS; const int Nb=line[i].getNumSamples();
            while(rp<0) rp += (float)Nb; int idx=(int)rp; float f=rp-(float)idx;
            auto at=[&](int k){ return line[i].getSample(0,(k+Nb)%Nb); };
            float y0=at(idx-1),y1=at(idx),y2=at(idx+1),y3=at(idx+2);
            float c0=y1,c1=0.5f*(y2-y0),c2=y0-2.5f*y1+2.0f*y2-0.5f*y3,c3=0.5f*(y3-y0)+1.5f*(y1-y2);
            v[i] = ((c3*f+c2)*f+c1)*f+c0;
        }

        house.process(v);

        for(int i=0;i<N;++i){
            const float xin = in + fb[i]*v[i];
            line[i].setSample(0, widx[i], ss::sat(xin));
        }
        if(++widx[0] >= line[0].getNumSamples()){
            for(int i=0;i<N;++i) if(++widx[i] >= line[i].getNumSamples()) widx[i]=0;
        }

        float dampL = damp.process(v[0]+v[2]+v[4]+v[6]);
        float dampR = damp.process(v[1]+v[3]+v[5]+v[7]);
        float dampBlend = juce::jmap(decay, 0.2f, 20.0f, 1.0f, 0.1f);
        float rawL = v[0]+v[2]+v[4]+v[6];
        float rawR = v[1]+v[3]+v[5]+v[7];
        float L = rawL + dampBlend*(dampL - rawL);
        float R = rawR + dampBlend*(dampR - rawR);
        
        outL = ss::sat(wetTrim * L * 0.25f);
        outR = ss::sat(wetTrim * R * 0.25f);
    }

    double sampleRate{44100.0};
    juce::AudioBuffer<float> line[N]; int widx[N]{};
    float targetMs[N]{}, fb[N]{}, size{0.9f}, decay{6.0f}, wetTrim{4.0f};
    juce::SmoothedValue<float> smMs[N];
    ss::OnePoleLPF damp;
    ss::Householder8 house;
    const float baseTemplate[N] = { 43.7f, 53.1f, 61.8f, 74.5f, 89.2f, 105.4f, 122.7f, 144.0f };
};

struct SignalSmithHall {
    void prepare(double sr, int maxMs){
        sampleRate=sr;
        fdn.prepare(sr, maxMs);
        mix.reset(sr, 0.03);
        width.reset(sr, 0.03);
    }

    void setParams(float size, float decaySec, float dampHz, float diffusion01, float early01, float width01, float preMs, float mix01)
    {
        fdn.setParams(size, decaySec, dampHz);
        mix.setTargetValue(juce::jlimit(0.0f,1.0f,mix01));
        width.setTargetValue(juce::jlimit(0.0f,1.0f,width01));
    }

    inline void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int N = buffer.getNumSamples();
        auto* L = buffer.getWritePointer(0);
        auto* R = buffer.getNumChannels()>1 ? buffer.getWritePointer(1) : nullptr;

        for(int n=0;n<N;++n){
            float inL = L[n];
            float inR = R ? R[n] : inL;

            float tailL=0, tailR=0;
            fdn.process(0.5f*(inL+inR), tailL, tailR);

            float mid  = 0.5f*(tailL + tailR);
            float side = 0.5f*(tailL - tailR);
            side *= width.getNextValue();
            float wetL = mid + side;
            float wetR = mid - side;

            const float m = mix.getNextValue();
            L[n] = juce::jmap(m, inL, wetL);
            if(R) R[n] = juce::jmap(m, inR, wetR);
        }
    }

    double sampleRate{44100.0};
    SimpleFDN8Hall fdn;
    juce::SmoothedValue<float> mix, width;
    float diffuserTotalMs{240.0f};
};
