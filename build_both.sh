#!/bin/bash

echo "=== Building Both Standalone and AU Versions ==="
echo ""

# Build standalone
echo "Building Standalone..."
cd "/Users/troycarson/Documents/JUCE Projects/Stepper"
xcodebuild -project build/Stepper.xcodeproj -scheme Stepper_Standalone -configuration Debug build

if [ $? -eq 0 ]; then
    echo "✓ Standalone build successful"
else
    echo "✗ Standalone build failed"
    exit 1
fi

echo ""

# Build AU
echo "Building AU..."
xcodebuild -project build/Stepper.xcodeproj -scheme Stepper_AU -configuration Debug build

if [ $? -eq 0 ]; then
    echo "✓ AU build successful"
else
    echo "✗ AU build failed"
    exit 1
fi

echo ""

# Install AU plugin
echo "Installing AU plugin..."
mkdir -p ~/Library/Audio/Plug-Ins/Components/
cp -rf build/Stepper_artefacts/Debug/AU/Stepper.component ~/Library/Audio/Plug-Ins/Components/

if [ $? -eq 0 ]; then
    echo "✓ AU plugin installed"
else
    echo "✗ AU plugin installation failed"
    exit 1
fi

echo ""

# Sign AU plugin
echo "Signing AU plugin..."
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/Stepper.component

if [ $? -eq 0 ]; then
    echo "✓ AU plugin signed"
else
    echo "✗ AU plugin signing failed"
    exit 1
fi

echo ""
echo "=== Both versions built and installed successfully! ==="
echo ""
echo "Standalone: build/Stepper_artefacts/Debug/Standalone/Stepper.app"
echo "AU Plugin: ~/Library/Audio/Plug-Ins/Components/Stepper.component"
