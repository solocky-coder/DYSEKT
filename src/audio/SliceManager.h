#pragma once
#include "Slice.h"
#include <array>
#include <atomic>

class SliceManager
{
public:
    static constexpr int kMaxSlices = 128;

    /// Number of samples within which a boundary snaps to an adjacent slice edge.
    static constexpr int kSnapTolerance = 10;

    SliceManager();

    int  createSlice (int start, int end);
    void deleteSlice (int idx);
    void clearAll();
    void rebuildMidiMap();
    int  midiNoteToSlice (int note) const;
    const std::vector<int>& midiNoteToSlices (int note) const;

    float resolveParam (int sliceIdx, LockBit lockBit, float sliceValue, float globalDefault) const;

    /// Result returned by validateNoOverlap().
    struct OverlapResult
    {
        bool overlaps       = false;  ///< True when an overlap with another slice was found
        int  conflictSlice  = -1;     ///< Index of the first conflicting slice (-1 if none)
        int  adjustedStart  = 0;      ///< Suggested start after snapping / clamping
        int  adjustedEnd    = 0;      ///< Suggested end after snapping / clamping
    };

    /** Check whether the proposed [start, end) range for slice @p sliceIdx
     *  overlaps any other active slice.  Pass sliceIdx == -1 when creating a
     *  new slice (all slices are treated as potential conflicts).
     *
     *  If the start or end position is within kSnapTolerance samples of a
     *  neighbouring boundary, the adjusted values snap to that boundary so the
     *  caller can silently correct the position without user-visible gaps.
     *
     *  Thread safety: must be called on the audio thread (or while the slice
     *  list is not being modified from another thread). */
    OverlapResult validateNoOverlap (int sliceIdx, int start, int end) const;

    Slice& getSlice (int idx) { return slices[idx]; }
    const Slice& getSlice (int idx) const { return slices[idx]; }
    int getNumSlices() const { return numSlices; }
    void setNumSlices (int n) { numSlices = juce::jlimit (0, kMaxSlices, n); }

    std::atomic<int> selectedSlice { -1 };
    std::atomic<int> rootNote { 36 };

    void setSlicePalette (const juce::Colour* p) { palette.store (p, std::memory_order_relaxed); }

private:
    std::atomic<const juce::Colour*> palette { nullptr };

    std::array<Slice, kMaxSlices> slices;
    int numSlices = 0;
    std::array<int, 128> midiMap;               // first slice for note (legacy compat)
    std::array<std::vector<int>, 128> midiMapMulti;  // all slices for note
};
