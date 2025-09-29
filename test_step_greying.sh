#!/bin/bash

echo "=== Testing Step Greying and Custom Fonts ==="
echo ""

# Kill any existing Stepper app
pkill -f "Stepper.app" 2>/dev/null
sleep 1

echo "Starting Stepper app..."
"/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app/Contents/MacOS/Stepper" &

echo ""
echo "=== Manual Test Instructions ==="
echo ""
echo "1. CUSTOM FONTS TEST:"
echo "   - Check if the step area controls (rate dropdown, S/T/D toggle, step count box)"
echo "   - have the custom 'Akira Expanded' font applied"
echo "   - The text should look different from the default system font"
echo ""
echo "2. STEP GREYING TEST:"
echo "   - Look at the 16 step buttons in the step area"
echo "   - Find the step count box (should show '16' by default)"
echo "   - Click and drag the step count box to change the value to '12'"
echo "   - Steps 13, 14, 15, and 16 should become greyed out (70% opacity)"
echo "   - Change it to '8' - steps 9-16 should be greyed out"
echo "   - Change it back to '16' - all steps should be normal opacity"
echo ""
echo "3. SEQUENCER TEST:"
echo "   - The sequencer should be running (you should see a grey highlight moving)"
echo "   - The highlight should move through all 16 steps"
echo "   - Try changing the rate dropdown to see different speeds"
echo ""
echo "Press Ctrl+C to stop the app when done testing"
echo ""

# Keep the script running so the app stays open
wait
