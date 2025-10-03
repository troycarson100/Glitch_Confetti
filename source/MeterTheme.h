#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Meter theme colors and thresholds
namespace MeterTheme {
    // Exact green from mock (soft mint)
    static constexpr uint32_t kGreenHex  = 0xFFA9E9A2;
    static constexpr uint32_t kYellowHex = 0xFFF5D266;
    static constexpr uint32_t kRedHex    = 0xFFFF6B6B;

    static inline juce::Colour green()  { return juce::Colour(kGreenHex);  }
    static inline juce::Colour yellow() { return juce::Colour(kYellowHex); }
    static inline juce::Colour red()    { return juce::Colour(kRedHex);    }

    // dB thresholds
    static constexpr float kYellowStartDb = -12.0f; // below = green
    static constexpr float kRedStartDb    =  -3.0f; // above = red
    static constexpr float kFloorDb       = -60.0f; // meter bottom
}

