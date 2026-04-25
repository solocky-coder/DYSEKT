cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)

# --- FluidSynth (SF2 live-playback engine — used by SfzPlayer) ---
#
# Resolution order:
#   1. find_package  — picks up a vcpkg / system install automatically
#   2. Bundled prebuilt zip — extract fluidsynth-2.3.4-win10-x64.zip into
#      the repo root so the tree looks like:
#        fluidsynth-prebuilt/
#          include/fluidsynth.h
#          include/fluidsynth/
#          lib/libfluidsynth.dll.a
#          bin/libfluidsynth-3.dll   ← copy this next to your VST3
#   3. Bundled source tree — clone FluidSynth into ./fluidsynth and it
#      will be compiled from source (slow, but works everywhere).
#
# ---------------------------------------------------------------------------

find_package(FluidSynth CONFIG QUIET)

if(FluidSynth_FOUND)
    set(DYSEKT_HAS_FLUIDSYNTH TRUE)
    message(STATUS "FluidSynth ${FluidSynth_VERSION} found via find_package — SF2 live playback enabled")
else()
    # --- Fallback 1: prebuilt zip extracted at ./fluidsynth-prebuilt ----------
    set(FLUIDSYNTH_PREBUILT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fluidsynth-prebuilt")
    if(EXISTS "${FLUIDSYNTH_PREBUILT_DIR}/include/fluidsynth.h" AND
       EXISTS "${FLUIDSYNTH_PREBUILT_DIR}/lib/libfluidsynth.dll.a")

        message(STATUS "FluidSynth prebuilt found at ./fluidsynth-prebuilt — SF2 live playback enabled")
        set(DYSEKT_HAS_FLUIDSYNTH TRUE)

        # Create an imported target that looks identical to the find_package one
        add_library(FluidSynth::libfluidsynth SHARED IMPORTED)
        set_target_properties(FluidSynth::libfluidsynth PROPERTIES
            IMPORTED_LOCATION             "${FLUIDSYNTH_PREBUILT_DIR}/bin/libfluidsynth-3.dll"
            IMPORTED_IMPLIB               "${FLUIDSYNTH_PREBUILT_DIR}/lib/libfluidsynth.dll.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FLUIDSYNTH_PREBUILT_DIR}/include"
        )

        # Copy the DLL (and its deps) next to the built VST3 automatically
        set(FLUIDSYNTH_RUNTIME_DLLS
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libfluidsynth-3.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libglib-2.0-0.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libgobject-2.0-0.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libgthread-2.0-0.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libintl-8.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libsndfile-1.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libinstpatch-2.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libgcc_s_sjlj-1.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libstdc++-6.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libwinpthread-1.dll"
            "${FLUIDSYNTH_PREBUILT_DIR}/bin/libgomp-1.dll"
        )
        foreach(_dll ${FLUIDSYNTH_RUNTIME_DLLS})
            if(EXISTS "${_dll}")
                install(FILES "${_dll}" DESTINATION ".")
            endif()
        endforeach()

    # --- Fallback 2: build from source at ./fluidsynth ----------------------
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fluidsynth/CMakeLists.txt")
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
            "  Option A: extract fluidsynth-2.3.4-win10-x64.zip into ./fluidsynth-prebuilt/\n"
            "  Option B: vcpkg install fluidsynth:x64-windows\n"
            "  Option C: clone FluidSynth source into ./fluidsynth/")
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
)

target_compile_definitions(Dysekt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    DYSEKT_HAS_FLUIDSYNTH=$<BOOL:${DYSEKT_HAS_FLUIDSYNTH}>
)

target_link_libraries(Dysekt
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        DysektFonts
        # FluidSynth — live SF2 playback (SfzPlayer)
        $<$<BOOL:${DYSEKT_HAS_FLUIDSYNTH}>:FluidSynth::libfluidsynth>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
