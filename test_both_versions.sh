#!/bin/bash

echo "=== Testing Both Standalone and AU Versions ==="
echo ""

# Kill any existing Stepper apps
pkill -f "Stepper.app" 2>/dev/null
pkill -f "Stepper.component" 2>/dev/null
sleep 1

echo "Starting Standalone app..."
"/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app/Contents/MacOS/Stepper" &

echo ""
echo "=== Manual Test Instructions ==="
echo ""
echo "1. STANDALONE VERSION TEST:"
echo "   - Check if the step area controls (rate dropdown, S/T/D toggle, step count box)"
echo "   - have the custom 'Akira Expanded' font applied"
echo "   - Change the step count box from 16 to 12"
echo "   - Steps 13, 14, 15, and 16 should become greyed out (70% opacity)"
echo "   - The sequencer should be running with a grey highlight moving through steps"
echo ""
echo "2. AU VERSION TEST:"
echo "   - Open Ableton Live"
echo "   - Look for 'Stepper' in the AU plugins"
echo "   - Load it on a track"
echo "   - Check if the same features work as in standalone:"
echo "     * Custom fonts in step area controls"
echo "     * Step greying when changing step count"
echo "     * Sequencer highlight synced to DAW playback"
echo "   - Test that the sequencer stops when you pause Ableton"
echo "   - Test that different rates work correctly"
echo ""
echo "3. FEATURES TO VERIFY:"
echo "   ✓ Custom fonts in step area (rate dropdown, S/T/D toggle, step count box)"
echo "   ✓ Step greying when step count is reduced"
echo "   ✓ Sequencer highlight moving through steps"
echo "   ✓ Rate dropdown working (1/1, 1/2, 1/4, 1/8, 1/16, 1/32)"
echo "   ✓ S/T/D toggle working (Straight, Dotted, Triplet)"
echo "   ✓ DAW sync (stops when DAW stops, starts when DAW starts)"
echo ""
echo "Press Ctrl+C to stop the standalone app when done testing"
echo ""

# Keep the script running so the app stays open
wait