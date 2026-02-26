#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

/**
 *  MidiLearnManager  v2
 *  ====================
 *  Generalised MIDI Learn covering every numeric parameter in
 *  Dysekt's SliceControlBar plus Slice Start/End.
 *
 *  Each slot index == the SliceParamField enum value it controls.
 *  kMidiLearnNumSlots must be >= the highest FieldXxx value in use.
 *
 *  THREAD SAFETY
 *  -------------
 *  ccForSlot[] — std::atomic<int> array.
 *                Audio thread reads; UI thread writes.
 *  armedSlot   — UI-thread only.
 */

static constexpr int kMidiLearnNumSlots = 32;

class MidiLearnManager
{
public:
    MidiLearnManager()
    {
        for (auto& a : ccForSlot)
            a.store (-1, std::memory_order_relaxed);
    }

    // ----------------------------------------------------------------
    // UI-thread API
    // ----------------------------------------------------------------

    /** Arm slot by SliceParamField int. Pass -1 to cancel. */
    void armLearn (int fieldId) noexcept { armedSlot = fieldId; }
    int  getArmedSlot() const noexcept   { return armedSlot; }
    bool isArmed()      const noexcept   { return armedSlot >= 0; }

    void clearMapping (int fieldId) noexcept
    {
        if (fieldId >= 0 && fieldId < kMidiLearnNumSlots)
            ccForSlot[fieldId].store (-1, std::memory_order_relaxed);
    }

    void clearAll() noexcept
    {
        for (auto& a : ccForSlot)
            a.store (-1, std::memory_order_relaxed);
        armedSlot = -1;
    }

    int getMappedCC (int fieldId) const noexcept
    {
        if (fieldId < 0 || fieldId >= kMidiLearnNumSlots) return -1;
        return ccForSlot[fieldId].load (std::memory_order_relaxed);
    }

    bool isMapped (int fieldId) const noexcept { return getMappedCC (fieldId) >= 0; }

    /** "CC 74"  or  "—" */
    juce::String getLabelText (int fieldId) const
    {
        int cc = getMappedCC (fieldId);
        return (cc >= 0) ? ("CC " + juce::String (cc))
                         : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
    }

    // ----------------------------------------------------------------
    // Audio-thread API
    // ----------------------------------------------------------------

    /**
     *  Returns true when the CC matches a mapped slot.
     *  If learn is armed, captures the CC number and returns false.
     */
    bool processCc (int cc, int value, int& outFieldId, float& outNorm) noexcept
    {
        if (armedSlot >= 0 && armedSlot < kMidiLearnNumSlots)
        {
            ccForSlot[armedSlot].store (cc, std::memory_order_relaxed);
            armedSlot = -1;
            return false;
        }

        for (int i = 0; i < kMidiLearnNumSlots; ++i)
        {
            int mapped = ccForSlot[i].load (std::memory_order_relaxed);
            if (mapped >= 0 && cc == mapped)
            {
                outFieldId = i;
                outNorm    = (float) value / 127.0f;
                return true;
            }
        }
        return false;
    }

    // ----------------------------------------------------------------
    // Persistence
    // ----------------------------------------------------------------

    void writeState (juce::MemoryOutputStream& stream) const
    {
        stream.writeInt (kMidiLearnNumSlots);
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
            stream.writeInt (ccForSlot[i].load (std::memory_order_relaxed));
    }

    void readState (juce::MemoryInputStream& stream)
    {
        int count = stream.readInt();
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
        {
            int v = (i < count) ? stream.readInt() : -1;
            ccForSlot[i].store (juce::jlimit (-1, 127, v), std::memory_order_relaxed);
        }
    }

private:
    std::array<std::atomic<int>, kMidiLearnNumSlots> ccForSlot;
    int armedSlot { -1 };
};
