cmake_minimum_required(VERSION 3.22)
project(Dysekt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)

# --- sfizz (SF2 / SFZ soundfont engine) ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sfizz/CMakeLists.txt")
    set(SFIZZ_CLIENTS    OFF CACHE BOOL "" FORCE)
    set(SFIZZ_SHARED     OFF CACHE BOOL "" FORCE)
    set(SFIZZ_JACK       OFF CACHE BOOL "" FORCE)
    set(SFIZZ_LV2        OFF CACHE BOOL "" FORCE)
    set(SFIZZ_VST        OFF CACHE BOOL "" FORCE)
    set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(SFIZZ_TESTS      OFF CACHE BOOL "" FORCE)
    add_subdirectory(sfizz EXCLUDE_FROM_ALL)
    set(DYSEKT_HAS_SFIZZ TRUE)
    message(STATUS "sfizz found — SF2/SFZ support enabled")
else()
    set(DYSEKT_HAS_SFIZZ FALSE)
    message(STATUS "sfizz NOT found at ./sfizz — SF2/SFZ support disabled.")
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

# /source-charset:utf-8  — tell the compiler the .cpp files are UTF-8 encoded
# /execution-charset:utf-8 — keep UTF-8 in string literals at runtime,
#   preventing C4566 "character cannot be represented in current code page"
#   warnings for Unicode characters such as the minus sign (U+2212) in MixerPanel.cpp
if(MSVC)
    target_compile_options(Dysekt PRIVATE /source-charset:utf-8 /execution-charset:utf-8)
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
    src/audio/SfzPlayer.cpp
    src/audio/SoundFontLoader.cpp
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
    src/ui/WaveformOverview.cpp

    # ⭐ REQUIRED FIX — this was missing
    src/ui/ArchiveIntegration.cpp
)

target_include_directories(Dysekt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/signalsmith-stretch
    ${CMAKE_CURRENT_SOURCE_DIR}/signalsmith-linear
    # Explicitly expose sfizz's public header directory.
    # sfizz::sfizz does not reliably propagate INTERFACE_INCLUDE_DIRECTORIES
    # through the sfizz CMake build, so MSVC cannot find <sfizz.h> without this.
    $<$<BOOL:${DYSEKT_HAS_SFIZZ}>:${CMAKE_CURRENT_SOURCE_DIR}/sfizz/src>
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
        DysektFonts
        $<$<BOOL:${DYSEKT_HAS_SFIZZ}>:sfizz::sfizz>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
