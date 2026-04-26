cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)

# --- sfizz (SFZ/SF2 engine — SoundFontLoader slice-render + SfzPlayer live SFZ playback) ---
if(TRUE)
    set(SFIZZ_CLIENTS    OFF CACHE BOOL "" FORCE)
    set(SFIZZ_SHARED     OFF CACHE BOOL "" FORCE)
    set(SFIZZ_JACK       OFF CACHE BOOL "" FORCE)
    set(SFIZZ_LV2        OFF CACHE BOOL "" FORCE)
    set(SFIZZ_VST        OFF CACHE BOOL "" FORCE)
    set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(SFIZZ_TESTS      OFF CACHE BOOL "" FORCE)
    # Disable sfizz_render CLI tool — it pulls in cxxopts which collides
    # with vcpkg's pre-registered cxxopts alias target on Windows CI.
    set(SFIZZ_RENDER     OFF CACHE BOOL "" FORCE)
    add_subdirectory(sfizz EXCLUDE_FROM_ALL)
    set(DYSEKT_HAS_SFIZZ TRUE)
    message(STATUS "sfizz found — SF2/SFZ slice-render support enabled")
else()
    set(DYSEKT_HAS_SFIZZ FALSE)
    message(STATUS "sfizz NOT found at ./sfizz — SF2/SFZ slice-render support disabled.")
endif()

# --- FluidSynth (SF2 live-playback engine — used by SfzPlayer) ---
#
# Preferred: install FluidSynth system-wide, then CMake's find_package picks it up.
#
#   Windows:  vcpkg install fluidsynth:x64-windows
#             Pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#
#   macOS:    brew install fluid-synth
#
#   Linux:    sudo apt install libfluidsynth-dev
#
# Alternatively, place a fluidsynth source tree at ./fluidsynth and the
# add_subdirectory fallback below will build it from source.
# ---------------------------------------------------------------------------

find_package(FluidSynth CONFIG QUIET)

if(FluidSynth_FOUND)
    set(DYSEKT_HAS_FLUIDSYNTH TRUE)
    message(STATUS "FluidSynth ${FluidSynth_VERSION} found via find_package — SF2 live playback enabled")
else()
    # Fallback: try a bundled source tree
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fluidsynth/CMakeLists.txt")
        set(FLUIDSYNTH_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
        set(FLUIDSYNTH_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
        set(enable-dbus                OFF CACHE BOOL "" FORCE)
        set(enable-jack                OFF CACHE BOOL "" FORCE)
        set(enable-pulseaudio          OFF CACHE BOOL "" FORCE)
        set(enable-alsa                OFF CACHE BOOL "" FORCE)
        set(enable-coreaudio           OFF CACHE BOOL "" FORCE)
        set(enable-portaudio           OFF CACHE BOOL "" FORCE)
        set(enable-openmp              OFF CACHE BOOL "" FORCE)
        add_subdirectory(fluidsynth EXCLUDE_FROM_ALL)
        set(DYSEKT_HAS_FLUIDSYNTH TRUE)
        message(STATUS "FluidSynth found at ./fluidsynth — building from source")
    else()
        set(DYSEKT_HAS_FLUIDSYNTH FALSE)
        message(WARNING
            "FluidSynth NOT found — SF2 live playback will be disabled.\n"
            "  Install via vcpkg / homebrew / apt, or clone FluidSynth into ./fluidsynth.")
    endif()
endif()

# Define the target
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

# FIXED: Use source-charset ONLY to fix character reading without breaking Nuendo compatibility
if (MSVC)
    target_compile_options(Dysekt PRIVATE /source-charset:utf-8)
endif()

juce_add_binary_data(DysektFonts SOURCES
    src/ui/fonts/BarlowCondensed-Regular.ttf
    src/ui/fonts/BarlowCondensed-SemiBold.ttf
    src/ui/fonts/JetBrainsMono-Regular.ttf
    src/ui/fonts/JetBrainsMono-Bold.ttf
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
    src/audio/SoundFontLoader.cpp
    src/audio/SfzPlayer.cpp
    src/params/ParamLayout.cpp
    src/ui/DysektLookAndFeel.cpp
    src/ui/HeaderBar.cpp
    src/ui/SliceControlBar.cpp
    src/ui/WaveformView.cpp
    src/ui/WaveformCache.cpp
    src/ui/SliceLane.cpp
    src/ui/ActionPanel.cpp
    src/ui/LogoBar.cpp
    src/ui/KeysPanel.cpp
    src/ui/FileBrowserPanel.cpp
    src/ui/SliceLcdDisplay.cpp
    src/ui/TrimDialog.cpp
    src/ui/MidiLearnDialog.cpp
    src/ui/MixerPanel.cpp
    src/ui/ShortcutsPanel.cpp
    src/ui/PadGridView.cpp
    src/ui/SliceWaveformLcd.cpp
    src/ui/DualLcdControlFrame.cpp
    src/ui/SfzDropdownPanel.cpp
    src/ui/SfzModulePanel.cpp
    src/ui/WaveformOverview.cpp
    src/ui/ArchiveIntegration.cpp
)

target_include_directories(Dysekt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/signalsmith-stretch
    ${CMAKE_CURRENT_SOURCE_DIR}/signalsmith-linear
    # sfizz C API header (sfizz.h) — needed by SoundFontLoader.cpp and SfzPlayer.cpp
    $<$<BOOL:${DYSEKT_HAS_SFIZZ}>:${CMAKE_CURRENT_SOURCE_DIR}/sfizz/src/sfizz>
)

target_compile_definitions(Dysekt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    DYSEKT_HAS_SFIZZ=$<BOOL:${DYSEKT_HAS_SFIZZ}>
    DYSEKT_HAS_FLUIDSYNTH=$<BOOL:${DYSEKT_HAS_FLUIDSYNTH}>
)

target_link_libraries(Dysekt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        DysektFonts
        # sfizz — slice-render pipeline (SoundFontLoader) + live SFZ playback (SfzPlayer)
        $<$<BOOL:${DYSEKT_HAS_SFIZZ}>:sfizz::sfizz>
        # FluidSynth — live SF2 playback (SfzPlayer)
        $<$<BOOL:${DYSEKT_HAS_FLUIDSYNTH}>:FluidSynth::libfluidsynth>
        $<$<PLATFORM_ID:Linux>:asound>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
