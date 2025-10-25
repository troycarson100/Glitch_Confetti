#!/bin/bash

echo "=== Rebuilding Stepper with Fixed CMake Configuration ==="
echo "Using JUCE_backup (complete JUCE installation)"
echo ""

cd "/Users/troycarson/Documents/JUCE Projects/Stepper"

# Clean build directory
echo "Step 1: Cleaning build directory..."
rm -rf build
mkdir build

# Run CMake
echo "Step 2: Running CMake..."
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi

echo ""
echo "Step 3: Building (this takes 3-5 minutes)..."
# Try with fewer parallel jobs to avoid filesystem race conditions
make -j2 Stepper_Standalone Stepper_AU

if [ $? -ne 0 ]; then
    echo "⚠️  Parallel build failed, retrying with single thread..."
    make Stepper_Standalone Stepper_AU
    
    if [ $? -ne 0 ]; then
        echo "❌ Build failed"
        exit 1
    fi
fi

echo ""
echo "Step 4: Installing AU plugin..."
cp -rf "Stepper_artefacts/Debug/AU/Stepper.component" ~/Library/Audio/Plug-Ins/Components/
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/Stepper.component

echo ""
echo "✅ Build complete!"
echo ""
echo "Standalone: build/Stepper_artefacts/Debug/Standalone/Stepper.app"
echo "AU Plugin: ~/Library/Audio/Plug-Ins/Components/Stepper.component"
echo ""
echo "🎉 Glass effect should now work with your audio (no test tone or buzz)"

