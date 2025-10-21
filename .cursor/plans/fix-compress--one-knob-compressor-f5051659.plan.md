<!-- f5051659-6149-4ed3-893a-b4ba45684874 7a433aba-486f-4e94-ab70-bbb8384e064a -->
# Fix Jumpy Gain Reduction Meter with Professional Smoothing

## Root Cause
The meter appears jumpy because the main `PluginEditor` timer runs at only 10Hz (100ms), updating the gain reduction value just 10 times per second. Even though the meter has its own 30Hz timer, it only repaints with stale data from the slow 10Hz updates.

## Solution Strategy

### 1. Increase Main UI Timer Frequency
**File**: `source/PluginEditor.cpp` line 255

Change from:
```cpp
startTimer(100); // Update every 100ms for smoother knob interaction
```

To:
```cpp
startTimer(16); // ~60Hz (16ms) for ultra-smooth UI updates
```

This ensures gain reduction values update 60 times per second, eliminating the 10Hz stuttering.

### 2. Replace LinearSmoothedValue with Proper Exponential Ballistics
**File**: `source/PluginEditor.h` lines 296-394

The current approach uses `LinearSmoothedValue` with `reset()` calls that recreate smoothers, causing discontinuities. Replace with proper exponential ballistics using first-order lowpass filters.

**Remove**: All three `LinearSmoothedValue` variables and the complex multi-stage pipeline.

**Add**: Exponential ballistics with asymmetric attack/release:
```cpp
private:
    // Exponential ballistics for smooth movement
    float currentDisplayValue = 0.0f;
    float targetValue = 0.0f;
    
    // Time constants for professional VU-style ballistics
    // Very smooth release for butter-smooth decay
    const float attackTimeMs = 10.0f;    // Fast attack (10ms)
    const float releaseTimeMs = 500.0f;  // Very slow release (500ms)
    
    // Calculated coefficients (set in constructor)
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // Peak hold for transient display
    float peakValue = 0.0f;
    float peakHoldCounter = 0.0f;
    const float peakHoldTimeMs = 1000.0f; // Hold peaks for 1 second
```

### 3. Implement Exponential Smoothing in Constructor
**File**: `source/PluginEditor.h` GainReductionMeter constructor

Replace the current initialization with:
```cpp
GainReductionMeter() 
{
    // Calculate exponential coefficients for ballistics
    // Using formula: coeff = exp(-1.0 / (timeMs * updateHz / 1000.0))
    const float updateHz = 60.0f; // 60Hz update rate
    attackCoeff = std::exp(-1.0f / (attackTimeMs * updateHz / 1000.0f));
    releaseCoeff = std::exp(-1.0f / (releaseTimeMs * updateHz / 1000.0f));
    
    startTimerHz(60); // 60Hz for smooth visual updates
}
```

### 4. Simplify setGainReduction Method
**File**: `source/PluginEditor.h` line 343-348

Replace with:
```cpp
void setGainReduction(float newDb)
{
    targetValue = juce::jlimit(0.0f, 30.0f, newDb);
}
```

No smoothing here - just store the target. All smoothing happens in timerCallback.

### 5. Rewrite timerCallback with Exponential Ballistics
**File**: `source/PluginEditor.h` lines 350-382

Replace entire timerCallback with proper exponential smoothing:
```cpp
void timerCallback() override
{
    // Apply asymmetric exponential smoothing (VU-style ballistics)
    float coeff;
    if (targetValue > currentDisplayValue) {
        // Fast attack for rises
        coeff = attackCoeff;
    } else {
        // Very slow release for smooth decay
        coeff = releaseCoeff;
    }
    
    // Exponential smoothing: y[n] = coeff * y[n-1] + (1-coeff) * x[n]
    currentDisplayValue = coeff * currentDisplayValue + (1.0f - coeff) * targetValue;
    
    // Update peak hold with smooth decay
    if (targetValue > peakValue) {
        peakValue = targetValue;
        peakHoldCounter = peakHoldTimeMs;
    } else {
        peakHoldCounter = juce::jmax(0.0f, peakHoldCounter - (1000.0f / 60.0f));
        if (peakHoldCounter <= 0.0f) {
            // Smooth peak decay
            peakValue = juce::jmax(currentDisplayValue, peakValue * 0.98f);
        }
    }
    
    repaint();
}
```

### 6. Update paint() Method
**File**: `source/PluginEditor.h` lines 319-341

Replace with simpler, more direct rendering:
```cpp
void paint(juce::Graphics& g) override
{
    auto bounds = getLocalBounds().toFloat();
    const float cornerRadius = 2.0f;
    
    // Background (grey)
    g.setColour(juce::Colour(0xFF666666));
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    // Gain reduction bar (orange) - use smoothed value directly
    if (currentDisplayValue > 0.01f) {
        float normalized = currentDisplayValue / 30.0f; // 0-30dB range
        float fillWidth = bounds.getWidth() * normalized;
        fillWidth = juce::jlimit(0.0f, bounds.getWidth(), fillWidth);
        
        auto fillRect = bounds.removeFromRight(fillWidth);
        g.setColour(juce::Colour(0xFFE96A3E)); // Orange #E96A3E
        g.fillRoundedRectangle(fillRect, cornerRadius);
    }
}
```

Remove the logarithmic curve mapping - exponential ballistics already provide natural visual smoothing.

## Expected Results

- **60Hz Updates**: Both data source (main timer) and display (meter timer) run at 60Hz, eliminating stuttering
- **Exponential Ballistics**: Proper first-order lowpass filtering creates mathematically smooth curves
- **Very Smooth Decay**: 500ms release time creates butter-smooth, professional VU-style movement
- **Fast Attack**: 10ms attack ensures responsive compression indication
- **No Discontinuities**: Single-stage exponential smoothing eliminates the jarring reset() calls
- **Professional Feel**: Matches the smoothness of high-end hardware compressor meters

## Implementation Order

1. Update main timer frequency in PluginEditor.cpp
2. Replace GainReductionMeter class variables and methods in PluginEditor.h
3. Build and test for smooth meter behavior

### To-dos

- [ ] Add juce::SmoothedValue<float> smoothedGain member to CompressEngine.h
- [ ] Initialize smoothedGain in prepare() method with 5ms smoothing time
- [ ] Rewrite processCompressor() with proper threshold mapping, ratio curve, soft knee, and gain smoothing
- [ ] Update attack/release times to 5ms/100ms for musical response
- [ ] Test compressor at various knob positions for smooth, musical compression