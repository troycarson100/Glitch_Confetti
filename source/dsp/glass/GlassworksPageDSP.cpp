#include "GlassworksPageDSP.h"
#include "../../StepSnapshot.h"
#include "RingsEngine.h"

// Debug logging helpers
namespace GlassLog {
    void msg(const juce::String& s) {
        DBG("[Glass] " + s);
        // Also write to file for terminal viewing
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
        logFile.appendText("[Glass] " + s + "\n");
    }
    
    void err(const juce::String& s) {
        DBG("[Glass:ERR] " + s);
        // Also write to file for terminal viewing
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
        logFile.appendText("[Glass:ERR] " + s + "\n");
    }
}

GlassworksPageDSP::GlassworksPageDSP()
{
    fprintf(stderr, "[Glass] >>>>>>> CONSTRUCTOR CALLED v2.0 <<<<<<<\n");
    fflush(stderr);
    GlassLog::msg("!!! GlassworksPageDSP constructor v2.0 - Snapshot-based !!!");
    fprintf(stderr, "[Glass] Constructor completed\n");
    fflush(stderr);
}

void GlassworksPageDSP::prepare(double sampleRate, int blockSize, int numChannels)
{
    // ABSOLUTE FIRST LINE - use fprintf to bypass any potential logging system issues
    fprintf(stderr, "[Glass] >>> prepare() FIRST LINE REACHED <<< sr=%f\n", sampleRate);
    fflush(stderr);
    
    // FIRST LINE - log before anything else
    DBG("[GlassworksPageDSP] === prepare() ENTRY v2.0 === sr=" << sampleRate);
    
    fprintf(stderr, "[Glass] DBG macro executed\n");
    fflush(stderr);
    
    GlassLog::msg("!!! prepare() CALLED - v2.0 !!!");
    
    fprintf(stderr, "[Glass] GlassLog executed\n");
    fflush(stderr);
    
    this->sampleRate = sampleRate;
    this->maxBlockSize = blockSize;
    this->numChannels = numChannels;
    
    fprintf(stderr, "[Glass] About to call rings.prepare()\n");
    fflush(stderr);
    
    DBG("[GlassworksPageDSP] About to call rings.prepare()...");
    
    // Prepare Rings engine
    rings.prepare(sampleRate, blockSize, numChannels);
    
    fprintf(stderr, "[Glass] rings.prepare() returned\n");
    fflush(stderr);
    
    DBG("[GlassworksPageDSP] rings.prepare() returned");
    
    // Prepare temporary buffers
    tempExciterBuffer.setSize(1, blockSize, false, false, true);
    wetBuffer.setSize(numChannels, blockSize, false, false, true);
    
    // Prepare parameter smoothing
    smStrike.reset(sampleRate, 0.015f);
    smMix.reset(sampleRate, 0.015f);
    smBrightness.reset(sampleRate, 0.015f);
    
    // Prepare DC blockers
    dcBlockerL.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 20.0f, 0.7f));
    dcBlockerR.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 20.0f, 0.7f));
    
    // Prepare exciter ADSR
    adsrParams.attack = 0.001f;  // 1ms
    adsrParams.decay = 0.008f;   // 8ms
    adsrParams.sustain = 0.0f;   // 0
    adsrParams.release = 0.001f; // 1ms
    exciterADSR.setParameters(adsrParams);
    // Note: ADSR.prepare() not available in this JUCE version
    
    // Prepare dry delay buffers for latency compensation
    dryDelayBuffers.resize(numChannels);
    dryDelayWriteHeads.resize(numChannels);
    dryDelayLatencySamples = rings.getLatencySamples();
    
    for (int ch = 0; ch < numChannels; ++ch) {
        dryDelayBuffers[ch].resize(dryDelayLatencySamples, 0.0f);
        dryDelayWriteHeads[ch] = 0;
    }
    
    prepared = true;
    
    DBG("[GlassworksPageDSP] === prepare() COMPLETE === prepared=true");
    GlassLog::msg("prepare OK sr=" + juce::String(sampleRate) + " block=" + juce::String(blockSize) + " ch=" + juce::String(numChannels));
}

void GlassworksPageDSP::processStep(int stepIndex, const StepSnapshot& snapshot, 
                                  juce::AudioBuffer<float>& buffer, int numSamples, 
                                  int /*channel*/, double /*bpm*/, bool /*stepEdge*/)
{
    if (!prepared || numSamples <= 0 || numSamples > maxBlockSize) {
        return;
    }

    juce::ScopedNoDenormals _;
    const int N = numSamples;
    const int chN = juce::jmax(1, buffer.getNumChannels());
    
    // Read parameters from snapshot
    const float pitchSt = juce::jlimit(-24.f, 24.f, snapshot.glass.pitchSemitones);
    const float bright = juce::jlimit(0.f, 1.f, snapshot.glass.brightness);
    const float decay = juce::jlimit(0.05f, 4.0f, snapshot.glass.decaySec);
    const float strike = juce::jlimit(0.f, 1.f, snapshot.glass.strike);
    const float mixVal = juce::jlimit(0.f, 1.f, snapshot.glass.mix);

    // Map pitch to frequency
    const float f0 = juce::jlimit(20.f, 12000.f, 440.0f * std::pow(2.0f, pitchSt/12.0f));

    // Map decay to damping (inverted)
    const float damping = juce::jlimit(0.f, 1.f,
        juce::jmap(std::log(decay), std::log(0.05f), std::log(4.0f), 0.85f, 0.15f));

    // TODO: Fix RingsParams build issue - Glass disabled for now
    // Just pass audio through unchanged until we fix the compilation errors
    (void)f0;
    (void)bright;
    (void)damping;
    (void)strike;
    (void)mixVal;
    
    // Audio passes through unprocessed
}

void GlassworksPageDSP::reset()
{
    rings.reset();
    exciterADSR.reset();
    
    // Clear all buffers
    tempExciterBuffer.clear();
    wetBuffer.clear();
    
    // Reset parameter smoothing
    smStrike.reset(sampleRate, 0.015f);
    smMix.reset(sampleRate, 0.015f);
    smBrightness.reset(sampleRate, 0.015f);
    
    // Reset DC blockers
    dcBlockerL.reset();
    dcBlockerR.reset();
    
    // Clear dry delay buffers
    for (auto& buffer : dryDelayBuffers) {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    
    GlassLog::msg("reset() called");
}
