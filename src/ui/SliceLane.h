#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/SliceManager.h"

class DysektProcessor;
class WaveformView;

class SliceLane : public juce::Component
{
public:
    explicit SliceLane (DysektProcessor& p);
    void setWaveformView (WaveformView* view) { waveformView = view; }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    void invalidateLabelCache() noexcept { labelCacheDirty = true; }

private:
    DysektProcessor& processor;
    WaveformView* waveformView = nullptr;

    struct CachedLabel
    {
        int   sliceIdx = 0;
        int   x        = 0;
    };

    std::array<CachedLabel, SliceManager::kMaxSlices> cachedLabels {};
    int cachedLabelCount = 0;

    struct LabelCacheKey
    {
        int  numSlices     = 0;
        int  selectedSlice = -1;
        int  visStart      = 0;
        int  visLen        = 0;
        int  width         = 0;
        bool operator== (const LabelCacheKey& o) const
        {
            return numSlices == o.numSlices && selectedSlice == o.selectedSlice
                && visStart == o.visStart && visLen == o.visLen && width == o.width;
        }
    };

    LabelCacheKey prevLabelCacheKey {};
    bool          labelCacheDirty   = true;
};