# Effect Router Implementation Plan

## Current Status (Checkpoint: c9e4fe8)

### ✅ COMPLETED
- **EffectRouter Model** (`source/EffectRouter.h`)
  - EffectID enum: SpaceDelay, AutoPan, Dirt, Chorus
  - SlotID enum: Slot1-4 (Page 1-4)
  - Assignment array with 1:1 permutation validation
  - swapSlots() logic with version tracking
  - ValueTree serialization/deserialization
  
- **Processor Integration** (`source/PluginProcessor.h/.cpp`)
  - EffectRouter instance added
  - getEffectRouter() accessor
  - State persistence in getStateInformation/setStateInformation
  - Router validation on load
  
- **Compilation**: ✅ Builds successfully (warnings only)

### 🚧 REMAINING WORK

## Phase 1: UI Dropdowns (2-3 hours)

### 1.1 Add Dropdown Components to PluginEditor.h
```cpp
// Add after tab button declarations (around line 400)
std::unique_ptr<juce::ComboBox> effectSelector1;
std::unique_ptr<juce::ComboBox> effectSelector2;
std::unique_ptr<juce::ComboBox> effectSelector3;
std::unique_ptr<juce::ComboBox> effectSelector4;
```

### 1.2 Create Dropdowns in setupTabSystem() (PluginEditor.cpp)
```cpp
void PluginEditor::setupTabSystem()
{
    // ... existing tab button setup ...
    
    // Create effect selector dropdowns
    for (int i = 0; i < 4; ++i)
    {
        auto* selector = (i == 0 ? effectSelector1.get() :
                          i == 1 ? effectSelector2.get() :
                          i == 2 ? effectSelector3.get() :
                          effectSelector4.get());
        
        selector = new juce::ComboBox();
        selector->addItem("Space Delay", 1);
        selector->addItem("Auto Pan", 2);
        selector->addItem("Dirt", 3);
        selector->addItem("Chorus", 4);
        
        // Set initial selection based on router
        auto& router = processorRef.getEffectRouter();
        EffectID currentEffect = router.getEffectInSlot(static_cast<SlotID>(i));
        selector->setSelectedId(static_cast<int>(currentEffect) + 1, juce::dontSendNotification);
        
        // Position next to tab button (adjust based on your layout)
        selector->setBounds(tabButtonX + tabButtonWidth + 5, tabButtonY, 80, 20);
        
        // Add change listener
        selector->onChange = [this, i]() { onEffectSelectorChanged(i); };
        
        addAndMakeVisible(selector);
    }
}
```

### 1.3 Implement onEffectSelectorChanged()
```cpp
void PluginEditor::onEffectSelectorChanged(int slotIndex)
{
    auto* selector = getEffectSelectorForSlot(slotIndex);
    int selectedEffectID = selector->getSelectedId() - 1; // ComboBox IDs are 1-based
    
    auto& router = processorRef.getEffectRouter();
    EffectID targetEffect = static_cast<EffectID>(selectedEffectID);
    SlotID targetSlot = static_cast<SlotID>(slotIndex);
    
    // This triggers a swap if the effect is already used elsewhere
    router.assignEffectToSlot(targetEffect, targetSlot);
    
    // Update all dropdowns to reflect the swap
    updateAllEffectSelectors();
    
    // Update backgrounds for affected slots
    updateBackgroundsAfterSwap();
    
    // Repaint to show new background
    repaint();
}
```

## Phase 2: Dynamic Backgrounds (1-2 hours)

### 2.1 Add Background Drawable Map to Assets.h
```cpp
struct UiAssets {
    // Existing assets...
    
    // Dynamic backgrounds: effect × slot
    std::unique_ptr<juce::Drawable> spaceDelayBg[4];  // Tab1-4
    std::unique_ptr<juce::Drawable> autoPanBg[4];      // Tab1-4
    std::unique_ptr<juce::Drawable> dirtBg[4];         // Tab1-4
    std::unique_ptr<juce::Drawable> chorusBg[4];       // Tab1-4
};
```

### 2.2 Load All Background Variants in Assets.cpp
```cpp
void UiAssets::loadAll()
{
    // ... existing loads ...
    
    // Load all background variants
    spaceDelayBg[0] = loadSvg("SpaceDelay_Background_Tab1.svg");
    spaceDelayBg[1] = loadSvg("SpaceDelay_Background_Tab2.svg");  // NEW
    spaceDelayBg[2] = loadSvg("SpaceDelay_Background_Tab3.svg");  // EXISTS
    spaceDelayBg[3] = loadSvg("SpaceDelay_Background_Tab4.svg");  // EXISTS
    
    autoPanBg[0] = loadSvg("Panner_Background_Tab1.svg");  // NEW
    autoPanBg[1] = loadSvg("Panner_Background_Tab2.svg");  // EXISTS
    autoPanBg[2] = loadSvg("Panner_Background_Tab3.svg");  // EXISTS
    autoPanBg[3] = loadSvg("Panner_Background_Tab4.svg");  // EXISTS
    
    dirtBg[0] = loadSvg("Dirt_Background_Tab1.svg");  // EXISTS
    dirtBg[1] = loadSvg("Dirt_Background_Tab2.svg");  // EXISTS
    dirtBg[2] = loadSvg("Dirt_Background_Tab3.svg");  // EXISTS
    dirtBg[3] = loadSvg("Dirt_Background_Tab4.svg");  // EXISTS
    
    chorusBg[0] = loadSvg("Chorus_Background_Tab1.svg");  // EXISTS
    chorusBg[1] = loadSvg("Chorus_Background_Tab2.svg");  // EXISTS
    chorusBg[2] = loadSvg("Chorus_Background_Tab3.svg");  // EXISTS
    chorusBg[3] = loadSvg("Chorus_Background_Tab4.svg");  // NEW
}
```

### 2.3 Update paint() to Use Router
```cpp
void PluginEditor::paint(juce::Graphics& g)
{
    // Get current slot's effect assignment
    auto& router = processorRef.getEffectRouter();
    
    // Determine which slot (page) we're on
    int slotIndex = static_cast<int>(currentPage);  // Assumes currentPage maps to slots
    EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slotIndex));
    
    // Get the correct background drawable
    juce::Drawable* background = getBackgroundForEffectAndSlot(effect, slotIndex);
    
    if (background)
    {
        background->drawWithin(g, getLocalBounds().toFloat(), 
                              juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        g.fillAll(juce::Colour(0xff2a2a2a)); // Fallback
    }
    
    // ... rest of paint method ...
}

juce::Drawable* PluginEditor::getBackgroundForEffectAndSlot(EffectID effect, int slot)
{
    switch (effect)
    {
        case EffectID::SpaceDelay: return assets.spaceDelayBg[slot].get();
        case EffectID::AutoPan:    return assets.autoPanBg[slot].get();
        case EffectID::Dirt:       return assets.dirtBg[slot].get();
        case EffectID::Chorus:     return assets.chorusBg[slot].get();
    }
    return nullptr;
}
```

## Phase 3: Component Visibility Swapping (1-2 hours)

### 3.1 Track Effect Groups
Currently we have:
- `spaceDelayGroup` (vector of components)
- `autopanGroup`
- `dirtGroup`
- `chorusGroup`

These need to be shown/hidden based on router assignment, not hardcoded page.

### 3.2 Update showPage() Logic
```cpp
void PluginEditor::showPage(FxPageID pageID)
{
    currentPage = pageID;
    int slotIndex = static_cast<int>(pageID);
    
    // Get effect currently in this slot
    auto& router = processorRef.getEffectRouter();
    EffectID effect = router.getEffectInSlot(static_cast<SlotID>(slotIndex));
    
    // Hide all effect groups
    hideAllGroups(spaceDelayGroup);
    hideAllGroups(autopanGroup);
    hideAllGroups(dirtGroup);
    hideAllGroups(chorusGroup);
    
    // Show the group for the effect assigned to this slot
    switch (effect)
    {
        case EffectID::SpaceDelay: showAllGroups(spaceDelayGroup); break;
        case EffectID::AutoPan:    showAllGroups(autopanGroup); break;
        case EffectID::Dirt:       showAllGroups(dirtGroup); break;
        case EffectID::Chorus:     showAllGroups(chorusGroup); break;
    }
    
    repaint();
}
```

## Phase 4: Dynamic DSP Routing (2-3 hours)

### 4.1 Extract Effect Processing Methods
In `PluginProcessor.cpp`, create these methods (move logic from processBlock):

```cpp
void PluginProcessor::processDelayEffect(juce::AudioBuffer<float>& buffer)
{
    // Move lines 572-576 here (delay processing logic)
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) {
        if (fxEnabled.load())
            spaceDelay.process(buffer, buffer.getNumSamples());
    }
}

void PluginProcessor::processAutoPanEffect(juce::AudioBuffer<float>& buffer)
{
    // Move lines 578-660 here (autopan processing logic)
    auto* autopanEnabledParam = valueTreeState.getRawParameterValue("autopanEnabled");
    bool isAutoPanEnabled = autopanEnabledParam ? (autopanEnabledParam->load() > 0.5f) : false;
    
    if (isAutoPanEnabled) {
        // ... all autopan logic ...
    }
}

void PluginProcessor::processDirtEffect(juce::AudioBuffer<float>& buffer)
{
    // Move lines 662-699 here (dirt processing logic)
    auto* dirtEnabledParam = valueTreeState.getRawParameterValue("dirtEnabled");
    bool isDirtEnabled = dirtEnabledParam ? (dirtEnabledParam->load() > 0.5f) : false;
    
    if (isDirtEnabled) {
        // ... all dirt logic ...
    }
}

void PluginProcessor::processChorusEffect(juce::AudioBuffer<float>& buffer)
{
    // Move lines 701-739 here (chorus processing logic)
    auto* chorusEnabledParam = valueTreeState.getRawParameterValue("chorusEnabled");
    bool isChorusEnabled = chorusEnabledParam ? (chorusEnabledParam->load() > 0.5f) : false;
    
    if (isChorusEnabled) {
        // ... all chorus logic ...
    }
}
```

### 4.2 Replace processBlock Effect Calls with Dynamic Routing
```cpp
void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // ... existing setup code ...
    
    // Store dry signal for master dry/wet mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);
    
    // === DYNAMIC EFFECT ROUTING ===
    // Process effects in page order (Slot1 → Slot2 → Slot3 → Slot4)
    auto routingOrder = effectRouter.getRoutingOrder();
    
    for (int i = 0; i < 4; ++i)
    {
        EffectID effect = routingOrder[i];
        
        switch (effect)
        {
            case EffectID::SpaceDelay:
                processDelayEffect(buffer);
                break;
                
            case EffectID::AutoPan:
                processAutoPanEffect(buffer);
                break;
                
            case EffectID::Dirt:
                processDirtEffect(buffer);
                break;
                
            case EffectID::Chorus:
                processChorusEffect(buffer);
                break;
        }
    }
    
    // ... rest of processBlock (master section, meters, etc.) ...
}
```

### 4.3 Optional: Crossfade on Router Change
To avoid pops when swapping during playback:

```cpp
// In PluginProcessor.h
juce::LinearSmoothedValue<float> routerCrossfade;
juce::AudioBuffer<float> routerBlendBuffer;

// In processBlock
int currentVersion = effectRouter.getRouterVersion();
if (currentVersion != lastRouterVersion)
{
    // Router changed - trigger crossfade
    routerCrossfade.setTargetValue(1.0f);  // Fade to new routing
    lastRouterVersion = currentVersion;
}

// Apply crossfade if active
if (routerCrossfade.isSmoothing())
{
    // Process with new routing into blend buffer
    // ... crossfade logic ...
}
```

## Phase 5: Testing Checklist

### 5.1 Functional Tests
- [ ] Dropdown shows current effect assignment
- [ ] Selecting different effect swaps pages
- [ ] No duplicate effects across pages
- [ ] Background updates correctly after swap
- [ ] Effect groups show/hide correctly
- [ ] Master section unchanged
- [ ] Tab clicks still work

### 5.2 Audio Tests
- [ ] Effects process in page order (1→2→3→4)
- [ ] Swapping pages changes routing order audibly
- [ ] No pops/clicks on swap
- [ ] All sequencers remain independent
- [ ] Effect state preserved after swap

### 5.3 State Tests
- [ ] Save project → reload → assignments restored
- [ ] Effect settings preserved per-effect
- [ ] Sequencer patterns preserved
- [ ] Invalid state handled gracefully

## Implementation Order

1. **Start with UI dropdowns** - Most visible progress
2. **Add background mapping** - Visual confirmation it works
3. **Implement component swapping** - Complete the UI
4. **Add DSP routing** - Make it functionally complete
5. **Test and debug** - Polish and fix edge cases

## Time Estimates

- Phase 1 (UI Dropdowns): 2-3 hours
- Phase 2 (Backgrounds): 1-2 hours
- Phase 3 (Component Swapping): 1-2 hours
- Phase 4 (DSP Routing): 2-3 hours
- Phase 5 (Testing): 1-2 hours

**Total: 7-12 hours of focused implementation**

## Safety Notes

- **Current checkpoint**: Commit `c9e4fe8` - Safe restore point
- **Safety backup**: Commit `d5daa57` - Pre-router implementation
- Each phase should be committed separately
- Test compilation after each major change
- Keep Master section completely untouched

## Missing Assets

Need to create these SVG files:
- `SpaceDelay_Background_Tab2.svg` (copy from Tab1, adjust position markers)
- `Panner_Background_Tab1.svg` (copy from Tab2, adjust position markers)
- `Chorus_Background_Tab4.svg` (copy from Tab3, adjust position markers)

All other Tab1-4 variants already exist in `assets/ui/`.

