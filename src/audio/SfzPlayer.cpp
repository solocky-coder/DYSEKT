// =============================================================================
//  SfzPlayer.cpp
// =============================================================================
#include "SfzPlayer.h"

SfzPlayer::SfzPlayer()  = default;

SfzPlayer::~SfzPlayer()
{
    // Drain any pending load so we don't leak
    delete pendingLoad.exchange (nullptr, std::memory_order_acq_rel);

#if DYSEKT_HAS_SFIZZ
    if (sfz != nullptr)
    {
        sfizz_free (sfz);
        sfz = nullptr;
    }
#endif
}

// =============================================================================
//  UI-thread API
// =============================================================================

void SfzPlayer::loadFile (const juce::File& f)
{
    auto* pkg = new PendingLoad();
    pkg->file        = f;
    pkg->shouldUnload = false;

    delete pendingLoad.exchange (pkg, std::memory_order_acq_rel);
}

void SfzPlayer::unload()
{
    auto* pkg = new PendingLoad();
    pkg->shouldUnload = true;

    delete pendingLoad.exchange (pkg, std::memory_order_acq_rel);
}

void SfzPlayer::setVolume    (float g) { volume.store (g, std::memory_order_relaxed); }
void SfzPlayer::setTranspose (int s)   { transpose.store (s, std::memory_order_relaxed); }
void SfzPlayer::setMidiChannel (int c) { midiChannel.store (c, std::memory_order_relaxed); }

juce::File SfzPlayer::getLoadedFile() const
{
    // activeFile is only written on the audio thread; safe to read for display
    // purposes from the UI thread (the worst case is a torn read of a File name,
    // which is harmless — we only use it for display).
    return activeFile;
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

#if DYSEKT_HAS_SFIZZ
    if (sfz != nullptr)
    {
        sfizz_set_sample_rate      (sfz, (float) sampleRate);
        sfizz_set_samples_per_block (sfz, maxBlockSize);
    }
#endif
}

void SfzPlayer::process (const juce::MidiBuffer& midiIn,
                          float* outL, float* outR, int numSamples)
{
    applyPendingLoad();

#if DYSEKT_HAS_SFIZZ
    if (sfz == nullptr || ! loaded.load (std::memory_order_relaxed))
        return;

    // ── Forward MIDI ──────────────────────────────────────────────────────────
    const int filterCh = midiChannel.load (std::memory_order_relaxed);
    const int trans    = transpose.load   (std::memory_order_relaxed);

    for (const auto meta : midiIn)
    {
        const auto msg = meta.getMessage();
        const int  ch  = msg.getChannel();   // 1-16

        // Channel filter (0 = omni passes everything)
        if (filterCh != 0 && ch != filterCh)
            continue;

        const int samplePos = meta.samplePosition;

        if (msg.isNoteOn())
        {
            int note = juce::jlimit (0, 127, msg.getNoteNumber() + trans);
            sfizz_send_note_on (sfz, samplePos, note, msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            int note = juce::jlimit (0, 127, msg.getNoteNumber() + trans);
            sfizz_send_note_off (sfz, samplePos, note, msg.getVelocity());
        }
        else if (msg.isController())
        {
            sfizz_send_cc (sfz, samplePos,
                           msg.getControllerNumber(),
                           msg.getControllerValue());
        }
        else if (msg.isPitchWheel())
        {
            // sfizz expects normalised ±1.0
            const float pw = (float)(msg.getPitchWheelValue() - 8192) / 8192.0f;
            sfizz_send_pitch_wheel (sfz, samplePos, pw);
        }
        else if (msg.isAftertouch())
        {
            sfizz_send_channel_aftertouch (sfz, samplePos,
                                            (float) msg.getAfterTouchValue() / 127.0f);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            sfizz_all_sound_off (sfz);
        }
    }

    // ── Render ────────────────────────────────────────────────────────────────
    // sfizz requires at least currentBlock samples in the scratch buffers.
    // numSamples may be smaller at session start — resize defensively.
    if ((int) scratchL.size() < numSamples)
    {
        scratchL.assign ((size_t) numSamples, 0.0f);
        scratchR.assign ((size_t) numSamples, 0.0f);
    }

    std::fill (scratchL.begin(), scratchL.begin() + numSamples, 0.0f);
    std::fill (scratchR.begin(), scratchR.begin() + numSamples, 0.0f);

    float* outs[2] = { scratchL.data(), scratchR.data() };
    sfizz_render_block (sfz, outs, 2, numSamples);

    // Mix into output with volume scaling
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

#if DYSEKT_HAS_SFIZZ
    // Tear down existing instance
    if (sfz != nullptr)
    {
        sfizz_free (sfz);
        sfz = nullptr;
    }
    loaded.store (false, std::memory_order_release);
    activeFile = juce::File();

    if (owner->shouldUnload)
        return;

    // Build new instance
    sfz = sfizz_create();
    sfizz_set_sample_rate       (sfz, (float) currentSR);
    sfizz_set_samples_per_block (sfz, currentBlock);

    const bool ok = sfizz_load_file (sfz,
                        owner->file.getFullPathName().toRawUTF8());
    if (ok)
    {
        activeFile = owner->file;
        loaded.store (true, std::memory_order_release);
    }
    else
    {
        sfizz_free (sfz);
        sfz = nullptr;
    }
#else
    juce::ignoreUnused (owner);
#endif
}
