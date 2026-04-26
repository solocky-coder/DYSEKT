// =============================================================================
//  SfzPlayer.cpp  —  Real-time SF2/SFZ playback engine
//                    SF2 → FluidSynth backend
//                    SFZ → sfizz backend
// =============================================================================
#include "SfzPlayer.h"

#if DYSEKT_HAS_SFIZZ
  // Include sfizz C API via path relative to the project root.
  // This avoids relying on target_include_directories propagation.
  #include "../../sfizz/src/sfizz.h"
#endif

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

#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth != nullptr) { sfizz_free (sfizzSynth); sfizzSynth = nullptr; }
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
    if (synth != nullptr && !isSfzFile)
    {
        // CC10 pan: 0 = hard L, 64 = centre, 127 = hard R
        const int cc10 = juce::jlimit (0, 127,
            juce::roundToInt ((p + 1.0f) * 0.5f * 127.0f));
        fluid_synth_cc (synth, 0, 10, cc10);
    }
#endif
#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth != nullptr && isSfzFile)
    {
        const int cc10 = juce::jlimit (0, 127,
            juce::roundToInt ((p + 1.0f) * 0.5f * 127.0f));
        sfizz_send_cc (sfizzSynth, 0, 10, cc10);
    }
#endif
}

void SfzPlayer::setFineTune (float cents)
{
    fineTune.store (juce::jlimit (-100.0f, 100.0f, cents), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr && !isSfzFile)
        fluid_synth_set_gen (synth, 0, GEN_FINETUNE, cents);
#endif
    // sfizz fine-tune is applied via pitch-bend offset on next note — no direct API
}

void SfzPlayer::setReverb (float level)
{
    reverb.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr && !isSfzFile)
        fluid_synth_set_reverb (synth,
            0.6,           // room size  (0.0–1.0)
            0.5,           // damping    (0.0–1.0)
            0.5,           // width      (0.0–100.0)
            (double) level); // level    (0.0–1.0)
#endif
    // sfizz: no reverb API — SFZ files define reverb via opcodes internally
}

void SfzPlayer::setChorus (float level)
{
    chorus.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr && !isSfzFile)
        fluid_synth_set_chorus (synth,
            3,                        // voice count (1–99)
            (double) level * 10.0,    // level       (0.0–10.0)
            0.3,                      // speed Hz    (0.29–5.0)
            8.0,                      // depth ms    (0.0–21.0)
            FLUID_CHORUS_MOD_SINE);
#endif
    // sfizz: no chorus API
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

#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth != nullptr)
    {
        sfizz_set_sample_rate       (sfizzSynth, (float) sampleRate);
        sfizz_set_samples_per_block (sfizzSynth, maxBlockSize);
    }
#endif
}

void SfzPlayer::process (const juce::MidiBuffer& midiIn,
                          float* outL, float* outR, int numSamples)
{
    applyPendingLoad();

    if (! loaded.load (std::memory_order_relaxed))
        return;

    const int filterCh = midiChannel.load (std::memory_order_relaxed);
    const int trans    = transpose.load   (std::memory_order_relaxed);
    const float vol    = volume.load      (std::memory_order_relaxed);

    // Ensure scratch buffers are large enough
    if ((int) scratchL.size() < numSamples)
    {
        scratchL.assign ((size_t) numSamples, 0.0f);
        scratchR.assign ((size_t) numSamples, 0.0f);
    }

#if DYSEKT_HAS_SFIZZ
    if (isSfzFile && sfizzSynth != nullptr)
    {
        // ── Forward MIDI to sfizz ─────────────────────────────────────────────
        for (const auto meta : midiIn)
        {
            const auto msg = meta.getMessage();
            const int  ch  = msg.getChannel();   // 1-16

            if (filterCh != 0 && ch != filterCh)
                continue;

            if (msg.isNoteOn())
            {
                const int note = juce::jlimit (0, 127, msg.getNoteNumber() + trans);
                sfizz_send_note_on (sfizzSynth, meta.samplePosition, note, msg.getVelocity());
            }
            else if (msg.isNoteOff())
            {
                const int note = juce::jlimit (0, 127, msg.getNoteNumber() + trans);
                sfizz_send_note_off (sfizzSynth, meta.samplePosition, note, msg.getVelocity());
            }
            else if (msg.isController())
            {
                sfizz_send_cc (sfizzSynth, meta.samplePosition,
                               msg.getControllerNumber(),
                               msg.getControllerValue());
            }
            else if (msg.isPitchWheel())
            {
                // sfizz expects -8192..+8191; JUCE provides 0..16383
                sfizz_send_pitch_wheel (sfizzSynth, meta.samplePosition,
                                        msg.getPitchWheelValue() - 8192);
            }
            else if (msg.isChannelPressure())
            {
                sfizz_send_channel_aftertouch (sfizzSynth, meta.samplePosition,
                                               msg.getChannelPressureValue());
            }
            else if (msg.isAftertouch())
            {
                sfizz_send_poly_aftertouch (sfizzSynth, meta.samplePosition,
                                            msg.getNoteNumber(),
                                            msg.getAfterTouchValue());
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                sfizz_all_sound_off (sfizzSynth);
            }
        }

        // ── Render sfizz ──────────────────────────────────────────────────────
        std::fill (scratchL.begin(), scratchL.begin() + numSamples, 0.0f);
        std::fill (scratchR.begin(), scratchR.begin() + numSamples, 0.0f);

        float* planes[2] = { scratchL.data(), scratchR.data() };
        sfizz_render_block (sfizzSynth, planes, 2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] += scratchL[(size_t) i] * vol;
            outR[i] += scratchR[(size_t) i] * vol;
        }
        return;
    }
#endif

#if DYSEKT_HAS_FLUIDSYNTH
    if (synth == nullptr)
        return;

    // Apply any pending program-change first.
    if (programChangePending.load (std::memory_order_acquire))
        applyProgramChange();

    // ── Forward MIDI to FluidSynth ────────────────────────────────────────────
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

    // ── Render FluidSynth ─────────────────────────────────────────────────────
    // fluid_synth_process ACCUMULATES — must zero before every call.
    std::fill (scratchL.begin(), scratchL.begin() + numSamples, 0.0f);
    std::fill (scratchR.begin(), scratchR.begin() + numSamples, 0.0f);

    float* planes[2] = { scratchL.data(), scratchR.data() };
    fluid_synth_process (synth, numSamples, 0, nullptr, 2, planes);

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

    // ── Tear down whatever is currently loaded ────────────────────────────────
    loaded.store (false, std::memory_order_release);
    activeFile = juce::File();
    isSfzFile  = false;

#if DYSEKT_HAS_FLUIDSYNTH
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
#endif

#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth != nullptr)
    {
        sfizz_all_sound_off (sfizzSynth);
        sfizz_free (sfizzSynth);
        sfizzSynth = nullptr;
    }
#endif

    // Post an empty preset list so the UI clears.
    delete freshPresets.exchange (new std::vector<Sf2PresetInfo>(),
                                  std::memory_order_acq_rel);

    if (owner->shouldUnload)
        return;

    const auto ext = owner->file.getFileExtension().toLowerCase();

    // ── SFZ path (sfizz) ─────────────────────────────────────────────────────
#if DYSEKT_HAS_SFIZZ
    if (ext == ".sfz")
    {
        isSfzFile  = true;
        sfizzSynth = sfizz_create();
        sfizz_set_sample_rate       (sfizzSynth, (float) currentSR);
        sfizz_set_samples_per_block (sfizzSynth, currentBlock);

        if (! sfizz_load_file (sfizzSynth, owner->file.getFullPathName().toRawUTF8()))
        {
            sfizz_free (sfizzSynth);
            sfizzSynth = nullptr;
            isSfzFile  = false;
            return;
        }

        activeFile = owner->file;
        loaded.store (true, std::memory_order_release);

        // Re-apply pan (sfizz responds to CC10)
        setPan (pan.load (std::memory_order_relaxed));

        // Post a single dummy preset entry so the UI shows "loaded"
        auto* list = new std::vector<Sf2PresetInfo>();
        list->push_back ({ 0, 0, owner->file.getFileNameWithoutExtension() });
        delete freshPresets.exchange (list, std::memory_order_acq_rel);

        return;
    }
#endif

    // ── SF2 path (FluidSynth) ─────────────────────────────────────────────────
#if DYSEKT_HAS_FLUIDSYNTH
    settings = new_fluid_settings();

#if JUCE_DEBUG
    fluid_settings_setint (settings, "synth.verbose", 0);
#endif

    fluid_settings_setint (settings, "synth.reverb.active", 1);
    fluid_settings_setint (settings, "synth.chorus.active", 1);

    synth = new_fluid_synth (settings);
    fluid_synth_set_sample_rate (synth, (float) currentSR);
    fluid_synth_set_gain        (synth, 2.0f);

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

    presetIndex.store (0, std::memory_order_relaxed);
    postPresetList();
    applyProgramChange();

#else
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
