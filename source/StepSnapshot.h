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
};
