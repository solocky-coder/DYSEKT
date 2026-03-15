#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <memory>
#include "SampleData.h"

class WaveformCache
{
public:
    struct CacheKey {
        int visibleStart = 0, visibleLen = 0, width = 0, numFrames = 0;
        const void* sampleSnapPtr = nullptr;
        bool operator==(const CacheKey& o) const noexcept { return visibleStart == o.visibleStart && visibleLen == o.visibleLen && width == o.width && numFrames == o.numFrames && sampleSnapPtr == o.sampleSnapPtr; }
        bool operator!=(const CacheKey& o) const noexcept { return !(*this == o); }
    };

    WaveformCache() = default;
    ~WaveformCache() = default;

    void clear();
    void rebuild(const juce::AudioBuffer<float>& buffer, const PeakArray& peaks, int numFrames, float zoom, float scroll, int width);
    const std::vector<WaveformPeak>& getPeaks() const { return cachedPeaks; }
    int getNumPeaks() const { return (int) cachedPeaks.size(); }

private:
    std::vector<WaveformPeak> cachedPeaks;
    CacheKey cachedKey;
    juce::Image cachedImage; // Optionally store a cached image if wanted
};