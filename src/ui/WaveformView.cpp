#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

// === Implementation for drawPlaybackCursors ===
void WaveformView::drawPlaybackCursors(juce::Graphics&) {}

// === Implementations for trim helpers ===
void WaveformView::exitTrimMode()
{
    trimMode = false;
    dragMode = None;
    trimDragging = false;
    repaint();
}

void WaveformView::getTrimBounds(int& outStart, int& outEnd) const
{
    outStart = trimInPoint;
    outEnd   = trimOutPoint;
}

void WaveformView::resetTrim()
{
    trimInPoint = trimStart;
    trimOutPoint = trimEnd;
    repaint();
}

WaveformView::WaveformView (DysektProcessor& p) : processor (p) {}

// Right-click context menu for slices integrated (no SliceLane dependency)
void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        auto sampleSnap = processor.sampleData.getSnapshot();
        if (!sampleSnap) return;
        const auto& ui = processor.getUiSliceSnapshot();
        int num = ui.numSlices;
        int samplePos = juce::jlimit(0, sampleSnap->buffer.getNumSamples(), pixelToSample(e.x));
        int foundIdx = -1;
        for (int i = 0; i < num; ++i)
        {
            const auto& s = ui.slices[(size_t)i];
            if (!s.active) continue;
            int start = s.startSample;
            int end = processor.sliceManager.getEndForSlice(i, ui.sampleNumFrames);
            if (samplePos >= start && samplePos < end)
            {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx >= 0)
        {
            const auto& s = ui.slices[(size_t) foundIdx];
            const bool allLocked = (s.lockMask == 0xFFFFFFFFu);
            const juce::String lockLabel = allLocked ? "Unlock Slice" : "Lock Slice";
            const bool lockA = (s.lockMask & kLockAttack)  != 0;
            const bool lockD = (s.lockMask & kLockDecay)   != 0;
            const bool lockS = (s.lockMask & kLockSustain) != 0;
            const bool lockR = (s.lockMask & kLockRelease) != 0;
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
            juce::Colour curCol = s.colour;
            for (int ci = 0; ci < 16; ++ci)
            {
                juce::Colour c ((juce::uint32) kPal[ci].argb);
                colourSub.addColouredItem(20 + ci, kPal[ci].name, c,
                                          true, c.toDisplayString(false) == curCol.toDisplayString(false));
            }
            juce::PopupMenu adsrSub;
            adsrSub.addItem(10, "Lock Attack",  true, lockA);
            adsrSub.addItem(11, "Lock Decay",   true, lockD);
            adsrSub.addItem(12, "Lock Sustain", true, lockS);
            adsrSub.addItem(13, "Lock Release", true, lockR);
            juce::PopupMenu menu;
            menu.addItem(1, "Delete Slice");
            menu.addSeparator();
            menu.addSubMenu("Slice Colour", colourSub);
            menu.addSeparator();
            menu.addItem(2, lockLabel, true, allLocked);
            menu.addSubMenu("ADSR Lock", adsrSub);

            auto* topLvl = getTopLevelComponent();
            float ms = DysektLookAndFeel::getMenuScale();
            const auto screenPt = e.getScreenPosition();
            menu.showMenuAsync(
                juce::PopupMenu::Options()
                    .withTargetScreenArea({ screenPt, screenPt })
                    .withParentComponent(topLvl)
                    .withStandardItemHeight((int) (24 * ms)),
                [this, foundIdx, allLocked](int result)
                {
                    auto toggleLock = [&] (uint32_t bit)
                    {
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdToggleLock;
                        cmd.intParam1 = (int) bit;
                        processor.pushCommand(cmd);
                    };
                    if (result == 1)
                    {
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdDeleteSlice;
                        cmd.intParam1 = foundIdx;
                        processor.pushCommand(cmd);
                    }
                    else if (result == 2)
                    {
                        DysektProcessor::Command cmd;
                        cmd.type        = DysektProcessor::CmdSetSliceLockAll;
                        cmd.intParam1   = foundIdx;
                        cmd.floatParam1 = allLocked ? 0.f : 1.f;
                        processor.pushCommand(cmd);
                    }
                    else if (result == 10) toggleLock(kLockAttack);
                    else if (result == 11) toggleLock(kLockDecay);
                    else if (result == 12) toggleLock(kLockSustain);
                    else if (result == 13) toggleLock(kLockRelease);
                    else if (result >= 20 && result < 36)
                    {
                        static const juce::uint32 kPalARGB[] = {
                            0xFF00C8FF, 0xFF00FF87, 0xFFFFE800, 0xFFFF6B00,
                            0xFFFF2D55, 0xFFFF2D9A, 0xFFB44FFF, 0xFF4A80FF,
                            0xFF00BFFF, 0xFF00FFD0, 0xFFA8FF3E, 0xFFFFD700,
                            0xFFFF7F50, 0xFFFF00FF, 0xFFE8E8E8, 0xFF888888,
                        };
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdSetSliceColour;
                        cmd.intParam1 = foundIdx;
                        cmd.intParam2 = (int) kPalARGB[result - 20];
                        processor.pushCommand(cmd);
                    }
                    repaint();
                });
            return; // consume right-click (don't fall through)
        }
    }

    // ... rest of your mouseDown unchanged ...
    syncAltStateFromMods (e.mods);
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr) return;
    int samplePos = std::max (0, std::min (pixelToSample (e.x), sampleSnap->buffer.getNumSamples()));
    if (trimMode)
    {
        int w = getWidth();
        auto totalFrames = sampleSnap->buffer.getNumSamples();
        int x1 = juce::jlimit(0, w, sampleToPixel(juce::jlimit(0, totalFrames - 1, trimInPoint)));
        int x2 = juce::jlimit(0, w, sampleToPixel(juce::jlimit(trimInPoint + 1, totalFrames, trimOutPoint)));
        if (std::abs (e.x - x1) < 8)
        {
            dragMode = DragTrimIn;
            trimDragging = true;
            return;
        }
        else if (std::abs (e.x - x2) < 8)
        {
            dragMode = DragTrimOut;
            trimDragging = true;
            return;
        }
        return;
    }
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;
    if (!(sliceDrawMode || altModeActive) && sel >= 0 && sel < num)
    {
        const auto& s = ui.slices[(size_t) sel];
        if (s.active)
        {
            const int selEnd = processor.sliceManager.getEndForSlice (sel, ui.sampleNumFrames);
            int x1 = sampleToPixel (s.startSample);

            if (std::abs (e.x - x1) < 6)
            {
                DysektProcessor::Command gestureCmd;
                gestureCmd.type = DysektProcessor::CmdBeginGesture;
                processor.pushCommand (gestureCmd);
                dragMode = DragEdgeLeft;
                dragSliceIdx = sel;
                dragPreviewStart = s.startSample;
                dragPreviewEnd = selEnd;
                dragOrigStart = s.startSample;
                dragOrigEnd   = selEnd;
                linkedSliceIdx = -1;
                for (int i = 0; i < num; ++i)
                {
                    if (i == sel || ! ui.slices[(size_t) i].active) continue;
                    if (processor.sliceManager.getEndForSlice (i, ui.sampleNumFrames) == s.startSample)
                    {
                        linkedSliceIdx = i;
                        linkedPreviewStart = ui.slices[(size_t) i].startSample;
                        linkedPreviewEnd   = s.startSample;
                        break;
                    }
                }
                return;
            }
        }
    }
    if (sliceDrawMode || altModeActive)
    {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdCreateSlice;
        cmd.intParam1 = samplePos;
        cmd.intParam2 = samplePos + 1;
        processor.pushCommand(cmd);
        return;
    }
    for (int i = 0; i < num; ++i)
    {
        const auto& sl = ui.slices[(size_t) i];
        if (sl.active && samplePos >= sl.startSample
            && samplePos < processor.sliceManager.getEndForSlice (i, ui.sampleNumFrames))
        {
            DysektProcessor::Command cmd;
            cmd.type      = DysektProcessor::CmdSelectSlice;
            cmd.intParam1 = i;
            processor.pushCommand (cmd);
            break;
        }
    }
}

... // All other methods unchanged

void WaveformView::paint(juce::Graphics& g)
{
    // ---- Existing waveform/slice overlays ----
    // (keep your waveform painting logic as it already is)

    // ---- ADSR dot row at the bottom ----
    const auto& ui = processor.getUiSliceSnapshot();
    int num = ui.numSlices;
    int w = getWidth();
    int h = getHeight();

    static constexpr int kDotZoneH = 6;
    const int dotZoneY = h - kDotZoneH;

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
        int endSample   = processor.sliceManager.getEndForSlice (i, ui.sampleNumFrames);
        int x1 = (int) ((float) (startSample - ui.visibleStart) / ui.visibleLen * w);
        int x2 = (int) ((float) (endSample   - ui.visibleStart) / ui.visibleLen * w);
        int sw = x2 - x1;
        if (sw < 2) continue;

        const uint32_t anyMask = kLockAttack | kLockDecay | kLockSustain | kLockRelease;
        const bool anyLocked = (sl.lockMask & anyMask) != 0;
        if (! anyLocked) continue;

        const bool allLocked = (sl.lockMask == 0xFFFFFFFFu);
        const float alpha    = (i == ui.selectedSlice) ? 0.95f : 0.70f;

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

// ... All other functions from your previous WaveformView.cpp, unchanged ...