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
    const float z = std::max (1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visLen = juce::jlimit (1, numFrames, (int) (numFrames / z));
    const int maxStart = juce::jmax (0, numFrames - visLen);
    const int visStart = juce::jlimit (0, maxStart, (int) (sc * (float) maxStart));

    int sel = ui.selectedSlice;
    int num = ui.numSlices;
    int w = getWidth();
    int h = getHeight();
    int previewIdx = -1;
    int previewStart = 0;
    int previewEnd = 0;
    const bool hasPreview = waveformView != nullptr
        && waveformView->getActiveSlicePreview (previewIdx, previewStart, previewEnd);

    // Collect visible slice info without per-frame heap allocations.
    struct SliceInfo { int idx; int x1; int x2; bool selected; juce::Colour col; };
    std::array<SliceInfo, SliceManager::kMaxSlices> visibleSlices {};
    int visibleCount = 0;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t) i];
        if (! s.active) continue;
        int startSample = s.startSample;
        int endSample = s.endSample;
        if (hasPreview && i == previewIdx)
        {
            startSample = previewStart;
            endSample = previewEnd;
        }

        int x1 = (int) ((float) (startSample - visStart) / visLen * w);
        int x2 = (int) ((float) (endSample - visStart) / visLen * w);
        x1 = std::max (0, x1);
        x2 = std::min (w, x2);
        if (x2 - x1 < 2) continue;

        if (visibleCount < SliceManager::kMaxSlices)
            visibleSlices[(size_t) visibleCount++] = { i, x1, x2, (i == sel), s.colour };
    }

    // Pass 1: Draw non-selected first, selected last (z-order) without sorting.
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool drawSelected = (pass == 1);
        for (int i = 0; i < visibleCount; ++i)
        {
            const auto& si = visibleSlices[(size_t) i];
            if (si.selected != drawSelected)
                continue;

            int sw = si.x2 - si.x1;
            g.setColour (si.selected ? si.col.withAlpha (0.55f) : si.col.withAlpha (0.25f));
            g.fillRect (si.x1, 1, sw, h - 2);

            if (si.selected)
            {
                g.setColour (si.col.withAlpha (0.9f));
                g.drawRect (si.x1, 1, sw, h - 2, 1);
            }
        }
    }

    // Build left-to-right label order by x position using insertion sort on indices.
    std::array<int, SliceManager::kMaxSlices> labelOrder {};
    int labelOrderCount = 0;
    for (int i = 0; i < visibleCount; ++i)
    {
        int pos = labelOrderCount;
        while (pos > 0 && visibleSlices[(size_t) labelOrder[(size_t) (pos - 1)]].x1 > visibleSlices[(size_t) i].x1)
        {
            labelOrder[(size_t) pos] = labelOrder[(size_t) (pos - 1)];
            --pos;
        }
        labelOrder[(size_t) pos] = i;
        ++labelOrderCount;
    }

    // ── Label position caching (Bug #3: prevents per-frame jitter) ──────────
    // Build a cache key from the inputs that affect label layout. Only
    // recompute label positions when something actually changed.
    LabelCacheKey currentKey;
    currentKey.numSlices     = num;
    currentKey.selectedSlice = sel;
    currentKey.visStart      = visStart;
    currentKey.visLen        = visLen;
    currentKey.width         = w;

    if (labelCacheDirty || !(currentKey == prevLabelCacheKey))
    {
        // Recompute stable label x-positions from scratch.
        cachedLabelCount = 0;
        std::array<int, SliceManager::kMaxSlices> labelEndsCache {};
        int labelEndCount = 0;

        g.setFont (DysektLookAndFeel::makeFont (12.0f, true));

        for (int oi = 0; oi < labelOrderCount; ++oi)
        {
            const auto& si = visibleSlices[(size_t) labelOrder[(size_t) oi]];
            int sw = si.x2 - si.x1;
            if (sw <= 14) continue;

            juce::String label = juce::String (si.idx + 1);
            int labelW = g.getCurrentFont().getStringWidth (label) + 6;
            int labelX = si.x1 + 3;

            for (int li = 0; li < labelEndCount; ++li)
            {
                if (labelX < labelEndsCache[(size_t) li])
                    labelX = labelEndsCache[(size_t) li] + 1;
            }

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

    // Draw labels using the stable cached positions.
    g.setFont (DysektLookAndFeel::makeFont (12.0f, true));
    for (int ci = 0; ci < cachedLabelCount; ++ci)
    {
        const auto& cl = cachedLabels[(size_t) ci];
        juce::String label = juce::String (cl.sliceIdx + 1);
        int labelW = g.getCurrentFont().getStringWidth (label) + 6;
        bool isSelected = (cl.sliceIdx == sel);

        // Find the slice colour for this label
        juce::Colour col { 0xFF4D8C99 };
        for (int i = 0; i < visibleCount; ++i)
        {
            if (visibleSlices[(size_t) i].idx == cl.sliceIdx)
            {
                col = visibleSlices[(size_t) i].col;
                break;
            }
        }

        g.setColour (isSelected ? getTheme().foreground.withAlpha (0.9f)
                                : col.withAlpha (0.7f));
        g.drawText (label, cl.x, 0, labelW, h, juce::Justification::centredLeft);
    }

    // ── Lock icons: draw a small solid rect in the top-right of fully-locked slices ──
    for (int i = 0; i < visibleCount; ++i)
    {
        const auto& si = visibleSlices[(size_t) i];
        const auto& sl = ui.slices[(size_t) si.idx];
        if (sl.lockMask != 0xFFFFFFFFu) continue;  // not fully locked

        const int iconSz = 5;
        const int iconX  = si.x2 - iconSz - 2;
        const int iconY  = 2;
        if (iconX < si.x1 + 4) continue;  // slice too narrow to show icon

        g.setColour (getTheme().lockActive.withAlpha (0.9f));
        g.fillRect (iconX, iconY, iconSz, iconSz);
        // Small shackle hint (top bar)
        g.setColour (getTheme().lockActive);
        g.drawRect (iconX, iconY, iconSz, iconSz, 1);
    }

    // Bottom separator line
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
        int x2 = (int) ((float) (s.endSample - visStart) / visLen * w);

        if (e.x >= x1 && e.x < x2)
            overlapping.push_back (i);
    }

    // ── Right-click: show context menu for the selected (or topmost) slice ──
    if (e.mods.isRightButtonDown())
    {
        int targetSlice = ui.selectedSlice;
        if (! overlapping.empty())
        {
            // Use topmost overlapping slice (same logic as left-click selection)
            auto it = std::find (overlapping.begin(), overlapping.end(), ui.selectedSlice);
            targetSlice = (it != overlapping.end()) ? *it : overlapping.front();
        }

        if (targetSlice >= 0 && targetSlice < ui.numSlices)
        {
            const auto& s = ui.slices[(size_t) targetSlice];
            // A slice is "fully locked" when every parameter lock bit is set
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
                        // Delete the slice
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdDeleteSlice;  // add this to your Cmd enum if missing
                        cmd.intParam1 = targetSlice;
                        processor.pushCommand (cmd);
                    }
                    else if (result == 2)
                    {
                        // Toggle lock: all bits on = full lock, 0 = unlock
                        DysektProcessor::Command cmd;
                        cmd.type      = DysektProcessor::CmdSetSliceLockAll;  // add to Cmd enum if missing
                        cmd.intParam1 = targetSlice;
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
        int target = current;

        // If current selection is in the list, cycle to the next one
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
        cmd.type = DysektProcessor::CmdSelectSlice;
        cmd.intParam1 = target;
        processor.pushCommand (cmd);
    }
}