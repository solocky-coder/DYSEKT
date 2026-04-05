#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

/**
 *  MidiLearnManager  v5
 *  ====================
 *  Auto-detection of encoder mode on the first kDetectSamples messages
 *  after a CC is learned. No manual mode selection required.
 *
 *  Detection logic (audio thread, per slot):
 *    - Values in 11-117 (excl. 56-72 bin-offset window) → absolute,
 *      but requires 2 consecutive hits before locking (guards against
 *      DirectLink init bursts and first-contact glitches, e.g. Axiom Air 25)
 *    - Values only in 0-10 or 118-127  → kRelTwosComp, lock after kDetectSamples
 *    - Values only in 56-72            → kRelBinOffset, lock after kDetectSamples
 *      (wider ±8 window vs v4's ±3 — handles fast encoder turns correctly)
 *    - Mixed evidence                  → pick sub-mode with most hits
 *  Output is suppressed during detection so no parameter jumps occur
 *  on the first N encoder messages after a fresh learn.
 *
 *  THREAD SAFETY
 *  -------------
 *  ccForSlot[], encodingForSlot[] — std::atomic<int> arrays.
 *  Audio thread reads; UI thread writes.
 *  armedSlot — std::atomic<int>. UI thread writes; audio thread reads.
 */

static constexpr int kMidiLearnNumSlots = 30;  // slots 0-27 (params) + 28 (TrimOut) + 29 (Hold)

class MidiLearnManager
{
public:
    enum EncoderMode { kAbsolute = 0, kRelTwosComp = 1, kRelSignBit = 2, kRelBinOffset = 3 };

    static constexpr int kDetectSamples = 6;

    MidiLearnManager()
    {
        for (auto& a : ccForSlot)       a.store (-1,        std::memory_order_relaxed);
        for (auto& a : encodingForSlot) a.store (kAbsolute, std::memory_order_relaxed);
        for (auto& a : detectCount)     a.store (0,         std::memory_order_relaxed);
        for (auto& a : detectLocked)    a.store (false,     std::memory_order_relaxed);
        for (auto& a : detectRelHits)   a.store (0,         std::memory_order_relaxed);
        for (auto& a : detectBinHits)   a.store (0,         std::memory_order_relaxed);
        for (auto& a : detectAbsHits)    a.store (0,  std::memory_order_relaxed);
        for (auto& a : prevDetectValue)  a.store (-1, std::memory_order_relaxed);
        for (auto& a : prevLearnValue)   a.store (-1, std::memory_order_relaxed);
    }

    // ── UI-thread API ─────────────────────────────────────────────────────────

    void armLearn (int fieldId) noexcept
    {
        if (fieldId >= 0 && fieldId < kMidiLearnNumSlots)
            prevLearnValue[fieldId].store (-1, std::memory_order_relaxed);
        armedSlot.store (fieldId, std::memory_order_relaxed);
    }
    int  getArmedSlot() const noexcept   { return armedSlot.load (std::memory_order_relaxed); }
    bool isArmed()      const noexcept   { return armedSlot.load (std::memory_order_relaxed) >= 0; }

    void clearMapping (int fieldId) noexcept
    {
        if (fieldId >= 0 && fieldId < kMidiLearnNumSlots)
        {
            ccForSlot      [fieldId].store (-1,        std::memory_order_relaxed);
            encodingForSlot[fieldId].store (kAbsolute, std::memory_order_relaxed);
            resetDetection (fieldId);
        }
    }

    void clearAll() noexcept
    {
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
        {
            ccForSlot      [i].store (-1,        std::memory_order_relaxed);
            encodingForSlot[i].store (kAbsolute, std::memory_order_relaxed);
            resetDetection (i);
        }
        armedSlot.store (-1, std::memory_order_relaxed);
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
        {
            encodingForSlot[fieldId].store (mode, std::memory_order_relaxed);
            detectLocked   [fieldId].store (true, std::memory_order_relaxed);
        }
    }

    bool isMapped  (int fieldId) const noexcept { return getMappedCC (fieldId) >= 0; }
    bool isEndless (int fieldId) const noexcept { return getEncoderMode (fieldId) != kAbsolute; }

    bool isDetectionComplete (int fieldId) const noexcept
    {
        if (fieldId < 0 || fieldId >= kMidiLearnNumSlots) return true;
        return detectLocked[fieldId].load (std::memory_order_relaxed);
    }

    juce::String getLabelText (int fieldId) const
    {
        const int cc = getMappedCC (fieldId);
        if (cc < 0) return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
        juce::String s = "CC " + juce::String (cc);
        if (! detectLocked[fieldId].load (std::memory_order_relaxed))
            s += " [?]";
        else
        {
            switch (getEncoderMode (fieldId))
            {
                case kRelTwosComp:  s += " [2s]"; break;
                case kRelSignBit:   s += " [SB]"; break;
                case kRelBinOffset: s += " [BO]"; break;
                default: break;
            }
        }
        return s;
    }

    // ── Audio-thread API ──────────────────────────────────────────────────────

    bool processCc (int cc, int value, int& outFieldId,
                    float& outNorm, bool& outIsRelative) noexcept
    {
        outIsRelative = false;

        // Capture learn
        const int armed = armedSlot.load (std::memory_order_relaxed);
        if (armed >= 0 && armed < kMidiLearnNumSlots)
        {
            // Movement guard: only capture a CC that is actually changing value.
            // Background transmissions (DAW sending CC1=0, CC7=100, mod wheel
            // resting at 0, etc.) repeat the same value every buffer. A real
            // encoder touch always produces a value different from the previous
            // message on that CC number.
            //
            // Strategy: the first message on any CC while armed just records the
            // value and the CC number as a candidate. If the next message on the
            // SAME CC number has a different value we confirm it as the intended
            // encoder. If it stays the same we keep waiting.
            //
            // prevLearnValue is keyed by armed slot (not by CC), so it resets
            // cleanly whenever the user arms a new slot.
            const int prev = prevLearnValue[armed].load (std::memory_order_relaxed);

            if (prev < 0)
            {
                // First message seen while armed — record it but don't capture yet.
                // Store the value with a bias of +1 so we can distinguish
                // "never seen" (-1) from "seen value 0" (stored as 1… wait, bias
                // breaks for value 127). Use a separate sentinel: store value as
                // (value | 0x200) so it is always > 127 and != -1.
                prevLearnValue[armed].store (value | 0x200, std::memory_order_relaxed);
            }
            else
            {
                // Subsequent message. Extract stored value (strip sentinel bit).
                const int prevVal = prev & 0x7F;
                if (value != prevVal)
                {
                    // Value changed → this CC is genuinely moving. Capture it.
                    ccForSlot [armed].store (cc,   std::memory_order_relaxed);
                    resetDetection (armed);                 // also clears prevLearnValue
                    armedSlot.store (-1, std::memory_order_relaxed);
                }
                else
                {
                    // Same value again — still a background static transmission.
                    // Keep waiting; update stored value in case it drifts later.
                    prevLearnValue[armed].store (value | 0x200, std::memory_order_relaxed);
                }
            }
            return false;
        }

        for (int i = 0; i < kMidiLearnNumSlots; ++i)
        {
            if (ccForSlot[i].load (std::memory_order_relaxed) != cc) continue;

            outFieldId = i;

            // ── Auto-detection phase ─────────────────────────────────────────
            if (! detectLocked[i].load (std::memory_order_relaxed))
            {
                const int n = detectCount[i].load (std::memory_order_relaxed);

                // Wider bin-offset window (±8 from 64) handles fast encoder turns
                // that v4's ±3 window misclassified as absolute.
                                const bool isDefinitelyRelative = (value <= 10 || value >= 118);
                const bool isBinOffset = (value >= 56 && value <= 72);
                const bool isAbsRange  = (value >= 11 && value <= 117) && ! isBinOffset && ! isDefinitelyRelative;

                if (isDefinitelyRelative)
                {
                    // Unambiguous twos-complement delta (value 0-10 or 118-127).
                    // Real absolute encoders never emit these extremes during use.
                    // Hard-lock immediately to prevent fast-turn false-absolute detection.
                    encodingForSlot[i].store (kRelTwosComp, std::memory_order_relaxed);
                    detectLocked   [i].store (true,         std::memory_order_relaxed);
                    return false;
                }
                else if (isAbsRange)
                {
                    // If the encoder sends the same value twice in a row it must be
                    // relative — absolute encoders never repeat during movement.
                    const int prev = prevDetectValue[i].load (std::memory_order_relaxed);
                    prevDetectValue[i].store (value, std::memory_order_relaxed);
                    if (prev == value)
                    {
                        encodingForSlot[i].store (kRelTwosComp, std::memory_order_relaxed);
                        detectLocked   [i].store (true,         std::memory_order_relaxed);
                        return false;
                    }

                    // Require 12 mid-range hits (up from 4) to lock as absolute.
                    // Gives the repeated-value shortcut 3× more time to fire first
                    // before committing to kAbsolute.
                    const int absH = detectAbsHits[i].fetch_add (1, std::memory_order_relaxed) + 1;
                    if (absH >= 12)
                    {
                        encodingForSlot[i].store (kAbsolute, std::memory_order_relaxed);
                        detectLocked   [i].store (true,      std::memory_order_relaxed);
                    }
                    return false;
                }
                else
                {
                    if (isBinOffset)
                        detectBinHits[i].fetch_add (1, std::memory_order_relaxed);
                    else
                        detectRelHits[i].fetch_add (1, std::memory_order_relaxed);

                    detectCount[i].store (n + 1, std::memory_order_relaxed);

                    if (n + 1 >= kDetectSamples)
                    {
                        const int binH = detectBinHits[i].load (std::memory_order_relaxed);
                        const int relH = detectRelHits[i].load (std::memory_order_relaxed);
                        const auto mode = (binH >= relH) ? kRelBinOffset : kRelTwosComp;
                        encodingForSlot[i].store (mode, std::memory_order_relaxed);
                        detectLocked   [i].store (true, std::memory_order_relaxed);
                    }
                }

                // Suppress output during detection — no parameter moves until
                // encoder type is confirmed. Eliminates jump-on-first-touch.
                return false;
            }

            // ── Normal decode (detection locked) ─────────────────────────────
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
                if (mode == kRelBinOffset)
                {
                    outNorm = (float)(value - 64);
                }
                else if (mode == kRelTwosComp)
                {
                    // Two's complement: 1-63 = +1..+63, 65-127 = -63..-1
                    if (value == 0 || value == 64)
                        outNorm = 0.0f;
                    else if (value <= 63)
                        outNorm = (float) value;         // +1 .. +63
                    else
                        outNorm = (float)(value - 128);  // -63 .. -1
                }
                else // kRelSignBit
                {
                    // Sign-bit: bit6 = direction (1=CCW), bits0-5 = magnitude
                    if (value == 0)
                        outNorm = 0.0f;
                    else
                    {
                        const int mag = value & 0x3F;           // bits 0-5
                        outNorm = (value & 0x40) ? -(float)mag  // bit6 set = CW on most encoders
                                                 :  (float)mag;
                    }
                }
            }
            return true;
        }
        return false;
    }

    // ── Persistence ───────────────────────────────────────────────────────────

    void writeState (juce::MemoryOutputStream& stream) const
    {
        stream.writeInt (kMidiLearnNumSlots);
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
            stream.writeInt (ccForSlot[i].load (std::memory_order_relaxed));
        stream.writeInt (kMidiLearnNumSlots);
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
            stream.writeInt (encodingForSlot[i].load (std::memory_order_relaxed));
        // v4: persist detection-locked so saved presets don't re-detect
        stream.writeInt (kMidiLearnNumSlots);
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
            stream.writeBool (detectLocked[i].load (std::memory_order_relaxed));
    }

    void readState (juce::MemoryInputStream& stream)
    {
        int count = stream.readInt();
        for (int i = 0; i < kMidiLearnNumSlots; ++i)
        {
            int v = (i < count) ? stream.readInt() : -1;
            ccForSlot[i].store (juce::jlimit (-1, 127, v), std::memory_order_relaxed);
        }
        if (! stream.isExhausted())
        {
            int eCount = stream.readInt();
            for (int i = 0; i < kMidiLearnNumSlots; ++i)
            {
                int e = (i < eCount) ? stream.readInt() : kAbsolute;
                encodingForSlot[i].store (juce::jlimit (0, 3, e), std::memory_order_relaxed);
            }
        }
        // v4: detection locked state
        if (! stream.isExhausted())
        {
            int lCount = stream.readInt();
            for (int i = 0; i < kMidiLearnNumSlots; ++i)
            {
                bool locked = (i < lCount) ? stream.readBool() : false;
                if (ccForSlot[i].load (std::memory_order_relaxed) < 0) locked = false;
                detectLocked[i].store (locked, std::memory_order_relaxed);
            }
        }
    }

private:
    void resetDetection (int i) noexcept
    {
        detectCount  [i].store (0,     std::memory_order_relaxed);
        detectLocked [i].store (false, std::memory_order_relaxed);
        detectRelHits[i].store (0,     std::memory_order_relaxed);
        detectBinHits[i].store (0,     std::memory_order_relaxed);
        detectAbsHits  [i].store (0,  std::memory_order_relaxed);
        prevDetectValue[i].store (-1, std::memory_order_relaxed);
        prevLearnValue [i].store (-1, std::memory_order_relaxed);
    }

    std::array<std::atomic<int>,  kMidiLearnNumSlots> ccForSlot;
    std::array<std::atomic<int>,  kMidiLearnNumSlots> encodingForSlot;
    std::atomic<int> armedSlot { -1 };

    // Detection state (audio thread only)
    std::array<std::atomic<int>,  kMidiLearnNumSlots> detectCount;
    std::array<std::atomic<bool>, kMidiLearnNumSlots> detectLocked;
    std::array<std::atomic<int>,  kMidiLearnNumSlots> detectRelHits;
    std::array<std::atomic<int>,  kMidiLearnNumSlots> detectBinHits;
    std::array<std::atomic<int>,  kMidiLearnNumSlots> detectAbsHits;
    std::array<std::atomic<int>,  kMidiLearnNumSlots> prevDetectValue;
    // Learn-capture guard: tracks the last value seen per slot while armed.
    // Prevents background CC transmissions (e.g. DAW sending CC1=0 every buffer)
    // from being captured before the user touches the intended encoder.
    std::array<std::atomic<int>,  kMidiLearnNumSlots> prevLearnValue;
};
