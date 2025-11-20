/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace BinaryData
{

//================== gui.xml ==================
static const unsigned char temp_binary_data_0[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"\n"
"<magic>\n"
"  <Styles>\n"
"    <Style name=\"default\" background-color=\"FF1E1E1E\" color=\"FFFFFFFF\"/>\n"
"    <Style name=\"knob\" knob-colour=\"FF9ACD32\" track-colour=\"FF404040\" text-colour=\"FFFFFFFF\"/>\n"
"    <Style name=\"button\" button-colour=\"FF404040\" button-on-colour=\"FFFF8800\" text-colour=\"FFFFFFFF\"/>\n"
"    <Style name=\"header\" background-color=\"FF2A2A2A\" color=\"FF9ACD32\"/>\n"
"    <Style name=\"effects-panel\" background-color=\"FF131313\" border=\"1px solid FF444444\"/>\n"
"    <Style name=\"sequencer-panel\" background-color=\"FF2A2A2A\" border=\"1px solid FF444444\"/>\n"
"  </Styles>\n"
"\n"
"  <View id=\"root\" display-order=\"flexbox\" flex-direction=\"column\" \n"
"        style-class=\"default\" width=\"974\" height=\"532\"\n"
"        background-image=\"ui/Background_Mustard.svg\" background-mode=\"stretch\">\n"
"    \n"
"    <!-- Header Section -->\n"
"    <View id=\"header\" flex=\"0 0 80px\" display-order=\"flexbox\" flex-direction=\"row\" \n"
"          style-class=\"header\" margin=\"5px\" padding=\"15px\" justify-content=\"space-between\">\n"
"      \n"
"      <Label id=\"title\" text=\"STEPPER\" font-size=\"24\" font-weight=\"bold\"\n"
"             text-alignment=\"left\" color=\"FF9ACD32\"/>\n"
"      \n"
"      <Label id=\"subtitle\" text=\"Step Sequencer Plugin\" font-size=\"14\" \n"
"             text-alignment=\"right\" color=\"FFAAAAAA\"/>\n"
"    </View>\n"
"\n"
"    <!-- Main Content Area -->\n"
"    <View id=\"main\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\" \n"
"          margin=\"5px\" padding=\"5px\">\n"
"      \n"
"      <!-- Left Panel: Input Meters -->\n"
"      <View id=\"input-meters\" flex=\"0 0 40px\" display-order=\"flexbox\" flex-direction=\"column\"\n"
"            margin=\"5px\" padding=\"5px\">\n"
"        <Label id=\"input-label\" text=\"IN\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"               text-alignment=\"centred\" margin-bottom=\"5px\"/>\n"
"        <!-- Note: Level meters would need custom components -->\n"
"      </View>\n"
"      \n"
"      <!-- Center Panel: Main Controls -->\n"
"      <View id=\"center\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" \n"
"            margin=\"5px\">\n"
"        \n"
"        <!-- Effect Parameters Section -->\n"
"        <View id=\"effects\" flex=\"0 0 200px\" display-order=\"flexbox\" flex-direction=\"column\"\n"
"              style-class=\"effects-panel\" margin=\"5px\" padding=\"15px\"\n"
"              background-image=\"ui/Effect_Background_Plate.svg\" background-mode=\"maintain-aspect\">\n"
"          \n"
"          <Label id=\"effects-title\" text=\"EFFECTS\" flex=\"0 0 25px\" \n"
"                 font-size=\"14\" font-weight=\"bold\" color=\"FFFFFFFF\" margin-bottom=\"10px\"/>\n"
"          \n"
"          <!-- Knob Grid: 2 rows x 4 columns -->\n"
"          <View id=\"knob-grid\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\">\n"
"            \n"
"            <!-- Top Row: Party, Steps, Density, Reverse -->\n"
"            <View id=\"row1\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\">\n"
"              <View id=\"party-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"party\" parameter=\"party\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"party-label\" text=\"PARTY\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"              <View id=\"steps-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"steps\" parameter=\"steps\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"steps-label\" text=\"STEPS\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"              <View id=\"density-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"density\" parameter=\"density\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"density-label\" text=\"DENSITY\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"              <View id=\"reverse-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"reverse\" parameter=\"rev_pc\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"reverse-label\" text=\"REVERSE\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"            </View>\n"
"            \n"
"            <!-- Bottom Row: Flick, Humanize, Mix, Output -->\n"
"            <View id=\"row2\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\">\n"
"              <View id=\"flick-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"flick\" parameter=\"flick_pc\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"flick-label\" text=\"FLICK\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"              <View id=\"humanize-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"humanize\" parameter=\"humanize\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"humanize-label\" text=\"HUMANIZE\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"              <View id=\"mix-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"mix\" parameter=\"mix\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"mix-label\" text=\"MIX\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"              <View id=\"output-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\" margin=\"3px\">\n"
"                <Slider id=\"output\" parameter=\"out_db\" slider-style=\"rotary\" \n"
"                        style-class=\"knob\" flex=\"1\"/>\n"
"                <Label id=\"output-label\" text=\"OUTPUT\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                       text-alignment=\"centred\" margin-top=\"3px\"/>\n"
"              </View>\n"
"            </View>\n"
"          </View>\n"
"        </View>\n"
"\n"
"        <!-- Step Sequencer Section -->\n"
"        <View id=\"sequencer\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\"\n"
"              style-class=\"sequencer-panel\" margin=\"5px\" padding=\"15px\"\n"
"              background-image=\"ui/Step_Background_Plate.svg\" background-mode=\"maintain-aspect\">\n"
"          \n"
"          <Label id=\"seq-title\" text=\"STEP SEQUENCER\" flex=\"0 0 25px\" \n"
"                 font-size=\"14\" font-weight=\"bold\" color=\"FFFFFFFF\" margin-bottom=\"10px\"/>\n"
"          \n"
"          <!-- Sequencer Controls -->\n"
"          <View id=\"seq-controls\" flex=\"0 0 35px\" display-order=\"flexbox\" flex-direction=\"row\"\n"
"                margin-bottom=\"10px\">\n"
"            <View id=\"division-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\" margin=\"2px\">\n"
"              <Label id=\"div-label\" text=\"DIV:\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                     text-alignment=\"right\" margin-right=\"5px\" flex=\"0 0 30px\"/>\n"
"              <ComboBox id=\"division\" parameter=\"seq_division\" flex=\"1\"\n"
"                        items=\"1/4,1/8,1/16,1/32,Free\"/>\n"
"            </View>\n"
"            <View id=\"steps-used-container\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\" margin=\"2px\">\n"
"              <Label id=\"steps-label\" text=\"STEPS:\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                     text-alignment=\"right\" margin-right=\"5px\" flex=\"0 0 40px\"/>\n"
"              <Slider id=\"steps-used\" parameter=\"seq_steps\" slider-style=\"linear-horizontal\" \n"
"                      flex=\"1\"/>\n"
"            </View>\n"
"            <ToggleButton id=\"follow-host\" text=\"FOLLOW\" parameter=\"seq_follow_host\" \n"
"                         style-class=\"button\" flex=\"0 0 60px\" margin=\"2px\"/>\n"
"            <ToggleButton id=\"run\" text=\"RUN\" parameter=\"seq_run\" \n"
"                         style-class=\"button\" flex=\"0 0 50px\" margin=\"2px\"/>\n"
"          </View>\n"
"          \n"
"          <!-- Step Grid: 2 rows x 8 columns (16 steps total) -->\n"
"          <View id=\"steps\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"column\">\n"
"            \n"
"            <!-- Steps 1-8 -->\n"
"            <View id=\"steps-row1\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\" margin-bottom=\"3px\">\n"
"              <ToggleButton id=\"step1\" text=\"1\" parameter=\"seq_step1\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step2\" text=\"2\" parameter=\"seq_step2\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step3\" text=\"3\" parameter=\"seq_step3\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step4\" text=\"4\" parameter=\"seq_step4\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step5\" text=\"5\" parameter=\"seq_step5\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step6\" text=\"6\" parameter=\"seq_step6\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step7\" text=\"7\" parameter=\"seq_step7\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step8\" text=\"8\" parameter=\"seq_step8\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"            </View>\n"
"            \n"
"            <!-- Steps 9-16 -->\n"
"            <View id=\"steps-row2\" flex=\"1\" display-order=\"flexbox\" flex-direction=\"row\">\n"
"              <ToggleButton id=\"step9\" text=\"9\" parameter=\"seq_step9\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step10\" text=\"10\" parameter=\"seq_step10\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step11\" text=\"11\" parameter=\"seq_step11\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step12\" text=\"12\" parameter=\"seq_step12\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step13\" text=\"13\" parameter=\"seq_step13\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step14\" text=\"14\" parameter=\"seq_step14\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step15\" text=\"15\" parameter=\"seq_step15\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"              <ToggleButton id=\"step16\" text=\"16\" parameter=\"seq_step16\" \n"
"                           style-class=\"button\" flex=\"1\" margin=\"1px\"\n"
"                           background-image=\"ui/Step_Inactive.svg\"\n"
"                           background-on-image=\"ui/Step_Active.svg\"/>\n"
"            </View>\n"
"          </View>\n"
"\n"
"          <!-- Preset Section -->\n"
"          <View id=\"presets\" flex=\"0 0 35px\" display-order=\"flexbox\" flex-direction=\"row\"\n"
"                margin-top=\"10px\">\n"
"            <Label id=\"preset-label\" text=\"PRESET:\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"                   text-alignment=\"right\" margin-right=\"5px\" flex=\"0 0 50px\"/>\n"
"            <ComboBox id=\"preset-combo\" flex=\"1\" margin-right=\"5px\"/>\n"
"            <TextButton id=\"save-preset\" text=\"SAVE\" flex=\"0 0 50px\" \n"
"                       button-colour=\"FF666666\"/>\n"
"          </View>\n"
"        </View>\n"
"      </View>\n"
"      \n"
"      <!-- Right Panel: Output Meters -->\n"
"      <View id=\"output-meters\" flex=\"0 0 40px\" display-order=\"flexbox\" flex-direction=\"column\"\n"
"            margin=\"5px\" padding=\"5px\">\n"
"        <Label id=\"output-label\" text=\"OUT\" font-size=\"10\" color=\"FFAAAAAA\" \n"
"               text-alignment=\"centred\" margin-bottom=\"5px\"/>\n"
"        <!-- Note: Level meters would need custom components -->\n"
"      </View>\n"
"    </View>\n"
"  </View>\n"
"</magic>";

const char* gui_xml = (const char*) temp_binary_data_0;

}

#include "BinaryData.h"

namespace BinaryData
{

const char* getNamedResource (const char* resourceNameUTF8, int& numBytes);
const char* getNamedResource (const char* resourceNameUTF8, int& numBytes)
{
    unsigned int hash = 0;

    if (resourceNameUTF8 != nullptr)
        while (*resourceNameUTF8 != 0)
            hash = 31 * hash + (unsigned int) *resourceNameUTF8++;

    switch (hash)
    {
        case 0x163d7b13:  numBytes = 14169; return gui_xml;
        case 0xc1a2f28f:  numBytes = 7155; return AutoPan_Icon_svg;
        case 0xe214babe:  numBytes = 3904; return Background_Mustard_svg;
        case 0x187f005b:  numBytes = 308; return Button_Step_Top_Active_svg;
        case 0x2759b740:  numBytes = 217; return Button_Step_Top_Inactive_svg;
        case 0x74a27ad9:  numBytes = 4991; return Chorus_Background_Tab1_svg;
        case 0x74b0925a:  numBytes = 5183; return Chorus_Background_Tab2_svg;
        case 0x74bea9db:  numBytes = 5248; return Chorus_Background_Tab3_svg;
        case 0x74ccc15c:  numBytes = 5257; return Chorus_Background_Tab4_svg;
        case 0xc53e8c17:  numBytes = 4446; return Chorus_Icon_svg;
        case 0xdf8efa09:  numBytes = 955; return Comp_Crush_Tab_Active_svg;
        case 0x6a51fd6e:  numBytes = 955; return Comp_Crush_Tab_Inactive_svg;
        case 0xd67e60c8:  numBytes = 21950; return Dice_Large_svg;
        case 0xce28d43a:  numBytes = 4991; return Dirt_Background_Tab1_svg;
        case 0xce36ebbb:  numBytes = 5188; return Dirt_Background_Tab2_svg;
        case 0xce45033c:  numBytes = 5352; return Dirt_Background_Tab3_svg;
        case 0xce531abd:  numBytes = 5269; return Dirt_Background_Tab4_svg;
        case 0x5f124556:  numBytes = 3153; return Dirt_Icon_svg;
        case 0x25f9d20f:  numBytes = 18163; return Drive_svg;
        case 0x3923a0a9:  numBytes = 4991; return DubEcho_Background_Tab1_svg;
        case 0x3931b82a:  numBytes = 5183; return DubEcho_Background_Tab2_svg;
        case 0x393fcfab:  numBytes = 5248; return DubEcho_Background_Tab3_svg;
        case 0x394de72c:  numBytes = 5257; return DubEcho_Background_Tab4_svg;
        case 0xfbfae847:  numBytes = 2710; return DubEcho_Icon_svg;
        case 0xcf336db8:  numBytes = 1235; return Effect_Background_Plate_svg;
        case 0x2d6c2f0b:  numBytes = 1573; return FX_Power_On_svg;
        case 0x1988464f:  numBytes = 203; return FX_Type_Carrot_Active_svg;
        case 0x0b295034:  numBytes = 343; return FX_Type_Carrot_Inactive_svg;
        case 0xa2f2e2ca:  numBytes = 37363; return Feedback_svg;
        case 0x29c8a7ab:  numBytes = 6425; return Filter_Background_Tab1_svg;
        case 0x29d6bf2c:  numBytes = 6663; return Filter_Background_Tab2_svg;
        case 0x29e4d6ad:  numBytes = 6693; return Filter_Background_Tab3_svg;
        case 0x29f2ee2e:  numBytes = 6703; return Filter_Background_Tab4_svg;
        case 0xb5f42e85:  numBytes = 6767; return Filter_Icon_svg;
        case 0x035354e1:  numBytes = 6438; return Form2_Background_Tab1_svg;
        case 0x03616c62:  numBytes = 6630; return Form2_Background_Tab2_svg;
        case 0xe1ffe696:  numBytes = 6684; return Form2_Background_Tab3_2_svg;
        case 0x036f83e3:  numBytes = 6684; return Form2_Background_Tab3_svg;
        case 0x037d9b64:  numBytes = 6682; return Form2_Background_Tab4_svg;
        case 0x2b26d2f7:  numBytes = 6498; return Form_Background_Tab1_svg;
        case 0x2b34ea78:  numBytes = 6760; return Form_Background_Tab2_svg;
        case 0x2b4301f9:  numBytes = 6749; return Form_Background_Tab3_svg;
        case 0x2b51197a:  numBytes = 6722; return Form_Background_Tab4_svg;
        case 0xbc401ab9:  numBytes = 2709; return Form_Icon_svg;
        case 0xabd04122:  numBytes = 6736; return Grain_Icon_svg;
        case 0xb122bf93:  numBytes = 4979; return Granular_Background_Tab1_svg;
        case 0xb130d714:  numBytes = 5169; return Granular_Background_Tab2_svg;
        case 0xb13eee95:  numBytes = 5236; return Granular_Background_Tab3_svg;
        case 0xb14d0616:  numBytes = 5251; return Granular_Background_Tab4_svg;
        case 0x3cf9f79d:  numBytes = 2046; return Granular_Icon_svg;
        case 0xdfc815b5:  numBytes = 6736; return Group_220_svg;
        case 0xbbe97ae4:  numBytes = 4551; return Hall_Icon_svg;
        case 0x1727eb0d:  numBytes = 1730; return Heat_Icon_svg;
        case 0x93eca6c5:  numBytes = 29926; return HighCut_svg;
        case 0xfba89bc6:  numBytes = 418; return Knob_Basic_Dice_svg;
        case 0xb81a971b:  numBytes = 759; return Knob_Basic_Inside_svg;
        case 0xbcda8dcf:  numBytes = 3534; return Knob_Basic_Ring_svg;
        case 0x0a7180bd:  numBytes = 777; return Knob_Macro_Inside_svg;
        case 0xa5c377f1:  numBytes = 3525; return Knob_Macro_Ring_svg;
        case 0x93d72835:  numBytes = 555; return Knob_Master_Inside_svg;
        case 0x90dc9d69:  numBytes = 3551; return Knob_Master_Ring_svg;
        case 0xc9d067cf:  numBytes = 388; return Locked_svg;
        case 0xbf9da913:  numBytes = 25949; return LowCut_svg;
        case 0x2d549d6d:  numBytes = 2708; return Macro1_Assign_Button_svg;
        case 0xfafeff2e:  numBytes = 2712; return Macro2_Assign_Button_svg;
        case 0x51240354:  numBytes = 2706; return Macro_Assign_Button_svg;
        case 0x6cba58d9:  numBytes = 1234; return Maste_Background_Plate_svg;
        case 0xa3349de1:  numBytes = 12015; return Mix_svg;
        case 0x559d0511:  numBytes = 4991; return Panner_Background_Tab1_svg;
        case 0x5d271c09:  numBytes = 5192; return Panner_Background_Tab21_svg;
        case 0x55ab1c92:  numBytes = 5296; return Panner_Background_Tab2_svg;
        case 0x55b93413:  numBytes = 5209; return Panner_Background_Tab3_svg;
        case 0x55c74b94:  numBytes = 5265; return Panner_Background_Tab4_svg;
        case 0xcc0b457b:  numBytes = 4979; return PhaseBloom_Background_Tab1_svg;
        case 0xcc195cfc:  numBytes = 5169; return PhaseBloom_Background_Tab2_svg;
        case 0xcc27747d:  numBytes = 5230; return PhaseBloom_Background_Tab3_svg;
        case 0xcc358bfe:  numBytes = 5248; return PhaseBloom_Background_Tab4_svg;
        case 0x192a12b5:  numBytes = 7419; return PhaseBloom_Icon_svg;
        case 0xc55f9954:  numBytes = 199; return PresetMenu_Background_svg;
        case 0xa78433a9:  numBytes = 204; return PresetMenu_Carrot_svg;
        case 0xf4d1f0e7:  numBytes = 4958; return Redux_Background_Tab1_svg;
        case 0xf4e00868:  numBytes = 5186; return Redux_Background_Tab2_svg;
        case 0xf4ee1fe9:  numBytes = 5220; return Redux_Background_Tab3_svg;
        case 0xf4fc376a:  numBytes = 5212; return Redux_Background_Tab4_svg;
        case 0xe05c52c9:  numBytes = 4678; return Redux_Icon_svg;
        case 0x36af5f12:  numBytes = 4946; return Reverb1_Background_Tab1_svg;
        case 0x36bd7693:  numBytes = 5178; return Reverb1_Background_Tab2_svg;
        case 0x36cb8e14:  numBytes = 5215; return Reverb1_Background_Tab3_svg;
        case 0x36d9a595:  numBytes = 5233; return Reverb1_Background_Tab4_svg;
        case 0x21026162:  numBytes = 6438; return Saturate_Background_Tab1_svg;
        case 0x211078e3:  numBytes = 6630; return Saturate_Background_Tab2_svg;
        case 0x211e9064:  numBytes = 6684; return Saturate_Background_Tab3_svg;
        case 0x212ca7e5:  numBytes = 6682; return Saturate_Background_Tab4_svg;
        case 0x67ad612e:  numBytes = 3237; return Saturate_Icon_svg;
        case 0x1e77a620:  numBytes = 355; return Save_Icon_svg;
        case 0xf2947e74:  numBytes = 6442; return Shimmer_Background_Tab1_svg;
        case 0xf2a295f5:  numBytes = 6680; return Shimmer_Background_Tab2_svg;
        case 0xf2b0ad76:  numBytes = 6692; return Shimmer_Background_Tab3_svg;
        case 0xf2bec4f7:  numBytes = 6696; return Shimmer_Background_Tab4_svg;
        case 0x800abb5c:  numBytes = 3424; return Shimmer_Icon_svg;
        case 0xd4bf8deb:  numBytes = 1983; return Slice_Icon_svg;
        case 0xed12ed53:  numBytes = 4970; return Slicer_Background_Tab1_svg;
        case 0xed2104d4:  numBytes = 5188; return Slicer_Background_Tab2_svg;
        case 0xed2f1c55:  numBytes = 5241; return Slicer_Background_Tab3_svg;
        case 0xed3d33d6:  numBytes = 5235; return Slicer_Background_Tab4_svg;
        case 0x051b93d0:  numBytes = 5003; return SpaceDelay_Background_Tab1_svg;
        case 0x0529ab51:  numBytes = 5192; return SpaceDelay_Background_Tab2_svg;
        case 0x0537c2d2:  numBytes = 5217; return SpaceDelay_Background_Tab3_svg;
        case 0x0545da53:  numBytes = 5245; return SpaceDelay_Background_Tab4_svg;
        case 0x513e7cf7:  numBytes = 5614; return Space_Icon_svg;
        case 0xfbbe842f:  numBytes = 4982; return SpectralFilter_Background_Tab1_svg;
        case 0xfbcc9bb0:  numBytes = 5210; return SpectralFilter_Background_Tab2_svg;
        case 0xfbdab331:  numBytes = 5244; return SpectralFilter_Background_Tab3_svg;
        case 0xfbe8cab2:  numBytes = 5235; return SpectralFilter_Background_Tab4_svg;
        case 0x9e8afc81:  numBytes = 4352; return SpectralFilter_Icon_svg;
        case 0x239664fe:  numBytes = 309; return Step_Active_svg;
        case 0x0e90bb5d:  numBytes = 1222; return Step_Background_Plate_svg;
        case 0xca2a7f23:  numBytes = 218; return Step_Inactive_svg;
        case 0x98f47731:  numBytes = 1586; return Step_Power_On_svg;
        case 0x67059122:  numBytes = 3313; return Tab_Title_AutoPan_svg;
        case 0x7e84025a:  numBytes = 2998; return Tab_Title_Space_Delayv2_svg;
        case 0xd3c801f0:  numBytes = 2998; return Tab_Title_Space_Delayv2_svg_backup2_svg;
        case 0x880d7612:  numBytes = 13971; return Time_svg;
        case 0xdb3f4328:  numBytes = 412; return Unlocked_svg;
        case 0xd85830be:  numBytes = 4565; return Verb_Icon_svg;
        case 0x90b4bb08:  numBytes = 41683; return Wow_Depth_svg;
        case 0xdd1caa65:  numBytes = 39199; return Wow_Rate_svg;
        case 0x09e052c0:  numBytes = 571776; return Bass_MenuTab_png;
        case 0x8c69252c:  numBytes = 315151; return Distort_MenuTab_png;
        case 0xe1d54df8:  numBytes = 690654; return Favorites_MenuTab_png;
        case 0x7f223bf3:  numBytes = 294819; return GuitarSynth_MenuTab_png;
        case 0xfa314ee7:  numBytes = 571765; return Lofi_MenuTab_png;
        case 0x35f0ce91:  numBytes = 274948; return Rhythmic_MenuTab_png;
        case 0xd07d35ac:  numBytes = 197682; return User_MenuTab_png;
        case 0x2b192f8c:  numBytes = 26320; return Akira_Expanded_otf;
        case 0x38dbefb2:  numBytes = 144556; return AlteHaasGroteskBold_ttf;
        case 0x156c7e7d:  numBytes = 143896; return AlteHaasGroteskRegular_ttf;
        default: break;
    }

    numBytes = 0;
    return nullptr;
}

const char* namedResourceList[] =
{
    "gui_xml",
    "AutoPan_Icon_svg",
    "Background_Mustard_svg",
    "Button_Step_Top_Active_svg",
    "Button_Step_Top_Inactive_svg",
    "Chorus_Background_Tab1_svg",
    "Chorus_Background_Tab2_svg",
    "Chorus_Background_Tab3_svg",
    "Chorus_Background_Tab4_svg",
    "Chorus_Icon_svg",
    "Comp_Crush_Tab_Active_svg",
    "Comp_Crush_Tab_Inactive_svg",
    "Dice_Large_svg",
    "Dirt_Background_Tab1_svg",
    "Dirt_Background_Tab2_svg",
    "Dirt_Background_Tab3_svg",
    "Dirt_Background_Tab4_svg",
    "Dirt_Icon_svg",
    "Drive_svg",
    "DubEcho_Background_Tab1_svg",
    "DubEcho_Background_Tab2_svg",
    "DubEcho_Background_Tab3_svg",
    "DubEcho_Background_Tab4_svg",
    "DubEcho_Icon_svg",
    "Effect_Background_Plate_svg",
    "FX_Power_On_svg",
    "FX_Type_Carrot_Active_svg",
    "FX_Type_Carrot_Inactive_svg",
    "Feedback_svg",
    "Filter_Background_Tab1_svg",
    "Filter_Background_Tab2_svg",
    "Filter_Background_Tab3_svg",
    "Filter_Background_Tab4_svg",
    "Filter_Icon_svg",
    "Form2_Background_Tab1_svg",
    "Form2_Background_Tab2_svg",
    "Form2_Background_Tab3_2_svg",
    "Form2_Background_Tab3_svg",
    "Form2_Background_Tab4_svg",
    "Form_Background_Tab1_svg",
    "Form_Background_Tab2_svg",
    "Form_Background_Tab3_svg",
    "Form_Background_Tab4_svg",
    "Form_Icon_svg",
    "Grain_Icon_svg",
    "Granular_Background_Tab1_svg",
    "Granular_Background_Tab2_svg",
    "Granular_Background_Tab3_svg",
    "Granular_Background_Tab4_svg",
    "Granular_Icon_svg",
    "Group_220_svg",
    "Hall_Icon_svg",
    "Heat_Icon_svg",
    "HighCut_svg",
    "Knob_Basic_Dice_svg",
    "Knob_Basic_Inside_svg",
    "Knob_Basic_Ring_svg",
    "Knob_Macro_Inside_svg",
    "Knob_Macro_Ring_svg",
    "Knob_Master_Inside_svg",
    "Knob_Master_Ring_svg",
    "Locked_svg",
    "LowCut_svg",
    "Macro1_Assign_Button_svg",
    "Macro2_Assign_Button_svg",
    "Macro_Assign_Button_svg",
    "Maste_Background_Plate_svg",
    "Mix_svg",
    "Panner_Background_Tab1_svg",
    "Panner_Background_Tab21_svg",
    "Panner_Background_Tab2_svg",
    "Panner_Background_Tab3_svg",
    "Panner_Background_Tab4_svg",
    "PhaseBloom_Background_Tab1_svg",
    "PhaseBloom_Background_Tab2_svg",
    "PhaseBloom_Background_Tab3_svg",
    "PhaseBloom_Background_Tab4_svg",
    "PhaseBloom_Icon_svg",
    "PresetMenu_Background_svg",
    "PresetMenu_Carrot_svg",
    "Redux_Background_Tab1_svg",
    "Redux_Background_Tab2_svg",
    "Redux_Background_Tab3_svg",
    "Redux_Background_Tab4_svg",
    "Redux_Icon_svg",
    "Reverb1_Background_Tab1_svg",
    "Reverb1_Background_Tab2_svg",
    "Reverb1_Background_Tab3_svg",
    "Reverb1_Background_Tab4_svg",
    "Saturate_Background_Tab1_svg",
    "Saturate_Background_Tab2_svg",
    "Saturate_Background_Tab3_svg",
    "Saturate_Background_Tab4_svg",
    "Saturate_Icon_svg",
    "Save_Icon_svg",
    "Shimmer_Background_Tab1_svg",
    "Shimmer_Background_Tab2_svg",
    "Shimmer_Background_Tab3_svg",
    "Shimmer_Background_Tab4_svg",
    "Shimmer_Icon_svg",
    "Slice_Icon_svg",
    "Slicer_Background_Tab1_svg",
    "Slicer_Background_Tab2_svg",
    "Slicer_Background_Tab3_svg",
    "Slicer_Background_Tab4_svg",
    "SpaceDelay_Background_Tab1_svg",
    "SpaceDelay_Background_Tab2_svg",
    "SpaceDelay_Background_Tab3_svg",
    "SpaceDelay_Background_Tab4_svg",
    "Space_Icon_svg",
    "SpectralFilter_Background_Tab1_svg",
    "SpectralFilter_Background_Tab2_svg",
    "SpectralFilter_Background_Tab3_svg",
    "SpectralFilter_Background_Tab4_svg",
    "SpectralFilter_Icon_svg",
    "Step_Active_svg",
    "Step_Background_Plate_svg",
    "Step_Inactive_svg",
    "Step_Power_On_svg",
    "Tab_Title_AutoPan_svg",
    "Tab_Title_Space_Delayv2_svg",
    "Tab_Title_Space_Delayv2_svg_backup2_svg",
    "Time_svg",
    "Unlocked_svg",
    "Verb_Icon_svg",
    "Wow_Depth_svg",
    "Wow_Rate_svg",
    "Bass_MenuTab_png",
    "Distort_MenuTab_png",
    "Favorites_MenuTab_png",
    "GuitarSynth_MenuTab_png",
    "Lofi_MenuTab_png",
    "Rhythmic_MenuTab_png",
    "User_MenuTab_png",
    "Akira_Expanded_otf",
    "AlteHaasGroteskBold_ttf",
    "AlteHaasGroteskRegular_ttf"
};

const char* originalFilenames[] =
{
    "gui.xml",
    "AutoPan_Icon.svg",
    "Background_Mustard.svg",
    "Button_Step_Top_Active.svg",
    "Button_Step_Top_Inactive.svg",
    "Chorus_Background_Tab1.svg",
    "Chorus_Background_Tab2.svg",
    "Chorus_Background_Tab3.svg",
    "Chorus_Background_Tab4.svg",
    "Chorus_Icon.svg",
    "Comp_Crush_Tab_Active.svg",
    "Comp_Crush_Tab_Inactive.svg",
    "Dice_Large.svg",
    "Dirt_Background_Tab1.svg",
    "Dirt_Background_Tab2.svg",
    "Dirt_Background_Tab3.svg",
    "Dirt_Background_Tab4.svg",
    "Dirt_Icon.svg",
    "Drive.svg",
    "DubEcho_Background_Tab1.svg",
    "DubEcho_Background_Tab2.svg",
    "DubEcho_Background_Tab3.svg",
    "DubEcho_Background_Tab4.svg",
    "DubEcho_Icon.svg",
    "Effect_Background_Plate.svg",
    "FX_Power_On.svg",
    "FX_Type_Carrot_Active.svg",
    "FX_Type_Carrot_Inactive.svg",
    "Feedback.svg",
    "Filter_Background_Tab1.svg",
    "Filter_Background_Tab2.svg",
    "Filter_Background_Tab3.svg",
    "Filter_Background_Tab4.svg",
    "Filter_Icon.svg",
    "Form2_Background_Tab1.svg",
    "Form2_Background_Tab2.svg",
    "Form2_Background_Tab3 2.svg",
    "Form2_Background_Tab3.svg",
    "Form2_Background_Tab4.svg",
    "Form_Background_Tab1.svg",
    "Form_Background_Tab2.svg",
    "Form_Background_Tab3.svg",
    "Form_Background_Tab4.svg",
    "Form_Icon.svg",
    "Grain_Icon.svg",
    "Granular_Background_Tab1.svg",
    "Granular_Background_Tab2.svg",
    "Granular_Background_Tab3.svg",
    "Granular_Background_Tab4.svg",
    "Granular_Icon.svg",
    "Group 220.svg",
    "Hall_Icon.svg",
    "Heat_Icon.svg",
    "High-Cut.svg",
    "Knob_Basic_Dice.svg",
    "Knob_Basic_Inside.svg",
    "Knob_Basic_Ring.svg",
    "Knob_Macro_Inside.svg",
    "Knob_Macro_Ring.svg",
    "Knob_Master_Inside.svg",
    "Knob_Master_Ring.svg",
    "Locked.svg",
    "Low-Cut.svg",
    "Macro1_Assign_Button.svg",
    "Macro2_Assign_Button.svg",
    "Macro_Assign_Button.svg",
    "Maste_Background_Plate.svg",
    "Mix.svg",
    "Panner_Background_Tab1.svg",
    "Panner_Background_Tab2-1.svg",
    "Panner_Background_Tab2.svg",
    "Panner_Background_Tab3.svg",
    "Panner_Background_Tab4.svg",
    "PhaseBloom_Background_Tab1.svg",
    "PhaseBloom_Background_Tab2.svg",
    "PhaseBloom_Background_Tab3.svg",
    "PhaseBloom_Background_Tab4.svg",
    "PhaseBloom_Icon.svg",
    "PresetMenu_Background.svg",
    "PresetMenu_Carrot.svg",
    "Redux_Background_Tab1.svg",
    "Redux_Background_Tab2.svg",
    "Redux_Background_Tab3.svg",
    "Redux_Background_Tab4.svg",
    "Redux_Icon.svg",
    "Reverb1_Background_Tab1.svg",
    "Reverb1_Background_Tab2.svg",
    "Reverb1_Background_Tab3.svg",
    "Reverb1_Background_Tab4.svg",
    "Saturate_Background_Tab1.svg",
    "Saturate_Background_Tab2.svg",
    "Saturate_Background_Tab3.svg",
    "Saturate_Background_Tab4.svg",
    "Saturate_Icon.svg",
    "Save_Icon.svg",
    "Shimmer_Background_Tab1.svg",
    "Shimmer_Background_Tab2.svg",
    "Shimmer_Background_Tab3.svg",
    "Shimmer_Background_Tab4.svg",
    "Shimmer_Icon.svg",
    "Slice_Icon.svg",
    "Slicer_Background_Tab1.svg",
    "Slicer_Background_Tab2.svg",
    "Slicer_Background_Tab3.svg",
    "Slicer_Background_Tab4.svg",
    "SpaceDelay_Background_Tab1.svg",
    "SpaceDelay_Background_Tab2.svg",
    "SpaceDelay_Background_Tab3.svg",
    "SpaceDelay_Background_Tab4.svg",
    "Space_Icon.svg",
    "SpectralFilter_Background_Tab1.svg",
    "SpectralFilter_Background_Tab2.svg",
    "SpectralFilter_Background_Tab3.svg",
    "SpectralFilter_Background_Tab4.svg",
    "SpectralFilter_Icon.svg",
    "Step_Active.svg",
    "Step_Background_Plate.svg",
    "Step_Inactive.svg",
    "Step_Power_On.svg",
    "Tab_Title_AutoPan.svg",
    "Tab_Title_Space_Delay-v2.svg",
    "Tab_Title_Space_Delay-v2.svg.backup2.svg",
    "Time.svg",
    "Unlocked.svg",
    "Verb_Icon.svg",
    "Wow Depth.svg",
    "Wow Rate.svg",
    "Bass_MenuTab.png",
    "Distort_MenuTab.png",
    "Favorites_MenuTab.png",
    "GuitarSynth_MenuTab.png",
    "Lofi_MenuTab.png",
    "Rhythmic_MenuTab.png",
    "User_MenuTab.png",
    "Akira Expanded.otf",
    "AlteHaasGroteskBold.ttf",
    "AlteHaasGroteskRegular.ttf"
};

const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8)
{
    for (unsigned int i = 0; i < (sizeof (namedResourceList) / sizeof (namedResourceList[0])); ++i)
        if (strcmp (namedResourceList[i], resourceNameUTF8) == 0)
            return originalFilenames[i];

    return nullptr;
}

}
