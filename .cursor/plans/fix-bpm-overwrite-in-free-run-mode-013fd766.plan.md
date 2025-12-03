<!-- 013fd766-1fe7-42e5-bc9b-2d9a55148030 c0135f1b-beee-4a09-b4f6-5543e49db421 -->
# Fix Sequencer Not Running After Free Run Toggle

## Problem Analysis

After turning free run ON and OFF, the sequencer doesn't run with DAW playback even when manually enabled via the power button. The issue is:

1. `userDisabledSequencer` flag may remain `true` from previous state, blocking auto-enable
2. When manually enabled via button, sequencer needs immediate PPQ lock-in but may not get it
3. The sequencer state isn't being properly synchronized between UI and audio thread

## Solution

### 1. Clear `userDisabledSequencer` when turning off free run

**File**: `source/PluginEditor.cpp` (around line 5034)

- When disabling sequencers after turning off free run, explicitly clear `userDisabledSequencer` flag
- This ensures the sequencer can auto-enable when DAW starts playing

### 2. Ensure sequencer locks-in immediately when manually enabled

**File**: `source/PluginProcessor.cpp` (around line 1095)

- When sequencer is enabled but not active, and DAW is playing with valid PPQ, immediately lock-in
- Also ensure `userDisabledSequencer` is cleared in this path

### 3. Add explicit lock-in trigger when button is clicked

**File**: `source/PluginEditor.cpp` (around line 4455)

- When sequencer power button is clicked ON, ensure `userDisabledSequencer` is cleared
- The audio thread will handle lock-in on next processBlock

### 4. Ensure continuous activation check clears `userDisabledSequencer`

**File**: `source/PluginProcessor.cpp` (around line 1086)

- When auto-enabling sequencer, explicitly clear `userDisabledSequencer` flag
- This ensures the flag doesn't block future auto-enables

### To-dos

- [ ] Clear userDisabledSequencer flag when turning off free run in PluginEditor.cpp togglePlayback()
- [ ] Ensure sequencer locks-in to PPQ immediately when manually enabled via button in PluginProcessor.cpp
- [ ] Explicitly clear userDisabledSequencer flag in all auto-enable paths in PluginProcessor.cpp
- [ ] Test: Turn free run ON, turn OFF, start DAW playback - sequencer should auto-enable and run