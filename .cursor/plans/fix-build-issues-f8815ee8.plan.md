<!-- f8815ee8-e613-4d71-b763-a13c63933429 b65028dc-ab66-4ad6-8476-38b8b7f17957 -->
# Refactor PluginEditor.cpp - Code Organization Plan

## Problem Analysis

The `PluginEditor.cpp` file contains **12,840 lines** of code, which causes:
- Long compilation times
- Difficult debugging and maintenance
- Potential compiler issues with very large files
- Code duplication across similar effect implementations
- Hard to locate specific functionality

## Current Structure (Identified Patterns)

Each effect page follows this pattern:
- Setup functions (`setupSpaceDelayUI`, `setupAutoPanKnobs`, etc.)
- Knob management (8 knobs per effect)
- Step button management (8-16 step buttons)
- FX power button
- Step power button  
- All Steps toggle
- Visibility management functions
- Update functions
- Randomization functions

**Effects:** Space Delay, AutoPan, Dirt, Chorus, Reverb, Granular, Slicer, Dub Delay, Redux, PhaseBloom, COMPRESS+

## Refactoring Strategy

### Phase 1: Create Effect Page Base Class
**File:** `source/ui/EffectPageBase.h/.cpp`
- Abstract base class for common effect page functionality
- Virtual methods for effect-specific behavior
- Shared knob management
- Shared step button management
- Common visibility/update logic

### Phase 2: Extract Individual Effect Pages
Create separate files for each effect (11 files total):

1. `source/ui/pages/SpaceDelayPage.h/.cpp`
2. `source/ui/pages/AutoPanPage.h/.cpp`
3. `source/ui/pages/DirtPage.h/.cpp`
4. `source/ui/pages/ChorusPage.h/.cpp`
5. `source/ui/pages/ReverbPage.h/.cpp`
6. `source/ui/pages/GranularPage.h/.cpp`
7. `source/ui/pages/SlicerPage.h/.cpp`
8. `source/ui/pages/DubDelayPage.h/.cpp`
9. `source/ui/pages/ReduxPage.h/.cpp`
10. `source/ui/pages/PhaseBloomPage.h/.cpp`
11. `source/ui/pages/CompressPage.h/.cpp`

Each page file will contain (~300-500 lines):
- Setup functions for that effect
- Knob/button management
- Update logic specific to that effect
- Visibility management

### Phase 3: Extract Master Section
**Files:** `source/ui/MasterSection.h/.cpp` (~500-800 lines)
- Master knobs setup
- Macro knobs
- Filter controls
- Input/Output meters
- Preset browser integration
- Tab system

### Phase 4: Extract Common UI Components
**Files:**
- `source/ui/SequencerSection.h/.cpp` - Generic sequencer UI management
- `source/ui/PowerButtonManager.h/.cpp` - FX and Step power button logic
- `source/ui/VisibilityManager.h/.cpp` - Common visibility helper functions

### Phase 5: Simplify PluginEditor
**Result:** `source/PluginEditor.cpp` reduced to ~500-1000 lines
- Constructor delegates to effect page objects
- Destructor cleanup
- Paint method (simplified)
- Timer callback (delegates to pages)
- Minimal coordination logic

## Implementation Steps

### Step 1: Create base infrastructure
- Create `source/ui/pages/` directory
- Implement `EffectPageBase` class with common interface
- Test compilation

### Step 2: Extract first effect as template
- Start with Space Delay (most mature implementation)
- Create `SpaceDelayPage` class
- Move all Space Delay code from PluginEditor.cpp
- Update PluginEditor to use SpaceDelayPage
- Test that Space Delay still works

### Step 3: Replicate for remaining effects
- Use Space Delay as template
- Extract each effect one by one
- Test after each extraction
- Maintain git commits for each effect

### Step 4: Extract Master Section
- Create MasterSection class
- Move master area code
- Test master controls

### Step 5: Final cleanup
- Remove empty/redundant code
- Update CMakeLists.txt with new files
- Verify all functionality works
- Commit final refactored state

## Expected Benefits

1. **Reduced file sizes:** Each file will be 300-800 lines instead of 12,840
2. **Faster compilation:** Smaller files compile much faster
3. **Better organization:** Easy to find and modify effect-specific code
4. **Reduced duplication:** Common code in base classes
5. **Easier testing:** Can test individual effect pages
6. **Fix build issues:** Smaller files are less likely to hit compiler limits
7. **Future extensibility:** Easy to add new effects using the pattern

## Risk Mitigation

- Git commit after each extraction
- Test each effect after moving its code
- Keep original PluginEditor.cpp backed up until all tests pass
- Use compiler to catch missing references
- Test both Standalone and AU builds

## Files to Update

- CMakeLists.txt (add new source files)
- PluginEditor.h (include new page headers)
- PluginEditor.cpp (delegate to page objects)
- All new page files (11 effect pages + base class + utility classes)

### To-dos

- [ ] Create source/ui/pages/ directory for effect page files
- [ ] Create EffectPageBase.h/.cpp with common effect page interface and shared functionality
- [ ] Extract Space Delay page as first template - move all Space Delay code to SpaceDelayPage.h/.cpp
- [ ] Test that Space Delay works correctly after extraction
- [ ] Extract AutoPan page following Space Delay pattern
- [ ] Extract Dirt page following Space Delay pattern
- [ ] Extract Chorus page following Space Delay pattern
- [ ] Extract Reverb page following Space Delay pattern
- [ ] Extract Granular page following Space Delay pattern
- [ ] Extract Slicer page following Space Delay pattern
- [ ] Extract Dub Delay page following Space Delay pattern
- [ ] Extract Redux page following Space Delay pattern
- [ ] Extract PhaseBloom page following Space Delay pattern
- [ ] Extract COMPRESS+ page following Space Delay pattern
- [ ] Create MasterSection.h/.cpp and move all master area code
- [ ] Create SequencerSection, PowerButtonManager, and VisibilityManager utility classes
- [ ] Update CMakeLists.txt to include all new source files
- [ ] Test all effects and functionality work correctly after refactoring
- [ ] Verify that the refactored code builds successfully for Standalone and AU