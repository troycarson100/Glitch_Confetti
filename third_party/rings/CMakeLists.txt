cmake_minimum_required(VERSION 3.15)

# Create a simple static library for Rings
add_library(rings STATIC
    dsp/resonator.cc
    dsp/string.cc
    dsp/exciter.cc
    dsp/string_synth_part.cc
    dsp/dsp.cc
    dsp/rings_ui.cc
    dsp/rings_voice.cc
    dsp/rings_oscillator.cc
    dsp/rings_modulator.cc
    dsp/rings_voice.cc
)

# Set include directories
target_include_directories(rings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stmlib
)

# Set compile definitions
target_compile_definitions(rings PRIVATE
    RINGS_FLOATING_POINT=1
)

# Set C++ standard
set_target_properties(rings PROPERTIES
    CXX_STANDARD 17
)


