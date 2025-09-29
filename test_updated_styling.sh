#!/bin/bash

echo "=== Testing Updated Step Rate and S/T/D Toggle Styling ==="
echo ""

# Kill any existing Stepper apps
pkill -f "Stepper.app" 2>/dev/null
sleep 1

echo "Starting Standalone app..."
"/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/Standalone/Stepper.app/Contents/MacOS/Stepper" &

echo ""
echo "=== Manual Test Instructions ==="
echo ""
echo "1. STEP RATE DROPDOWN TEST:"
echo "   - Check that the step rate dropdown has NO grey background (transparent)"
echo "   - Verify the border is 2px thick (thicker than before)"
echo "   - Look for the carrot arrow icon (FX_Type_Carrot_Inactive.svg)"
echo "   - Click the dropdown to open it"
echo "   - Verify the dropdown background is #131313 (dark grey)"
echo "   - Check that the carrot changes to FX_Type_Carrot_Active.svg when open"
echo "   - Close the dropdown and verify carrot returns to inactive state"
echo ""
echo "2. S/T/D TOGGLE BUTTON TEST:"
echo "   - Check that the S/T/D toggle has NO grey background (transparent)"
echo "   - Verify the border is 2px thick (thicker than before)"
echo "   - Click the button to cycle through S/D/T states"
echo "   - Verify the text changes correctly"
echo ""
echo "3. STEP COUNT BOX TEST:"
echo "   - Check that the step count box still has the 2px border"
echo "   - Verify it still has the custom font"
echo "   - Test the step greying functionality by changing the value"
echo ""
echo "4. SEQUENCER TEST:"
echo "   - Verify the sequencer highlight is still working"
echo "   - Test different rates to ensure they work correctly"
echo "   - Test the S/T/D toggle functionality"
echo ""
echo "Press Ctrl+C to stop the app when done testing"
echo ""

# Keep the script running so the app stays open
wait
