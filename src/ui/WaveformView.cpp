#include "WaveformView.h"
// ... other includes remain unchanged ...

// ... keep all your member function definitions above unchanged until mouseDown ...

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr) return;
    int samplePos = std::max (0, std::min (pixelToSample (e.x), sampleSnap->buffer.getNumSamples()));

    // ── [1] TRIM MODE: drag handles always highest priority ──
    if (trimMode)
    {
        const int x1 = sampleToPixel (trimInPoint);
        const int x2 = sampleToPixel (trimOutPoint);
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
        // Do not continue—if not on marker, can't drag.
        return;
    }

    // ── [2] EDGE DRAG: Only in normal mode (not slices/alt, not trim) ──
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
            else if (std::abs (e.x - x2) < 6)
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
                linkedSliceIdx = -1;
                for (int i = 0; i < num; ++i)
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

    // ── [3] MPC-STYLE SINGLE CLICK ADD ──
    if (sliceDrawMode || altModeActive)
    {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdCreateSlice;
        cmd.intParam1 = samplePos;
        cmd.intParam2 = samplePos + 1; // or samplePos+minimalSize
        processor.pushCommand(cmd);
        return;
    }

    // ── [4] Default: select slice by clicking in its region ──
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

// ---- rest of your WaveformView.cpp remains unchanged ----
