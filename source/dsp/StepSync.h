#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

struct StepSync
{
    double sampleRate = 44100.0;
    double bpm        = 120.0;

    // Host state per block
    double ppqStart = 0.0;
    double ppqEnd   = 0.0;
    bool   playing  = false;

    // NEW: Sequencer phase origin (PPQ at which step 0 starts)
    double originPPQ   = 0.0;
    bool   wasPlaying  = false;
    bool   resetQueued = false;

    void prepare (double sr) { sampleRate = sr; }

    // Optional external reset (e.g., user pressed "Reset")
    void requestReset() { resetQueued = true; }

    // Fill from host; return false if we cannot sync this block
    bool beginBlock (juce::AudioPlayHead* ph, int numSamples)
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (ph == nullptr || !ph->getCurrentPosition (pos))
            return false;

        playing = pos.isPlaying;
        if (pos.bpm > 1.0) bpm = pos.bpm;

        if (!playing || pos.ppqPosition <= 0.0 || bpm <= 0.0)
        {
            wasPlaying = playing;
            return false;
        }

        // Set/refresh origin when starting playback or when asked
        if (!wasPlaying && playing)        originPPQ = pos.ppqPosition;
        if (resetQueued) { originPPQ = pos.ppqPosition; resetQueued = false; }

        ppqStart = pos.ppqPosition;

        // Δbeats across block = (samples / sr) * (bpm / 60)
        const double blockBeats = (numSamples / sampleRate) * (bpm / 60.0);
        ppqEnd = ppqStart + blockBeats;

        wasPlaying = playing;
        return true;
    }

    // Step index at arbitrary PPQ (origin-relative, not bar-locked)
    static int stepAtPPQ (double ppq, double origin, double stepBeats, int activeSteps)
    {
        const double relBeats = (ppq - origin);                    // in beats
        const double idx      = std::floor (relBeats / juce::jmax (1e-9, stepBeats));
        const int wrap        = juce::jmax (1, activeSteps);
        int result = ((int) idx) % wrap;
        return result < 0 ? result + wrap : result;
    }

    // Iterate all step crossings inside this block (sample-accurate)
    template <typename Callback>
    void emitBoundaries (double stepBeats, int activeSteps, int numSamples, Callback cb) const
    {
        // first boundary >= ppqStart
        const double startIdx   = std::floor ((ppqStart - originPPQ) / stepBeats);
        const double firstBound = originPPQ + (startIdx + 1.0) * stepBeats;

        if (ppqEnd <= firstBound) return;

        const double totalBeats = juce::jmax (1e-9, ppqEnd - ppqStart);

        for (double b = firstBound; b < ppqEnd + 1e-9; b += stepBeats)
        {
            const double alpha = (b - ppqStart) / totalBeats;     // 0..1 within block
            const int offset   = (int) juce::jlimit (0.0, (double) numSamples - 1,
                                                     std::round (alpha * numSamples));
            const int newStep  = stepAtPPQ (b, originPPQ, stepBeats, activeSteps);
            cb (offset, newStep);
        }
    }
};