#!/bin/bash

echo "=== AU Plugin Test ==="
echo ""

# Remove any existing plugin
echo "Removing existing plugin..."
rm -rf "/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component" 2>/dev/null

# Copy the plugin
echo "Installing AU plugin..."
cp -r "/Users/troycarson/Documents/JUCE Projects/Stepper/build/Stepper_artefacts/Debug/AU/Stepper.component" ~/Library/Audio/Plug-Ins/Components/

if [ -d "/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component" ]; then
    echo "✅ Plugin installed successfully"
    
    # Try to remove any code signature
    echo "Removing code signature..."
    codesign --remove-signature "/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component" 2>/dev/null
    
    # Check if it's unsigned
    echo "Checking plugin status..."
    codesign -dv "/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component" 2>&1
    
    echo ""
    echo "🔍 Next steps:"
    echo "1. Open Ableton Live"
    echo "2. Look for 'Stepper' in Audio Units > YourCompany"
    echo "3. If it doesn't appear, check Console.app for errors"
    echo "4. If it crashes, we may need to try a different approach"
    
else
    echo "❌ Failed to install plugin"
fi

echo ""
echo "Note: If you get code signing errors, we may need to:"
echo "1. Create a developer certificate"
echo "2. Use a different signing method"
echo "3. Or focus on the standalone version for now"
