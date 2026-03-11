#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

/**
 *  MidiLearnManager  v3
 *  ====================
 *  Generalised MIDI Learn covering every numeric parameter in
 *  Dysekt's SliceControlBar plus Slice Start/End.
 *
 *  Each slot index == the SliceParamField enum value it controls.
 *  kMidiLearnNumSlots must be >= the highest FieldXxx value in use.
 *
 *  ENDLESS ENCODER SUPPORT
 *  -----------------------
 *  Per-slot encoding mode (stored in encodingForSlot[]):
 *    kAbsolute      (0)  — standard 7-bit absolute CC (default)
 *    kRelTwosComp   (1)  — 2's complement: 1-63 = +CW, 65-127 = -CCW
 *    kRelSignBit    (2)  — sign bit: 1-63 = +CW, 65-127 = -CCW (same decode, different hardware convention)
 *    kRelBinOffset  (3)  — binary offset: 65 = +1, 63 = -1 (Reaktor/Traktor style)
 *
 *  When processCc() decodes a relative encoder it sets outIsRelative=true
 *  and outNorm contains a signed step count (integer cast to float, e.g. +1.0 or -3.0).
 *  The caller is responsible for scaling the step to whatever sensitivity makes sense.
 *
 *  THREAD SAFETY
 *  -------------
 *  ccForSlot[], encodingForSlot[] — std::atomic<int> arrays.
 *                Audio thread reads; UI thread writes.
 *  armedSlot   — UI-thread only.
 */

static constexpr int kMidiLearnNumSlots = 32;

class MidiLearnManager
{
public:
    enum EncoderMode { kAbsolute = 0, kRelTwosComp = 1, kRelSignBit = 2, kRelBinOffset = 3 };

    MidiLearnManager()
    {
        for (auto& a : ccForSlot)       a.store (-1, std::memory_order_relaxed);
        for (auto& a : encodingForSlot) a.store (kAbsolute, std::memory_order_relaxed);
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
        {
            ccForSlot[fieldId].store (-1, std::memory_order_relaxed);
            encodingForSlot[fieldId].store (kAbsolute, std::memory_order_relaxed);
        }
    }

    void clearAll() noexcept
    {
        for (auto& a : ccForSlot)       a.store (-1, std::memory_order_relaxed);
        for (auto& a : encodingForSlot) a.store (kAbsolute, std::memory_order_relaxed);
        armedSlot = -1;
    }

    int getMappedCC (int fieldId) const noexcept
    {
        if (fieldId < 0 || fieldId >= kMidiLearnNumSlots) return -1;
        return ccForSlot[fieldId].load (std::memory_order_relaxed);
    }

    EncoderMode getEncoderMode (int fieldId) const noexcept
    {
        if (fieldId < 0 || fieldId >= kMidiLearnNumSlots) return kAbsolute;
        return static_cast<EncoderMode> (encodingForSlot[fieldId].load (std::memory_order_relaxed));
    }

    void setEncoderMode (int fieldId, EncoderMode mode) noexcept
    {
        if (fieldId >= 0 && fieldId < kMidiLearnNumSlots)
            encodingForSlot[fieldId].store (mode, std::memory_order_relaxed);
    }

    bool isMapped (int fieldId) const noexcept { return getMappedCC (fieldId) >= 0; }
    bool isEndless (int fieldId) const noexcept { return getEncoderMode (fieldId) != kAbsolute; }

    /** "CC 74 [REL]"  or  "—" */
    juce::String getLabelText (int fieldId) const
    {
        int cc = getMappedCC (fieldId);
        if (cc < 0) return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
        juce::String s = "CC " + juce::String (cc);
        switch (getEncoderMode (fieldId))
        {
            case kRelTwosComp:  s += " [2s]";  break;
            case kRelSignBit:   s += " [SB]";  break;
            case kRelBinOffset: s += " [BO]";  break;
            default: break;
        }
        return s;
    }

    // ----------------------------------------------------------------
    // Audio-thread API
    // ----------------------------------------------------------------

    /**
     *  Returns true when the CC matches a mapped slot.
     *
     *  outIsRelative = false → outNorm is 0.0–1.0 absolute
     *  outIsRelative = true  → outNorm is a signed step integer cast to float
     *                          (+1.0 = one click CW, -1.0 = one click CCW, etc.)
     *
     *  If learn is armed, captures the CC number and returns false.
     */
    bool processCc (int cc, int value, int& outFieldId,
                    float& outNorm, bool& outIsRelative) noexcept
    {
        outIsRelative = false;

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
                const auto mode = static_cast<EncoderMode> (
                    encodingForSlot[i].load (std::memory_order_relaxed));

                if (mode == kAbsolute)
                {
                    outNorm       = (float) value / 127.0f;
                    outIsRelative = false;
                }
                else
                {
                    outIsRelative = true;
                    // Decode relative step.  All three modes use the same
                    // 64-centre convention (64 = no movement); they differ
                    // only in hardware docs, not in the actual byte values.
                    //   2's complement / sign-bit: 1-63 = +CW, 65-127 = -CCW
                    //   Binary offset:             65   = +1,  63    = -1
                    if (mode == kRelBinOffset)
                    {
                        // 65 = +1, 66 = +2 … ; 63 = -1, 62 = -2 …
                        outNorm = (float)(value - 64);
                    }
                    else
                    {
                        // Two's complement / sign-bit
                        if (value == 0 || value == 64)
                            outNorm = 0.0f;
                        else if (value <= 63)
                            outNorm = (float) value;          // +1 .. +63
                        else
                            outNorm = (float)(value - 128);   // -63 .. -1
                    }
                }
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
        // v3: also persist encoding mode
        stream.writeInt (kMidiLearnNumSlots);
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
            stream.writeInt (encodingForSlot[i].load (std::memory_order_relaxed));
    }

    void readState (juce::MemoryInputStream& stream)
    {
        int count = stream.readInt();
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
        {
            int v = (i < count) ? stream.readInt() : -1;
            ccForSlot[i].store (juce::jlimit (-1, 127, v), std::memory_order_relaxed);
        }
        // v3 encoding modes (safe to skip for v2 presets)
        if (! stream.isExhausted())
        {
            int eCount = stream.readInt();
            for (int i = 0; i < kMidiLearnNumSlots; ++i)
            {
                int e = (i < eCount) ? stream.readInt() : kAbsolute;
                e = juce::jlimit (0, 3, e);
                encodingForSlot[i].store (e, std::memory_order_relaxed);
            }
        }
    }

private:
    std::array<std::atomic<int>, kMidiLearnNumSlots> ccForSlot;
    std::array<std::atomic<int>, kMidiLearnNumSlots> encodingForSlot;
    int armedSlot { -1 };
};
