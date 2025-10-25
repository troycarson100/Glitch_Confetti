#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "SimpleResonator.h"
#include "../../StepSnapshot.h"

// GlassBackend: main DSP facade for Glass effect
// Zero external dependencies, guaranteed to build

class GlassBackend {
public:
    void prepare(double sampleRate, int blockSize, int numChannels);
    void reset();
    
    // Main processing: reads params from snapshot, generates exciter, processes
    void processBlock(juce::AudioBuffer<float>& buffer, 
                     int numSamples,
                     const StepSnapshot& snapshot,
                     double sampleRate,
                     bool stepEdge = false);
    
    bool isPrepared() const { return prepared; }
    
private:
    SimpleResonator resonator;
    
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    int numChannels = 2;
    bool prepared = false;
    
    // Buffers
    juce::AudioBuffer<float> exciterBuffer;
    juce::AudioBuffer<float> wetBuffer;
    
    // Exciter HPF for brightness
    juce::IIRFilter exciterHPF;
    
    // Heartbeat counter for logging
    int heartbeatCounter = 0;
    
    // Build exciter signal
    void buildExciter(juce::AudioBuffer<float>& ioBuffer, int numSamples, float strike, bool stepEdge);
};


