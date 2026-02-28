cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. SFZero (SoundFont support) ---
include(FetchContent)
FetchContent_Declare(
    sfzero_module
    GIT_REPOSITORY https://github.com
    GIT_TAG        master
)
FetchContent_MakeAvailable(sfzero_module)

# --- 2. JUCE Setup ---
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)
juce_add_module("${sfzero_module_SOURCE_DIR}")

# --- 3. Bungee Library ---
set(BUNGEE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/bungee)
add_library(pffft STATIC EXCLUDE_FROM_ALL
    ${BUNGEE_ROOT}/submodules/pffft/pffft.c
    ${BUNGEE_ROOT}/submodules/pffft/fftpack.c
)
set_target_properties(pffft PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB BUNGEE_SOURCES "${BUNGEE_ROOT}/src/*.cpp")
add_library(bungee_lib STATIC EXCLUDE_FROM_ALL ${BUNGEE_SOURCES})
target_link_libraries(bungee_lib PRIVATE pffft)

# --- 4. Dysekt Plugin Definition ---
juce_add_plugin(Dysekt
    COMPANY_NAME "DunSoft"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    FORMATS VST3
    PRODUCT_NAME "DYSEKT"
)

juce_add_binary_data(DysektFonts SOURCES
    src/ui/fonts/IBMPlexSans-Regular.ttf
    src/ui/fonts/IBMPlexSans-Bold.ttf
    src/ui/assets/dysekt_logo.svg
)

target_sources(Dysekt PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/audio/SampleData.cpp
    src/audio/SliceManager.cpp
    src/audio/VoicePool.cpp
    src/audio/GrainEngine.cpp
    src/audio/LazyChopEngine.cpp
    src/params/ParamLayout.cpp
    src/ui/DysektLookAndFeel.cpp
    src/ui/HeaderBar.cpp
    src/ui/SliceControlBar.cpp
    src/ui/WaveformView.cpp
    src/ui/WaveformCache.cpp
    src/ui/ScrollZoomBar.cpp
    src/ui/SliceLane.cpp
    src/ui/ActionPanel.cpp
    src/ui/AutoChopPanel.cpp
    src/ui/LogoBar.cpp
    src/ui/KeysPanel.cpp
    src/ui/FileBrowserPanel.cpp
)

target_include_directories(Dysekt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/signalsmith-stretch
    ${CMAKE_CURRENT_SOURCE_DIR}/signalsmith-linear
    ${BUNGEE_ROOT}
)

target_link_libraries(Dysekt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        bungee_lib
        DysektFonts
        sfzero  # <--- LINKED HERE
    PUBLIC
        juce::juce_recommended_config_flags
)
