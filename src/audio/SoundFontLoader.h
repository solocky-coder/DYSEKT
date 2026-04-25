#pragma once
// =============================================================================
//  SoundFontLoader.h  —  SF2 → DYSEKT sample engine
//  ─────────────────────────────────────────────────
//  NOTE: sfizz support has been removed. SF2 files are played back directly
//  via FluidSynth through SfzPlayer. This file is kept only for the
//  SfzSliceDescriptor / SfzSlicePayload structs which are referenced by
//  PluginProcessor for its pendingSfzSlices atomic.
// =============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Forward declaration — full definition is in PluginProcessor.h
class DysektProcessor;

// =============================================================================
//  Per-note slice descriptor — carried through to processBlock
// =============================================================================
struct SfzSliceDescriptor
{
    int startSample = 0;
    int endSample   = 0;
    int midiNote    = 36;
};

// Heap-allocated payload posted via pendingSfzSlices atomic.
// processBlock takes ownership, creates slices, then deletes it.
struct SfzSlicePayload
{
    std::vector<SfzSliceDescriptor> slices;
};
