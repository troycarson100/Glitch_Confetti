#!/bin/bash
cd build 2>/dev/null || { mkdir build && cd build; cmake ..; }
make -j2 Stepper_Standalone 2>&1 | tail -100



