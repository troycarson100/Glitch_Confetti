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
    spaceDelayBackgroundTab1 = loadSVG (SpaceDelay_Background_Tab1_svg, SpaceDelay_Background_Tab1_svgSize);
    pannerBackgroundTab2 = loadSVG (Panner_Background_Tab2_svg, Panner_Background_Tab2_svgSize);
    dirtBackgroundTab3 = loadSVG (Dirt_Background_Tab3_svg, Dirt_Background_Tab3_svgSize);
    chorusBackgroundTab4 = loadSVG (Chorus_Background_Tab4_svg, Chorus_Background_Tab4_svgSize);
    effectPlate       = loadSVG (Effect_Background_Plate_svg, Effect_Background_Plate_svgSize);
    stepActive        = loadSVG (Step_Active_svg, Step_Active_svgSize);
    stepInactive      = loadSVG (Step_Inactive_svg, Step_Inactive_svgSize);
    stepTopActive     = loadSVG (Button_Step_Top_Active_svg, Button_Step_Top_Active_svgSize);
    stepTopInactive   = loadSVG (Button_Step_Top_Inactive_svg, Button_Step_Top_Inactive_svgSize);
    knobRing          = loadSVG (Knob_Basic_Ring_svg, Knob_Basic_Ring_svgSize);
    knobInside        = loadSVG (Knob_Basic_Inside_svg, Knob_Basic_Inside_svgSize);
    knobMasterRing    = loadSVG (Knob_Master_Ring_svg, Knob_Master_Ring_svgSize);
    knobMasterInside  = loadSVG (Knob_Master_Inside_svg, Knob_Master_Inside_svgSize);
    tabTitleSpaceDelay= loadSVG (Tab_Title_Space_Delay_svg, Tab_Title_Space_Delay_svgSize);
    tabTitleAutoPan   = loadSVG (Tab_Title_AutoPan_svg, Tab_Title_AutoPan_svgSize);
    tabDirtIcon       = loadSVG (Dirt_Icon_svg, Dirt_Icon_svgSize);
    tabChorusIcon     = loadSVG (Chorus_Icon_svg, Chorus_Icon_svgSize);
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
#else
    auto assetsDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                        .getSiblingFile ("assets"); // adjust if different
    backgroundMustard = loadSVGFromFile (assetsDir.getChildFile ("Background_Mustard.svg"));
    spaceDelayBackgroundTab1 = loadSVGFromFile (assetsDir.getChildFile ("SpaceDelay_Background_Tab1.svg"));
    effectPlate       = loadSVGFromFile (assetsDir.getChildFile ("Effect_Background_Plate.svg"));
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
