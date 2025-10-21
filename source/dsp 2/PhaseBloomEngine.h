#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

/**
 * PhaseBloomEngine - Lush stereo phaser with harmonic enrichment and "blooming" stereo character
 * 
 * Features:
 * - Stereo synced phaser using juce::dsp::Phaser
 * - Host BPM sync for rate parameter
 * - Harmonic enrichment with saturation
 * - Stereo spread control for phase offset between L/R
 * - 8 parameters: Depth, Rate, Feedback, Center, Bloom, Spread, Resonance, Mix
 */
class PhaseBloomEngine
{
public:
    PhaseBloomEngine();
    ~PhaseBloomEngine() = default;
    
    // DSP lifecycle
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, double hostBPM);
    
    // Parameter setters
    void setDepth(float depth);           // 0.0 to 1.0
    void setRate(float rate);             // 0.0 to 1.0 (maps to tempo divisions)
    void setFeedback(float feedback);     // -1.0 to +1.0
    void setCenter(float center);         // 100-4000 Hz
    void setBloom(float bloom);           // 0.0 to 1.0 (harmonic enrichment)
    void setSpread(float spread);         // 0.0 to 1.0 (stereo phase offset)
    void setResonance(float resonance);   // 0.0 to 1.0 (Q factor)
    void setMix(float mix);               // 0.0 to 1.0 (dry/wet)
    void setEnabled(bool enabled);
    
    // Rate conversion helpers
    static float rateToHz(float rateValue, double hostBPM);
    static juce::String getRateLabel(float rateValue);
    
    // Bloom effect helpers
    void updateBloomDelays(float bloomAmount, double sampleRate, int slot);
    void processBloomBlock(juce::AudioBuffer<float>& buffer, int slot);
    
    // Resonance control helper
    void updatePhaserResonance(float resonanceValue, int slot);
    
private:
    // JUCE DSP phaser instances for left and right channels (per-slot)
    static constexpr int NUM_SLOTS = 4;
    juce::dsp::Phaser<float> phaserL[NUM_SLOTS];
    juce::dsp::Phaser<float> phaserR[NUM_SLOTS];
    
    // Multi-tap diffusion delays for Bloom effect (4 delays per channel)
    static constexpr int NUM_BLOOM_DELAYS = 4;
    juce::dsp::DelayLine<float> bloomDelayL[NUM_SLOTS][NUM_BLOOM_DELAYS];
    juce::dsp::DelayLine<float> bloomDelayR[NUM_SLOTS][NUM_BLOOM_DELAYS];
    
    // Smoothed delay times for each bloom delay (prevents clicks)
    juce::SmoothedValue<float> bloomDelayTimes[NUM_SLOTS][NUM_BLOOM_DELAYS];
    
    // Base delay offsets in milliseconds for diffusion network
    static constexpr float BLUR_DELAY_OFFSETS[NUM_BLOOM_DELAYS] = { 7.0f, 11.0f, 13.0f, 17.0f };
    
    // LFO state for stereo spread
    float lfoPhase = 0.0f;
    double sampleRate = 44100.0;
    
    // Parameters (smoothed for zipper-free operation)
    juce::SmoothedValue<float> depth;
    juce::SmoothedValue<float> rate;
    juce::SmoothedValue<float> feedback;
    juce::SmoothedValue<float> center;
    juce::SmoothedValue<float> bloom;
    juce::SmoothedValue<float> spread;
    juce::SmoothedValue<float> resonance;
    juce::SmoothedValue<float> mix;
    
    // State
    bool isEnabled = false;
    
    // Tempo-synced rate divisions (beat divisions) - corrected for proper musical timing
    static constexpr float RATE_DIVISIONS[9] = {
        4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f
    };
    
    static constexpr const char* RATE_LABELS[9] = {
        "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"
    };
};
