#!/bin/bash

echo "Testing Stepper Sequencer..."

# Check if the standalone app is running
if pgrep -f "Stepper.app" > /dev/null; then
    echo "✅ Stepper standalone app is running"
    
    # Check if we can see any debug output
    echo "Checking for debug output..."
    
    # Try to get some basic info about the process
    echo "Process info:"
    ps aux | grep Stepper | grep -v grep
    
    echo ""
    echo "If you can see the Stepper app window, look for:"
    echo "1. Gray highlight moving through the step buttons"
    echo "2. Highlight should move every second (fallback mode)"
    echo "3. 16 step buttons should be visible"
    
else
    echo "❌ Stepper standalone app is not running"
    echo "Trying to start it..."
    open "/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app"
fi

echo ""
echo "For AU plugin testing:"
echo "1. Open Ableton Live"
echo "2. Look for 'Stepper' in the Audio Units section"
echo "3. If it crashes, check Console.app for error messages"
