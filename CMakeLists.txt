cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. SFZero (SoundFont support) ---
# We download this BEFORE JUCE so the module is ready to be registered
include(FetchContent)
FetchContent_Declare(
    sfzero_module
    GIT_REPOSITORY https://github.com
    GIT_TAG        master
)
FetchContent_MakeAvailable(sfzero_module)

# --- 2. JUCE Setup ---
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)

# Register the downloaded SFZero as a JUCE module
# This makes sfzero::Synth available in your C++ code
juce_add_module("${sfzero_module_SOURCE_DIR}")

# --- 3. Bungee (built as static library) ---
set(BUNGEE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/bungee)

# pffft (C library used by Bungee)
add_library(pffft STATIC EXCLUDE_FROM_ALL
    ${BUNGEE_ROOT}/submodules/pffft/pffft.c
    ${BUNGEE_ROOT}/submodules/pffft/fftpack.c
)
set_target_properties(pffft PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    target_compile_options(pffft PRIVATE /fp:fast)
    target_compile_definitions(pffft PRIVATE _USE_MATH_DEFINES)
else()
    target_compile_options(pffft PRIVATE -ffast-math -fno-finite-math-only -fno-exceptions)
endif()

# Bungee library
file(GLOB BUNGEE_SOURCES "${BUNGEE_ROOT}/src/*.cpp")
add_library(bungee_lib STATIC EXCLUDE_FROM_ALL ${BUNGEE_SOURCES})
set_target_properties(bungee_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(bungee_lib PRIVATE
    ${BUNGEE_ROOT}/submodules/eigen
    ${BUNGEE_ROOT}/submodules
    ${BUNGEE_ROOT}
    $<$<CXX_COMPILER_ID:MSVC>:${CMAKE_CURRENT_SOURCE_DIR}/src/audio/msvc_compat>
)
target_include_directories(bungee_lib PUBLIC ${BUNGEE_ROOT})
target_compile_definitions(bungee_lib PRIVATE
    BUNGEE_SELF_TEST=0
    eigen_assert=BUNGEE_ASSERT1
    EIGEN_DONT_PARALLELIZE=1
    BUNGEE_VERSION="0.0.0"
)
if (MSVC)
    target_compile_options(bungee_lib PRIVATE /W0
        /FI${CMAKE_CURRENT_SOURCE_DIR}/src/audio/bungee_msvc_compat.h)
    target_compile_definitions(bungee_lib PRIVATE _USE_MATH_DEFINES)
else()
    target_compile_options(bungee_lib PRIVATE -fwrapv -w)
endif()
target_link_libraries(bungee_lib PRIVATE pffft)

# --- 4. Dysekt Plugin Definition ---
juce_add_plugin(Dysekt
    COMPANY_NAME "DunSoft"
    PLUGIN_MANUFACTURER_CODE Dnst
    PLUGIN_CODE Dysk
    FORMATS VST3
    PRODUCT_NAME "DYSEKT"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
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

target_compile_definitions(Dysekt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Dysekt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        bungee_lib
        DysektFonts
        sfzero  # <--- LINKED SFZERO TARGET
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
