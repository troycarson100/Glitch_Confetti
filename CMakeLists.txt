cmake_minimum_required(VERSION 3.15)
project(Stepper VERSION 1.0.0)

# Use C++17
set(CMAKE_CXX_STANDARD 17)

# Use existing JUCE from PampleTest
add_subdirectory(../PampleTest/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

# Create binary data for assets (GUI XML + SVGs + Fonts)
file(GLOB UI_SVGS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.svg")
file(GLOB FONT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.ttf" "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.otf")
juce_add_binary_data(Assets SOURCES assets/gui.xml ${UI_SVGS} ${FONT_FILES})

# Add the plugin
juce_add_plugin(Stepper
    COMPANY_NAME "GlitchCorp"
    COMPANY_ID "com.glitchcorp"
    BUNDLE_ID "com.glitchcorp.stepper"
    PRODUCT_NAME "Stepper"
    PLUGIN_NAME "Stepper"
    PLUGIN_CODE Stp1
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx"
)

# Source files
set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/StepSnapshot.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/RandomizationManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/RandomizationManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PageTargetRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PageTargetRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/FontManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/FontManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/FxDelay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/FxDelay.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/FxAutoPan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/FxAutoPan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/DspFlags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/granular/GranularEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/granular/GranularEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/UiFlags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/Assets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/Assets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/DelayPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/DelayPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/PanIndicator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/PanIndicator.cpp
)

target_sources(Stepper PRIVATE ${SourceFiles})

# Compile definitions
target_compile_definitions(Stepper
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link libraries
target_link_libraries(Stepper
    PRIVATE
        Assets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Disable code signing for local Debug builds to avoid blocking launches
set_target_properties(Stepper_Standalone PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
)
set_target_properties(Stepper_AU PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
)
set_target_properties(Stepper_VST3 PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
)