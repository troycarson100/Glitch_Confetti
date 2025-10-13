#!/bin/bash

echo "=== Fixing AU Plugin Signing ==="

AU_PATH="/Users/troycarson/Library/Audio/Plug-Ins/Components/Stepper.component"

# Remove existing signature
echo "Removing existing signature..."
codesign --remove-signature "$AU_PATH" 2>/dev/null

# Clean extended attributes
echo "Cleaning extended attributes..."
xattr -cr "$AU_PATH"

# Remove quarantine attribute
echo "Removing quarantine..."
xattr -d com.apple.quarantine "$AU_PATH" 2>/dev/null

# Sign with ad-hoc signature
echo "Signing with ad-hoc signature..."
codesign --force --deep --sign - "$AU_PATH"

if [ $? -eq 0 ]; then
    echo "✓ AU plugin signed successfully"
else
    echo "✗ Signing failed"
    exit 1
fi

# Verify signature
echo "Verifying signature..."
codesign --verify --verbose "$AU_PATH"

echo ""
echo "=== AU Plugin Ready ==="
echo "You may need to:"
echo "1. Quit and restart your DAW"
echo "2. Or go to System Settings > Privacy & Security > Allow applications from:"
echo "   and allow the Stepper plugin"
