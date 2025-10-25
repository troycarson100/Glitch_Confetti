#!/bin/bash
cd "/Users/troycarson/Documents/JUCE Projects/Stepper/build"
make -j4 Stepper_Standalone 2>&1 | grep -A5 "GlassworksPageDSP.cpp" | tail -30



