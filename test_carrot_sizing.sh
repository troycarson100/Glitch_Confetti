#!/bin/bash

echo "=== Testing Carrot SVG Sizing and Positioning ==="
echo ""

# Kill any existing Stepper apps
pkill -f "Stepper.app" 2>/dev/null
sleep 1

echo "Starting Standalone app..."
"/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app/Contents/MacOS/Stepper" &

echo ""
echo "=== Manual Test Instructions ==="
echo ""
echo "1. CARROT SIZE TEST:"
echo "   - Check that the carrot arrow in the rate dropdown is 30% smaller than before"
echo "   - The carrot should appear more proportional to the text"
echo ""
echo "2. CARROT POSITION TEST:"
echo "   - Verify the carrot is positioned closer to the rate number"
echo "   - There should be less space between the text and the carrot"
echo ""
echo "3. CARROT STATE TEST:"
echo "   - Click the rate dropdown to open it"
echo "   - Verify the carrot changes to the active state (FX_Type_Carrot_Active.svg)"
echo "   - Close the dropdown and verify it returns to inactive state"
echo "   - Both states should be the same smaller size"
echo ""
echo "4. OVERALL LAYOUT TEST:"
echo "   - Ensure the rate dropdown still looks clean and balanced"
echo "   - Check that the smaller carrot doesn't make the dropdown look cramped"
echo ""
echo "Press Ctrl+C to stop the app when done testing"
echo ""

# Keep the script running so the app stays open
wait
