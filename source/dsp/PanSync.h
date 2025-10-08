// PanSync.h - Correct tempo-sync mapping for AutoPan
#pragma once

enum class Div {
    Bars4, Bars2, Bar, DottedHalf, Half, DottedQuarter, Quarter,
    TripletQuarter, Eighth, DottedEighth, TripletEighth,
    Sixteenth, DottedSixteenth, TripletSixteenth, ThirtySecond, SixtyFourth
};

static inline float quarterNotesPerCycle(Div d)
{
    switch (d)
    {
        case Div::Bars4:            return 16.0f;  // 4 bars in 4/4
        case Div::Bars2:            return 8.0f;   // 2 bars
        case Div::Bar:              return 4.0f;   // 1 bar
        case Div::DottedHalf:       return 3.0f;   // dotted 1/2 = 3 QN
        case Div::Half:             return 2.0f;   // 1/2
        case Div::DottedQuarter:    return 1.5f;   // dotted 1/4
        case Div::Quarter:          return 1.0f;   // 1/4
        case Div::TripletQuarter:   return 2.0f/3.0f;
        case Div::Eighth:           return 0.5f;   // 1/8
        case Div::DottedEighth:     return 0.75f;  // dotted 1/8
        case Div::TripletEighth:    return 1.0f/3.0f;
        case Div::Sixteenth:        return 0.25f;  // 1/16
        case Div::DottedSixteenth:  return 0.375f; // dotted 1/16
        case Div::TripletSixteenth: return 1.0f/6.0f;
        case Div::ThirtySecond:     return 0.125f; // 1/32
        case Div::SixtyFourth:      return 0.0625f; // 1/64
    }
    return 1.0f;
}

static inline float syncedHz(float bpm, Div d)
{
    const float qnHz = bpm / 60.0f;      // 1 cycle per quarter → BPM/60 Hz
    return qnHz / quarterNotesPerCycle(d);
}

// Helper function to get display name for UI
static inline juce::String getDivDisplayName(Div d)
{
    switch (d)
    {
        case Div::Bars4:            return "4 bars";
        case Div::Bars2:            return "2 bars";
        case Div::Bar:              return "1 bar";
        case Div::DottedHalf:       return "1/2.";
        case Div::Half:             return "1/2";
        case Div::DottedQuarter:    return "1/4.";
        case Div::Quarter:          return "1/4";
        case Div::TripletQuarter:   return "1/4T";
        case Div::Eighth:           return "1/8";
        case Div::DottedEighth:     return "1/8.";
        case Div::TripletEighth:    return "1/8T";
        case Div::Sixteenth:        return "1/16";
        case Div::DottedSixteenth:  return "1/16.";
        case Div::TripletSixteenth: return "1/16T";
        case Div::ThirtySecond:     return "1/32";
        case Div::SixtyFourth:      return "1/64";
    }
    return "1/4";
}

// Array of all divisions for easy iteration
static constexpr Div allDivisions[] = {
    Div::Bars4, Div::Bars2, Div::Bar, Div::DottedHalf, Div::Half, 
    Div::DottedQuarter, Div::Quarter, Div::TripletQuarter, 
    Div::Eighth, Div::DottedEighth, Div::TripletEighth,
    Div::Sixteenth, Div::DottedSixteenth, Div::TripletSixteenth, Div::ThirtySecond, Div::SixtyFourth
};

static constexpr int numDivisions = sizeof(allDivisions) / sizeof(allDivisions[0]);
