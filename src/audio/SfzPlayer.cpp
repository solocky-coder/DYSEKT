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

// ── SFZ ADSR setters ──────────────────────────────────────────────────────────

void SfzPlayer::setSfzAttack  (float s) noexcept
{
    sfzAttackSec .store (juce::jlimit (0.0f,  30.0f, s), std::memory_order_relaxed);
    sfzAdsrDirty .store (true, std::memory_order_release);
}
void SfzPlayer::setSfzDecay   (float s) noexcept
{
    sfzDecaySec  .store (juce::jlimit (0.0f,  30.0f, s), std::memory_order_relaxed);
    sfzAdsrDirty .store (true, std::memory_order_release);
}
void SfzPlayer::setSfzSustain (float p) noexcept
{
    sfzSustainPct.store (juce::jlimit (0.0f, 100.0f, p), std::memory_order_relaxed);
    sfzAdsrDirty .store (true, std::memory_order_release);
}
void SfzPlayer::setSfzRelease (float s) noexcept
{
    sfzReleaseSec.store (juce::jlimit (0.0f,  60.0f, s), std::memory_order_relaxed);
    sfzAdsrDirty .store (true, std::memory_order_release);
}

// ── Per-zone vol/pan (SFZ only — sfizz OSC) ──────────────────────────────────

void SfzPlayer::setZoneVolume (int regionIndex, float volDb) noexcept
{
#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth == nullptr || !isSfzFile) return;
    const int numRegions = sfizz_get_num_regions (sfizzSynth);
    if (regionIndex < 0 || regionIndex >= numRegions) return;

    char path[64];
    snprintf (path, sizeof (path), "/region%d/volume", regionIndex);
    sfizz_arg_t arg;
    arg.f = juce::jlimit (-144.0f, 6.0f, volDb);
    sfizz_send_message (sfizzSynth, nullptr, 0, path, "f", &arg);
#else
    juce::ignoreUnused (regionIndex, volDb);
#endif
}

void SfzPlayer::setZonePan (int regionIndex, float pan) noexcept
{
#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth == nullptr || !isSfzFile) return;
    const int numRegions = sfizz_get_num_regions (sfizzSynth);
    if (regionIndex < 0 || regionIndex >= numRegions) return;

    // SFZ pan opcode is in percent: -100 (L) .. 0 (C) .. +100 (R)
    char path[64];
    snprintf (path, sizeof (path), "/region%d/pan", regionIndex);
    sfizz_arg_t arg;
    arg.f = juce::jlimit (-100.0f, 100.0f, pan * 100.0f);
    sfizz_send_message (sfizzSynth, nullptr, 0, path, "f", &arg);
#else
    juce::ignoreUnused (regionIndex, pan);
#endif
}

void SfzPlayer::setZoneTune (int regionIndex, float cents) noexcept
{
#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth == nullptr || !isSfzFile) return;
    const int numRegions = sfizz_get_num_regions (sfizzSynth);
    if (regionIndex < 0 || regionIndex >= numRegions) return;

    char path[64];
    snprintf (path, sizeof (path), "/region%d/tune", regionIndex);
    sfizz_arg_t arg;
    arg.f = juce::jlimit (-100.0f, 100.0f, cents);
    sfizz_send_message (sfizzSynth, nullptr, 0, path, "f", &arg);
#else
    juce::ignoreUnused (regionIndex, cents);
#endif
}

void SfzPlayer::sendAdsrToSfizz()
{
#if DYSEKT_HAS_SFIZZ
    if (sfizzSynth == nullptr) return;
    const int numRegions = sfizz_get_num_regions (sfizzSynth);
    if (numRegions <= 0) return;

    const float attack  = sfzAttackSec .load (std::memory_order_relaxed);
    const float decay   = sfzDecaySec  .load (std::memory_order_relaxed);
    const float sustain = sfzSustainPct.load (std::memory_order_relaxed);
    const float release = sfzReleaseSec.load (std::memory_order_relaxed);

    auto sendFloat = [&] (int region, const char* opcode, float value)
    {
        char path[64];
        snprintf (path, sizeof (path), "/region%d/%s", region, opcode);
        sfizz_arg_t arg;
        arg.f = value;
        sfizz_send_message (sfizzSynth, nullptr, 0, path, "f", &arg);
    };

    for (int i = 0; i < numRegions; ++i)
    {
        sendFloat (i, "ampeg_attack",  attack);
        sendFloat (i, "ampeg_decay",   decay);
        sendFloat (i, "ampeg_sustain", sustain);
        sendFloat (i, "ampeg_release", release);
    }
#endif
}

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
    {
        // Use the non-deprecated group API (fx_group = -1 = all groups).
        fluid_synth_set_reverb_group_roomsize (synth, -1, 0.6);
        fluid_synth_set_reverb_group_damp     (synth, -1, 0.5);
        fluid_synth_set_reverb_group_width    (synth, -1, 0.5);
        fluid_synth_set_reverb_group_level    (synth, -1, (double) level);

        // GEN_REVERBSEND controls the per-voice send amount (0-1000 centibels,
        // where 1000 = full send).  Without this the SF2 preset's own send
        // value (often 0) overrides everything and no signal reaches the
        // reverb engine regardless of the level setting above.
        fluid_synth_set_gen (synth, 0, GEN_REVERBSEND, level * 1000.0f);
    }
#endif
    // sfizz: no reverb API — SFZ files define reverb via opcodes internally
}

void SfzPlayer::setChorus (float level)
{
    chorus.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
#if DYSEKT_HAS_FLUIDSYNTH
    if (synth != nullptr && !isSfzFile)
    {
        // Use the non-deprecated group API (fx_group = -1 = all groups).
        fluid_synth_set_chorus_group_nr    (synth, -1, 3);
        fluid_synth_set_chorus_group_level (synth, -1, (double) level * 10.0);
        fluid_synth_set_chorus_group_speed (synth, -1, 0.3);
        fluid_synth_set_chorus_group_depth (synth, -1, 8.0);
        fluid_synth_set_chorus_group_type  (synth, -1, FLUID_CHORUS_MOD_SINE);

        // GEN_CHORUSSEND controls the per-voice send amount (0-1000 centibels).
        // Same as GEN_REVERBSEND — must be forced or the SF2 preset value wins.
        fluid_synth_set_gen (synth, 0, GEN_CHORUSSEND, level * 1000.0f);
    }
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

    scratchL.assign   ((size_t) maxBlockSize, 0.0f);
    scratchR.assign   ((size_t) maxBlockSize, 0.0f);


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
        scratchL.assign   ((size_t) numSamples, 0.0f);
        scratchR.assign   ((size_t) numSamples, 0.0f);

    }

#if DYSEKT_HAS_SFIZZ
    if (isSfzFile && sfizzSynth != nullptr)
    {
        // ── Flush pending ADSR changes via sfizz OSC messages ────────────────
        if (sfzAdsrDirty.exchange (false, std::memory_order_acquire))
            sendAdsrToSfizz();

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
    // fluid_synth_process routes:
    //   dry audio  → out[]  (nout buffers)
    //   reverb/chorus wet → fx[]  (nfx buffers)
    // Passing nfx=0 / fx=nullptr silently discards all effects output.
    // We must supply FX buffers and mix them into the output ourselves.
    const int n = numSamples;
    std::fill (scratchL.begin(), scratchL.begin() + n, 0.0f);
    std::fill (scratchR.begin(), scratchR.begin() + n, 0.0f);

    // FX scratch buffers (reverb L/R + chorus L/R, interleaved as two stereo pairs)
    float* dryPlanes[2] = { scratchL.data(), scratchR.data() };

    // FluidSynth 2.x uses 4 FX buses: [0]=reverb-L, [1]=reverb-R,
    // [2]=chorus-L, [3]=chorus-R.  Aliasing them to the dry buffers
    // causes FluidSynth to accumulate effects directly into the dry output —
    // the canonical approach when no dedicated FX buses exist (from FluidSynth docs).
    float* fxPlanes[4] = { scratchL.data(), scratchR.data(),
                            scratchL.data(), scratchR.data() };

    fluid_synth_process (synth, n, 4, fxPlanes, 2, dryPlanes);

    for (int i = 0; i < n; ++i)
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
        sfizzSynth = sfizz_create_synth();
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
