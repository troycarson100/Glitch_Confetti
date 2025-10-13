#pragma once

// Step snapshot for DSP parameters - supports both Delay and AutoPan
struct StepSnapshot {
    struct {
        float timeMs = 250.0f;
        float feedback = 20.0f;
        float wowDepth = 0.0f;
        float wowRate = 1.0f;
        float saturation = 0.0f;  // "Drive" knob
        float highCut = 20000.0f;
        float lowCut = 20.0f;
        float mix = 50.0f;
    } delay;
    
    struct {
        float rate = 0.43f;        // 0-1 normalized (maps to divisions in sync mode)
        float phase = 180.0f;      // 0-360 degrees
        int waveType = 0;          // 0=Sine, 1=Triangle, 2=RampDown, 3=RampUp, 4=Random
        float waveShape = 0.5f;    // 0-1
        bool inverted = false;     // false=Normal, true=Inverted
        float amount = 0.5f;       // 0-1 depth/amount
    } autopan;
    
    struct {
        float drive = 12.0f;       // 0-36 dB
        float color = 0.0f;        // -1 to +1 (pre-emphasis tilt)
        float asym = 0.0f;         // -1 to +1 (bias/asymmetry)
        float texture = 0.35f;     // 0-1 (curve hardness)
        float lowCut = 60.0f;      // 20-300 Hz
        float highCut = 12000.0f;  // 3k-22k Hz
        float tone = 0.0f;         // -1 to +1 (post tilt)
        float mix = 1.0f;          // 0-1 (dry/wet)
    } dirt;
    
    struct {
        float delayTime = 18.0f;   // 5-50 ms (base delay)
        float rate = 0.8f;         // 0.02-8 Hz (LFO rate)
        float depth = 5.0f;        // 0-12 ms (modulation amplitude)
        float feedback = 0.15f;    // 0-0.9 (feedback amount)
        float voices = 4.0f;       // 2-8 (number of voices)
        float width = 0.85f;       // 0-1 (stereo width)
        float tone = 0.25f;        // 0-1 (wave shape: 0=sin, 0.5=tri, 1=soft square)
        float mix = 0.5f;          // 0-1 (dry/wet mix)
    } chorus;
    
    struct {
        float type = 0.0f;         // 0-2 (0=Hall, 1=Room, 2=Shimmer)
        float size = 0.7f;         // 0.1-1.5 (room size/decay time)
        float predelayMs = 20.0f;  // 0-200 ms (predelay time)
        float dampHz = 8000.0f;    // 1000-20000 Hz (HF damping)
        float diffusion = 0.7f;    // 0-1 (diffusion amount)
        float early = 0.35f;       // 0-1 (early reflections level - Room mode)
        float decaySec = 4.0f;     // 0.2-20s (RT60 decay time)
        float mix = 0.25f;         // 0-1 (dry/wet mix)
    } reverb;
    
    struct {
        float sizeMs = 80.0f;      // 5-200 ms (grain size) - longer for smoothness
        float densityHz = 8.0f;    // 1-40 Hz (grains per second) - lower for clarity
        float position = 1.0f;     // 0-1 (buffer position: 0=oldest, 1=latest)
        float sprayMs = 20.0f;     // 0-200 ms (position randomization) - less spray
        float pitchSemi = 0.0f;    // -24 to +24 semitones (pitch shift)
        float random = 0.15f;      // 0-1 (global randomization) - less chaos
        float texture = 0.2f;      // 0-1 (window morph: Hann→Blackman→Rect) - smoother
        float mix = 0.5f;          // 0-1 (dry/wet mix) - start at 50%
    } granular;
    
    struct {
        float pattern = 0.0f;      // 0-7 (pattern index)
        float division = 3.0f;     // 0-5 (division index: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32)
        float offset = 0.5f;       // 0-1 (bipolar: 0.5=center, 0=early, 1=late)
        float shape = 0.5f;        // 0-1 (envelope curvature)
        float releaseMs = 20.0f;   // 5-80 ms (crossfade/tail duration)
        float mix = 0.75f;         // 0-1 (wet/dry mix)
    } slicer;
    
    struct {
        float timeMs = 450.0f;     // 1-2000 ms (delay time)
        float feedback = 0.45f;    // 0-0.98 (feedback amount)
        float toneHz = 6500.0f;    // 200-20000 Hz (tone LPF cutoff)
        float drive = 0.15f;       // 0-1 (pre-delay soft clip)
        bool pingPong = true;      // Ping-pong mode
        float wowFlutter = 0.35f;  // 0-1 (wow/flutter depth)
        float regenDamp = 0.25f;   // 0-1 (regen damping)
        float mix = 0.35f;         // 0-1 (dry/wet mix)
    } dubdelay;
};
