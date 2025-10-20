# Stepper Plugin: Random Buttons, Step Sequencer & All Steps Toggle - Developer Digest

## Overview
This document provides a comprehensive technical explanation of how the random buttons, step sequencer, and All Steps toggle functionality work in the Stepper plugin. This is intended for developers who need to understand, debug, or modify these systems.

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Step Sequencer System](#step-sequencer-system)
3. [Random Button System](#random-button-system)
4. [All Steps Toggle System](#all-steps-toggle-system)
5. [Data Flow & Integration](#data-flow--integration)
6. [Current Issues & Debugging](#current-issues--debugging)
7. [Code Examples](#code-examples)

---

## Architecture Overview

### Core Components
- **PluginProcessor**: Audio processing and parameter management
- **PluginEditor**: UI components and user interaction
- **StepSnapshot**: Data structure storing parameter values per step
- **SeqState**: Sequencer state management
- **APVTS**: JUCE's parameter system for knob-to-DSP communication

### Key Files
- `source/PluginProcessor.h/cpp` - Core audio processing logic
- `source/PluginEditor.h/cpp` - UI implementation
- `source/StepSnapshot.h` - Parameter storage structure
- `source/ui/StepSequencer.h` - Sequencer UI components

---

## Step Sequencer System

### Data Structure: StepSnapshot
```cpp
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
    
    // ... similar structures for other effects (autopan, dirt, chorus, etc.)
};
```

### Sequencer State Management
```cpp
struct SeqState {
    std::atomic<bool> enabled{true};
    std::atomic<bool> active{false};
    std::atomic<int> stepsUsed{16};
    std::atomic<int> divisionIndex{5};
    std::atomic<int> currentStep{0};
    std::atomic<int> stdMode{0};
    std::atomic<double> phase{0.0};
    std::atomic<bool> needsReset{false};
};
```

### How Step Sequencer Works

1. **Step Selection**: User clicks step buttons (0-15) to select which step to edit
2. **Snapshot Storage**: Each step stores a complete `StepSnapshot` with all effect parameters
3. **Sequencer Playback**: During playback, sequencer advances through steps and applies snapshots
4. **Parameter Updates**: When user adjusts knobs, values are saved to the current step's snapshot

### Step Button Implementation
```cpp
// Step buttons are created in PluginEditor::setupSequencerArea()
for (int i = 0; i < 16; ++i) {
    stepButtons[i] = std::make_unique<CustomStepButton>(i);
    stepButtons[i]->onClick = [this, i]() {
        // Update UI selected step
        uiSelectedStep = i;
        processorRef.setSelectedStep(i);
        
        // Load snapshot values into knobs
        auto snapshot = processorRef.getSafeSnapshot(i);
        // ... update all knobs with snapshot values
    };
}
```

---

## Random Button System

### Types of Random Buttons

1. **Main Dice Button** (Effect Area): Randomizes all unlocked knobs for current step
2. **Individual Dice Buttons** (Per Knob): Randomizes single knob
3. **Step Dice Button** (Sequencer Area): Randomizes all steps' snapshots

### Main Dice Button Implementation
```cpp
// Located in PluginEditor::setupEffectsArea()
diceButton->onClick = [this]() { 
    randomizeKnobValues(); 
};

void PluginEditor::randomizeKnobValues() {
    for (int i = 0; i < 8; ++i) {
        if (knobLocked[i]) continue; // Respect lock state
        
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        knobs[i]->setValue(randomValue);
        
        // Update UI elements
        valueLabels[i]->setText(juce::String((int)std::round(randomValue * 100)));
        indicatorBars[i]->setValue(randomValue);
    }
}
```

### Individual Dice Button Implementation
```cpp
// Created for each knob in setupKnobs()
diceButtons[i]->onClick = [this, i]() { 
    randomizeIndividualKnob(i); 
};

void PluginEditor::randomizeIndividualKnob(int knobIndex) {
    if (knobLocked[knobIndex]) return;
    
    float randomValue = juce::Random::getSystemRandom().nextFloat();
    knobs[knobIndex]->setValue(randomValue);
    
    // Update UI
    valueLabels[knobIndex]->setText(juce::String((int)std::round(randomValue * 100)));
    indicatorBars[knobIndex]->setValue(randomValue);
}
```

### Step Dice Button Implementation
```cpp
// Located in PluginEditor::setupSequencerArea()
stepDiceButton->onClick = [this]() {
    // Randomize all 16 step snapshots
    for (int step = 0; step < 16; ++step) {
        auto snapshot = processorRef.getSafeSnapshot(step);
        
        // Randomize each parameter (respecting locks)
        if (!knobLocked[0]) snapshot.delay.timeMs = 10.0f + juce::Random::getSystemRandom().nextFloat() * (2000.0f - 10.0f);
        if (!knobLocked[1]) snapshot.delay.feedback = juce::Random::getSystemRandom().nextFloat() * 0.95f;
        // ... continue for all parameters
        
        processorRef.setStepSnapshot(step, snapshot);
    }
    
    // Update current step UI to show new values
    int selectedStep = processorRef.getSelectedStep();
    if (selectedStep >= 0 && selectedStep < 16) {
        auto snapshot = processorRef.getSafeSnapshot(selectedStep);
        // ... update knobs with snapshot values
    }
};
```

---

## All Steps Toggle System

### Purpose
When enabled, the All Steps toggle makes knob adjustments apply to ALL steps simultaneously, rather than just the current step.

### Implementation
```cpp
// Toggle button setup in PluginEditor::setupAllStepsToggle()
allStepsToggle->onClick = [this]() {
    allStepsEnabled = allStepsToggle->getToggleState();
    DBG("[UI] All Steps toggle: " + juce::String(allStepsEnabled ? "ON" : "OFF"));
};

// Parameter change handling in PluginProcessor::parameterChanged()
void PluginProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    // Handle Space Delay parameters
    if (parameterID == "timeMs" || parameterID == "feedback" || /* ... other params */) {
        int knobIndex = getKnobIndexFromParameterID(parameterID);
        
        if (knobIndex >= 0) {
            if (allStepsEnabled.load()) {
                // Update ALL steps with the new value
                for (int step = 0; step < 16; ++step) {
                    StepSnapshot snapshot = getSafeSnapshot(step);
                    switch (knobIndex) {
                        case 0: snapshot.delay.timeMs = actualValue; break;
                        case 1: snapshot.delay.feedback = actualValue; break;
                        // ... handle all parameters
                    }
                    setStepSnapshot(step, snapshot);
                }
            } else {
                // Update only current step
                updateCurrentStepSnapshot(knobIndex, actualValue);
            }
        }
    }
}
```

### Current State
**IMPORTANT**: The All Steps toggle system has been **REMOVED** from the current codebase due to implementation issues. The toggle buttons and related code are no longer present in the current version.

---

## Data Flow & Integration

### Parameter Update Flow
```
User Adjusts Knob → APVTS Parameter Change → PluginProcessor::parameterChanged() 
→ Update StepSnapshot → Save to Processor → Update UI Labels
```

### Step Selection Flow
```
User Clicks Step Button → Update uiSelectedStep → Load StepSnapshot 
→ Update All Knobs → Update UI Labels → Update Indicator Bars
```

### Randomization Flow
```
User Clicks Random Button → Generate Random Values → Update Knobs 
→ Trigger Parameter Change → Update StepSnapshot → Update UI
```

### Sequencer Playback Flow
```
Transport Playing → Sequencer Advances → Load StepSnapshot 
→ Apply to DSP → Update Playing Step UI
```

---

## Current Issues & Debugging

### Known Issues

1. **Random Button Snapping**: Values snap back to defaults after randomization
   - **Cause**: Mismatch between UI parameter ranges and StepSnapshot storage
   - **Location**: `PluginEditor::randomizeKnobValues()` and parameter change handlers

2. **Unmovable Knobs**: Some knobs don't respond to user input
   - **Cause**: Parameter attachments not properly connected or parameter ranges incorrect
   - **Location**: `PluginEditor::setupKnobs()` APVTS attachments

3. **Step Sequencer Not Saving**: Manual knob adjustments don't persist
   - **Cause**: `parameterChanged()` method not properly updating StepSnapshots
   - **Location**: `PluginProcessor::parameterChanged()`

### Debugging Tips

1. **Enable Debug Logging**: Look for `DBG()` statements in the code
2. **Check Parameter IDs**: Ensure APVTS parameter IDs match between UI and processor
3. **Verify StepSnapshot Structure**: Check that field names match between UI and storage
4. **Test Parameter Ranges**: Ensure knob ranges match StepSnapshot value ranges

### Debug Commands
```cpp
// Add to PluginEditor for debugging
DBG("[DEBUG] Knob " << knobIndex << " value: " << knobs[knobIndex]->getValue());
DBG("[DEBUG] Snapshot value: " << snapshot.delay.timeMs);
DBG("[DEBUG] All Steps enabled: " << allStepsEnabled.load());
```

---

## Code Examples

### Complete Random Button Implementation
```cpp
// Main effect area random button
void PluginEditor::randomizeKnobValues() {
    DBG("[UI] Randomizing knob values...");
    
    for (int i = 0; i < 8; ++i) {
        if (knobLocked[i]) continue; // Respect lock state
        
        float randomValue = juce::Random::getSystemRandom().nextFloat();
        knobs[i]->setValue(randomValue);
        
        // Update value label
        juce::String valueText;
        switch (i) {
            case 0: valueText = juce::String(randomValue * 2000.0f, 0) + "ms"; break;
            case 1: valueText = juce::String(randomValue * 100, 0) + "%"; break;
            // ... handle all parameters
        }
        valueLabels[i]->setText(valueText, juce::dontSendNotification);
        
        // Update indicator bar
        indicatorBars[i]->setValue(randomValue);
    }
    
    DBG("[UI] All knob values randomized");
}
```

### Step Sequencer Update
```cpp
void PluginEditor::updateSequencerUI() {
    int selectedStep = uiSelectedStep;
    int playingStep = processorRef.getCurrentStep();
    const int stepsUsed = processorRef.getSeqState().stepsUsed.load();
    
    for (int i = 0; i < 16; ++i) {
        if (stepButtons[i] != nullptr) {
            stepButtons[i]->setSelected(i == selectedStep);
            bool sequencerEnabled = processorRef.getSeqState().enabled.load();
            stepButtons[i]->setPlaying(sequencerEnabled && (i == playingStep));
            bool shouldBeEnabled = i < stepsUsed;
            stepButtons[i]->setEnabledStep(shouldBeEnabled);
        }
    }
    
    // Update step amount display
    if (stepAmountLabel != nullptr && !stepAmountLabel->hasKeyboardFocus(true)) {
        stepAmountLabel->setText(juce::String(stepsUsed), false);
    }
    
    repaint();
}
```

### Parameter Change Handler
```cpp
void PluginProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    // Handle Space Delay parameters
    if (parameterID == "timeMs" || parameterID == "feedback" || 
        parameterID == "wowDepth" || parameterID == "wowRate" || 
        parameterID == "drive" || parameterID == "hiCut" || 
        parameterID == "lowCut" || parameterID == "mix") {
        
        int knobIndex = -1;
        if (parameterID == "timeMs") knobIndex = 0;
        else if (parameterID == "feedback") knobIndex = 1;
        // ... map all parameters to knob indices
        
        if (knobIndex >= 0) {
            float actualValue = newValue;
            
            // Convert normalized values to actual ranges
            switch (knobIndex) {
                case 0: actualValue = 10.0f + newValue * (2000.0f - 10.0f); break; // timeMs
                case 1: actualValue = newValue * 0.95f; break; // feedback
                // ... handle all parameter conversions
            }
            
            if (allStepsEnabled.load()) {
                // Update ALL steps
                for (int step = 0; step < 16; ++step) {
                    StepSnapshot snapshot = getSafeSnapshot(step);
                    switch (knobIndex) {
                        case 0: snapshot.delay.timeMs = actualValue; break;
                        case 1: snapshot.delay.feedback = actualValue; break;
                        // ... handle all parameters
                    }
                    setStepSnapshot(step, snapshot);
                }
            } else {
                // Update only current step
                updateCurrentStepSnapshot(knobIndex, actualValue);
            }
        }
    }
}
```

---

## Summary

The Stepper plugin's random button and step sequencer system is built around:

1. **StepSnapshot structures** that store parameter values for each step
2. **Random button implementations** that generate values and update both UI and snapshots
3. **Parameter change handlers** that coordinate between UI, APVTS, and snapshot storage
4. **Step sequencer UI** that manages step selection and playback visualization

The current implementation has some issues with parameter synchronization and the All Steps toggle has been removed. The system requires careful coordination between UI parameter ranges, APVTS parameter IDs, and StepSnapshot field names to function correctly.

For debugging, focus on:
- Parameter ID matching between UI and processor
- Value range conversions between normalized (0-1) and actual ranges
- StepSnapshot field name consistency
- APVTS attachment correctness
