#pragma once
// =============================================================================
//  SfzPlayer.h  —  Real-time SF2/SFZ playback engine (sfizz backend)
// =============================================================================
//  Owned by DysektProcessor.  prepareToPlay / processBlock / loadFile are
//  called from the audio thread (processBlock) or UI thread (load/param set).
//
//  Thread safety:
//    loadFile()        — UI thread; swaps sfizz_t* atomically via pendingLoad
//    setVolume/Trans() — UI thread; stored as std::atomic<float>
//    prepare()         — audio thread (prepareToPlay)
//    process()         — audio thread (processBlock)
// =============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Forward-declare sfizz_t — full include is in SfzPlayer.cpp only.
// Keeps this header compilable without sfizz on the include path.
#if DYSEKT_HAS_SFIZZ
  struct sfizz_s;
  typedef struct sfizz_s sfizz_t;
#endif

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

    void setVolume    (float gainLinear);   ///< 0..2
    void setTranspose (int semitones);      ///< -24..+24
    void setMidiChannel (int ch);           ///< 0 = omni, 1-16 = specific

    float       getVolume()      const noexcept { return volume.load(); }
    int         getTranspose()   const noexcept { return transpose.load(); }
    int         getMidiChannel() const noexcept { return midiChannel.load(); }
    juce::File  getLoadedFile()  const;
    bool        isLoaded()       const noexcept { return loaded.load(); }

    // ── Called from audio thread ──────────────────────────────────────────────

    void prepare  (double sampleRate, int maxBlockSize);

    /**
     * Process one block. MIDI events from @p midiIn whose channel matches
     * midiChannel (0 = all) are forwarded to sfizz.  Rendered stereo audio is
     * mixed additively into @p outL / @p outR (already-cleared by processBlock).
     */
    void process  (const juce::MidiBuffer& midiIn,
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
#if DYSEKT_HAS_SFIZZ
    sfizz_t* sfz          { nullptr };
#endif
    double   currentSR    { 44100.0 };
    int      currentBlock { 256 };

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
