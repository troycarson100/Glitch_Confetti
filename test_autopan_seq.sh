#!/bin/bash
echo "===== AUTOPAN SEQUENCER DEBUG TEST ====="
echo "1. Open the standalone app"
echo "2. Go to AutoPan page"
echo "3. Click the POWER BUTTON to enable sequencer"
echo "4. Press PLAY in the DAW transport"
echo "5. Watch for debug output below..."
echo ""
echo "Expected output:"
echo "  - [AUTOPAN SEQ] ✓ Activated on play edge"
echo "  - [AUTOPAN SEQ] Lock-in at PPQ=..."
echo "  - [AUTOPAN SEQ] ★ Step changed to: 0/1/2/3..."
echo ""
echo "====================================="
echo ""

"/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Release/Standalone/Stepper.app/Contents/MacOS/Stepper" 2>&1 | grep -E "AUTOPAN|AutoPan"

