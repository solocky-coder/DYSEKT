#include "SliceLane.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "../audio/Slice.h"
#include <algorithm>
#include <array>

SliceLane::SliceLane (DysektProcessor& p) : processor (p) {}

void SliceLane::paint (juce::Graphics& g)
{
    // Fill background for entire lane (no reserved top band, no color bar)
    g.fillAll (getTheme().darkBar.brighter (0.04f));
    // Do NOT draw any box or color where the old bar was!

    auto sampleSnap = processor.sampleData.getSnapshot();
    int numFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 0;
    if (numFrames <= 0)
        return;

    const auto& ui = processor.getUiSliceSnapshot();
    const float z  = std::max (1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visLen   = juce::jlimit (1, numFrames, (int) (numFrames / z));
    const int maxStart = juce::jmax (0, numFrames - visLen);
    const int visStart = juce::jlimit (0, maxStart, (int) (sc * (float) maxStart));

    int sel = ui.selectedSlice;
    int num = ui.numSlices;
    int w   = getWidth();
    int h   = getHeight();

    // No color bar, so bodyH = full height minus dot row
    static constexpr int kDotZoneH = 6;
    const int bodyH = h - kDotZoneH;
    const int dotZoneY = bodyH;

    // ---- Pass-through for overlays etc. (unchanged logic) ----

    // ---- ADSR Dots (lock row, unchanged) ----
    static const juce::Colour kDotA { 0xFF00FF87 };
    static const juce::Colour kDotD { 0xFFFFE800 };
    static const juce::Colour kDotS { 0xFF00C8FF };
    static const juce::Colour kDotR { 0xFFFF6B00 };
    static const uint32_t     kAdsrBits[4] = { kLockAttack, kLockDecay,
                                                kLockSustain, kLockRelease };
    static const juce::Colour kAdsrCols[4] = { kDotA, kDotD, kDotS, kDotR };

    g.setColour (getTheme().darkBar.darker (0.20f));
    g.fillRect (0, dotZoneY, w, kDotZoneH);
    g.setColour (getTheme().separator.withAlpha (0.60f));
    g.drawHorizontalLine (dotZoneY, 0.0f, (float) w);

    static constexpr int kDotSz  = 2;
    static constexpr int kDotGap = 5;
    const int dotRowY = dotZoneY + kDotZoneH / 2 - kDotSz / 2;

    for (int i = 0; i < num; ++i)
    {
        const auto& sl = ui.slices[(size_t) i];
        if (! sl.active) continue;
        int startSample = sl.startSample;
        int endSample   = processor.sliceManager.getEndForSlice (i, numFrames);
        int x1 = (int) ((float) (startSample - visStart) / visLen * w);
        int x2 = (int) ((float) (endSample   - visStart) / visLen * w);
        int sw = x2 - x1;
        if (sw < 2) continue;

        const uint32_t anyMask = kLockAttack | kLockDecay | kLockSustain | kLockRelease;
        const bool anyLocked = (sl.lockMask & anyMask) != 0;
        if (! anyLocked) continue;

        const bool allLocked = (sl.lockMask == 0xFFFFFFFFu);
        const float alpha    = (i == sel) ? 0.95f : 0.70f;

        if (allLocked || sw < 26)
        {
            if (sw < 7) continue;
            const int px = x1 + sw / 2;
            const int py = dotZoneY + 1;
            g.setColour (getTheme().lockActive.withAlpha (alpha));
            g.fillRect (px - 1, py,     2, 2);
            g.fillRect (px - 2, py + 2, 4, 3);
        }
        else
        {
            const int totalW = kDotGap * 3 + kDotSz;
            if (totalW > sw - 2) continue;
            const int startX = x1 + (sw - totalW) / 2;
            for (int d = 0; d < 4; ++d)
            {
                const bool locked = (sl.lockMask & kAdsrBits[d]) != 0;
                const int  dx     = startX + d * kDotGap;
                const juce::Colour dc = kAdsrCols[d];

                if (locked)
                {
                    g.setColour (dc.withAlpha (alpha));
                    g.fillRect (dx, dotRowY, kDotSz, kDotSz);
                }
                else
                {
                    g.setColour (dc.withAlpha (0.15f));
                    g.drawRect (dx, dotRowY, kDotSz, kDotSz, 1);
                }
            }
        }
    }

    g.setColour (getTheme().separator);
    g.drawHorizontalLine (h - 1, 0.0f, (float) w);
}

// All input/interaction logic remains unchanged and in your code.