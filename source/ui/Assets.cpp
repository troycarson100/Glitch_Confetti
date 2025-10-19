#include "UiFlags.h"
#include "Assets.h"
#include "BinaryData.h"

static std::unique_ptr<juce::Drawable> loadSVG (const void* data, size_t size) {
    auto in = juce::MemoryInputStream (data, size, false);
    auto svg = juce::Drawable::createFromImageDataStream (in);
    return svg;
}

static std::unique_ptr<juce::Drawable> loadSVGFromFile (const juce::File& f) {
    if (! f.existsAsFile()) return {};
    auto svg = juce::Drawable::createFromImageFile (f);
    return svg;
}

bool UiAssets::loadAll()
{
#if UI_USE_EMBEDDED_SVGS
    using namespace BinaryData; // ensure svgs added to Projucer/CMake resources
    backgroundMustard = loadSVG (Background_Mustard_svg, Background_Mustard_svgSize);
    
    // Load all background variants (effect × tab position)
    spaceDelayBackgroundTab1 = loadSVG (SpaceDelay_Background_Tab1_svg, SpaceDelay_Background_Tab1_svgSize);
    spaceDelayBackgroundTab2 = loadSVG (SpaceDelay_Background_Tab2_svg, SpaceDelay_Background_Tab2_svgSize);
    spaceDelayBackgroundTab3 = loadSVG (SpaceDelay_Background_Tab3_svg, SpaceDelay_Background_Tab3_svgSize);
    spaceDelayBackgroundTab4 = loadSVG (SpaceDelay_Background_Tab4_svg, SpaceDelay_Background_Tab4_svgSize);
    
    pannerBackgroundTab1 = loadSVG (Panner_Background_Tab1_svg, Panner_Background_Tab1_svgSize);
    pannerBackgroundTab2 = loadSVG (Panner_Background_Tab2_svg, Panner_Background_Tab2_svgSize);
    pannerBackgroundTab3 = loadSVG (Panner_Background_Tab3_svg, Panner_Background_Tab3_svgSize);
    pannerBackgroundTab4 = loadSVG (Panner_Background_Tab4_svg, Panner_Background_Tab4_svgSize);
    
    dirtBackgroundTab1 = loadSVG (Dirt_Background_Tab1_svg, Dirt_Background_Tab1_svgSize);
    dirtBackgroundTab2 = loadSVG (Dirt_Background_Tab2_svg, Dirt_Background_Tab2_svgSize);
    dirtBackgroundTab3 = loadSVG (Dirt_Background_Tab3_svg, Dirt_Background_Tab3_svgSize);
    dirtBackgroundTab4 = loadSVG (Dirt_Background_Tab4_svg, Dirt_Background_Tab4_svgSize);
    
    chorusBackgroundTab1 = loadSVG (Chorus_Background_Tab1_svg, Chorus_Background_Tab1_svgSize);
    chorusBackgroundTab2 = loadSVG (Chorus_Background_Tab2_svg, Chorus_Background_Tab2_svgSize);
    chorusBackgroundTab3 = loadSVG (Chorus_Background_Tab3_svg, Chorus_Background_Tab3_svgSize);
    chorusBackgroundTab4 = loadSVG (Chorus_Background_Tab4_svg, Chorus_Background_Tab4_svgSize);
    
    reverbBackgroundTab1 = loadSVG (Reverb1_Background_Tab1_svg, Reverb1_Background_Tab1_svgSize);
    reverbBackgroundTab2 = loadSVG (Reverb1_Background_Tab2_svg, Reverb1_Background_Tab2_svgSize);
    reverbBackgroundTab3 = loadSVG (Reverb1_Background_Tab3_svg, Reverb1_Background_Tab3_svgSize);
    reverbBackgroundTab4 = loadSVG (Reverb1_Background_Tab4_svg, Reverb1_Background_Tab4_svgSize);
    
    granularBackgroundTab1 = loadSVG (Granular_Background_Tab1_svg, Granular_Background_Tab1_svgSize);
    granularBackgroundTab2 = loadSVG (Granular_Background_Tab2_svg, Granular_Background_Tab2_svgSize);
    granularBackgroundTab3 = loadSVG (Granular_Background_Tab3_svg, Granular_Background_Tab3_svgSize);
    granularBackgroundTab4 = loadSVG (Granular_Background_Tab4_svg, Granular_Background_Tab4_svgSize);
    
    slicerBackgroundTab1 = loadSVG (Slicer_Background_Tab1_svg, Slicer_Background_Tab1_svgSize);
    slicerBackgroundTab2 = loadSVG (Slicer_Background_Tab2_svg, Slicer_Background_Tab2_svgSize);
    slicerBackgroundTab3 = loadSVG (Slicer_Background_Tab3_svg, Slicer_Background_Tab3_svgSize);
    slicerBackgroundTab4 = loadSVG (Slicer_Background_Tab4_svg, Slicer_Background_Tab4_svgSize);
    
    effectPlate       = loadSVG (Effect_Background_Plate_svg, Effect_Background_Plate_svgSize);
    stepActive        = loadSVG (Step_Active_svg, Step_Active_svgSize);
    stepInactive      = loadSVG (Step_Inactive_svg, Step_Inactive_svgSize);
    stepTopActive     = loadSVG (Button_Step_Top_Active_svg, Button_Step_Top_Active_svgSize);
    stepTopInactive   = loadSVG (Button_Step_Top_Inactive_svg, Button_Step_Top_Inactive_svgSize);
    knobRing          = loadSVG (Knob_Basic_Ring_svg, Knob_Basic_Ring_svgSize);
    knobInside        = loadSVG (Knob_Basic_Inside_svg, Knob_Basic_Inside_svgSize);
    knobMasterRing    = loadSVG (Knob_Master_Ring_svg, Knob_Master_Ring_svgSize);
    knobMasterInside  = loadSVG (Knob_Master_Inside_svg, Knob_Master_Inside_svgSize);
    tabTitleSpaceDelay= loadSVG (Tab_Title_Space_Delayv2_svg, Tab_Title_Space_Delayv2_svgSize);
    tabTitleAutoPan   = loadSVG (Tab_Title_AutoPan_svg, Tab_Title_AutoPan_svgSize);
    tabDirtIcon       = loadSVG (Dirt_Icon_svg, Dirt_Icon_svgSize);
    tabChorusIcon     = loadSVG (Chorus_Icon_svg, Chorus_Icon_svgSize);
    tabVerbIcon       = loadSVG (Verb_Icon_svg, Verb_Icon_svgSize);
    tabGranularIcon   = loadSVG (Granular_Icon_svg, Granular_Icon_svgSize);
    
    // Load new consistent icons with uniform containing boxes
    tabSpaceIcon      = loadSVG (Space_Icon_svg, Space_Icon_svgSize);
    tabAutoPanIcon    = loadSVG (AutoPan_Icon_svg, AutoPan_Icon_svgSize);
    tabDirtIconNew    = loadSVG (Dirt_Icon_svg, Dirt_Icon_svgSize);
    tabChorusIconNew  = loadSVG (Chorus_Icon_svg, Chorus_Icon_svgSize);
    tabHallIcon       = loadSVG (Hall_Icon_svg, Hall_Icon_svgSize);
    tabGrainIcon      = loadSVG (Grain_Icon_svg, Grain_Icon_svgSize);
    tabSlicerIcon     = loadSVG (Slice_Icon_svg, Slice_Icon_svgSize);
    tabDubDelayIcon   = loadSVG (DubEcho_Icon_svg, DubEcho_Icon_svgSize);
    
    dubdelayBackgroundTab1 = loadSVG (DubEcho_Background_Tab1_svg, DubEcho_Background_Tab1_svgSize);
    dubdelayBackgroundTab2 = loadSVG (DubEcho_Background_Tab2_svg, DubEcho_Background_Tab2_svgSize);
    dubdelayBackgroundTab3 = loadSVG (DubEcho_Background_Tab3_svg, DubEcho_Background_Tab3_svgSize);
    dubdelayBackgroundTab4 = loadSVG (DubEcho_Background_Tab4_svg, DubEcho_Background_Tab4_svgSize);
    
    tabReduxIcon      = loadSVG (Redux_Icon_svg, Redux_Icon_svgSize);
    
    reduxBackgroundTab1 = loadSVG (Redux_Background_Tab1_svg, Redux_Background_Tab1_svgSize);
    reduxBackgroundTab2 = loadSVG (Redux_Background_Tab2_svg, Redux_Background_Tab2_svgSize);
    reduxBackgroundTab3 = loadSVG (Redux_Background_Tab3_svg, Redux_Background_Tab3_svgSize);
    reduxBackgroundTab4 = loadSVG (Redux_Background_Tab4_svg, Redux_Background_Tab4_svgSize);
    
    // PhaseBloom assets - use actual PhaseBloom binary data
    tabPhaseBloomIcon = loadSVG (PhaseBloom_Icon_svg, PhaseBloom_Icon_svgSize);
    phasebloomBackgroundTab1 = loadSVG (PhaseBloom_Background_Tab1_svg, PhaseBloom_Background_Tab1_svgSize);
    phasebloomBackgroundTab2 = loadSVG (PhaseBloom_Background_Tab2_svg, PhaseBloom_Background_Tab2_svgSize);
    phasebloomBackgroundTab3 = loadSVG (PhaseBloom_Background_Tab3_svg, PhaseBloom_Background_Tab3_svgSize);
    phasebloomBackgroundTab4 = loadSVG (PhaseBloom_Background_Tab4_svg, PhaseBloom_Background_Tab4_svgSize);
    
    fxPowerOn         = loadSVG (FX_Power_On_svg, FX_Power_On_svgSize);
    stepPowerOn       = loadSVG (Step_Power_On_svg, Step_Power_On_svgSize);
    knobDice          = loadSVG (Knob_Basic_Dice_svg, Knob_Basic_Dice_svgSize);
    diceLarge         = loadSVG (Dice_Large_svg, Dice_Large_svgSize);
    macroAssign       = loadSVG (Macro_Assign_Button_svg, Macro_Assign_Button_svgSize);
    macro1AssignButton = loadSVG (Macro1_Assign_Button_svg, Macro1_Assign_Button_svgSize);
    macro2AssignButton = loadSVG (Macro2_Assign_Button_svg, Macro2_Assign_Button_svgSize);
    lockedIcon        = loadSVG (Locked_svg, Locked_svgSize);
    unlockedIcon      = loadSVG (Unlocked_svg, Unlocked_svgSize);
    fxTypeCarrotInactive = loadSVG (FX_Type_Carrot_Inactive_svg, FX_Type_Carrot_Inactive_svgSize);
    fxTypeCarrotActive   = loadSVG (FX_Type_Carrot_Active_svg, FX_Type_Carrot_Active_svgSize);
    presetMenuBackground = loadSVG (PresetMenu_Background_svg, PresetMenu_Background_svgSize);
    presetMenuCarrot     = loadSVG (PresetMenu_Carrot_svg, PresetMenu_Carrot_svgSize);
    saveIcon             = loadSVG (Save_Icon_svg, Save_Icon_svgSize);
    compCrushTabInactive = loadSVG (Comp_Crush_Tab_Inactive_svg, Comp_Crush_Tab_Inactive_svgSize);
    compCrushTabActive   = loadSVG (Comp_Crush_Tab_Active_svg, Comp_Crush_Tab_Active_svgSize);
    
    // Load category menu tab PNGs
    favoritesMenuTab = juce::ImageCache::getFromMemory(BinaryData::Favorites_MenuTab_png, BinaryData::Favorites_MenuTab_pngSize);
    rhythmicMenuTab = juce::ImageCache::getFromMemory(BinaryData::Rhythmic_MenuTab_png, BinaryData::Rhythmic_MenuTab_pngSize);
    distortMenuTab = juce::ImageCache::getFromMemory(BinaryData::Distort_MenuTab_png, BinaryData::Distort_MenuTab_pngSize);
    lofiMenuTab = juce::ImageCache::getFromMemory(BinaryData::Lofi_MenuTab_png, BinaryData::Lofi_MenuTab_pngSize);
    bassMenuTab = juce::ImageCache::getFromMemory(BinaryData::Bass_MenuTab_png, BinaryData::Bass_MenuTab_pngSize);
    guitarSynthMenuTab = juce::ImageCache::getFromMemory(BinaryData::GuitarSynth_MenuTab_png, BinaryData::GuitarSynth_MenuTab_pngSize);
    userMenuTab = juce::ImageCache::getFromMemory(BinaryData::User_MenuTab_png, BinaryData::User_MenuTab_pngSize);
#else
    auto assetsDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                        .getSiblingFile ("assets"); // adjust if different
    backgroundMustard = loadSVGFromFile (assetsDir.getChildFile ("Background_Mustard.svg"));
    spaceDelayBackgroundTab1 = loadSVGFromFile (assetsDir.getChildFile ("SpaceDelay_Background_Tab1.svg"));
    effectPlate       = loadSVGFromFile (assetsDir.getChildFile ("Effect_Background_Plate.svg"));
    
    // Redux assets
    tabReduxIcon = loadSVGFromFile (assetsDir.getChildFile ("ui/Redux_Icon.svg"));
    reduxBackgroundTab1 = loadSVGFromFile (assetsDir.getChildFile ("ui/Redux_Background_Tab1.svg"));
    reduxBackgroundTab2 = loadSVGFromFile (assetsDir.getChildFile ("ui/Redux_Background_Tab2.svg"));
    reduxBackgroundTab3 = loadSVGFromFile (assetsDir.getChildFile ("ui/Redux_Background_Tab3.svg"));
    reduxBackgroundTab4 = loadSVGFromFile (assetsDir.getChildFile ("ui/Redux_Background_Tab4.svg"));
    
    // PhaseBloom assets
    tabPhaseBloomIcon = loadSVGFromFile (assetsDir.getChildFile ("ui/PhaseBloom_Icon.svg"));
    phasebloomBackgroundTab1 = loadSVGFromFile (assetsDir.getChildFile ("ui/PhaseBloom_Background_Tab1.svg"));
    phasebloomBackgroundTab2 = loadSVGFromFile (assetsDir.getChildFile ("ui/PhaseBloom_Background_Tab2.svg"));
    phasebloomBackgroundTab3 = loadSVGFromFile (assetsDir.getChildFile ("ui/PhaseBloom_Background_Tab3.svg"));
    phasebloomBackgroundTab4 = loadSVGFromFile (assetsDir.getChildFile ("ui/PhaseBloom_Background_Tab4.svg"));
    
    // Comp Crush Tab assets (loaded via binary data above)
    
    // … repeat
#endif

#if UI_STRICT_NULL_GUARDS
    int ok = 1;
    ok &= (bool) backgroundMustard;
    ok &= (bool) effectPlate;
    ok &= (bool) stepActive && (bool) stepInactive;
    ok &= (bool) knobRing && (bool) knobInside;
    if (!ok) { DBG("[UI] Missing critical SVGs; will use fallback drawing."); }
#endif
    return true;
}
