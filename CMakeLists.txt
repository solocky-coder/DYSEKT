cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)

# --- sfizz (SF2 / SFZ soundfont engine) ---
# Clone with: git submodule add https://github.com/sfztools/sfizz.git sfizz
#             git submodule update --init --recursive sfizz
if(FALSE)
    set(SFIZZ_CLIENTS OFF CACHE BOOL "" FORCE)      # don't build stand-alone apps
    set(SFIZZ_SHARED  OFF CACHE BOOL "" FORCE)      # static lib only
    set(SFIZZ_JACK    OFF CACHE BOOL "" FORCE)
    set(SFIZZ_LV2     OFF CACHE BOOL "" FORCE)
    set(SFIZZ_VST     OFF CACHE BOOL "" FORCE)
    set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(SFIZZ_TESTS   OFF CACHE BOOL "" FORCE)
    add_subdirectory(sfizz EXCLUDE_FROM_ALL)
    set(DYSEKT_HAS_SFIZZ TRUE)
    message(STATUS "sfizz found — SF2/SFZ support enabled")
else()
    set(DYSEKT_HAS_SFIZZ FALSE)
    message(STATUS "sfizz NOT found at ./sfizz — SF2/SFZ support disabled. "
                   "Run: git submodule add https://github.com/sfztools/sfizz.git sfizz")
endif()

# --- Bungee (built as static library) ---
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
    src/ui/assets/Empty.wav
)

target_sources(Dysekt PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/audio/SampleData.cpp
    src/audio/SliceManager.cpp
    src/audio/VoicePool.cpp
    src/audio/GrainEngine.cpp
    src/audio/LazyChopEngine.cpp
    src/audio/TransientDetector.cpp
    src/audio/SoundFontLoader.cpp
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
    src/ui/OscilloscopeView.cpp
    src/ui/SliceLcdDisplay.cpp              
    src/ui/TrimDialog.cpp
    src/ui/MixerStrip.cpp
    src/ui/MasterMixerStrip.cpp
    src/ui/MixerPanel.cpp
    src/ui/ShortcutsPanel.cpp
    src/ui/SliceWaveformLcd.cpp
    src/ui/DualLcdControlFrame.cpp
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
    DYSEKT_HAS_SFIZZ=$<BOOL:${DYSEKT_HAS_SFIZZ}>
)

target_link_libraries(Dysekt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        bungee_lib
        DysektFonts
        $<$<BOOL:${DYSEKT_HAS_SFIZZ}>:sfizz::sfizz>
        $<$<PLATFORM_ID:Linux>:asound>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
