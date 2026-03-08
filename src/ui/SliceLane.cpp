#include "SliceLane.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <array>

SliceLane::SliceLane (DysektProcessor& p) : processor (p) {}

void SliceLane::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().darkBar.brighter (0.04f));

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

    int previewIdx = -1, previewStart = 0, previewEnd = 0;
    const bool hasPreview = waveformView != nullptr
        && waveformView->getActiveSlicePreview (previewIdx, previewStart, previewEnd);

    // ── Collect visible slices ────────────────────────────────────────────────
    struct SliceInfo { int idx; int x1; int x2; bool selected; juce::Colour col; };
    std::array<SliceInfo, SliceManager::kMaxSlices> vis {};
    int visCount = 0;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t) i];
        if (! s.active) continue;
        int startSample = s.startSample;
        int endSample   = processor.sliceManager.getEndForSlice (i, numFrames);
        if (hasPreview && i == previewIdx) { startSample = previewStart; endSample = previewEnd; }

        int x1 = (int) ((float) (startSample - visStart) / visLen * w);
        int x2 = (int) ((float) (endSample   - visStart) / visLen * w);
        x1 = std::max (0, x1);
        x2 = std::min (w, x2);
        if (x2 - x1 < 2) continue;
        if (visCount < SliceManager::kMaxSlices)
            vis[(size_t) visCount++] = { i, x1, x2, (i == sel), s.colour };
    }

    // Opacity steps for non-selected slices (5-step cycle, same hue per slice)
    static constexpr float kOpacity[5] = { 0.11f, 0.19f, 0.27f, 0.19f, 0.11f };

    const int Y1 = 1, Y2 = h - 1;

    // ── Pass 1: fills ─────────────────────────────────────────────────────────
    for (int i = 0; i < visCount; ++i)
    {
        const auto& si = vis[(size_t) i];
        if (si.selected)
        {
            g.setColour (si.col.withAlpha (0.28f));
            g.fillRect (si.x1, Y1, si.x2 - si.x1, Y2 - Y1);
        }
        else
        {
            g.setColour (si.col.withAlpha (kOpacity[(size_t) si.idx % 5]));
            g.fillRect (si.x1, Y1, si.x2 - si.x1, Y2 - Y1);
        }
    }

    // ── Pass 2: dividers — one shared dark line per boundary ─────────────────
    // Left edge of every slice (including first)
    for (int i = 0; i < visCount; ++i)
    {
        g.setColour (getTheme().darkBar.darker (0.3f));
        g.fillRect (vis[(size_t) i].x1, Y1, 1, Y2 - Y1);
    }
    // Right edge of last visible slice
    if (visCount > 0)
        g.fillRect (vis[(size_t) (visCount - 1)].x2 - 1, Y1, 1, Y2 - Y1);

    // ── Pass 3: top accent bars + selection border ────────────────────────────
    for (int i = 0; i < visCount; ++i)
    {
        const auto& si = vis[(size_t) i];
        int sw = si.x2 - si.x1;
        if (sw < 3) continue;

        if (si.selected)
        {
            // Bright 2px top bar (inset 1px from dividers)
            g.setColour (si.col.withAlpha (0.95f));
            g.fillRect (si.x1 + 1, Y1, sw - 2, 2);

            // 4-sided border inset by 1px from each divider
            g.setColour (si.col.withAlpha (0.70f));
            // top
            g.drawHorizontalLine (Y1,     (float) (si.x1 + 1), (float) (si.x2 - 1));
            // bottom
            g.drawHorizontalLine (Y2 - 1, (float) (si.x1 + 1), (float) (si.x2 - 1));
            // left
            g.drawVerticalLine   (si.x1 + 1, (float) Y1, (float) Y2);
            // right
            g.drawVerticalLine   (si.x2 - 2, (float) Y1, (float) Y2);
        }
        else
        {
            // Hairline top tick at full colour brightness (inset 1px)
            g.setColour (si.col.withAlpha (0.50f));
            g.fillRect (si.x1 + 1, Y1, sw - 2, 1);
        }
    }

    // ── Pass 4: labels ────────────────────────────────────────────────────────
    // Recompute stable label positions (cache logic preserved)
    LabelCacheKey currentKey;
    currentKey.numSlices     = num;
    currentKey.selectedSlice = sel;
    currentKey.visStart      = visStart;
    currentKey.visLen        = visLen;
    currentKey.width         = w;

    if (labelCacheDirty || !(currentKey == prevLabelCacheKey))
    {
        cachedLabelCount = 0;
        std::array<int, SliceManager::kMaxSlices> labelEndsCache {};
        int labelEndCount = 0;

        g.setFont (DysektLookAndFeel::makeFont (12.0f, true));

        for (int i = 0; i < visCount; ++i)
        {
            const auto& si = vis[(size_t) i];
            int sw = si.x2 - si.x1;
            if (sw <= 14) continue;

            juce::String label = juce::String (si.idx + 1);
            int labelW = g.getCurrentFont().getStringWidth (label) + 6;
            int labelX = si.x1 + 4;  // +4 to clear the left border

            for (int li = 0; li < labelEndCount; ++li)
                if (labelX < labelEndsCache[(size_t) li])
                    labelX = labelEndsCache[(size_t) li] + 1;

            if (labelX + labelW < w && cachedLabelCount < SliceManager::kMaxSlices)
            {
                cachedLabels[(size_t) cachedLabelCount++] = { si.idx, labelX };
                if (labelEndCount < SliceManager::kMaxSlices)
                    labelEndsCache[(size_t) labelEndCount++] = labelX + labelW;
            }
        }

        prevLabelCacheKey = currentKey;
        labelCacheDirty   = false;
    }

    g.setFont (DysektLookAndFeel::makeFont (12.0f, true));
    for (int ci = 0; ci < cachedLabelCount; ++ci)
    {
        const auto& cl = cachedLabels[(size_t) ci];
        juce::String label = juce::String (cl.sliceIdx + 1);
        int labelW = g.getCurrentFont().getStringWidth (label) + 6;
        bool isSel = (cl.sliceIdx == sel);

        juce::Colour col { 0xFF1A8FD1 };
        for (int i = 0; i < visCount; ++i)
            if (vis[(size_t) i].idx == cl.sliceIdx) { col = vis[(size_t) i].col; break; }

        g.setColour (isSel ? col.withAlpha (1.0f) : col.withAlpha (0.70f));
        g.drawText (label, cl.x, 0, labelW, h, juce::Justification::centredLeft);
    }

    // ── Lock icons ────────────────────────────────────────────────────────────
    for (int i = 0; i < visCount; ++i)
    {
        const auto& si = vis[(size_t) i];
        const auto& sl = ui.slices[(size_t) si.idx];
        if (sl.lockMask != 0xFFFFFFFFu) continue;

        const int iconSz = 5;
        const int iconX  = si.x2 - iconSz - 3;
        const int iconY  = 2;
        if (iconX < si.x1 + 4) continue;

        g.setColour (getTheme().lockActive.withAlpha (0.9f));
        g.fillRect (iconX, iconY, iconSz, iconSz);
        g.setColour (getTheme().lockActive);
        g.drawRect (iconX, iconY, iconSz, iconSz, 1);
    }

    // ── Bottom separator ──────────────────────────────────────────────────────
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

            juce::PopupMenu menu;
            menu.addItem (1, "Delete Slice");
            menu.addSeparator();
            menu.addItem (2, lockLabel, true, allLocked);

            auto* topLvl = getTopLevelComponent();
            float ms = DysektLookAndFeel::getMenuScale();
            menu.showMenuAsync (
                juce::PopupMenu::Options()
                    .withTargetComponent (this)
                    .withParentComponent (topLvl)
                    .withStandardItemHeight ((int) (24 * ms)),
                [this, targetSlice, allLocked] (int result)
                {
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
