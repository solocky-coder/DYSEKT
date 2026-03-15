#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class WaveformCache
{
public:
    struct CacheKey {
        int visibleStart = 0;
        int visibleLen = 0;
        int width = 0;
        int numFrames = 0;
        const void* sampleSnapPtr = nullptr;

        bool operator==(const CacheKey& o) const noexcept {
            return visibleStart == o.visibleStart
                && visibleLen == o.visibleLen
                && width == o.width
                && numFrames == o.numFrames
                && sampleSnapPtr == o.sampleSnapPtr;
        }
        bool operator!=(const CacheKey& o) const noexcept { return !(*this == o); }
    };

    WaveformCache() = default;
    ~WaveformCache() = default;

    void clear();
    void update(const juce::AudioBuffer<float>* buf, const CacheKey& key);
    // ... Other waveform cache logic and member functions ...

private:
    juce::Image cachedImage;
    CacheKey cachedKey;
    // ... Any additional members ...
};
