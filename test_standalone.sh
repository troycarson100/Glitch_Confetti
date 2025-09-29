#!/bin/bash

echo "=== Stepper Standalone Test ==="
echo ""

# Kill any existing instances
echo "Stopping any existing Stepper instances..."
pkill -f "Stepper.app" 2>/dev/null
sleep 2

# Start the app
echo "Starting Stepper standalone app..."
open "/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app"

# Wait a moment for it to start
sleep 3

# Check if it's running
if pgrep -f "Stepper.app" > /dev/null; then
    echo "✅ Stepper app started successfully"
    echo ""
    echo "🔍 What to look for in the Stepper window:"
    echo "1. 16 step buttons arranged in a grid"
    echo "2. Gray highlight (#5B5B5B) moving through the buttons"
    echo "3. Highlight should advance every second (fallback mode)"
    echo "4. Rate dropdown should work (1/4, 1/8, 1/16, etc.)"
    echo "5. Step amount control should work"
    echo ""
    echo "📊 Debug output should show:"
    echo "- 'Fallback sequencer: testStep=X' every second"
    echo "- 'applyStepHighlight: newStep=X' when steps change"
    echo ""
    echo "If you see the gray highlight moving, the sequencer is working!"
    echo "If not, there may be an issue with the UI rendering."
    
else
    echo "❌ Failed to start Stepper app"
    echo "Check Console.app for error messages"
fi

echo ""
echo "Press Ctrl+C to stop this test"
