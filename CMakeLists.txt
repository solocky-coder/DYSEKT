cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SFZero (SoundFont support) ---
include(FetchContent)
FetchContent_Declare(
    sfzero_module
    GIT_REPOSITORY https://github.com
    GIT_TAG        master
)
FetchContent_MakeAvailable(sfzero_module)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)
juce_add_module("${sfzero_module_SOURCE_DIR}")

# --- Bungee Library ---
set(BUNGEE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/bungee)
add_library(pffft STATIC EXCLUDE_FROM_ALL
    ${BUNGEE_ROOT}/submodules/pffft/pffft.c
    ${BUNGEE_ROOT}/submodules/pffft/fftpack.c
)
set_target_properties(pffft PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB BUNGEE_SOURCES "${BUNGEE_ROOT}/src/*.cpp")
add_library(bungee_lib STATIC EXCLUDE_FROM_ALL ${BUNGEE_SOURCES})
target_link_libraries(bungee_lib PRIVATE pffft)

# --- Dysekt Plugin ---
juce_add_plugin(Dysekt
    COMPANY_NAME "DunSoft"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    FORMATS VST3
)

target_sources(Dysekt PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/WaveformView.cpp
    # ... add your other source files here ...
)

target_link_libraries(Dysekt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        bungee_lib
        sfzero  # <--- LINKED HERE
    PUBLIC
        juce::juce_recommended_config_flags
)
