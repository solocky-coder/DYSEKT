// =============================================================================
//  SfzPlayer.cpp
// =============================================================================
#include "SfzPlayer.h"

// sfizz.h included at file scope so sfizz_t is visible throughout this TU.
#if DYSEKT_HAS_SFIZZ
#  include <sfizz.h>
// Convenience macro: cast the opaque void* handle to sfizz_t*
#  define SFZ (static_cast<sfizz_t*> (sfzHandle))
#endif

SfzPlayer::SfzPlayer()  = default;

SfzPlayer::~SfzPlayer()
{
    delete pendingLoad.exchange (nullptr, std::memory_order_acq_rel);

#if DYSEKT_HAS_SFIZZ
    if (sfzHandle != nullptr)
    {
        sfizz_free (SFZ);
        sfzHandle = nullptr;
    }
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
    auto* pkg         = new PendingLoad();
    pkg->shouldUnload = true;
    delete pendingLoad.exchange (pkg, std::memory_order_acq_rel);
}

void SfzPlayer::setVolume      (float g) { volume.store      (g, std::memory_order_relaxed); }
void SfzPlayer::setTranspose   (int s)   { transpose.store   (s, std::memory_order_relaxed); }
void SfzPlayer::setMidiChannel (int c)   { midiChannel.store (c, std::memory_order_relaxed); }

juce::File SfzPlayer::getLoadedFile() const { return activeFile; }

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
    if (sfzHandle != nullptr)
    {
        sfizz_set_sample_rate       (SFZ, (float) sampleRate);
        sfizz_set_samples_per_block (SFZ, maxBlockSize);
    }
#endif
}

void SfzPlayer::process (const juce::MidiBuffer& midiIn,
                          float* outL, float* outR, int numSamples)
{
    applyPendingLoad();

#if DYSEKT_HAS_SFIZZ
    if (sfzHandle == nullptr || ! loaded.load (std::memory_order_relaxed))
        return;

    const int filterCh = midiChannel.load (std::memory_order_relaxed);
    const int trans    = transpose.load   (std::memory_order_relaxed);

    for (const auto meta : midiIn)
    {
        const auto msg = meta.getMessage();
        const int  ch  = msg.getChannel();

        if (filterCh != 0 && ch != filterCh)
            continue;

        const int samplePos = meta.samplePosition;

        if (msg.isNoteOn())
            sfizz_send_note_on (SFZ, samplePos,
                                juce::jlimit (0, 127, msg.getNoteNumber() + trans),
                                msg.getVelocity());
        else if (msg.isNoteOff())
            sfizz_send_note_off (SFZ, samplePos,
                                 juce::jlimit (0, 127, msg.getNoteNumber() + trans),
                                 msg.getVelocity());
        else if (msg.isController())
            sfizz_send_cc (SFZ, samplePos,
                           msg.getControllerNumber(),
                           msg.getControllerValue());
        else if (msg.isPitchWheel())
            sfizz_send_pitch_wheel (SFZ, samplePos,
                                     (float)(msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isAftertouch())
            sfizz_send_channel_aftertouch (SFZ, samplePos,
                                            (float) msg.getAfterTouchValue() / 127.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            sfizz_all_sound_off (SFZ);
    }

    if ((int) scratchL.size() < numSamples)
    {
        scratchL.assign ((size_t) numSamples, 0.0f);
        scratchR.assign ((size_t) numSamples, 0.0f);
    }

    std::fill (scratchL.begin(), scratchL.begin() + numSamples, 0.0f);
    std::fill (scratchR.begin(), scratchR.begin() + numSamples, 0.0f);

    float* outs[2] = { scratchL.data(), scratchR.data() };
    sfizz_render_block (SFZ, outs, 2, numSamples);

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
//  applyPendingLoad  (audio thread)
// =============================================================================

void SfzPlayer::applyPendingLoad()
{
    auto* pkg = pendingLoad.exchange (nullptr, std::memory_order_acq_rel);
    if (pkg == nullptr) return;

    std::unique_ptr<PendingLoad> owner (pkg);

#if DYSEKT_HAS_SFIZZ
    if (sfzHandle != nullptr)
    {
        sfizz_free (SFZ);
        sfzHandle = nullptr;
    }
    loaded.store (false, std::memory_order_release);
    activeFile = juce::File();

    if (owner->shouldUnload) return;

    sfzHandle = sfizz_create();
    sfizz_set_sample_rate       (SFZ, (float) currentSR);
    sfizz_set_samples_per_block (SFZ, currentBlock);

    if (sfizz_load_file (SFZ, owner->file.getFullPathName().toRawUTF8()))
    {
        activeFile = owner->file;
        loaded.store (true, std::memory_order_release);
    }
    else
    {
        sfizz_free (SFZ);
        sfzHandle = nullptr;
    }
#else
    juce::ignoreUnused (owner);
#endif
}
