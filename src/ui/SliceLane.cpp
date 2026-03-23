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

void SliceLane::mouseDown (const juce::MouseEvent& e)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    int numFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 0;
    if (numFrames <= 0)
        return;

    const auto& ui = processor.getUiSliceSnapshot();
    const float z = std::max (1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visLen = juce::jlimit (1, numFrames, (int) (numFrames / z));
    const int maxStart = juce::jmax (0, numFrames - visLen);
    const int visStart = juce::jlimit (0, maxStart, (int) (sc * (float) maxStart));
    int w = getWidth();
    int num = ui.numSlices;

    // Collect all overlapping slice indices at click position
    std::vector<int> overlapping;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t) i];
        if (! s.active) continue;

        int x1 = (int) ((float) (s.startSample - visStart) / visLen * w);
        const int slaneEnd = processor.sliceManager.getEndForSlice (i, numFrames);
        int x2 = (int) ((float) (slaneEnd - visStart) / visLen * w);

        if (e.x >= x1 && e.x < x2)
            overlapping.push_back (i);
    }

    // ── Right-click: show context menu for the selected (or topmost) slice ──
    if (e.mods.isRightButtonDown())
    {
        int targetSlice = ui.selectedSlice;
        if (! overlapping.empty())
        {
            auto it = std::find (overlapping.begin(), overlapping.end(), ui.selectedSlice);
            targetSlice = (it != overlapping.end()) ? *it : overlapping.front();
        }

        if (targetSlice >= 0 && targetSlice < ui.numSlices)
        {
            const auto& s = ui.slices[(size_t) targetSlice];
            const bool allLocked = (s.lockMask == 0xFFFFFFFFu);
            const juce::String lockLabel = allLocked ? "Unlock Slice" : "Lock Slice";

            const bool lockA = (s.lockMask & kLockAttack)  != 0;
            const bool lockD = (s.lockMask & kLockDecay)   != 0;
            const bool lockS = (s.lockMask & kLockSustain) != 0;
            const bool lockR = (s.lockMask & kLockRelease) != 0;

            // 16-colour palette
            static const struct { const char* name; juce::uint32 argb; } kPal[] = {
                { "Cyan",    0xFF00C8FF }, { "Green",   0xFF00FF87 },
                { "Yellow",  0xFFFFE800 }, { "Orange",  0xFFFF6B00 },
                { "Red",     0xFFFF2D55 }, { "Pink",    0xFFFF2D9A },
                { "Violet",  0xFFB44FFF }, { "Blue",    0xFF4A80FF },
                { "Sky",     0xFF00BFFF }, { "Mint",    0xFF00FFD0 },
                { "Lime",    0xFFA8FF3E }, { "Gold",    0xFFFFD700 },
                { "Coral",   0xFFFF7F50 }, { "Magenta", 0xFFFF00FF },
                { "White",   0xFFE8E8E8 }, { "Silver",  0xFF888888 },
            };

            juce::PopupMenu colourSub;
            juce::Colour curCol = ui.slices[(size_t) targetSlice].colour;
            for (int ci = 0; ci < 16; ++ci)
            {
                juce::Colour c ((juce::uint32) kPal[ci].argb);
                colourSub.addColouredItem (20 + ci, kPal[ci].name, c,
                                           true, c.toDisplayString (false) == curCol.toDisplayString (false));
            }

            juce::PopupMenu adsrSub;
            adsrSub.addItem (10, "Lock Attack",  true, lockA);
            adsrSub.addItem (11, "Lock Decay",   true, lockD);
            adsrSub.addItem (12, "Lock Sustain", true, lockS);
            adsrSub.addItem (13, "Lock Release", true, lockR);

            juce::PopupMenu menu;
            menu.addItem (1, "Delete Slice");
            menu.addSeparator();
            menu.addSubMenu ("Slice Colour", colourSub);
            menu.addSeparator();
            menu.addItem (2, lockLabel, true, allLocked);
            menu.addSubMenu ("ADSR Lock", adsrSub);

            auto* topLvl = getTopLevelComponent();
            float ms = DysektLookAndFeel::getMenuScale();
            const auto screenPt = e.getScreenPosition();
            menu.showMenuAsync (
                juce::PopupMenu::Options()
                    .withTargetScreenArea ({ screenPt, screenPt })
                    .withParentComponent (topLvl)
                    .withStandardItemHeight ((int) (24 * ms)),
                [this, targetSlice, allLocked] (int result)
                {
                    auto toggleLock = [&] (uint32_t bit)
                    {
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdToggleLock;
                        cmd.intParam1 = (int) bit;
                        processor.pushCommand (cmd);
                    };

                    if (result == 1)
                    {
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdDeleteSlice;
                        cmd.intParam1 = targetSlice;
                        processor.pushCommand (cmd);
                    }
                    else if (result == 2)
                    {
                        DysektProcessor::Command cmd;
                        cmd.type        = DysektProcessor::CmdSetSliceLockAll;
                        cmd.intParam1   = targetSlice;
                        cmd.floatParam1 = allLocked ? 0.f : 1.f;
                        processor.pushCommand (cmd);
                    }
                    else if (result == 10) toggleLock (kLockAttack);
                    else if (result == 11) toggleLock (kLockDecay);
                    else if (result == 12) toggleLock (kLockSustain);
                    else if (result == 13) toggleLock (kLockRelease);
                    else if (result >= 20 && result < 36)
                    {
                        // Set slice colour
                        static const juce::uint32 kPalARGB[] = {
                            0xFF00C8FF, 0xFF00FF87, 0xFFFFE800, 0xFFFF6B00,
                            0xFFFF2D55, 0xFFFF2D9A, 0xFFB44FFF, 0xFF4A80FF,
                            0xFF00BFFF, 0xFF00FFD0, 0xFFA8FF3E, 0xFFFFD700,
                            0xFFFF7F50, 0xFFFF00FF, 0xFFE8E8E8, 0xFF888888,
                        };
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdSetSliceColour;
                        cmd.intParam1 = targetSlice;
                        cmd.intParam2 = (int) kPalARGB[result - 20];
                        processor.pushCommand (cmd);
                    }
                    repaint();
                });
        }
        return;
    }

    // ── Left-click: cycle selection through overlapping slices ──────────────
    if (! overlapping.empty())
    {
        int current = ui.selectedSlice;
        int target  = current;

        auto it = std::find (overlapping.begin(), overlapping.end(), current);
        if (it != overlapping.end())
        {
            ++it;
            if (it == overlapping.end())
                it = overlapping.begin();
            target = *it;
        }
        else
        {
            target = overlapping.front();
        }

        DysektProcessor::Command cmd;
        cmd.type      = DysektProcessor::CmdSelectSlice;
        cmd.intParam1 = target;
        processor.pushCommand (cmd);
    }
}