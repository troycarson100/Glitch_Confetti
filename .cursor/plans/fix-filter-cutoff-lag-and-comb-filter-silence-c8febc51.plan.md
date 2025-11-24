<!-- c8febc51-1014-4a8f-821e-696510ed8bb7 d4ed840c-9ef7-4ce0-bab2-adc314f87c84 -->
# Fix Plugin Shutdown Crash

## Problem Analysis

The crash occurs when closing Ableton with the plugin loaded. The crash log shows multiple "JUCE Timer" threads, indicating JUCE's shared timer thread is calling callbacks on components being destroyed. Key issues:

1. JUCE Timer callbacks can be queued and fire after `stopTimer()` is called
2. MessageManager may be destroyed before components finish destruction
3. Async operations (`callAsync`, `callAfterDelay`) may fire during shutdown
4. Child component timers may fire after parent starts destruction

## Solution Strategy

Implement a multi-layered shutdown safety system:

1. **Global shutdown flag** checked by all async operations
2. **MessageManager validity checks** before all UI operations
3. **Timer stopping BEFORE destruction** using MessageManagerLock
4. **SafePointer for all async callbacks**
5. **Proper destruction order** ensuring children are stopped before parent

## Implementation Steps

### 1. Add Global Shutdown Flag to PluginProcessor

**File**: `source/PluginProcessor.h`

- Add `static std::atomic<bool> globalShutdownFlag;` to track global shutdown state
- Initialize to `false` in constructor
- Set to `true` in `releaseResources()` and `editorBeingDeleted()`

### 2. Enhance PluginEditor Destructor

**File**: `source/PluginEditor.cpp`

- Use `juce::MessageManagerLock` to prevent timer callbacks during destruction
- Stop ALL timers (including child components) BEFORE any destruction begins
- Clear all async operation queues
- Set global shutdown flag

### 3. Add MessageManagerLock During Timer Stopping

**File**: `source/PluginEditor.cpp` in `~PluginEditor()`

- Wrap timer stopping in `MessageManagerLock` to prevent race conditions
- Stop editor timer first
- Iterate through all child components and stop their timers
- Only then proceed with destruction

### 4. Enhance All Timer Callbacks

**Files**:

- `source/PluginEditor.h` (GainReductionMeter, SmallGainReductionMeter)
- `source/ui/OutputSpectrumView.h`
- `source/DualBarMeter.h`
- `source/ui/PanManBar.h`
- `source/ui/StepSequencer.h`

- Check `PluginProcessor::globalShutdownFlag` at start of each `timerCallback()`
- Check MessageManager validity
- Check parent component validity
- Early return if any check fails

### 5. Enhance PluginEditor::timerCallback()

**File**: `source/PluginEditor.cpp`

- Check `globalShutdownFlag` at very start
- Check MessageManager validity
- Early return if shutdown detected

### 6. Fix All callAsync and callAfterDelay Calls

**Files**:

- `source/PluginEditor.cpp` (checkLicenseOnStartup, showLicenseDialog)
- `source/GumroadLicenseManager.cpp` (run method)
- `source/dsp/SpectrumAnalyzer.h` (setFilterFrequencies)
- `source/ui/StepSequencer.h` (reset button)
- `source/ui/GumroadLicenseDialog.h` (validateAndClose)

- Check `globalShutdownFlag` before posting async messages
- Check MessageManager validity before `callAsync`
- Use SafePointer for all callbacks
- Check shutdown flag inside callback lambda

### 7. Enhance AsyncUpdater Safety

**File**: `source/RandomizationManager.cpp`

- Check `globalShutdownFlag` in `handleAsyncUpdate()`
- Check MessageManager validity
- Early return if shutdown detected

### 8. Add Timer Stopping Helper Function

**File**: `source/PluginEditor.cpp`

- Create `stopAllTimersSafely()` function that:
  - Uses MessageManagerLock
  - Stops editor timer
  - Recursively stops all child component timers
  - Handles exceptions gracefully

### 9. Enhance editorBeingDeleted

**File**: `source/PluginProcessor.cpp`

- Set `globalShutdownFlag` FIRST
- Disable all async operations
- Clear all sequencer states
- Ensure no audio processing can access UI

### 10. Add Final Safety Check in Component Destructors

**Files**: All timer-using components

- In destructors, set a local `isDestroying` flag
- Check this flag in `timerCallback()` if it exists
- This provides defense-in-depth

## Critical Code Changes

### PluginProcessor.h

```cpp
static std::atomic<bool> globalShutdownFlag;
```

### PluginEditor.cpp ~PluginEditor()

```cpp
// Use MessageManagerLock to prevent timer callbacks during destruction
juce::MessageManagerLock mmLock;
if (mmLock.lockWasGained()) {
    stopAllTimersSafely();
}
```

### All timerCallback() methods

```cpp
void timerCallback() override {
    if (PluginProcessor::globalShutdownFlag.load() || 
        juce::MessageManager::getInstanceWithoutCreating() == nullptr ||
        getParentComponent() == nullptr || !isVisible())
        return;
    // ... rest of callback
}
```

## Testing

After implementation, test:

1. Load plugin in Ableton
2. Close Ableton
3. Verify no crash dialog appears
4. Check console for any error messages
5. Test with plugin playing audio
6. Test with plugin UI open
7. Test with sequencers running

## Expected Outcome

- No crash when closing Ableton with plugin loaded
- Clean shutdown without error dialogs
- All timers stopped before component destruction
- All async operations cancelled before shutdown

### To-dos

- [ ] Add static globalShutdownFlag to PluginProcessor.h and initialize in constructor
- [ ] Set globalShutdownFlag in PluginProcessor::releaseResources() and editorBeingDeleted()
- [ ] Enhance PluginEditor destructor to use MessageManagerLock and stop all timers before destruction
- [ ] Create stopAllTimersSafely() helper function in PluginEditor.cpp
- [ ] Add globalShutdownFlag check to PluginEditor::timerCallback()
- [ ] Add globalShutdownFlag checks to all child component timerCallback() methods
- [ ] Add globalShutdownFlag checks to all callAsync and callAfterDelay calls
- [ ] Add globalShutdownFlag check to RandomizationManager::handleAsyncUpdate()
- [ ] Test plugin shutdown in Ableton to verify crash is fixed