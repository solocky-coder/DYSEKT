#pragma once
// =============================================================================
//  SfzPlayer.h  —  Real-time SF2 playback engine (FluidSynth backend)
// =============================================================================
//  Owned by DysektProcessor.  prepareToPlay / processBlock / loadFile are
//  called from the audio thread (processBlock) or UI thread (load/param set).
//
//  Thread safety:
//    loadFile()           — UI thread; PendingLoad posted via atomic
//    setVolume/Trans()    — UI thread; stored as std::atomic<float>
//    setPresetByIndex()   — UI thread; sets atomics + programChangePending flag
//    prepare()            — audio thread (prepareToPlay)
//    process()            — audio thread (processBlock); applies pending loads
//                           and program changes at the top of each block
//
//  Preset list handoff (audio → UI):
//    After a successful sfont load the audio thread allocates a new
//    std::vector<Sf2PresetInfo>* and stores it in freshPresets.
//    getPresetList() (UI thread) swaps it out and caches it.
// =============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

#if DYSEKT_HAS_FLUIDSYNTH
  #include <fluidsynth.h>
#endif

// -----------------------------------------------------------------------------
//  Preset descriptor — used by the UI to populate the preset picker
// -----------------------------------------------------------------------------
struct Sf2PresetInfo
{
    int          bank   { 0 };
    int          preset { 0 };
    juce::String name;
};

// =============================================================================
class SfzPlayer
{
public:
    SfzPlayer();
    ~SfzPlayer();

    // ── Called from UI thread ─────────────────────────────────────────────────

    /** Queue a new SF2 file for loading. Returns immediately. */
    void loadFile (const juce::File& f);

    /** Unload current instrument (silent output). */
    void unload();

    void setVolume      (float gainLinear);   ///< 0..2
    void setTranspose   (int semitones);      ///< -24..+24
    void setMidiChannel (int ch);             ///< 0 = omni, 1-16 = specific

    /** Select a preset by its index in the list returned by getPresetList(). */
    void setPresetByIndex (int idx);

    float      getVolume()      const noexcept { return volume.load(); }
    int        getTranspose()   const noexcept { return transpose.load(); }
    int        getMidiChannel() const noexcept { return midiChannel.load(); }
    int        getCurrentPresetIndex() const noexcept { return presetIndex.load(); }
    juce::File getLoadedFile()  const;
    bool       isLoaded()       const noexcept { return loaded.load(); }

    /**
     * Returns the cached preset list for the currently loaded SF2.
     * If the audio thread has posted new data since the last call,
     * the cache is updated first (wait-free on both sides).
     * Safe to call from any thread except the audio thread.
     */
    std::vector<Sf2PresetInfo> getPresetList() const;

    // ── Called from audio thread ──────────────────────────────────────────────

    void prepare (double sampleRate, int maxBlockSize);

    /**
     * Process one block. MIDI events from @p midiIn whose channel matches
     * midiChannel (0 = all) are forwarded to FluidSynth.  Rendered stereo
     * audio is mixed additively into @p outL / @p outR.
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

    // ── Pending preset list (audio → UI handoff) ──────────────────────────────
    mutable std::atomic<std::vector<Sf2PresetInfo>*> freshPresets { nullptr };
    mutable std::vector<Sf2PresetInfo>               cachedPresets;

    // ── Audio-thread FluidSynth state ─────────────────────────────────────────
#if DYSEKT_HAS_FLUIDSYNTH
    fluid_settings_t* settings { nullptr };
    fluid_synth_t*    synth    { nullptr };
    int               sfontId  { -1 };
#endif
    double   currentSR    { 44100.0 };
    int      currentBlock { 256 };
    juce::File activeFile;

    // ── Shared params (atomic, UI-writable) ───────────────────────────────────
    std::atomic<float> volume      { 1.0f };
    std::atomic<int>   transpose   { 0 };
    std::atomic<int>   midiChannel { 0 };    // 0 = omni
    std::atomic<int>   presetIndex { 0 };    // index into cachedPresets
    std::atomic<bool>  loaded      { false };

    /** Set when presetIndex changes; audio thread picks it up in process(). */
    std::atomic<bool>  programChangePending { false };

    // ── Scratch buffer for FluidSynth interleaved → planar conversion ─────────
    std::vector<float> scratchL, scratchR;

    // ── Private helpers ───────────────────────────────────────────────────────
    void applyPendingLoad();      ///< called at top of process()
    void applyProgramChange();    ///< called at top of process() when flag set

    /** Build and post a fresh preset list after a successful sfont load.
     *  Called from the audio thread — no locks needed on write side. */
    void postPresetList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SfzPlayer)
};
