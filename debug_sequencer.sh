#!/bin/bash

echo "=== Stepper Sequencer Debug ==="
echo ""

echo "🔍 Current Status:"
echo "Standalone app running: $(pgrep -f "Stepper.app" > /dev/null && echo "✅ Yes" || echo "❌ No")"
echo "AU plugin installed: $([ -d "/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component" ] && echo "✅ Yes" || echo "❌ No")"

echo ""
echo "📱 What to look for in the Stepper window:"
echo "1. 16 step buttons arranged in a 4x4 grid"
echo "2. Each button should have an SVG graphic (step icon)"
echo "3. Gray highlight (#5B5B5B) should move through buttons every second"
echo "4. Rate dropdown should show options like 1/4, 1/8, 1/16, etc."
echo "5. Step amount control should be draggable"

echo ""
echo "🐛 Troubleshooting:"
echo "If you don't see the gray highlight:"
echo "- The fallback sequencer should be running every second"
echo "- Check if the app window is visible and not minimized"
echo "- Look for debug messages in Console.app (search for 'Stepper')"

echo ""
echo "📊 Debug Messages to Look For:"
echo "- 'Fallback sequencer: testStep=X' (every second)"
echo "- 'applyStepHighlight: newStep=X' (when steps change)"
echo "- 'No playHead available' (normal for standalone)"

echo ""
echo "🎯 Testing Steps:"
echo "1. Look at the Stepper app window"
echo "2. Count the step buttons (should be 16)"
echo "3. Watch for gray highlight moving every second"
echo "4. Try changing the rate dropdown"
echo "5. Try dragging the step amount control"

echo ""
echo "If the highlight is still not visible, the issue might be:"
echo "- UI rendering problem"
echo "- Timer not running"
echo "- Button state not updating"
echo "- Graphics context issue"

echo ""
echo "Press Ctrl+C to stop this debug session"
