#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "DysektLookAndFeel.h"

// ─────────────────────────────────────────────────────────────────────────────
WaveformView::WaveformView (DysektProcessor& p)
    : processor (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
}

// ─── View helpers ─────────────────────────────────────────────────────────────

int WaveformView::pixelToSample (int px) const
{
    auto snap = processor.sampleData.getSnapshot();
    if (snap == nullptr || snap->buffer.getNumSamples() == 0) return 0;
    const int total = snap->buffer.getNumSamples();
    const float zoom   = processor.zoom.load();
    const float scroll = processor.scroll.load();
    const int visLen   = juce::jmax (1, (int)(total / zoom));
    const int visStart = (int)((total - visLen) * scroll);
    return visStart + (int)(px * (float)visLen / (float)juce::jmax (1, getWidth()));
}

int WaveformView::sampleToPixel (int sample) const
{
    auto snap = processor.sampleData.getSnapshot();
    if (snap == nullptr || snap->buffer.getNumSamples() == 0) return 0;
    const int total = snap->buffer.getNumSamples();
    const float zoom   = processor.zoom.load();
    const float scroll = processor.scroll.load();
    const int visLen   = juce::jmax (1, (int)(total / zoom));
    const int visStart = (int)((total - visLen) * scroll);
    return (int)((float)(sample - visStart) / (float)visLen * (float)getWidth());
}

WaveformView::ViewState WaveformView::buildViewState (const SampleData::SnapshotPtr& snap) const
{
    ViewState vs;
    if (snap == nullptr || snap->buffer.getNumSamples() == 0) return vs;
    vs.numFrames = snap->buffer.getNumSamples();
    const float zoom   = processor.zoom.load();
    const float scroll = processor.scroll.load();
    vs.visibleLen   = juce::jmax (1, (int)(vs.numFrames / zoom));
    vs.visibleStart = (int)((vs.numFrames - vs.visibleLen) * scroll);
    vs.width        = getWidth();
    vs.samplesPerPixel = (float)vs.visibleLen / (float)juce::jmax (1, vs.width);
    vs.valid = true;
    return vs;
}

void WaveformView::syncAltStateFromMods (const juce::ModifierKeys& mods)
{
    altModeActive = mods.isAltDown();
}

// ─── State queries ────────────────────────────────────────────────────────────

bool WaveformView::isInteracting() const noexcept
{
    return dragMode != None || midDragging;
}

bool WaveformView::hasActiveSlicePreview() const noexcept
{
    return (dragMode == DragEdgeLeft  || dragMode == DragEdgeRight ||
            dragMode == MoveSlice     || dragMode == DuplicateSlice ||
            dragMode == DrawSlice     || shiftPreviewActive);
}

bool WaveformView::getActiveSlicePreview (int& sliceIdx, int& startSample, int& endSample) const
{
    if (!hasActiveSlicePreview()) return false;
    sliceIdx    = dragSliceIdx;
    startSample = dragPreviewStart;
    endSample   = dragPreviewEnd;
    return true;
}

// ─── Slice draw mode ──────────────────────────────────────────────────────────

void WaveformView::setSliceDrawMode (bool active)
{
    sliceDrawMode = active;
    dragMode = None;
    repaint();
}

// ─── Trim mode ────────────────────────────────────────────────────────────────

void WaveformView::enterTrimMode (int start, int end)
{
    trimMode     = true;
    trimStart    = start;
    trimEnd      = end;
    trimInPoint  = start;
    trimOutPoint = end;
    repaint();
}

void WaveformView::exitTrimMode()
{
    trimMode = false;
    repaint();
}

void WaveformView::getTrimBounds (int& outStart, int& outEnd) const
{
    outStart = trimInPoint;
    outEnd   = trimOutPoint;
}

void WaveformView::setTrimMode (bool active)
{
    if (active)
    {
        auto snap = processor.sampleData.getSnapshot();
        const int total = snap ? snap->buffer.getNumSamples() : 0;
        enterTrimMode (0, total);
    }
    else
    {
        exitTrimMode();
    }
}

void WaveformView::resetTrim()
{
    auto snap = processor.sampleData.getSnapshot();
    const int total = snap ? snap->buffer.getNumSamples() : 0;
    trimInPoint  = 0;
    trimOutPoint = total;
    trimStart    = 0;
    trimEnd      = total;
    repaint();
}

// ─── Cache ────────────────────────────────────────────────────────────────────

void WaveformView::rebuildCacheIfNeeded()
{
    auto snap = processor.sampleData.getSnapshot();
    if (snap == nullptr) return;
    auto vs = buildViewState (snap);
    if (!vs.valid) return;
    CacheKey key { vs.visibleStart, vs.visibleLen, vs.width, vs.numFrames, snap.get() };
    if (key == prevCacheKey) return;
    prevCacheKey = key;
    cache.rebuild (snap->buffer, snap->mipmaps, vs.numFrames, processor.zoom.load(), processor.scroll.load(), vs.width);
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void WaveformView::paint (juce::Graphics& g)
{
    auto snap = processor.sampleData.getSnapshot();
    cachedPaintViewState = buildViewState (snap);
    paintViewStateActive = true;

    // Background
    const auto& theme = getTheme();
    g.fillAll (theme.waveformBg);

    if (snap == nullptr || !cachedPaintViewState.valid)
    {
        g.setColour (theme.foreground.withAlpha (0.3f));
        g.setFont (14.0f);
        g.drawText ("Drop a sample here", getLocalBounds(), juce::Justification::centred);
        paintViewStateActive = false;
        return;
    }

    rebuildCacheIfNeeded();
    drawWaveform (g);
    drawSlices (g);
    drawPlaybackCursors (g);

    if (sliceDrawMode)   paintDrawSlicePreview (g);
    if (processor.lazyChop.isActive()) paintLazyChopOverlay (g);
    if (!transientPreviewPositions.empty()) paintTransientMarkers (g);
    if (trimMode)
    {
        paintTrimOverlay (g);
        paintTrimMarkers (g);
    }

    paintViewStateActive = false;
}

void WaveformView::drawWaveform (juce::Graphics& g)
{
    if (!cachedPaintViewState.valid) return;
    const auto& theme = getTheme();
    const int h = getHeight();
    const int mid = h / 2;
    const int w = getWidth();

    if (cache.getPeaks().empty()) return;

    if (softWaveform)
    {
        // Gradient fill
        juce::ColourGradient grad (theme.waveformColour.withAlpha (0.6f), 0, 0,
                                   theme.waveformColour.withAlpha (0.1f), 0, (float)h, false);
        g.setGradientFill (grad);
        juce::Path wavePath;
        wavePath.startNewSubPath (0, (float)mid);
        for (int x = 0; x < w; ++x)
        {
            float peak = cache.getPeaks()[x].maxVal;
            wavePath.lineTo ((float)x, mid - peak * mid);
        }
        for (int x = w - 1; x >= 0; --x)
        {
            float peak = cache.getPeaks()[x].maxVal;
            wavePath.lineTo ((float)x, mid + peak * mid);
        }
        wavePath.closeSubPath();
        g.fillPath (wavePath);

        // Glowing outline
        g.setColour (theme.waveformColour.withAlpha (0.9f));
        juce::Path outline;
        outline.startNewSubPath (0, (float)mid);
        for (int x = 0; x < w; ++x)
            outline.lineTo ((float)x, mid - cache.getPeaks()[x].maxVal * mid);
        g.strokePath (outline, juce::PathStrokeType (1.5f));
        juce::Path outlineBot;
        outlineBot.startNewSubPath (0, (float)mid);
        for (int x = 0; x < w; ++x)
            outlineBot.lineTo ((float)x, mid + cache.getPeaks()[x].maxVal * mid);
        g.strokePath (outlineBot, juce::PathStrokeType (1.5f));
    }
    else
    {
        // Classic solid fill
        g.setColour (theme.waveformColour.withAlpha (0.85f));
        for (int x = 0; x < w; ++x)
        {
            float peak = cache.getPeaks()[x].maxVal;
            int top = mid - (int)(peak * mid);
            int bot = mid + (int)(peak * mid);
            g.drawVerticalLine (x, (float)top, (float)bot);
        }
    }

    // Centre line
    g.setColour (theme.separator.withAlpha (0.4f));
    g.drawHorizontalLine (mid, 0.0f, (float)w);
}

void WaveformView::drawSlices (juce::Graphics& g)
{
    if (!cachedPaintViewState.valid) return;
    const auto& ui = processor.getUiSliceSnapshot();
    const auto& theme = getTheme();
    const int h = getHeight();

    for (int i = 0; i < ui.numSlices; ++i)
    {
        const auto& sl = ui.slices[i];
        int startPx = sampleToPixel (sl.startSample);
        int endPx   = sampleToPixel (sl.endSample);

        juce::Colour sliceCol = theme.slicePalette.isEmpty()
            ? theme.accent
            : theme.slicePalette[i % theme.slicePalette.size()];

        bool isSelected = (i == ui.selectedSlice);
        bool isPreview  = false;
        int previewStart = sl.startSample, previewEnd = sl.endSample;

        if (dragSliceIdx == i && (dragMode == DragEdgeLeft || dragMode == DragEdgeRight ||
                                   dragMode == MoveSlice || dragMode == DuplicateSlice))
        {
            isPreview    = true;
            previewStart = dragPreviewStart;
            previewEnd   = dragPreviewEnd;
            startPx      = sampleToPixel (previewStart);
            endPx        = sampleToPixel (previewEnd);
        }

        // Fill
        g.setColour (sliceCol.withAlpha (isSelected ? 0.35f : 0.18f));
        g.fillRect (startPx, 0, endPx - startPx, h);

        // Start edge
        g.setColour (sliceCol.withAlpha (isSelected ? 1.0f : 0.7f));
        g.drawVerticalLine (startPx, 0.0f, (float)h);

        // End edge (dimmer)
        g.setColour (sliceCol.withAlpha (isSelected ? 0.7f : 0.4f));
        g.drawVerticalLine (endPx, 0.0f, (float)h);

        // Hover highlight
        if (hoveredEdge != HoveredEdge::None && dragSliceIdx == i)
        {
            int edgePx = (hoveredEdge == HoveredEdge::Left) ? startPx : endPx;
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawVerticalLine (edgePx, 0.0f, (float)h);
        }
    }

    // Ghost for duplicate
    if (dragMode == DuplicateSlice && ghostEnd > ghostStart)
    {
        int gs = sampleToPixel (ghostStart);
        int ge = sampleToPixel (ghostEnd);
        g.setColour (theme.accent.withAlpha (0.25f));
        g.fillRect (gs, 0, ge - gs, h);
        g.setColour (theme.accent.withAlpha (0.7f));
        g.drawVerticalLine (gs, 0.0f, (float)h);
        g.drawVerticalLine (ge, 0.0f, (float)h);
    }

    // Draw-mode preview
    if (dragMode == DrawSlice && drawEnd > drawStart)
    {
        int ds = sampleToPixel (drawStart);
        int de = sampleToPixel (drawEnd);
        g.setColour (theme.accent.withAlpha (0.3f));
        g.fillRect (ds, 0, de - ds, h);
        g.setColour (theme.accent);
        g.drawVerticalLine (ds, 0.0f, (float)h);
        g.drawVerticalLine (de, 0.0f, (float)h);
    }
}

void WaveformView::drawPlaybackCursors (juce::Graphics& g)
{
    if (!cachedPaintViewState.valid) return;
    const auto& theme = getTheme();
    const auto& ui    = processor.getUiSliceSnapshot();
    const int h = getHeight();

    for (int i = 0; i < (int)processor.voicePool.voicePositions.size(); ++i)
    {
        float pos = processor.voicePool.voicePositions[i].load (std::memory_order_relaxed);
        if (pos <= 0.0f) continue;
        auto snap = processor.sampleData.getSnapshot();
        if (snap == nullptr) continue;
        int sample = (int)(pos * snap->buffer.getNumSamples());
        int px = sampleToPixel (sample);
        if (px < 0 || px >= getWidth()) continue;
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine (px, 0.0f, (float)h);
    }
}

void WaveformView::paintDrawSlicePreview (juce::Graphics& g)
{
    // Crosshair / cursor hint for draw mode
    const auto& theme = getTheme();
    auto pos = getMouseXYRelative();
    g.setColour (theme.accent.withAlpha (0.5f));
    g.drawVerticalLine (pos.x, 0.0f, (float)getHeight());
}

void WaveformView::paintLazyChopOverlay (juce::Graphics& g)
{
    const auto& theme = getTheme();
    g.setColour (theme.accent.withAlpha (0.12f));
    g.fillAll();
    g.setColour (theme.accent.withAlpha (0.7f));
    g.setFont (11.0f);
    g.drawText ("LAZY CHOP", getLocalBounds().removeFromTop (16), juce::Justification::centred);
}

void WaveformView::paintTransientMarkers (juce::Graphics& g)
{
    const auto& theme = getTheme();
    g.setColour (theme.accent.withAlpha (0.6f));
    for (int sample : transientPreviewPositions)
    {
        int px = sampleToPixel (sample);
        if (px >= 0 && px < getWidth())
            g.drawVerticalLine (px, 0.0f, (float)getHeight());
    }
}

void WaveformView::paintTrimOverlay (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const int h = getHeight();
    const int inPx  = sampleToPixel (trimInPoint);
    const int outPx = sampleToPixel (trimOutPoint);

    // Shade outside trim region
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    if (inPx > 0)
        g.fillRect (0, 0, inPx, h);
    if (outPx < getWidth())
        g.fillRect (outPx, 0, getWidth() - outPx, h);

    // Trim region tint
    g.setColour (theme.accent.withAlpha (0.08f));
    g.fillRect (inPx, 0, outPx - inPx, h);

    // Buttons
    const int btnH = 22, btnW = 60, margin = 6;
    const int btnY = h - btnH - margin;

    trimApplyBtnBounds  = { inPx + margin,                     btnY, btnW, btnH };
    trimCancelBtnBounds = { getWidth() - btnW - margin,         btnY, btnW, btnH };
    trimResetBtnBounds  = { getWidth() / 2 - btnW / 2,         btnY, btnW, btnH };

    auto drawBtn = [&] (juce::Rectangle<int> b, const juce::String& label, juce::Colour col)
    {
        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (b.toFloat(), 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (11.0f);
        g.drawText (label, b, juce::Justification::centred);
    };

    drawBtn (trimApplyBtnBounds,  "APPLY",  juce::Colour (0xff2e7d32));
    drawBtn (trimResetBtnBounds,  "RESET",  juce::Colour (0xff555555));
    drawBtn (trimCancelBtnBounds, "CANCEL", juce::Colour (0xffc62828));
}

void WaveformView::paintTrimMarkers (juce::Graphics& g)
{
    const int h = getHeight();
    auto drawMarker = [&] (int px, juce::Colour col)
    {
        g.setColour (col);
        g.drawVerticalLine (px, 0.0f, (float)h);
        juce::Rectangle<float> handle ((float)px - 5, 2.0f, 10.0f, 14.0f);
        g.fillRoundedRectangle (handle, 2.0f);
    };
    drawMarker (sampleToPixel (trimInPoint),  juce::Colour (0xff4caf50));
    drawMarker (sampleToPixel (trimOutPoint), juce::Colour (0xfff44336));
}

// ─── Resized ──────────────────────────────────────────────────────────────────

void WaveformView::resized()
{
    prevCacheKey = {};  // force cache rebuild on next paint
}

// ─── Mouse ────────────────────────────────────────────────────────────────────

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);

    // Trim mode — check buttons first
    if (trimMode)
    {
        if (trimApplyBtnBounds.contains (e.getPosition()))
        {
            if (onTrimApplied) onTrimApplied (trimInPoint, trimOutPoint);
            exitTrimMode();
            return;
        }
        if (trimCancelBtnBounds.contains (e.getPosition()))
        {
            if (onTrimCancelled) onTrimCancelled();
            exitTrimMode();
            return;
        }
        if (trimResetBtnBounds.contains (e.getPosition()))
        {
            resetTrim();
            return;
        }

        // Start dragging trim markers
        const int inPx  = sampleToPixel (trimInPoint);
        const int outPx = sampleToPixel (trimOutPoint);
        if (std::abs (e.x - inPx) <= kTrimMarkerHitTolerance)
            { dragMode = DragTrimIn; return; }
        if (std::abs (e.x - outPx) <= kTrimMarkerHitTolerance)
            { dragMode = DragTrimOut; return; }
        return;
    }

    // Middle mouse — pan/zoom
    if (e.mods.isMiddleButtonDown())
    {
        midDragging           = true;
        midDragStartX         = e.x;
        midDragStartY         = e.y;
        midDragStartZoom      = processor.zoom.load();
        auto snap = processor.sampleData.getSnapshot();
        const int total = snap ? snap->buffer.getNumSamples() : 1;
        const int visLen  = juce::jmax (1, (int)(total / midDragStartZoom));
        const int visStart = (int)((total - visLen) * processor.scroll.load());
        midDragAnchorFrac        = (float)pixelToSample (e.x) / (float)juce::jmax (1, total);
        midDragAnchorPixelFrac   = (float)e.x / (float)juce::jmax (1, getWidth());
        return;
    }

    if (e.mods.isRightButtonDown()) return;

    auto snap = processor.sampleData.getSnapshot();
    if (snap == nullptr) return;

    const int total = snap->buffer.getNumSamples();
    const int clickSample = pixelToSample (e.x);
    const auto& ui = processor.getUiSliceSnapshot();

    // Slice draw mode
    if (sliceDrawMode || altModeActive)
    {
        dragMode   = DrawSlice;
        drawStart  = clickSample;
        drawEnd    = clickSample;
        drawStartedFromAlt = altModeActive;
        return;
    }

    // Check for edge hits
    constexpr int kEdgeTol = 6;
    for (int i = 0; i < ui.numSlices; ++i)
    {
        int sp = sampleToPixel (ui.slices[i].startSample);
        int ep = sampleToPixel (ui.slices[i].endSample);

        if (std::abs (e.x - sp) <= kEdgeTol)
        {
            dragMode      = DragEdgeLeft;
            dragSliceIdx  = i;
            dragPreviewStart = ui.slices[i].startSample;
            dragPreviewEnd   = ui.slices[i].endSample;
            dragOrigStart    = ui.slices[i].startSample;
            dragOrigEnd      = ui.slices[i].endSample;
            // Select slice
            DysektProcessor::Command c;
            c.type = DysektProcessor::CmdSelectSlice;
            c.intParam1 = i;
            processor.pushCommand (c);
            return;
        }
        if (std::abs (e.x - ep) <= kEdgeTol)
        {
            dragMode      = DragEdgeRight;
            dragSliceIdx  = i;
            dragPreviewStart = ui.slices[i].startSample;
            dragPreviewEnd   = ui.slices[i].endSample;
            dragOrigStart    = ui.slices[i].startSample;
            dragOrigEnd      = ui.slices[i].endSample;
            DysektProcessor::Command c;
            c.type = DysektProcessor::CmdSelectSlice;
            c.intParam1 = i;
            processor.pushCommand (c);
            return;
        }
    }

    // Click inside a slice — select / move
    for (int i = ui.numSlices - 1; i >= 0; --i)
    {
        if (clickSample >= ui.slices[i].startSample && clickSample < ui.slices[i].endSample)
        {
            DysektProcessor::Command sel;
            sel.type = DysektProcessor::CmdSelectSlice;
            sel.intParam1 = i;
            processor.pushCommand (sel);

            if (e.mods.isShiftDown())
            {
                // Duplicate
                dragMode     = DuplicateSlice;
                dragSliceIdx = i;
                ghostStart   = ui.slices[i].startSample;
                ghostEnd     = ui.slices[i].endSample;
            }
            else
            {
                dragMode      = MoveSlice;
                dragSliceIdx  = i;
                dragOffset    = clickSample - ui.slices[i].startSample;
                dragSliceLen  = ui.slices[i].endSample - ui.slices[i].startSample;
                dragPreviewStart = ui.slices[i].startSample;
                dragPreviewEnd   = ui.slices[i].endSample;
                dragOrigStart    = ui.slices[i].startSample;
                dragOrigEnd      = ui.slices[i].endSample;
            }
            return;
        }
    }
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (midDragging)
    {
        // Zoom with vertical drag, pan with horizontal drag
        const float zoomFactor = std::pow (1.005f, -(float)(e.y - midDragStartY));
        float newZoom = juce::jlimit (1.0f, 64.0f, midDragStartZoom * zoomFactor);
        processor.zoom.store (newZoom);

        // Pan so anchor sample stays under cursor
        auto snap = processor.sampleData.getSnapshot();
        if (snap)
        {
            const int total = snap->buffer.getNumSamples();
            const int visLen = juce::jmax (1, (int)(total / newZoom));
            int anchorSample = (int)(midDragAnchorFrac * total);
            int newStart = anchorSample - (int)(midDragAnchorPixelFrac * visLen);
            newStart = juce::jlimit (0, total - visLen, newStart);
            processor.scroll.store ((float)newStart / (float)juce::jmax (1, total - visLen));
        }
        repaint();
        return;
    }

    if (dragMode == DragTrimIn)
    {
        auto snap = processor.sampleData.getSnapshot();
        const int total = snap ? snap->buffer.getNumSamples() : 0;
        trimInPoint = juce::jlimit (0, trimOutPoint - kMinTrimRegionSamples, pixelToSample (e.x));
        repaint(); return;
    }
    if (dragMode == DragTrimOut)
    {
        auto snap = processor.sampleData.getSnapshot();
        const int total = snap ? snap->buffer.getNumSamples() : 0;
        trimOutPoint = juce::jlimit (trimInPoint + kMinTrimRegionSamples, total, pixelToSample (e.x));
        repaint(); return;
    }

    auto snap = processor.sampleData.getSnapshot();
    if (snap == nullptr) return;
    const int total = snap->buffer.getNumSamples();
    const int sample = juce::jlimit (0, total, pixelToSample (e.x));
    const auto& ui = processor.getUiSliceSnapshot();

    if (dragMode == DrawSlice)
    {
        if (sample > drawStart) drawEnd = sample;
        else { drawEnd = drawStart; drawStart = sample; }
        repaint();
        return;
    }

    if (dragMode == DragEdgeLeft && dragSliceIdx >= 0)
    {
        dragPreviewStart = juce::jlimit (0, dragPreviewEnd - 1, sample);
        processor.liveDragSliceIdx.store (dragSliceIdx, std::memory_order_relaxed);
        repaint();
        return;
    }
    if (dragMode == DragEdgeRight && dragSliceIdx >= 0)
    {
        dragPreviewEnd = juce::jlimit (dragPreviewStart + 1, total, sample);
        processor.liveDragSliceIdx.store (dragSliceIdx, std::memory_order_relaxed);
        repaint();
        return;
    }
    if (dragMode == MoveSlice && dragSliceIdx >= 0)
    {
        int newStart = juce::jlimit (0, total - dragSliceLen, sample - dragOffset);
        dragPreviewStart = newStart;
        dragPreviewEnd   = newStart + dragSliceLen;
        processor.liveDragSliceIdx.store (dragSliceIdx, std::memory_order_relaxed);
        repaint();
        return;
    }
    if (dragMode == DuplicateSlice && dragSliceIdx >= 0)
    {
        int len = ghostEnd - ghostStart;
        ghostStart = juce::jlimit (0, total - len, sample - len / 2);
        ghostEnd   = ghostStart + len;
        repaint();
        return;
    }
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (midDragging) { midDragging = false; return; }

    processor.liveDragSliceIdx.store (-1, std::memory_order_relaxed);

    if (dragMode == DragTrimIn || dragMode == DragTrimOut)
    {
        dragMode = None;
        return;
    }

    auto snap = processor.sampleData.getSnapshot();

    if (dragMode == DrawSlice && snap)
    {
        const int total = snap->buffer.getNumSamples();
        if (drawEnd > drawStart)
        {
            DysektProcessor::Command c;
            c.type          = DysektProcessor::CmdCreateSlice;
            c.positions[0]  = drawStart;
            c.positions[1]  = drawEnd;
            c.numPositions  = 2;
            processor.pushCommand (c);
        }
        drawStart = drawEnd = 0;
        dragMode = None;
        repaint();
        return;
    }

    if ((dragMode == DragEdgeLeft || dragMode == DragEdgeRight) && dragSliceIdx >= 0 && snap)
    {
        DysektProcessor::Command c;
        c.type          = DysektProcessor::CmdSetSliceBounds;
        c.intParam1     = dragSliceIdx;
        c.positions[0]  = dragPreviewStart;
        c.positions[1]  = dragPreviewEnd;
        c.numPositions  = 2;
        processor.pushCommand (c);
    }

    if (dragMode == MoveSlice && dragSliceIdx >= 0 && snap)
    {
        DysektProcessor::Command c;
        c.type          = DysektProcessor::CmdSetSliceBounds;
        c.intParam1     = dragSliceIdx;
        c.positions[0]  = dragPreviewStart;
        c.positions[1]  = dragPreviewEnd;
        c.numPositions  = 2;
        processor.pushCommand (c);
    }

    if (dragMode == DuplicateSlice && dragSliceIdx >= 0 && snap)
    {
        DysektProcessor::Command c;
        c.type          = DysektProcessor::CmdCreateSlice;
        c.positions[0]  = ghostStart;
        c.positions[1]  = ghostEnd;
        c.numPositions  = 2;
        processor.pushCommand (c);
    }

    dragMode     = None;
    dragSliceIdx = -1;
    repaint();
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);
    const auto& ui = processor.getUiSliceSnapshot();
    constexpr int kEdgeTol = 6;
    hoveredEdge  = HoveredEdge::None;
    dragSliceIdx = -1;

    for (int i = 0; i < ui.numSlices; ++i)
    {
        int sp = sampleToPixel (ui.slices[i].startSample);
        int ep = sampleToPixel (ui.slices[i].endSample);
        if (std::abs (e.x - sp) <= kEdgeTol)
            { hoveredEdge = HoveredEdge::Left;  dragSliceIdx = i; setMouseCursor (juce::MouseCursor::LeftEdgeResizeCursor);  repaint(); return; }
        if (std::abs (e.x - ep) <= kEdgeTol)
            { hoveredEdge = HoveredEdge::Right; dragSliceIdx = i; setMouseCursor (juce::MouseCursor::RightEdgeResizeCursor); repaint(); return; }
    }
    setMouseCursor (sliceDrawMode ? juce::MouseCursor::CrosshairCursor
                                  : juce::MouseCursor::NormalCursor);
    repaint();
}

void WaveformView::mouseEnter (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);
    repaint();
}

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    hoveredEdge = HoveredEdge::None;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void WaveformView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    auto snap = processor.sampleData.getSnapshot();
    if (snap == nullptr) return;
    const int total = snap->buffer.getNumSamples();

    float zoom = processor.zoom.load();
    float anchorFrac = (float)e.x / (float)juce::jmax (1, getWidth());

    // Zoom with ctrl, scroll otherwise
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        float factor = 1.0f + w.deltaY * 0.25f;
        float newZoom = juce::jlimit (1.0f, 64.0f, zoom * factor);
        processor.zoom.store (newZoom);
    }
    else
    {
        const int visLen = juce::jmax (1, (int)(total / zoom));
        int shift = (int)(w.deltaX * visLen * 0.15f - w.deltaY * visLen * 0.15f);
        const int visStart = (int)((total - visLen) * processor.scroll.load());
        int newStart = juce::jlimit (0, total - visLen, visStart + shift);
        processor.scroll.store ((float)newStart / (float)juce::jmax (1, total - visLen));
    }
    repaint();
}

void WaveformView::modifierKeysChanged (const juce::ModifierKeys& mods)
{
    syncAltStateFromMods (mods);
    repaint();
}

// ─── File drag ────────────────────────────────────────────────────────────────

bool WaveformView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
            ext == ".ogg" || ext == ".flac" || ext == ".mp3"  ||
            ext == ".sf2" || ext == ".sfz")
            return true;
    }
    return false;
}

void WaveformView::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    juce::File f (files[0]);
    if (onLoadRequest) onLoadRequest (f);
}
