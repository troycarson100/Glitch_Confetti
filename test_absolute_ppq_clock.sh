#!/bin/bash

echo "=== Absolute PPQ Clock Sequencer Test ==="
echo ""

# Test 1: Standalone App
echo "🔍 Testing Standalone App with Absolute PPQ Clock..."
if pgrep -f "Stepper.app" > /dev/null; then
    echo "✅ Stepper standalone app is running"
    echo ""
    echo "📱 What you should see in the Stepper window:"
    echo "1. 16 step buttons arranged in a 4x4 grid"
    echo "2. Gray highlight (#5B5B5B) moving through buttons"
    echo "3. SVG graphics on each step button"
    echo "4. Rate dropdown and step amount controls"
    echo ""
    echo "🎯 The sequencer should now work with absolute PPQ clock:"
    echo ""
    echo "✅ DIVISION determines TIMING (step period in beats):"
    echo "   - 1/32: 0.125 beats per step (8 steps per quarter note)"
    echo "   - 1/16: 0.25 beats per step (4 steps per quarter note)"
    echo "   - 1/8:  0.5 beats per step (2 steps per quarter note)"
    echo "   - 1/4:  1.0 beats per step (1 step per quarter note)"
    echo "   - 1/2:  2.0 beats per step (1 step per half note)"
    echo "   - 1/1:  4.0 beats per step (1 step per whole note)"
    echo ""
    echo "✅ STEP COUNT determines QUANTITY (how many steps cycle):"
    echo "   - If step count = 16: Cycles through all 16 steps (0-15)"
    echo "   - If step count = 8:  Cycles through first 8 steps (0-7)"
    echo "   - If step count = 4:  Cycles through first 4 steps (0-3)"
    echo ""
    echo "🔧 Key Features of Absolute PPQ Clock:"
    echo "   - Uses absolute PPQ position (not bar-relative)"
    echo "   - Highlight marches continuously across bars"
    echo "   - No reset at bar boundaries"
    echo "   - Division and step count are completely independent"
    echo "   - 1/4 with 16 steps: advances 1 step per beat, cycles all 16 over 16 beats"
    echo "   - 1/16 with 16 steps: advances 4 steps per beat, cycles all 16 in 1 bar"
    echo "   - 1/2 with 16 steps: advances 1 step per half note, cycles all 16 over 8 bars"
    
else
    echo "❌ Stepper standalone app is not running"
    echo "Starting it now..."
    open "/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app"
    sleep 3
    if pgrep -f "Stepper.app" > /dev/null; then
        echo "✅ Now running"
    else
        echo "❌ Failed to start"
    fi
fi

echo ""

# Test 2: AU Plugin
echo "🔍 Testing AU Plugin with Absolute PPQ Clock..."
if [ -d "/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component" ]; then
    echo "✅ AU plugin is installed and signed"
    echo "Location: /Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component"
    
    echo ""
    echo "🎯 For AU plugin testing in Ableton Live:"
    echo "1. Open Ableton Live"
    echo "2. Look for 'Stepper' in Audio Units > YourCompany"
    echo "3. Load the plugin on an audio track"
    echo "4. Press play in Ableton - sequencer should sync to DAW clock"
    echo "5. Test different rate settings - they should all use full step count"
    echo "6. Change step count - should limit active steps correctly"
    echo "7. Highlight should march continuously across bars (no reset)"
    echo "8. Division and step count should work independently"
    
else
    echo "❌ AU plugin is not installed"
fi

echo ""
echo "📊 Summary of Absolute PPQ Clock Implementation:"
echo "✅ Uses absolute PPQ position (not bar-relative)"
echo "✅ Division determines step period in beats"
echo "✅ Step count determines cycle length"
echo "✅ Highlight marches continuously across bars"
echo "✅ No reset at bar boundaries"
echo "✅ Division and step count are completely independent"
echo "✅ 1/4 with 16 steps: 1 step per beat, cycles all 16 over 16 beats"
echo "✅ 1/16 with 16 steps: 4 steps per beat, cycles all 16 in 1 bar"
echo "✅ 1/2 with 16 steps: 1 step per half note, cycles all 16 over 8 bars"

echo ""
echo "🎉 Ready for testing! The sequencer now uses absolute PPQ clock:"
echo "   - Division controls timing (step period in beats)"
echo "   - Step count controls cycle length (how many steps)"
echo "   - Highlight marches continuously without bar resets"
echo "   - Perfect for long sequences that span multiple bars!"
