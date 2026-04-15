#include "SampleData.h"
#include <cmath>
#include <algorithm>
#include <memory>

namespace
{
void buildMipmapsForBuffer (const juce::AudioBuffer<float>& src,
                             std::array<SampleData::MipmapLevel, SampleData::kNumMipmapLevels>& outMipmaps)
{
    int numFrames = src.getNumSamples();
    if (numFrames <= 0 || src.getNumChannels() < 1)
    {
        for (auto& m : outMipmaps)
        {
            m.samplesPerPeak = 0;
            m.maxPeaks.clear();
            m.minPeaks.clear();
        }
        return;
    }

    const float* dataL = src.getReadPointer (0);
    const float* dataR = src.getNumChannels() > 1 ? src.getReadPointer (1) : dataL;
    if (dataL == nullptr)
        return;

    static constexpr int kBlockSizes[SampleData::kNumMipmapLevels] = { 64, 512, 4096 };

    for (int level = 0; level < SampleData::kNumMipmapLevels; ++level)
    {
        auto& m = outMipmaps[(size_t) level];
        m.samplesPerPeak = kBlockSizes[level];
        int numPeaks = (numFrames + m.samplesPerPeak - 1) / m.samplesPerPeak;
        m.maxPeaks.resize ((size_t) numPeaks);
        m.minPeaks.resize ((size_t) numPeaks);

        for (int i = 0; i < numPeaks; ++i)
        {
            int start = i * m.samplesPerPeak;
            int end   = std::min (start + m.samplesPerPeak, numFrames);
            float hi  = -1.0f;
            float lo  =  1.0f;
            for (int s = start; s < end; ++s)
            {
                float val = (dataL[s] + dataR[s]) * 0.5f;
                if (val > hi) hi = val;
                if (val < lo) lo = val;
            }
            m.maxPeaks[(size_t) i] = hi;
            m.minPeaks[(size_t) i] = lo;
        }
    }
}
} // namespace

SampleData::SampleData() = default;

std::unique_ptr<SampleData::DecodedSample> SampleData::decodeFromFile (const juce::File& file,
                                                                         double projectSampleRate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
        return nullptr;

    auto numFrames        = (int) reader->lengthInSamples;
    auto numChannels      = (int) reader->numChannels;
    auto sourceSampleRate = reader->sampleRate;

    // ── Guard: defend against bad metadata from JUCE's MP3/VBR reader ─────────
    // dr_mp3 can return sampleRate=0 or lengthInSamples=0 for certain MP3 files.
    // Any of these conditions would crash in AudioBuffer allocation or division.
    if (numFrames <= 0 || numChannels <= 0 || numChannels > 64)
        return nullptr;
    if (sourceSampleRate <= 0.0 || ! std::isfinite (sourceSampleRate))
        return nullptr;
    // Hard cap: reject anything over 30 min @ 48 kHz to avoid bad-alloc on corrupt headers
    const int kMaxSamples = 48000 * 60 * 30;
    if (numFrames > kMaxSamples)
        return nullptr;

    // ── MP3 safety pad ────────────────────────────────────────────────────────
    // JUCE's dr_mp3 wrapper skips the Xing/VBR header frame during init but
    // still counts it in lengthInSamples. Allocating one extra MPEG frame
    // (1152 samples) prevents read() from writing past the buffer end.
    const bool isMp3      = file.hasFileExtension ("mp3|MP3");
    const int  allocFrames = numFrames + (isMp3 ? 1152 : 0);

    juce::AudioBuffer<float> sourceBuffer (numChannels, allocFrames);
    sourceBuffer.clear();
    reader->read (&sourceBuffer, 0, numFrames, 0, true, true);

    // Trim reported size back to numFrames — avoidReallocating keeps the larger
    // allocation so the pad stays zero-filled; downstream code sees numFrames only.
    sourceBuffer.setSize (numChannels, numFrames, /*keepExisting=*/true,
                          /*clearExtra=*/false, /*avoidReallocating=*/true);

    // ── Resample if needed ────────────────────────────────────────────────────
    if (std::abs (sourceSampleRate - projectSampleRate) > 0.01)
    {
        double ratio = sourceSampleRate / projectSampleRate;

        // Guard: a ratio outside [0.1, 10.0] means the reader returned garbage
        // metadata — reject rather than allocating a gigantic / zero-size buffer.
        if (ratio < 0.1 || ratio > 10.0)
            return nullptr;

        int resampledLen = (int) std::ceil ((double) numFrames / ratio);
        if (resampledLen <= 0 || resampledLen > kMaxSamples)
            return nullptr;

        juce::AudioBuffer<float> resampledBuffer (numChannels, resampledLen);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio,
                                  sourceBuffer.getReadPointer (ch),
                                  resampledBuffer.getWritePointer (ch),
                                  resampledLen);
        }

        sourceBuffer = std::move (resampledBuffer);
        numFrames    = resampledLen;
    }

    // ── Up-mix to stereo ──────────────────────────────────────────────────────
    juce::AudioBuffer<float> newBuffer (2, numFrames);

    if (numChannels >= 2)
    {
        newBuffer.copyFrom (0, 0, sourceBuffer, 0, 0, numFrames);
        newBuffer.copyFrom (1, 0, sourceBuffer, 1, 0, numFrames);
    }
    else
    {
        newBuffer.copyFrom (0, 0, sourceBuffer, 0, 0, numFrames);
        newBuffer.copyFrom (1, 0, sourceBuffer, 0, 0, numFrames);
    }

    auto decoded          = std::make_unique<DecodedSample>();
    decoded->buffer       = std::move (newBuffer);
    decoded->fileName     = file.getFileName();
    decoded->filePath     = file.getFullPathName();
    buildMipmapsForBuffer (decoded->buffer, decoded->peakMipmaps);
    return decoded;
}

void SampleData::applyDecodedSample (std::unique_ptr<DecodedSample> decoded)
{
    if (decoded == nullptr)
        return;

    buffer        = std::move (decoded->buffer);
    peakMipmaps   = std::move (decoded->peakMipmaps);
    loadedFileName = decoded->fileName;
    loadedFilePath = decoded->filePath;

    auto view          = std::make_shared<Snapshot>();
    view->buffer       = buffer;
    view->peakMipmaps  = peakMipmaps;
    view->fileName     = loadedFileName;
    view->filePath     = loadedFilePath;

#if INTERSECT_HAS_STD_ATOMIC_SHARED_PTR
    snapshot.store (std::static_pointer_cast<const Snapshot> (view),
                    std::memory_order_release);
#else
    std::atomic_store_explicit (&snapshot,
                                std::static_pointer_cast<const Snapshot> (view),
                                std::memory_order_release);
#endif
    loaded = true;
}

bool SampleData::loadFromFile (const juce::File& file, double projectSampleRate)
{
    auto decoded = decodeFromFile (file, projectSampleRate);
    if (decoded == nullptr)
        return false;
    applyDecodedSample (std::move (decoded));
    return true;
}

void SampleData::clear()
{
    buffer.setSize (2, 0, false, false, true);
    for (auto& m : peakMipmaps)
    {
        m.samplesPerPeak = 0;
        m.maxPeaks.clear();
        m.minPeaks.clear();
    }
#if INTERSECT_HAS_STD_ATOMIC_SHARED_PTR
    snapshot.store (std::shared_ptr<const Snapshot>{}, std::memory_order_release);
#else
    std::atomic_store_explicit (&snapshot, std::shared_ptr<const Snapshot>{},
                                std::memory_order_release);
#endif
    loadedFileName.clear();
    loadedFilePath.clear();
    loaded = false;
}

SampleData::SnapshotPtr SampleData::getSnapshot() const
{
#if INTERSECT_HAS_STD_ATOMIC_SHARED_PTR
    return snapshot.load (std::memory_order_acquire);
#else
    return std::atomic_load_explicit (&snapshot, std::memory_order_acquire);
#endif
}

float SampleData::getInterpolatedSample (double pos, int channel) const
{
    if (! loaded || channel < 0 || channel > 1)
        return 0.0f;

    int   ipos = (int) pos;
    float frac = (float) (pos - ipos);

    if (ipos < 0 || ipos >= buffer.getNumSamples() - 1)
        return 0.0f;

    auto* data = buffer.getReadPointer (channel);
    if (data == nullptr)
        return 0.0f;

    return data[ipos] + (data[ipos + 1] - data[ipos]) * frac;
}

std::unique_ptr<SampleData::DecodedSample> SampleData::createTrimmed (
    const DecodedSample& src, int trimIn, int trimOut)
{
    const int numFrames = src.buffer.getNumSamples();
    trimIn  = juce::jlimit (0, numFrames, trimIn);
    trimOut = juce::jlimit (trimIn + 1, numFrames, trimOut);
    const int trimLen = trimOut - trimIn;
    if (trimLen <= 0)
        return nullptr;

    auto result        = std::make_unique<DecodedSample>();
    result->fileName   = src.fileName;
    result->filePath   = src.filePath;

    const int numCh = src.buffer.getNumChannels();
    result->buffer.setSize (numCh, trimLen);
    for (int ch = 0; ch < numCh; ++ch)
        result->buffer.copyFrom (ch, 0, src.buffer, ch, trimIn, trimLen);

    buildMipmapsForBuffer (result->buffer, result->peakMipmaps);
    return result;
}
