#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

// ... all declarations unchanged

WaveformView::WaveformView (DysektProcessor& p) : processor (p) {}

// ... all unchanged methods before mouseDown

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
        return;

    // Middle-mouse drag: scroll+zoom (like ScrollZoomBar)
    if (e.mods.isMiddleButtonDown())
    {
        midDragging = true;
        midDragStartZoom = processor.zoom.load();
        midDragStartX = e.x;
        midDragStartY = e.y;

        int w = getWidth();
        float z = midDragStartZoom;
        float sc = processor.scroll.load();
        float viewFrac = 1.0f / z;
        float viewStart = sc * (1.0f - viewFrac);

        midDragAnchorPixelFrac = (w > 0) ? (float) e.x / (float) w : 0.5f;
        midDragAnchorFrac = juce::jlimit (0.0f, 1.0f, viewStart + midDragAnchorPixelFrac * viewFrac);
        return;
    }

    int samplePos = std::max (0, std::min (pixelToSample (e.x), sampleSnap->buffer.getNumSamples()));

    // ── Trim mode: only allow dragging the trim markers ───────────────────────
    if (trimMode)
    {
        const int x1 = sampleToPixel (trimInPoint);
        const int x2 = sampleToPixel (trimOutPoint);
        if (std::abs (e.x - x1) < 8)
        {
            dragMode = DragTrimIn;
            trimDragging = true;
        }
        else if (std::abs (e.x - x2) < 8)
        {
            dragMode = DragTrimOut;
            trimDragging = true;
        }
        // Click inside trim region does nothing (no slice selection)
        return;
    }

    // Shift+click: preview audio from pointer position
    if (e.mods.isShiftDown() && ! sliceDrawMode && ! altModeActive
        && ! processor.lazyChop.isActive())
    {
        shiftPreviewActive = true;
        processor.shiftPreviewRequest.store (samplePos, std::memory_order_relaxed);
        return;
    }

    // ----------- CUSTOM BEHAVIOR FOR "ADD SLICE" -------------
    // If in Add Slice mode, drop marker on click, allow multiple slices (NO DRAG required)
    if (sliceDrawMode)
    {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdCreateSlice;
        cmd.intParam1 = samplePos;
        cmd.intParam2 = samplePos;
        processor.pushCommand(cmd);

        // Stay in draw mode for multiple slice drops by click
        repaint();
        return;
    }

    if (altModeActive)
    {
        drawStart = samplePos;
        drawEnd = samplePos;
        drawStartedFromAlt = (! sliceDrawMode && e.mods.isAltDown());
        dragMode = DrawSlice;
        return;
    }

    // Check slice edges (6px hot zone) — only for already-selected slice
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;

    if (sel >= 0 && sel < num)
    {
        const auto& s = ui.slices[(size_t) sel];
        if (s.active)
        {
            const int selEnd = processor.sliceManager.getEndForSlice (sel, ui.sampleNumFrames);
            int x1 = sampleToPixel (s.startSample);
            int x2 = sampleToPixel (selEnd);

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

                // Find the slice whose end is flush with our start (link it)
                linkedSliceIdx = -1;
                for (int i = 0; i < ui.numSlices; ++i)
                {
                    if (i == sel || ! ui.slices[(size_t) i].active) continue;
                    if (processor.sliceManager.getEndForSlice (i, ui.sampleNumFrames) == s.startSample)
                    {
                        linkedSliceIdx    = i;
                        linkedPreviewStart = ui.slices[(size_t) i].startSample;
                        linkedPreviewEnd   = s.startSample;
                        break;
                    }
                }
                return;
            }
            if (std::abs (e.x - x2) < 6)
            {
                DysektProcessor::Command gestureCmd;
                gestureCmd.type = DysektProcessor::CmdBeginGesture;
                processor.pushCommand (gestureCmd);
                dragMode = DragEdgeRight;
                dragSliceIdx = sel;
                dragPreviewStart = s.startSample;
                dragPreviewEnd = selEnd;
                dragOrigStart = s.startSample;
                dragOrigEnd   = selEnd;

                // Find the slice whose start is flush with our end (link it)
                linkedSliceIdx = -1;
                for (int i = 0; i < ui.numSlices; ++i)
                {
                    if (i == sel || ! ui.slices[(size_t) i].active) continue;
                    if (ui.slices[(size_t) i].startSample == selEnd)
                    {
                        linkedSliceIdx     = i;
                        linkedPreviewStart = selEnd;
                        linkedPreviewEnd   = processor.sliceManager.getEndForSlice (i, ui.sampleNumFrames);
                        break;
                    }
                }
                return;
            }
        }
    }

    // ── Click on waveform outside any interaction: select whichever slice
    //    contains this sample position. Clicks between or outside slices
    //    do nothing (selection stays as-is).
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

// ... all other methods from your original file remain unchanged ...