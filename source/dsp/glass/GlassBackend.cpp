#include "GlassBackend.h"
#include "GlassBuildFlags.h"

void GlassBackend::prepare(double sr, int block, int ch)
{
    sampleRate = (sr > 0 ? sr : 48000.0);
    maxBlockSize = block;
    numChannels = ch;
    
    // Prepare resonator
    resonator.prepare(sampleRate, maxBlockSize, numChannels);
    
    // Prepare buffers
    exciterBuffer.setSize(1, maxBlockSize, false, false, true);
    wetBuffer.setSize(numChannels, maxBlockSize, false, false, true);
    
    // Exciter HPF (~5kHz for brightness)
    exciterHPF.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 5000.0, 0.7));
    
    prepared = true;
    
    // File logging to ensure we see this
    juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
    logFile.appendText("[Glass] Backend prepared: sr=" + juce::String(sampleRate) + " block=" + juce::String(maxBlockSize) + " ch=" + juce::String(numChannels) + "\n");
    
    DBG("[Glass] Backend prepared: sr=" << sampleRate << " block=" << maxBlockSize << " ch=" << numChannels);
}

void GlassBackend::reset()
{
    resonator.reset();
    exciterHPF.reset();
    exciterBuffer.clear();
    wetBuffer.clear();
    heartbeatCounter = 0;
}

void GlassBackend::processBlock(juce::AudioBuffer<float>& buffer, 
                                int numSamples,
                                const StepSnapshot& snapshot,
                                double /*sr*/,
                                bool stepEdge)
{
    // Always log when processBlock is called
    static int processCallCount = 0;
    if ((++processCallCount % 30) == 0) {
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
        logFile.appendText("[Glass] Backend processBlock called: prepared=" + juce::String(prepared ? 1 : 0) + 
                          " N=" + juce::String(numSamples) + " max=" + juce::String(maxBlockSize) + "\n");
    }
    
    if (!prepared || numSamples <= 0 || numSamples > maxBlockSize) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
            logFile.appendText("[Glass] Backend bypass: prepared=" + juce::String(prepared ? 1 : 0) + 
                              " N=" + juce::String(numSamples) + " max=" + juce::String(maxBlockSize) + "\n");
            loggedOnce = true;
        }
        return;
    }
    
    juce::ScopedNoDenormals _;
    const int N = numSamples;
    const int chN = juce::jmax(1, buffer.getNumChannels());
    
    // Heartbeat logging
    if ((++heartbeatCounter % 30) == 0) {
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
        logFile.appendText("[Glass] Backend heartbeat N=" + juce::String(N) + " ch=" + juce::String(chN) + "\n");
    }
    
    // Read Glass parameters from snapshot
    const float pitchSt = juce::jlimit(-24.f, 24.f, snapshot.glass.pitchSemitones);
    const float f0Hz = juce::jlimit(20.f, 12000.f, 440.0f * std::pow(2.0f, pitchSt / 12.0f));
    const float brightness = juce::jlimit(0.f, 1.f, snapshot.glass.brightness);
    const float decaySec = juce::jlimit(0.05f, 4.0f, snapshot.glass.decaySec);
    const float strike = juce::jlimit(0.f, 1.f, snapshot.glass.strike);
    const float spread = juce::jlimit(0.f, 1.f, snapshot.glass.spread);
    const float mix = juce::jlimit(0.f, 1.f, snapshot.glass.mix);
    
    // Map decay to damping (logarithmic: longer decay → less damping)
    const float damping = juce::jlimit(0.f, 1.f,
        juce::jmap(std::log(decaySec), std::log(0.05f), std::log(4.0f), 0.85f, 0.15f));
    
    // Configure resonator
    SimpleResonatorParams params;
    params.f0Hz = f0Hz;
    params.brightness = brightness;
    params.damping = damping;
    params.spread = spread;
    resonator.setParams(params);
    
    // Build exciter
    buildExciter(buffer, N, strike, stepEdge);
    
    // Process through resonator
    wetBuffer.setSize(chN, N, false, false, true);
    wetBuffer.clear();
    
    resonator.process(exciterBuffer.getReadPointer(0), N,
                     wetBuffer.getWritePointer(0),
                     chN > 1 ? wetBuffer.getWritePointer(1) : wetBuffer.getWritePointer(0));
    
    // RMS logging
    if ((heartbeatCounter % 30) == 0) {
        const float wetRMS = wetBuffer.getRMSLevel(0, 0, N);
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
        logFile.appendText("[Glass] Backend RMS wet=" + juce::String(wetRMS, 6) + "\n");
    }
    
    // Mix (or force 100% wet while testing)
    #if GLASS_DEBUG_HARNESS
    {
        // Force 100% wet for testing audibility
        for (int ch = 0; ch < chN; ++ch) {
            buffer.copyFrom(ch, 0, wetBuffer, ch, 0, N);
        }
        if ((heartbeatCounter % 30) == 0) {
            juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
            logFile.appendText("[Glass] Debug harness: forcing 100% wet (mix ignored)\n");
        }
    }
    #else
    {
        // Constant-power dry/wet mix
        const float a = std::cos(0.5f * juce::MathConstants<float>::pi * mix);
        const float b = std::sin(0.5f * juce::MathConstants<float>::pi * mix);
        
        for (int ch = 0; ch < chN; ++ch) {
            auto* out = buffer.getWritePointer(ch);
            const auto* wet = wetBuffer.getReadPointer(ch);
            for (int n = 0; n < N; ++n) {
                out[n] = a * out[n] + b * wet[n];
            }
        }
    }
    #endif
}

void GlassBackend::buildExciter(juce::AudioBuffer<float>& ioBuffer, int numSamples, float strike, bool stepEdge)
{
    exciterBuffer.setSize(1, numSamples, false, false, true);
    float* exc = exciterBuffer.getWritePointer(0);
    juce::FloatVectorOperations::clear(exc, numSamples);
    
    #if GLASS_DEBUG_HARNESS
    {
        // Debug harness: inject test tone + first-block impulse
        
        // 1 kHz test tone at -18 dBFS
        static double testPhase = 0.0;
        const double testInc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / sampleRate;
        for (int n = 0; n < numSamples; ++n) {
            exc[n] += 0.125f * std::sin(static_cast<float>(testPhase));
            testPhase += testInc;
            if (testPhase > 2.0 * juce::MathConstants<double>::pi) {
                testPhase -= 2.0 * juce::MathConstants<double>::pi;
            }
        }
        
        // One-time impulse on first block
        static bool sentImpulse = false;
        if (!sentImpulse && numSamples > 0) {
            exc[0] += 0.5f;
            sentImpulse = true;
            juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("glass_debug.log");
            logFile.appendText("[Glass] Debug harness: injected test tone + impulse\n");
        }
    }
    #else
    {
        // Production mode: step-gated noise burst OR input-resonate fallback
        
        if (stepEdge) {
            // Generate 1ms attack / 8ms decay noise burst
            juce::Random rand;
            const int attackSamples = static_cast<int>(0.001 * sampleRate);
            const int decaySamples = static_cast<int>(0.008 * sampleRate);
            const int totalSamples = juce::jmin(attackSamples + decaySamples, numSamples);
            
            for (int n = 0; n < totalSamples; ++n) {
                float env;
                if (n < attackSamples) {
                    env = static_cast<float>(n) / attackSamples;
                } else {
                    const float t = static_cast<float>(n - attackSamples) / decaySamples;
                    env = std::exp(-5.0f * t);
                }
                exc[n] = (rand.nextFloat() * 2.0f - 1.0f) * env * strike;
            }
            
            // HPF for brightness
            exciterHPF.processSamples(exc, numSamples);
        } else {
            // Input-resonate fallback: quiet HPF'd input keeps it audible
            const int chN = juce::jmax(1, ioBuffer.getNumChannels());
            const auto* L = ioBuffer.getReadPointer(0);
            const auto* R = (chN > 1) ? ioBuffer.getReadPointer(1) : nullptr;
            
            for (int n = 0; n < numSamples; ++n) {
                float src = L[n] * 0.5f + (R ? R[n] * 0.5f : 0.f);
                exc[n] = std::tanh(src * strike * 0.15f);
            }
            
            // HPF
            exciterHPF.processSamples(exc, numSamples);
        }
    }
    #endif
}


