#include "WaveformCache.h"

void WaveformCache::clear()
{
    cachedPeaks.clear();
    cachedImage = {};
    cachedKey = {};
}

void WaveformCache::rebuild(const juce::AudioBuffer<float>& buffer, const PeakArray& peaks, int numFrames, float /*zoom*/, float /*scroll*/, int width)
{
    cachedPeaks.clear();
    if (width <= 0 || numFrames <= 0)
        return;
    // Use provided peaks if present and matches width, otherwise re-compute
    if ((int)peaks.size() >= width) {
        // Copy the relevant visible portion
        cachedPeaks.assign(peaks.begin(), peaks.begin() + width);
        return;
    }
    // No peaks: fallback makes flat line (could calculate actual min/max for audio)
    cachedPeaks.resize(width, WaveformPeak{ 0.0f, 0.0f });
}