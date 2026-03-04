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
    cache.rebuild (snap->buffer, snap->peakMipmaps, vs.numFrames, processor.zoom.load(), processor.scroll.load(), vs.width);
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

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    if (inPx > 0)
        g.fillRect (0, 0, inPx, h);
    if (outPx < getWidth())
        g.fillRect (outPx, 0, getWidth() - outPx, h);
}