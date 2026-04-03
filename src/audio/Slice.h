#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>

enum LockBit : uint32_t
{
    kLockBpm         = 1,
    kLockPitch       = 2,
    kLockAlgorithm   = 4,
    kLockAttack      = 8,
    kLockDecay       = 16,
    kLockSustain     = 32,
    kLockRelease     = 64,
    kLockMuteGroup   = 128,
    kLockStretch       = 512,
    kLockTonality      = 1024,
    kLockFormant       = 2048,
    kLockFormantComp   = 4096,
    kLockGrainMode     = 8192,
    kLockVolume        = 16384,
    kLockReleaseTail   = 32768,
    kLockReverse       = 65536,
    kLockOutputBus     = 131072,
    kLockLoop          = 262144,
    kLockOneShot       = 524288,
    kLockCentsDetune   = 1048576,
    kLockPan           = 2097152,
    kLockFilter        = 4194304,
    kLockChromaticChannel = 8388608,
    kLockChromaticLegato  = 16777216,
    kLockHold              = 33554432,
};

struct Slice
{
    bool     active         = false;
    int      startSample    = 0;
    int      midiNote       = 36;
    float    bpm            = 120.0f;
    float    pitchSemitones = 0.0f;
    int      algorithm      = 0;
    float    attackSec      = 0.0f;
    float    holdSec        = 0.0f;
    float    decaySec       = 0.0f;
    float    sustainLevel   = 1.0f;
    float    releaseSec     = 0.010f;
    int      muteGroup      = 1;
    int      loopMode       = 0;
    bool     stretchEnabled = false;
    float    tonalityHz     = 0.0f;
    float    formantSemitones = 0.0f;
    bool     formantComp    = false;
    int      grainMode      = 0;
    float    volume         = 0.0f;
    bool     releaseTail    = false;
    bool     reverse        = false;
    int      outputBus      = 0;
    bool     oneShot        = false;
    float    centsDetune    = 0.0f;
    float    pan            = 0.0f;
    float    filterCutoff   = 20000.0f;
    float    filterRes      = 0.0f;
    int      chromaticChannel = 0;
    bool     chromaticLegato  = false;
    int      rrCounter      = 0;

    // === NEW: Per-slice marker ratio system ===
    int      uuid           = 0;        // Unique ID, never reused after deletion
    float    markerRatio    = 0.0f;     // 0.0 = slice start, 1.0 = slice end (normalized)

    uint32_t lockMask       = 0;
    juce::Colour colour     { 0.4f, 0.7f, 0.95f, 1.0f };
};