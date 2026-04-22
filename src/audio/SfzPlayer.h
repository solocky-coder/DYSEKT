#pragma once
// =============================================================================
//  SfzPlayer.h  —  Real-time SF2/SFZ playback engine (sfizz backend)
// =============================================================================
//  No sfizz headers are included here — sfizz.h lives in SfzPlayer.cpp only.
//  The internal sfizz handle is stored as void* and cast in the .cpp.
// =============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

class SfzPlayer
{
public:
    SfzPlayer();
    ~SfzPlayer();

    // ── Called from UI thread ─────────────────────────────────────────────────

    /** Queue a new SF2/SFZ file for loading. Returns immediately. */
    void loadFile (const juce::File& f);

    /** Unload current instrument (silent output). */
    void unload();

    void setVolume      (float gainLinear);   ///< 0..2
    void setTranspose   (int semitones);      ///< -24..+24
    void setMidiChannel (int ch);             ///< 0 = omni, 1-16 = specific

    float      getVolume()      const noexcept { return volume.load(); }
    int        getTranspose()   const noexcept { return transpose.load(); }
    int        getMidiChannel() const noexcept { return midiChannel.load(); }
    juce::File getLoadedFile()  const;
    bool       isLoaded()       const noexcept { return loaded.load(); }

    // ── Called from audio thread ──────────────────────────────────────────────

    void prepare (double sampleRate, int maxBlockSize);

    /**
     * Process one block.  MIDI events matching midiChannel (0 = omni) are
     * forwarded to sfizz.  Stereo output is mixed additively into outL/outR.
     */
    void process (const juce::MidiBuffer& midiIn,
                  float* outL, float* outR, int numSamples);

private:
    // ── Pending load (UI → audio thread handoff) ──────────────────────────────
    struct PendingLoad
    {
        juce::File file;
        bool       shouldUnload { false };
    };

    std::atomic<PendingLoad*> pendingLoad { nullptr };

    // ── Audio-thread state ────────────────────────────────────────────────────
    // Stored as void* so this header needs no sfizz dependency.
    // Cast to sfizz_t* wherever used in SfzPlayer.cpp.
    void*  sfzHandle  { nullptr };

    double currentSR    { 44100.0 };
    int    currentBlock { 256 };

    juce::File activeFile;

    // ── Shared params (atomic, UI-writable) ───────────────────────────────────
    std::atomic<float> volume      { 1.0f };
    std::atomic<int>   transpose   { 0 };
    std::atomic<int>   midiChannel { 0 };    // 0 = omni
    std::atomic<bool>  loaded      { false };

    // ── Scratch buffer for sfizz output ──────────────────────────────────────
    std::vector<float> scratchL, scratchR;

    void applyPendingLoad();   // called at top of process()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SfzPlayer)
};
