#!/bin/bash

echo "=== Testing Rate Fix and S/T/D Toggle ==="
echo ""

echo "1. Building standalone app..."
cd "/Users/troycarson/Documents/JUCE Projects/Stepper"
xcodebuild -project build/Stepper.xcodeproj -scheme Stepper_Standalone -configuration Debug build > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "✓ Standalone build successful"
else
    echo "✗ Standalone build failed"
    exit 1
fi

echo ""
echo "2. Building AU plugin..."
xcodebuild -project build/Stepper.xcodeproj -scheme Stepper_AU -configuration Debug build > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "✓ AU build successful"
else
    echo "✗ AU build failed"
fi

echo ""
echo "3. Installing AU plugin..."
mkdir -p ~/Library/Audio/Plug-Ins/Components
cp -r "build/Stepper_artefacts/Debug/AU/Stepper.component" ~/Library/Audio/Plug-Ins/Components/ > /dev/null 2>&1
xattr -cr ~/Library/Audio/Plug-Ins/Components/Stepper.component > /dev/null 2>&1
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/Stepper.component > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "✓ AU plugin installed and signed"
else
    echo "✗ AU plugin installation failed"
fi

echo ""
echo "=== Test Instructions ==="
echo ""
echo "STANDALONE APP:"
echo "1. Open: build/Stepper_artefacts/Debug/Standalone/Stepper.app"
echo "2. Test rate dropdown - each rate should have different speeds:"
echo "   - 1/1: Slowest (4 beats per step)"
echo "   - 1/2: Medium-slow (2 beats per step)"
echo "   - 1/4: Medium (1 beat per step)"
echo "   - 1/8: Medium-fast (0.5 beats per step)"
echo "   - 1/16: Fast (0.25 beats per step)"
echo "   - 1/32: Fastest (0.125 beats per step)"
echo ""
echo "3. Test S/T/D toggle:"
echo "   - S (Straight): Normal timing"
echo "   - D (Dotted): 1.5x slower than Straight"
echo "   - T (Triplet): 2/3 faster than Straight"
echo ""
echo "AU PLUGIN:"
echo "1. Open Ableton Live"
echo "2. Add Stepper plugin to a track"
echo "3. Test the same rate and S/T/D functionality as above"
echo "4. Verify the sequencer highlight moves with DAW playback"
echo ""
echo "=== Expected Behavior ==="
echo "- Rate dropdown should change sequencer speed"
echo "- S/T/D toggle should modify timing (slower/faster)"
echo "- All rates should have different speeds"
echo "- Sequencer should sync with DAW transport"
echo ""
