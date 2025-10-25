#!/bin/bash

echo "=== Building Stepper with Glass Fix ==="
echo "This will take 2-3 minutes..."
echo ""

cd "/Users/troycarson/Documents/JUCE Projects/Stepper/build"

# Force complete rebuild of Glass files
echo "Cleaning all Glass and Rings build artifacts..."
rm -rf CMakeFiles/Stepper.dir/source/dsp/glass/
rm -rf CMakeFiles/Stepper.dir/Source/dsp/glass/
rm -f CMakeFiles/Stepper.dir/compiler_depend.ts
rm -f CMakeFiles/Stepper.dir/compiler_depend.make

# Build both targets
echo "Building Standalone and AU..."
make -j8 Stepper_Standalone Stepper_AU

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    echo "Installing AU plugin..."
    cp -rf "Stepper_artefacts/Debug/AU/Stepper.component" ~/Library/Audio/Plug-Ins/Components/
    codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/Stepper.component
    
    echo ""
    echo "✅ AU plugin installed and signed"
    echo ""
    echo "Built apps:"
    echo "  Standalone: build/Stepper_artefacts/Debug/Standalone/Stepper.app"
    echo "  AU Plugin: ~/Library/Audio/Plug-Ins/Components/Stepper.component"
    echo ""
    echo "🎉 Ready to test! The Glass effect will now use your real audio instead of a test tone."
else
    echo ""
    echo "❌ Build failed. Check errors above."
    exit 1
fi

