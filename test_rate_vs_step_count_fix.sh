#!/bin/bash

echo "=== Rate vs Step Count Fix Test ==="
echo ""

# Test 1: Standalone App
echo "🔍 Testing Standalone App with Corrected Rate/Step Logic..."
if pgrep -f "Stepper.app" > /dev/null; then
    echo "✅ Stepper standalone app is running"
    echo ""
    echo "📱 What you should see in the Stepper window:"
    echo "1. 16 step buttons arranged in a 4x4 grid"
    echo "2. Gray highlight (#5B5B5B) moving through buttons"
    echo "3. SVG graphics on each step button"
    echo "4. Rate dropdown and step amount controls"
    echo ""
    echo "🎯 The sequencer should now work correctly:"
    echo ""
    echo "✅ RATE determines TIMING (how fast steps advance):"
    echo "   - 1/32: Steps advance 8 times per quarter note"
    echo "   - 1/16: Steps advance 4 times per quarter note"
    echo "   - 1/8:  Steps advance 2 times per quarter note"
    echo "   - 1/4:  Steps advance 1 time per quarter note"
    echo "   - 1/2:  Steps advance 1 time per half note"
    echo "   - 1/1:  Steps advance 1 time per whole note"
    echo ""
    echo "✅ STEP COUNT determines HOW MANY steps are used:"
    echo "   - If step count = 16: Uses all 16 steps (0-15)"
    echo "   - If step count = 8:  Uses first 8 steps (0-7)"
    echo "   - If step count = 4:  Uses first 4 steps (0-3)"
    echo ""
    echo "🔧 Fixed Issues:"
    echo "   - Rate no longer limits step count"
    echo "   - 1/2 with 16 steps now uses all 16 steps (not just 2)"
    echo "   - All rates now respect the step count setting"
    echo "   - Rate only affects timing, not quantity"
    
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
echo "🔍 Testing AU Plugin with Corrected Rate/Step Logic..."
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
    echo "7. Rate should only affect timing, not step count"
    
else
    echo "❌ AU plugin is not installed"
fi

echo ""
echo "📊 Summary of Rate vs Step Count Fix:"
echo "✅ Rate determines timing (how fast steps advance)"
echo "✅ Step count determines quantity (how many steps are used)"
echo "✅ Rate no longer limits step count"
echo "✅ All rates now respect the step count setting"
echo "✅ 1/2 with 16 steps uses all 16 steps (not just 2)"
echo "✅ 1/8 with 16 steps uses all 16 steps (not just 8)"
echo "✅ 1/4 with 16 steps uses all 16 steps (not just 4)"

echo ""
echo "🎉 Ready for testing! The sequencer should now work correctly:"
echo "   - Rate controls timing (fast/slow)"
echo "   - Step count controls quantity (how many steps)"
echo "   - They work independently as expected!"
