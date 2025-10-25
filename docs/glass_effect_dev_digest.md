# Glass Effect Development Digest

## Issue Summary
The Glass effect is completely silent despite being properly routed and enabled. The DSP is not processing audio.

## Current Status
- ✅ Glass effect is being called in the routing (`isGlassEnabled=true`)
- ✅ Glass effect case is being hit in `processBlock`
- ❌ DSP `processStep` method is never reached
- ❌ No audio output from Glass effect

## Root Cause Analysis

### 1. Missing DSP Processing Call
The Glass effect is being routed but the actual DSP processing call is missing or not being reached.

### 2. Parameter Binding Issues
The DSP expects per-step parameters but the UI uses simple parameter names.

### 3. Sequencer Integration Missing
The Glass effect may not be properly integrated with the sequencer system.

## Code Files to Fix

### 1. PluginProcessor.cpp - Missing DSP Call

**Location:** Around line 1766 in the Glass case

**Current Code:**
```cpp
case EffectID::Glass:
{
    // Check if effect is enabled
    auto* glassEnabledParam = valueTreeState.getRawParameterValue("glassEnabled");
    bool isGlassEnabled = glassEnabledParam ? (glassEnabledParam->load() > 0.5f) : false;
    
    if (isGlassEnabled)
    {
        // Get Glass sequencer state
        auto& glassSeq = getGlassSeqState();
        
        // Check if sequencer is enabled and active
        if (glassSeq.enabled.load() && glassSeq.active.load())
        {
            // Get current step snapshot
            int currentStep = glassSeq.currentStep.load();
            // MISSING: Call to glassworksPageDSP.processStep()
        }
    }
    break;
}
```

**Fix Required:**
```cpp
case EffectID::Glass:
{
    // Check if effect is enabled
    auto* glassEnabledParam = valueTreeState.getRawParameterValue("glassEnabled");
    bool isGlassEnabled = glassEnabledParam ? (glassEnabledParam->load() > 0.5f) : false;
    
    if (isGlassEnabled)
    {
        // Get Glass sequencer state
        auto& glassSeq = getGlassSeqState();
        
        // Check if sequencer is enabled and active
        if (glassSeq.enabled.load() && glassSeq.active.load())
        {
            // Get current step snapshot
            int currentStep = glassSeq.currentStep.load();
            auto snapshot = getGlassSafeSnapshot(currentStep);
            
            // CALL THE DSP PROCESSING
            glassworksPageDSP.processStep(currentStep, snapshot, buffer, buffer.getNumSamples(), 0, getBPM(), false);
        }
        else
        {
            // Process without sequencer (direct parameter mode)
            StepSnapshot snapshot;
            snapshot.glass.pitchSemitones = 0.0f; // Default values
            snapshot.glass.brightness = 0.6f;
            snapshot.glass.decaySec = 1.2f;
            snapshot.glass.strike = 0.8f;
            snapshot.glass.density = 1.0f;
            snapshot.glass.shimmer = 0.12f;
            snapshot.glass.spread = 0.35f;
            snapshot.glass.mix = 0.3f;
            
            // CALL THE DSP PROCESSING
            glassworksPageDSP.processStep(0, snapshot, buffer, buffer.getNumSamples(), 0, getBPM(), false);
        }
    }
    break;
}
```

### 2. GlassworksPageDSP.cpp - Parameter Binding Fix

**Location:** `bindParamPtrs()` method (lines 108-127)

**Current Code:**
```cpp
// Bind 8 parameters using UI parameter names
juce::String paramID;
for (int param = 0; param < 8; ++param) {
    switch (param) {
        case 0: paramID = "glassPitchSemitones"; break;
        case 1: paramID = "glassBrightness"; break;
        case 2: paramID = "glassDecaySec"; break;
        case 3: paramID = "glassStrike"; break;
        case 4: paramID = "glassDensity"; break;
        case 5: paramID = "glassShimmer"; break;
        case 6: paramID = "glassSpread"; break;
        case 7: paramID = "glassMix"; break;
    }

    auto* ptr = apvts.getRawParameterValue(paramID);
    if (ptr == nullptr) {
        GlassLog::err("missing param: " + paramID);
        return false;
    }

    stepParamPtrs[param] = ptr;
    boundCount++;
}
```

**Fix Required:** Add logging and ensure parameters are found:
```cpp
// Bind 8 parameters using UI parameter names
juce::String paramID;
for (int param = 0; param < 8; ++param) {
    switch (param) {
        case 0: paramID = "glassPitchSemitones"; break;
        case 1: paramID = "glassBrightness"; break;
        case 2: paramID = "glassDecaySec"; break;
        case 3: paramID = "glassStrike"; break;
        case 4: paramID = "glassDensity"; break;
        case 5: paramID = "glassShimmer"; break;
        case 6: paramID = "glassSpread"; break;
        case 7: paramID = "glassMix"; break;
    }

    auto* ptr = apvts.getRawParameterValue(paramID);
    if (ptr == nullptr) {
        GlassLog::err("missing param: " + paramID);
        return false;
    }

    stepParamPtrs[param] = ptr;
    boundCount++;
    GlassLog::msg("Bound param " + juce::String(param) + ": " + paramID);
}

GlassLog::msg("bindParamPtrs: OK, bound " + juce::String(boundCount) + " parameters");
paramsBoundOk = true;
return true;
```

### 3. GlassworksPageDSP.cpp - Ensure DSP is Called

**Location:** `processStep()` method (line 158)

**Current Code:** The method exists but may not be called due to missing integration.

**Fix Required:** Add logging to confirm the method is reached:
```cpp
void GlassworksPageDSP::processStep(int stepIndex, const StepSnapshot& snapshot, 
                                   juce::AudioBuffer<float>& buffer, int numSamples, 
                                   int channel, double bpm, bool stepEdge)
{
    // ADD LOGGING TO CONFIRM METHOD IS REACHED
    static int processCounter = 0;
    if (++processCounter >= 100) {
        processCounter = 0;
        GlassLog::msg("processStep called - stepIndex=" + juce::String(stepIndex) + " numSamples=" + juce::String(numSamples));
    }

    // Force-proof guards (no crashes)
    if (!prepared || !paramsBoundOk || !healthy) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            GlassLog::err("processStep bypassed - not prepared/bound/healthy");
            loggedOnce = true;
        }
        return;
    }

    // Rest of the method...
}
```

### 4. PluginProcessor.cpp - Ensure Glass DSP is Prepared

**Location:** `prepareToPlay()` method

**Current Code:** May be missing Glass DSP preparation.

**Fix Required:** Add Glass DSP preparation:
```cpp
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // ... existing code ...
    
    // Prepare Glass DSP
    glassworksPageDSP.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    glassworksPageDSP.bindParamPtrs(valueTreeState, 16);
    
    // ... rest of method ...
}
```

## Debug Steps for Developer

### 1. Add Comprehensive Logging
Add logging to track the entire flow:

```cpp
// In PluginProcessor.cpp Glass case
case EffectID::Glass:
{
    static int debugCounter = 0;
    if (++debugCounter >= 100) {
        debugCounter = 0;
        DBG("[Glass] Processing Glass effect - case hit!");
    }
    
    // ... existing code ...
    
    if (isGlassEnabled)
    {
        DBG("[Glass] Glass is enabled, processing...");
        
        // ... sequencer logic ...
        
        DBG("[Glass] Calling glassworksPageDSP.processStep()");
        glassworksPageDSP.processStep(currentStep, snapshot, buffer, buffer.getNumSamples(), 0, getBPM(), false);
        DBG("[Glass] glassworksPageDSP.processStep() completed");
    }
    else
    {
        DBG("[Glass] Glass is disabled");
    }
    break;
}
```

### 2. Verify Parameter Creation
Ensure Glass parameters are created in `createParameterLayout()`:

```cpp
// In PluginProcessor.cpp createParameterLayout()
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassPitchSemitones", "Glass Pitch Semitones", -24.0f, 24.0f, 0.0f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassBrightness", "Glass Brightness", 0.0f, 1.0f, 0.6f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassDecaySec", "Glass Decay Sec", 0.05f, 4.0f, 1.2f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassStrike", "Glass Strike", 0.0f, 1.0f, 0.8f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassDensity", "Glass Density", 0.0f, 1.0f, 1.0f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassShimmer", "Glass Shimmer", 0.0f, 0.5f, 0.12f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassSpread", "Glass Spread", 0.0f, 1.0f, 0.35f));
params.push_back(std::make_unique<juce::AudioParameterFloat>("glassMix", "Glass Mix", 0.0f, 1.0f, 0.3f));
params.push_back(std::make_unique<juce::AudioParameterBool>("glassEnabled", "Glass Enabled", true));
```

### 3. Test Without Sequencer
For immediate testing, bypass the sequencer requirement:

```cpp
case EffectID::Glass:
{
    // Check if effect is enabled
    auto* glassEnabledParam = valueTreeState.getRawParameterValue("glassEnabled");
    bool isGlassEnabled = glassEnabledParam ? (glassEnabledParam->load() > 0.5f) : false;
    
    if (isGlassEnabled)
    {
        // FORCE PROCESSING WITHOUT SEQUENCER FOR TESTING
        StepSnapshot snapshot;
        snapshot.glass.pitchSemitones = 0.0f;
        snapshot.glass.brightness = 0.6f;
        snapshot.glass.decaySec = 1.2f;
        snapshot.glass.strike = 0.8f;
        snapshot.glass.density = 1.0f;
        snapshot.glass.shimmer = 0.12f;
        snapshot.glass.spread = 0.35f;
        snapshot.glass.mix = 0.5f; // 50% mix for testing
        
        // ALWAYS CALL THE DSP
        glassworksPageDSP.processStep(0, snapshot, buffer, buffer.getNumSamples(), 0, getBPM(), false);
    }
    break;
}
```

## Expected Behavior After Fix

1. **Console Logs:** Should see `[Glass] processStep called` messages
2. **Audio Output:** Should hear glass/bell resonance effect
3. **Parameter Response:** Knob adjustments should affect the sound
4. **Mix Control:** Mix knob should blend dry/wet signals

## Files to Modify

1. `source/PluginProcessor.cpp` - Add DSP call in Glass case
2. `source/dsp/glass/GlassworksPageDSP.cpp` - Add logging and fix parameter binding
3. `source/dsp/glass/GlassworksPageDSP.h` - Ensure proper declarations

## Priority

**CRITICAL** - The Glass effect is completely non-functional. The DSP is never being called, which is why there's no audio output.

## Testing

After implementing fixes:
1. Build and install AU plugin
2. Load Glass effect on tab 4
3. Turn Mix knob to 50%
4. Play audio through plugin
5. Should hear immediate glass resonance effect
6. Check console for processing logs
