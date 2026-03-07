#include "SliceManager.h"
#include <algorithm>
#include <cmath>

// =============================================================================
//  Constructor
// =============================================================================

SliceManager::SliceManager()
{
    midiMap.fill (-1);
}

// =============================================================================
//  getEndForSlice  — the central derived-end query
// =============================================================================

int SliceManager::getEndForSlice (int idx, int totalFrames) const noexcept
{
    if (idx < 0 || idx >= numSlices)
        return totalFrames;

    // The end of slice N is the start of slice N+1.
    // Slices are kept sorted by startSample (sortByStart is called whenever
    // the array is modified), so slices[idx+1].startSample is always correct.
    if (idx + 1 < numSlices && slices[(size_t)(idx + 1)].active)
        return slices[(size_t)(idx + 1)].startSample;

    return totalFrames;
}

// =============================================================================
//  sortByStart  — keep slices ordered by startSample
// =============================================================================

void SliceManager::sortByStart()
{
    // Insertion sort — stable, in-place, fast for small N (≤128).
    for (int i = 1; i < numSlices; ++i)
    {
        Slice key = slices[(size_t) i];
        int j = i - 1;
        while (j >= 0 && slices[(size_t) j].startSample > key.startSample)
        {
            slices[(size_t)(j + 1)] = slices[(size_t) j];
            --j;
        }
        slices[(size_t)(j + 1)] = key;
    }
}

// =============================================================================
//  assignDefaults / assignColour
// =============================================================================

void SliceManager::assignDefaults (Slice& s, int idx)
{
    s.active         = true;
    s.midiNote       = std::min (rootNote.load() + idx, 127);
    s.lockMask       = 0;
    s.bpm            = 120.0f;
    s.pitchSemitones = 0.0f;
    s.algorithm      = 0;
    s.attackSec      = 0.005f;
    s.decaySec       = 0.1f;
    s.sustainLevel   = 1.0f;
    s.releaseSec     = 0.02f;
    s.muteGroup      = 1;
    s.loopMode       = 0;
}

void SliceManager::assignColour (Slice& s, int idx)
{
    const auto* p = palette.load (std::memory_order_relaxed);
    s.colour = p ? p[idx % 16] : juce::Colour (0xFF4D8C99);
}

// =============================================================================
//  insertMarker  — the primary ADD-mode entry point
// =============================================================================

int SliceManager::insertMarker (int markerPos, int totalFrames)
{
    if (numSlices >= kMaxSlices)
        return -1;

    markerPos = juce::jlimit (0, totalFrames, markerPos);

    // Reject if within kSnapTolerance of any existing boundary.
    for (int i = 0; i < numSlices; ++i)
    {
        if (! slices[(size_t) i].active) continue;
        if (std::abs (slices[(size_t) i].startSample - markerPos) < kSnapTolerance)
            return -1;
        int end = getEndForSlice (i, totalFrames);
        if (std::abs (end - markerPos) < kSnapTolerance)
            return -1;
    }

    // Find which slice currently owns this position.
    // The new marker splits that slice into [its start .. markerPos) and
    // [markerPos .. its end).  The left half keeps the original slice's
    // parameters; the right half gets fresh defaults.
    int splitIdx = -1;
    for (int i = 0; i < numSlices; ++i)
    {
        const auto& s = slices[(size_t) i];
        if (! s.active) continue;
        int end = getEndForSlice (i, totalFrames);
        if (markerPos > s.startSample && markerPos < end)
        {
            splitIdx = i;
            break;
        }
    }

    if (splitIdx < 0)
    {
        // markerPos is after all existing slices — append a new tail slice.
        if (numSlices == 0 || markerPos > getEndForSlice (numSlices - 1, totalFrames))
        {
            int newIdx = numSlices;
            Slice& ns  = slices[(size_t) newIdx];
            ns             = Slice{};
            ns.startSample = markerPos;
            assignDefaults (ns, newIdx);
            assignColour   (ns, newIdx);
            ++numSlices;
            rebuildMidiMap();
            return newIdx;
        }
        return -1;
    }

    // Minimum-width guard: both halves must be ≥64 samples.
    int leftLen  = markerPos - slices[(size_t) splitIdx].startSample;
    int rightLen = getEndForSlice (splitIdx, totalFrames) - markerPos;
    if (leftLen < 64 || rightLen < 64)
        return -1;

    // Copy the original slice's parameters for the right-hand new slice.
    Slice rightSlice       = slices[(size_t) splitIdx]; // copy all params
    rightSlice.startSample = markerPos;
    rightSlice.midiNote    = std::min (rootNote.load() + numSlices, 127);
    assignColour (rightSlice, numSlices);

    // Append to end of array, then sort to put it in the correct position.
    slices[(size_t) numSlices] = rightSlice;
    ++numSlices;

    sortByStart();
    rebuildMidiMap();

    // Return the index of the slice that now starts at markerPos.
    for (int i = 0; i < numSlices; ++i)
        if (slices[(size_t) i].startSample == markerPos)
            return i;

    return -1;
}

// =============================================================================
//  deleteSlice
// =============================================================================

void SliceManager::deleteSlice (int idx)
{
    if (idx < 0 || idx >= numSlices)
        return;

    // Shift all slices after idx down by one.
    for (int i = idx; i < numSlices - 1; ++i)
        slices[(size_t) i] = slices[(size_t)(i + 1)];

    slices[(size_t)(numSlices - 1)].active = false;
    --numSlices;

    // Fix selected slice index.
    if (selectedSlice >= numSlices)
        selectedSlice.store (std::max (0, numSlices - 1));
    if (numSlices == 0)
        selectedSlice.store (-1);

    rebuildMidiMap();
}

// =============================================================================
//  createSlice  — legacy compat wrapper (LazyChopEngine, CmdSplitSlice, etc.)
// =============================================================================

int SliceManager::createSlice (int start, int end)
{
    if (numSlices >= kMaxSlices)
        return -1;

    if (start > end) std::swap (start, end);

    // We treat `start` as a new marker position.
    // If a slice already starts exactly here, return that index.
    for (int i = 0; i < numSlices; ++i)
        if (slices[(size_t) i].active && slices[(size_t) i].startSample == start)
            return i;

    // Append a new slice at `start`.  Its end will be derived automatically
    // as the next slice's startSample once the array is sorted.
    int newIdx = numSlices;
    Slice& ns  = slices[(size_t) newIdx];
    ns             = Slice{};
    ns.startSample = start;
    assignDefaults (ns, newIdx);
    assignColour   (ns, newIdx);
    ++numSlices;

    sortByStart();
    rebuildMidiMap();

    // Find and return the index of the slice that now starts at `start`.
    for (int i = 0; i < numSlices; ++i)
        if (slices[(size_t) i].startSample == start)
            return i;

    return newIdx; // fallback
}

// =============================================================================
//  clearAll
// =============================================================================

void SliceManager::clearAll()
{
    numSlices = 0;
    for (auto& s : slices)
        s.active = false;
    selectedSlice.store (-1);
    rebuildMidiMap();
}

// =============================================================================
//  rebuildMidiMap
// =============================================================================

void SliceManager::rebuildMidiMap()
{
    midiMap.fill (-1);
    for (int i = 0; i < numSlices; ++i)
    {
        if (slices[(size_t) i].active)
        {
            int note = slices[(size_t) i].midiNote;
            if (note >= 0 && note < 128 && midiMap[(size_t) note] < 0)
                midiMap[(size_t) note] = i;
        }
    }
}

// =============================================================================
//  sortByStart (public, for use after bulk state restore)
// =============================================================================
// Already defined above as a private helper; the public declaration in the
// header makes it available to PluginProcessor::setStateInformation().

// =============================================================================
//  MIDI map queries
// =============================================================================

int SliceManager::midiNoteToSlice (int note) const
{
    if (note < 0 || note >= 128) return -1;
    return midiMap[(size_t) note];
}


// =============================================================================
//  resolveParam
// =============================================================================

float SliceManager::resolveParam (int sliceIdx, LockBit lockBit,
                                   float sliceValue, float globalDefault) const
{
    if (sliceIdx < 0 || sliceIdx >= numSlices)
        return globalDefault;

    return (slices[(size_t) sliceIdx].lockMask & lockBit) ? sliceValue : globalDefault;
}
