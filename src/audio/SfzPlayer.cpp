// =============================================================================
//  SfzPlayer.cpp  —  Real-time SF2 playback engine (FluidSynth backend)
// =============================================================================
#include "SfzPlayer.h"

// =============================================================================
//  Constructor / destructor
// =============================================================================

SfzPlayer::SfzPlayer()
{
#if DYSEKT_HAS_FLUIDSYNTH
    // Prevent FluidSynth from spawning its own audio driver thread.
    // We only use it as an offline render engine inside processBlock.
    const char* drv[] { nullptr };
    fluid_audio_driver_register (drv);
#endif
}

SfzPlayer::~SfzPlayer()
{
    // Drain any pending load so we don't leak.
    delete pendingLoad.exchange (nullptr, std::memory_order_acq_rel);

    // Drain any pending preset list.
    delete freshPresets.exchange (nullptr, std::memory_order_acq_rel);

#if DYSEKT_HAS_FLUIDSYNTH
    if (synth    != nullptr) { delete_fluid_synth    (synth);    synth    = nullptr; }
    if (settings != nullptr) { delete_fluid_settings (settings); settings = nullptr; }
#endif
}

// =============================================================================
//  UI-thread API
// =============================================================================

void SfzPlayer::loadFile (const juce::File& f)
{
    auto* pkg        = new PendingLoad();
    pkg->file        = f;
    pkg->shouldUnload = false;
    delete pendingLoad.exchange (pkg, std::memory_order_acq_rel);
}

void SfzPlayer::unload()
{
    auto* pkg        = new PendingLoad();
    pkg->shouldUnload = true;
    delete pendingLoad.exchange (pkg, std::memory_order_acq_rel);
}

void SfzPlayer::setVolume      (float g) { volume.store (g, std::memory_order_relaxed); }
void SfzPlayer::setTranspose   (int s)   { transpose.store (s, std::memory_order_relaxed); }
void SfzPlayer::setMidiChannel (int c)   { midiChannel.store (c, std::memory_order_relaxed); }

void SfzPlayer::setPan (float p)
{
    pan.store (juce::jlimit (-1.0f, 1.0f, p), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr)
    {
        // CC10 pan: 0 = hard L, 64 = centre, 127 = hard R
        const int cc10 = juce::jlimit (0, 127,
            juce::roundToInt ((p + 1.0f) * 0.5f * 127.0f));
        fluid_synth_cc (synth, 0, 10, cc10);
    }
#endif
}

void SfzPlayer::setFineTune (float cents)
{
    fineTune.store (juce::jlimit (-100.0f, 100.0f, cents), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr)
        fluid_synth_set_gen (synth, 0, GEN_FINETUNE, cents);
#endif
}

void SfzPlayer::setReverb (float level)
{
    reverb.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr)
        fluid_synth_set_reverb (synth,
            0.6,           // room size  (0.0–1.0)
            0.5,           // damping    (0.0–1.0)
            0.5,           // width      (0.0–100.0)
            (double) level); // level    (0.0–1.0)
#endif
}

void SfzPlayer::setChorus (float level)
{
    chorus.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr)
        fluid_synth_set_chorus (synth,
            3,                        // voice count (1–99)
            (double) level * 10.0,    // level       (0.0–10.0)
            0.3,                      // speed Hz    (0.29–5.0)
            8.0,                      // depth ms    (0.0–21.0)
            FLUID_CHORUS_MOD_SINE);
#endif
}

void SfzPlayer::setPresetByIndex (int idx)
{
    presetIndex.store (idx, std::memory_order_relaxed);
    programChangePending.store (true, std::memory_order_release);
}

juce::File SfzPlayer::getLoadedFile() const
{
    // activeFile is only written on the audio thread; a torn read is harmless
    // here because we only use it for display.
    return activeFile;
}

std::vector<Sf2PresetInfo> SfzPlayer::getPresetList() const
{
    // Swap in any freshly-posted list from the audio thread.
    if (auto* p = freshPresets.exchange (nullptr, std::memory_order_acq_rel))
    {
        cachedPresets = std::move (*p);
        delete p;
    }
    return cachedPresets;
}

// =============================================================================
//  Audio-thread API
// =============================================================================

void SfzPlayer::prepare (double sampleRate, int maxBlockSize)
{
    currentSR    = sampleRate;
    currentBlock = maxBlockSize;

    scratchL.assign ((size_t) maxBlockSize, 0.0f);
    scratchR.assign ((size_t) maxBlockSize, 0.0f);

#if DYSEKT_HAS_FLUIDSYNTH
    // Keep the existing synth in sync with the new sample rate.
    if (synth != nullptr)
        fluid_synth_set_sample_rate (synth, (float) sampleRate);
#endif
}

void SfzPlayer::process (const juce::MidiBuffer& midiIn,
                          float* outL, float* outR, int numSamples)
{
    applyPendingLoad();

#if DYSEKT_HAS_FLUIDSYNTH
    if (synth == nullptr || ! loaded.load (std::memory_order_relaxed))
        return;

    // Apply any pending program-change first.
    if (programChangePending.load (std::memory_order_acquire))
        applyProgramChange();

    // ── Forward MIDI ──────────────────────────────────────────────────────────
    const int filterCh = midiChannel.load (std::memory_order_relaxed);
    const int trans    = transpose.load   (std::memory_order_relaxed);

    // FluidSynth always operates on channel 0 internally; we use it as a
    // mono channel and filter / forward incoming MIDI accordingly.
    constexpr int kFluidCh = 0;

    for (const auto meta : midiIn)
    {
        const auto msg = meta.getMessage();
        const int  ch  = msg.getChannel();   // 1-16

        if (filterCh != 0 && ch != filterCh)
            continue;

        if (msg.isNoteOn())
        {
            const int note = juce::jlimit (0, 127, msg.getNoteNumber() + trans);
            fluid_synth_noteon (synth, kFluidCh, note, msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            const int note = juce::jlimit (0, 127, msg.getNoteNumber() + trans);
            fluid_synth_noteoff (synth, kFluidCh, note);
        }
        else if (msg.isController())
        {
            fluid_synth_cc (synth, kFluidCh,
                            msg.getControllerNumber(),
                            msg.getControllerValue());
        }
        else if (msg.isPitchWheel())
        {
            // FluidSynth expects 0..16383; JUCE provides 0..16383 directly.
            fluid_synth_pitch_bend (synth, kFluidCh, msg.getPitchWheelValue());
        }
        else if (msg.isChannelPressure())
        {
            fluid_synth_channel_pressure (synth, kFluidCh,
                                          msg.getChannelPressureValue());
        }
        else if (msg.isAftertouch())
        {
            fluid_synth_key_pressure (synth, kFluidCh,
                                      msg.getNoteNumber(),
                                      msg.getAfterTouchValue());
        }
        else if (msg.isProgramChange())
        {
            fluid_synth_program_change (synth, kFluidCh,
                                        msg.getProgramChangeNumber());
        }
        else if (msg.isSysEx())
        {
            fluid_synth_sysex (synth,
                               reinterpret_cast<const char*> (msg.getSysExData()),
                               msg.getSysExDataSize(),
                               nullptr, nullptr, nullptr, 0);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            fluid_synth_all_notes_off (synth, kFluidCh);
        }
    }

    // ── Render ────────────────────────────────────────────────────────────────
    if ((int) scratchL.size() < numSamples)
    {
        scratchL.assign ((size_t) numSamples, 0.0f);
        scratchR.assign ((size_t) numSamples, 0.0f);
    }
    else
    {
        // fluid_synth_process ACCUMULATES — must zero before every call
        // or stale audio from the previous block feeds back indefinitely.
        std::fill (scratchL.begin(), scratchL.begin() + numSamples, 0.0f);
        std::fill (scratchR.begin(), scratchR.begin() + numSamples, 0.0f);
    }

    // FluidSynth writes directly into planar buffers passed as an array.
    float* planes[2] = { scratchL.data(), scratchR.data() };
    fluid_synth_process (synth, numSamples, 0, nullptr, 2, planes);

    // Mix into output with volume scaling.
    const float vol = volume.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] += scratchL[(size_t) i] * vol;
        outR[i] += scratchR[(size_t) i] * vol;
    }
#else
    juce::ignoreUnused (midiIn, outL, outR, numSamples);
#endif
}

// =============================================================================
//  Private helpers
// =============================================================================

void SfzPlayer::applyPendingLoad()
{
    auto* pkg = pendingLoad.exchange (nullptr, std::memory_order_acq_rel);
    if (pkg == nullptr)
        return;

    std::unique_ptr<PendingLoad> owner (pkg);

#if DYSEKT_HAS_FLUIDSYNTH
    // ── Tear down existing synth ──────────────────────────────────────────────
    if (synth != nullptr)
    {
        fluid_synth_all_notes_off (synth, 0);
        delete_fluid_synth    (synth);    synth    = nullptr;
    }
    if (settings != nullptr)
    {
        delete_fluid_settings (settings); settings = nullptr;
    }
    sfontId = -1;
    loaded.store (false, std::memory_order_release);
    activeFile = juce::File();

    // Post an empty preset list so the UI clears.
    delete freshPresets.exchange (new std::vector<Sf2PresetInfo>(),
                                  std::memory_order_acq_rel);

    if (owner->shouldUnload)
        return;

    // ── Create new synth ──────────────────────────────────────────────────────
    settings = new_fluid_settings();

#if JUCE_DEBUG
    fluid_settings_setint (settings, "synth.verbose", 0);
#endif

    fluid_settings_setint (settings, "synth.reverb.active", 1);
    fluid_settings_setint (settings, "synth.chorus.active", 1);

    synth = new_fluid_synth (settings);
    fluid_synth_set_sample_rate (synth, (float) currentSR);

    // Boost gain — FluidSynth defaults are quiet.
    fluid_synth_set_gain (synth, 2.0f);

    // ── Load the soundfont ────────────────────────────────────────────────────
    sfontId = fluid_synth_sfload (synth,
                  owner->file.getFullPathName().toRawUTF8(), 1);

    if (sfontId == FLUID_FAILED)
    {
        delete_fluid_synth    (synth);    synth    = nullptr;
        delete_fluid_settings (settings); settings = nullptr;
        return;
    }

    activeFile = owner->file;
    loaded.store (true, std::memory_order_release);

    // Re-apply user params that FluidSynth loses when synth is recreated.
    setPan      (pan.load      (std::memory_order_relaxed));
    setFineTune (fineTune.load (std::memory_order_relaxed));
    setReverb   (reverb.load   (std::memory_order_relaxed));
    setChorus   (chorus.load   (std::memory_order_relaxed));

    // Reset program-change index to first preset.
    presetIndex.store (0, std::memory_order_relaxed);

    // Build and post the preset list before applying the initial selection.
    postPresetList();
    applyProgramChange();

#else  // DYSEKT_HAS_FLUIDSYNTH not defined
    juce::ignoreUnused (owner);
#endif
}

void SfzPlayer::applyProgramChange()
{
    programChangePending.store (false, std::memory_order_release);

#if DYSEKT_HAS_FLUIDSYNTH
    if (synth == nullptr || sfontId == FLUID_FAILED)
        return;

    // Read the latest cached list (audio thread's own copy — no UI access).
    // freshPresets may hold a pointer we posted; we skip it here since we
    // already populated presetListCache in postPresetList().
    // Instead we re-enumerate to get a local snapshot.
    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id (synth, sfontId);
    if (sfont == nullptr)
        return;

    const int idx = juce::jlimit (0, 16383, presetIndex.load (std::memory_order_relaxed));

    // Walk to the idx-th preset.
    fluid_sfont_iteration_start (sfont);
    int count = 0;
    fluid_preset_t* chosen = nullptr;
    for (fluid_preset_t* p = fluid_sfont_iteration_next (sfont);
         p != nullptr;
         p = fluid_sfont_iteration_next (sfont), ++count)
    {
        chosen = p;
        if (count == idx) break;
    }

    if (chosen == nullptr)
        return;

    const int bank   = fluid_preset_get_banknum (chosen);
    const int preset = fluid_preset_get_num     (chosen);
    const int offset = fluid_synth_get_bank_offset (synth, sfontId);

    fluid_synth_program_select (synth, 0, sfontId,
                                (unsigned int)(offset + bank),
                                (unsigned int) preset);
#endif
}

void SfzPlayer::postPresetList()
{
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth == nullptr || sfontId == FLUID_FAILED)
        return;

    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id (synth, sfontId);
    if (sfont == nullptr)
        return;

    auto* list = new std::vector<Sf2PresetInfo>();

    fluid_sfont_iteration_start (sfont);
    for (fluid_preset_t* p = fluid_sfont_iteration_next (sfont);
         p != nullptr;
         p = fluid_sfont_iteration_next (sfont))
    {
        Sf2PresetInfo info;
        info.bank   = fluid_preset_get_banknum (p);
        info.preset = fluid_preset_get_num     (p);
        info.name   = juce::String::fromUTF8   (fluid_preset_get_name (p));
        list->push_back (info);
    }

    // Discard any previous unread list and post the fresh one.
    delete freshPresets.exchange (list, std::memory_order_acq_rel);
#endif
}
