#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

WaveformView::WaveformView(DysektProcessor& p) : processor(p) {}

void WaveformView::setSliceDrawMode(bool active)
{
    sliceDrawMode = active;
    setMouseCursor(active ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);
}

bool WaveformView::hasActiveSlicePreview() const noexcept
{
    if (dragSliceIdx < 0)
        return false;

    return dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice;
}

bool WaveformView::getActiveSlicePreview(int& sliceIdx, int& startSample, int& endSample) const
{
    if (!hasActiveSlicePreview())
        return false;

    sliceIdx = dragSliceIdx;
    startSample = dragPreviewStart;
    endSample = dragPreviewEnd;
    return true;
}

bool WaveformView::getLinkedSlicePreview(int& sliceIdx, int& startSample, int& endSample) const
{
    if (linkedSliceIdx < 0 || dragMode == None)
        return false;
    sliceIdx = linkedSliceIdx;
    startSample = linkedPreviewStart;
    endSample = linkedPreviewEnd;
    return true;
}

bool WaveformView::isInteracting() const noexcept
{
    return dragMode != None || midDragging || shiftPreviewActive;
}

WaveformView::ViewState WaveformView::buildViewState(const SampleData::SnapshotPtr& sampleSnap) const
{
    ViewState state;
    if (sampleSnap == nullptr)
        return state;

    const int numFrames = sampleSnap->buffer.getNumSamples();
    const int width = getWidth();
    if (numFrames <= 0 || width <= 0)
        return state;

    const float z = std::max(1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visibleLen = juce::jlimit(1, numFrames, (int)(numFrames / z));
    const int maxStart = juce::jmax(0, numFrames - visibleLen);
    const int visibleStart = juce::jlimit(0, maxStart, (int)(sc * (float)maxStart));

    state.numFrames = numFrames;
    state.visibleStart = visibleStart;
    state.visibleLen = visibleLen;
    state.width = width;
    state.samplesPerPixel = (float)visibleLen / (float)width;
    state.valid = true;
    return state;
}

int WaveformView::pixelToSample(int px) const
{
    if (paintViewStateActive && cachedPaintViewState.valid)
    {
        return cachedPaintViewState.visibleStart +
                (int)((float)px / (float)cachedPaintViewState.width * cachedPaintViewState.visibleLen);
    }

    const auto state = buildViewState(processor.sampleData.getSnapshot());
    if (!state.valid)
        return 0;
    return state.visibleStart + (int)((float)px / (float)state.width * state.visibleLen);
}

int WaveformView::sampleToPixel(int sample) const
{
    if (paintViewStateActive && cachedPaintViewState.valid)
    {
        return (int)((float)(sample - cachedPaintViewState.visibleStart)
            / (float)cachedPaintViewState.visibleLen
            * (float)cachedPaintViewState.width);
    }

    const auto state = buildViewState(processor.sampleData.getSnapshot());
    if (!state.valid)
        return 0;
    return (int)((float)(sample - state.visibleStart) / (float)state.visibleLen * (float)state.width);
}

void WaveformView::rebuildCacheIfNeeded()
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    const auto view = buildViewState(sampleSnap);
    if (!view.valid)
        return;

    const CacheKey key{ view.visibleStart, view.visibleLen, view.width, view.numFrames, sampleSnap.get() };
    if (key == prevCacheKey)
        return;

    cache.rebuild(sampleSnap->buffer, sampleSnap->peakMipmaps,
                  view.numFrames, processor.zoom.load(), processor.scroll.load(), view.width);
    prevCacheKey = key;
}

void WaveformView::paint(juce::Graphics& g)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    g.fillAll(getTheme().waveformBg);

    // Grid lines
    int cy = getHeight() / 2;
    g.setColour(getTheme().gridLine.withAlpha(0.5f));
    g.drawHorizontalLine(cy, 0.0f, (float)getWidth());
    g.setColour(getTheme().gridLine.withAlpha(0.2f));
    g.drawHorizontalLine(getHeight() / 4, 0.0f, (float)getWidth());
    g.drawHorizontalLine(getHeight() * 3 / 4, 0.0f, (float)getWidth());

    if (sampleSnap != nullptr)
    {
        cachedPaintViewState = buildViewState(sampleSnap);
        paintViewStateActive = cachedPaintViewState.valid;
        rebuildCacheIfNeeded();
        drawWaveform(g);
        drawSlices(g);
        paintLazyChopOverlay(g);
        paintTransientMarkers(g);
        paintTrimOverlay(g);
        drawPlaybackCursors(g);
        paintViewStateActive = false;
    }
    else
    {
        paintViewStateActive = false;
        g.setColour(getTheme().foreground.withAlpha(0.25f));
        g.setFont(DysektLookAndFeel::makeFont(22.0f));
        g.drawText("DROP AUDIO FILE", getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformView::paintLazyChopOverlay(juce::Graphics& g)
{
    if (!(processor.lazyChop.isActive() && processor.lazyChop.isPlaying() && processor.lazyChop.getChopPos() >= 0))
        return;

    int previewIdx = LazyChopEngine::getPreviewVoiceIndex();
    float playhead = processor.voicePool.voicePositions[previewIdx].load(std::memory_order_relaxed);
    if (playhead <= 0.0f)
        return;

    int chopSample = processor.lazyChop.getChopPos();
    int headSample = (int)playhead;
    int x1 = sampleToPixel(std::min(chopSample, headSample));
    int x2 = sampleToPixel(std::max(chopSample, headSample));
    if (x2 > x1)
    {
        g.setColour(juce::Colour(0xFFCC4444).withAlpha(0.15f));
        g.fillRect(x1, 0, x2 - x1, getHeight());
        g.setColour(juce::Colour(0xFFCC4444).withAlpha(0.5f));
        g.drawVerticalLine(sampleToPixel(chopSample), 0.0f, (float)getHeight());
    }
}

void WaveformView::paintTrimOverlay(juce::Graphics& g)
{
    if (!trimMode) return;

    const int w = getWidth();
    const int h = getHeight();
    const int x1 = sampleToPixel(trimInPoint);
    const int x2 = sampleToPixel(trimOutPoint);
    const auto ac = getTheme().accent;

    // Excluded regions — dark overlay outside trim window
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    if (x1 > 0)
        g.fillRect(0, 0, x1, h);
    if (x2 < w)
        g.fillRect(x2, 0, w - x2, h);

    // In-point marker — bright vertical + top triangle handle
    g.setColour(ac.withAlpha(0.90f));
    g.drawVerticalLine(x1, 0.0f, (float)h);
    {
        juce::Path tri;
        tri.addTriangle((float)x1, 0.0f,
                        (float)x1 + 10.0f, 0.0f,
                        (float)x1, 10.0f);
        g.fillPath(tri);
    }

    // Out-point marker — bright vertical + top triangle handle (flipped)
    g.drawVerticalLine(x2, 0.0f, (float)h);
    {
        juce::Path tri;
        tri.addTriangle((float)x2, 0.0f,
                        (float)x2 - 10.0f, 0.0f,
                        (float)x2, 10.0f);
        g.fillPath(tri);
    }

    // Thin accent tint inside trim window
    g.setColour(ac.withAlpha(0.04f));
    if (x2 > x1)
        g.fillRect(x1, 0, x2 - x1, h);
}

void WaveformView::paintTransientMarkers(juce::Graphics& g)
{
    if (transientPreviewPositions.empty())
        return;

    g.setColour(getTheme().accent.withAlpha(0.6f));
    float dashLengths[] = { 4.0f, 3.0f };
    for (int pos : transientPreviewPositions)
    {
        int px = sampleToPixel(pos);
        if (px >= 0 && px < getWidth())
        {
            juce::Path dashPath;
            dashPath.startNewSubPath((float)px, 0.0f);
            dashPath.lineTo((float)px, (float)getHeight());
            juce::PathStrokeType stroke(1.0f);
            juce::Path dashedPath;
            stroke.createDashedStroke(dashedPath, dashPath, dashLengths, 2);
            g.fillPath(dashedPath);
        }
    }
}

void WaveformView::drawWaveform(juce::Graphics& g)
{
    const int   cy = getHeight() / 2;
    const float scale = (float)getHeight() * UILayout::waveformVerticalScale;

    auto& peaks = cache.getPeaks();
    const int numPeaks = std::min(cache.getNumPeaks(), getWidth());
    if (numPeaks <= 0)
        return;

    float samplesPerPixel = 1.0f;
    if (paintViewStateActive && cachedPaintViewState.valid)
        samplesPerPixel = cachedPaintViewState.samplesPerPixel;
    else
    {
        const auto view = buildViewState(processor.sampleData.getSnapshot());
        if (view.valid)
            samplesPerPixel = view.samplesPerPixel;
    }

    // Top/bottom fill path
    juce::Path fillPath;
    if (samplesPerPixel >= 1.0f)
    {
        fillPath.startNewSubPath(0.0f, (float)cy - peaks[0].maxVal * scale);
        for (int px = 1; px < numPeaks; ++px)
            fillPath.lineTo((float)px, (float)cy - peaks[(size_t)px].maxVal * scale);
        for (int px = numPeaks - 1; px >= 0; --px)
            fillPath.lineTo((float)px, (float)cy - peaks[(size_t)px].minVal * scale);
        fillPath.closeSubPath();
    }

    // HARD MODE / flat waveform
    if (!softWaveform)
    {
        // Sub-sample zoom: just lines
        if (samplesPerPixel < 1.0f)
        {
            g.setColour(getTheme().waveform.withAlpha(0.9f));
            juce::Path path;
            bool started = false;
            for (int px = 0; px < numPeaks; ++px)
            {
                float y = (float)cy - peaks[(size_t)px].maxVal * scale;
                if (!started) { path.startNewSubPath((float)px, y); started = true; }
                else path.lineTo((float)px, y);
            }
            g.strokePath(path, juce::PathStrokeType(1.5f));

            if (samplesPerPixel < 0.125f)
            {
                const float dotR = 2.5f;
                for (int px = 0; px < numPeaks; ++px)
                {
                    float exactPos = (float)pixelToSample(0) + (float)px * samplesPerPixel;
                    float frac = exactPos - std::floor(exactPos);
                    if (frac < samplesPerPixel)
                    {
                        float y = (float)cy - peaks[(size_t)px].maxVal * scale;
                        g.fillEllipse((float)px - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
                    }
                }
            }
        }
        else
        {
            g.setColour(getTheme().waveform);
            g.fillPath(fillPath);

            if (samplesPerPixel < 8.0f)
            {
                juce::Path midPath;
                float midY0 = (float)cy - (peaks[0].maxVal + peaks[0].minVal) * 0.5f * scale;
                midPath.startNewSubPath(0.0f, midY0);
                for (int px = 1; px < numPeaks; ++px)
                {
                    float mid = (peaks[(size_t)px].maxVal + peaks[(size_t)px].minVal) * 0.5f;
                    midPath.lineTo((float)px, (float)cy - mid * scale);
                }
                g.strokePath(midPath, juce::PathStrokeType(1.5f));
            }
        }
    }
    else
    {
        // SOFT MODE — gradient fill and bright outlines
        const juce::Colour waveCol = getTheme().waveform;
        const juce::Colour bgCol = getTheme().waveformBg;
        const int h = getHeight();

        if (samplesPerPixel < 1.0f)
        {
            g.setColour(waveCol.withAlpha(0.95f));
            juce::Path path;
            bool started = false;
            for (int px = 0; px < numPeaks; ++px)
            {
                float y = (float)cy - peaks[(size_t)px].maxVal * scale;
                if (!started) { path.startNewSubPath((float)px, y); started = true; }
                else path.lineTo((float)px, y);
            }
            g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (samplesPerPixel < 0.125f)
            {
                g.setColour(waveCol);
                const float dotR = 3.0f;
                for (int px = 0; px < numPeaks; ++px)
                {
                    float exactPos = (float)pixelToSample(0) + (float)px * samplesPerPixel;
                    float frac = exactPos - std::floor(exactPos);
                    if (frac < samplesPerPixel)
                    {
                        float y = (float)cy - peaks[(size_t)px].maxVal * scale;
                        g.fillEllipse((float)px - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
                    }
                }
            }
        }
        else
        {
            // 1. Gradient fill main shape
            {
                juce::ColourGradient grad(
                    waveCol.withAlpha(0.0f), 0.0f, 0.0f,
                    waveCol.withAlpha(0.0f), 0.0f, (float)h, false);
                grad.addColour(0.35, waveCol.withAlpha(0.18f));
                grad.addColour(0.5, waveCol.withAlpha(0.28f));
                grad.addColour(0.65, waveCol.withAlpha(0.18f));
                g.setGradientFill(grad);
                g.fillPath(fillPath);
            }

            // 2. Top outline (max envelope)
            {
                juce::Path topPath;
                topPath.startNewSubPath(0.0f, (float)cy - peaks[0].maxVal * scale);
                for (int px = 1; px < numPeaks; ++px)
                    topPath.lineTo((float)px, (float)cy - peaks[(size_t)px].maxVal * scale);

                g.setColour(waveCol.withAlpha(0.25f));
                g.strokePath(topPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                g.setColour(waveCol.withAlpha(0.90f));
                g.strokePath(topPath, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // 3. Bottom outline (min envelope)
            {
                juce::Path botPath;
                botPath.startNewSubPath(0.0f, (float)cy - peaks[0].minVal * scale);
                for (int px = 1; px < numPeaks; ++px)
                    botPath.lineTo((float)px, (float)cy - peaks[(size_t)px].minVal * scale);

                g.setColour(waveCol.withAlpha(0.25f));
                g.strokePath(botPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(waveCol.withAlpha(0.90f));
                g.strokePath(botPath, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // 4. Midline, if close zoom
            if (samplesPerPixel < 8.0f)
            {
                juce::Path midPath;
                float midY0 = (float)cy - (peaks[0].maxVal + peaks[0].minVal) * 0.5f * scale;
                midPath.startNewSubPath(0.0f, midY0);
                for (int px = 1; px < numPeaks; ++px)
                {
                    float mid = (peaks[(size_t)px].maxVal + peaks[(size_t)px].minVal) * 0.5f;
                    midPath.lineTo((float)px, (float)cy - mid * scale);
                }
                g.setColour(waveCol.withAlpha(0.85f));
                g.strokePath(midPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
}

void WaveformView::drawSlices(juce::Graphics& g)
{
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t)i];
        if (!s.active) continue;

        int drawStartSample = s.startSample;
        int drawEndSample = processor.sliceManager.getEndForSlice(i, ui.sampleNumFrames);
        if (i == sel && dragSliceIdx == i
            && (dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice))
        {
            drawStartSample = dragPreviewStart;
            drawEndSample = dragPreviewEnd;
        }
        else if (i == linkedSliceIdx && linkedSliceIdx >= 0 && dragMode != None)
        {
            drawStartSample = linkedPreviewStart;
            drawEndSample = linkedPreviewEnd;
        }
        else if (dragMode == None)
        {
            // Live preview from SliceControlBar knob drag (processor atomics)
            const int liveIdx = processor.liveDragSliceIdx.load(std::memory_order_acquire);
            if (liveIdx == i)
            {
                drawStartSample = processor.liveDragBoundsStart.load(std::memory_order_relaxed);
                drawEndSample = processor.liveDragBoundsEnd.load(std::memory_order_relaxed);
            }
        }

        int x1 = std::max(0, sampleToPixel(drawStartSample));
        int x2 = std::min(getWidth(), sampleToPixel(drawEndSample));
        int sw = x2 - x1;
        if (sw <= 0) continue;

        if (i == sel)
        {
            // Selected: purple overlay
            g.setColour(getTheme().selectionOverlay.withAlpha(0.22f));
            g.fillRect(x1, 0, sw, getHeight());

            // Markers with triangle handles at bottom
            g.setColour(getTheme().foreground.withAlpha(0.8f));
            g.drawVerticalLine(x1, 0.0f, (float)getHeight());
            g.drawVerticalLine(x2 - 1, 0.0f, (float)getHeight());

            // Triangle handles at bottom for start (larger/brighter when hovered)
            {
                bool hov = (hoveredEdge == HoveredEdge::Left);
                float tw = hov ? 10.0f : 7.0f;
                float th = hov ? 12.0f : 9.0f;
                float alpha = hov ? 1.0f : 0.9f;
                juce::Path triS;
                triS.addTriangle((float)x1, (float)getHeight(),
                    (float)x1 + tw, (float)getHeight(),
                    (float)x1, (float)getHeight() - th);
                g.setColour(getTheme().foreground.withAlpha(alpha));
                g.fillPath(triS);
            }

            // Triangle handles at bottom for end (larger/brighter when hovered)
            {
                bool hov = (hoveredEdge == HoveredEdge::Right);
                float tw = hov ? 10.0f : 7.0f;
                float th = hov ? 12.0f : 9.0f;
                float alpha = hov ? 1.0f : 0.9f;
                juce::Path triE;
                triE.addTriangle((float)(x2 - 1), (float)getHeight(),
                    (float)(x2 - 1) - tw, (float)getHeight(),
                    (float)(x2 - 1), (float)getHeight() - th);
                g.setColour(getTheme().foreground.withAlpha(alpha));
                g.fillPath(triE);
            }

            // "S" and "E" labels near handles
            g.setFont(DysektLookAndFeel::makeFont(10.0f, true));
            g.setColour(getTheme().foreground.withAlpha(0.7f));
            g.drawText("S", x1 + 2, getHeight() - 24, 12, 12, juce::Justification::centredLeft);
            g.drawText("E", x2 - 14, getHeight() - 24, 12, 12, juce::Justification::centredRight);

            // Label
            g.setColour(getTheme().foreground.withAlpha(0.85f));
            g.setFont(DysektLookAndFeel::makeFont(13.0f, true));
            g.drawText("Slice " + juce::String(i + 1), x1 + 3, 3, 70, 14, juce::Justification::centredLeft);
        }
        else
        {
            // Non-selected: thin vertical edge lines only (no overlay fill)
            g.setColour(s.colour.withAlpha(0.30f));
            g.drawVerticalLine(x1, 0.0f, (float)getHeight());
            g.drawVerticalLine(x2 - 1, 0.0f, (float)getHeight());
        }
    }
}

void WaveformView::drawPlaybackCursors(juce::Graphics& g)
{
    int previewIdx = LazyChopEngine::getPreviewVoiceIndex();
    for (int i = 0; i < VoicePool::kMaxVoices; ++i)
    {
        float pos = processor.voicePool.voicePositions[i].load(std::memory_order_relaxed);
        if (pos > 0.0f)
        {
            int px = sampleToPixel((int)pos);
            if (px >= 0 && px < getWidth())
            {
                if (i == previewIdx && processor.lazyChop.isActive())
                    g.setColour(juce::Colour(0xFFCC4444));  // red for preview
                else
                    g.setColour(getTheme().accent.withAlpha(0.7f));  // yellow

                g.drawVerticalLine(px, 0.0f, (float)getHeight());
            }
        }
    }
}

void WaveformView::resized()
{
    prevCacheKey = {}; // force cache rebuild
}

void WaveformView::syncAltStateFromMods(const juce::ModifierKeys& mods)
{
    const bool alt = mods.isAltDown();
    if (alt == altModeActive)
        return;

    altModeActive = alt;
    hoveredEdge = HoveredEdge::None;

    if (alt)
        setMouseCursor(juce::MouseCursor::IBeamCursor);
    else if (dragMode != DrawSlice)
        setMouseCursor(juce::MouseCursor::NormalCursor);

    repaint();
}

void WaveformView::mouseMove(const juce::MouseEvent& e)
{
    syncAltStateFromMods(e.mods);

    // Trim mode: only show resize cursor near markers
    if (trimMode)
    {
        const int x1 = sampleToPixel(trimInPoint);
        const int x2 = sampleToPixel(trimOutPoint);
        if (std::abs(e.x - x1) < 8 || std::abs(e.x - x2) < 8)
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr) return;
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;
    HoveredEdge newEdge = HoveredEdge::None;

    if (sel >= 0 && sel < num && !sliceDrawMode && !altModeActive)
    {
        const auto& s = ui.slices[(size_t)sel];
        if (s.active)
        {
            int x1 = sampleToPixel(s.startSample);
            int x2 = sampleToPixel(processor.sliceManager.getEndForSlice(sel, ui.sampleNumFrames));
            if (std::abs(e.x - x1) < 6) newEdge = HoveredEdge::Left;
            else if (std::abs(e.x - x2) < 6) newEdge = HoveredEdge::Right;
        }
    }
    if (altModeActive)
        setMouseCursor(juce::MouseCursor::IBeamCursor);
    else if (sliceDrawMode)
        setMouseCursor(juce::MouseCursor::IBeamCursor);
    else
        setMouseCursor(newEdge != HoveredEdge::None ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);

    if (newEdge != hoveredEdge) { hoveredEdge = newEdge; repaint(); }
}

void WaveformView::mouseEnter(const juce::MouseEvent& e) { mouseMove(e); }

void WaveformView::mouseExit(const juce::MouseEvent&)
{
    if (hoveredEdge != HoveredEdge::None) { hoveredEdge = HoveredEdge::None; repaint(); }
}

void WaveformView::modifierKeysChanged(const juce::ModifierKeys& mods)
{
    syncAltStateFromMods(mods);
}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    syncAltStateFromMods(e.mods);

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

        midDragAnchorPixelFrac = (w > 0) ? (float)e.x / (float)w : 0.5f;
        midDragAnchorFrac = juce::jlimit(0.0f, 1.0f, viewStart + midDragAnchorPixelFrac * viewFrac);
        return;
    }

    int samplePos = std::max(0, std::min(pixelToSample(e.x), sampleSnap->buffer.getNumSamples()));

    // Trim mode: only allow dragging the trim markers
    if (trimMode)
    {
        const int x1 = sampleToPixel(trimInPoint);
        const int x2 = sampleToPixel(trimOutPoint);
        if (std::abs(e.x - x1) < 8)
        {
            dragMode = DragTrimIn;
            trimDragging = true;
        }
        else if (std::abs(e.x - x2) < 8)
        {
            dragMode = DragTrimOut;
            trimDragging = true;
        }
        return;
    }

    // Shift+click: preview audio from pointer position
    if (e.mods.isShiftDown() && !sliceDrawMode && !altModeActive
        && !processor.lazyChop.isActive())
    {
        shiftPreviewActive = true;
        processor.shiftPreviewRequest.store(samplePos, std::memory_order_relaxed);
        return;
    }

    // PC STYLE: Add slice as a single click at samplePos, not a drag region
    if (sliceDrawMode)
    {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdCreateSlice;
        cmd.intParam1 = samplePos;
        cmd.intParam2 = samplePos;
        processor.pushCommand(cmd);
        repaint();
        return;
    }

    // ALT-click alternative for future custom drawing mode
    if (altModeActive)
    {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdCreateSlice;
        cmd.intParam1 = samplePos;
        cmd.intParam2 = samplePos;
        processor.pushCommand(cmd);
        repaint();
        return;
    }

    // Check slice edges (6px hot zone) — only for already-selected slice
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;

    if (sel >= 0 && sel < num)
    {
        const auto& s = ui.slices[(size_t)sel];
        if (s.active)
        {
            const int selEnd = processor.sliceManager.getEndForSlice(sel, ui.sampleNumFrames);
            int x1 = sampleToPixel(s.startSample);
            int x2 = sampleToPixel(selEnd);

            if (std::abs(e.x - x1) < 6)
            {
                DysektProcessor::Command gestureCmd;
                gestureCmd.type = DysektProcessor::CmdBeginGesture;
                processor.pushCommand(gestureCmd);
                dragMode = DragEdgeLeft;
                dragSliceIdx = sel;
                dragPreviewStart = s.startSample;
                dragPreviewEnd = selEnd;
                dragOrigStart = s.startSample;
                dragOrigEnd = selEnd;

                // Find the slice whose end is flush with our start (link it)
                linkedSliceIdx = -1;
                for (int i = 0; i < ui.numSlices; ++i)
                {
                    if (i == sel || !ui.slices[(size_t)i].active) continue;
                    if (processor.sliceManager.getEndForSlice(i, ui.sampleNumFrames) == s.startSample)
                    {
                        linkedSliceIdx = i;
                        linkedPreviewStart = ui.slices[(size_t)i].startSample;
                        linkedPreviewEnd = s.startSample;
                        break;
                    }
                }
                return;
            }
            if (std::abs(e.x - x2) < 6)
            {
                DysektProcessor::Command gestureCmd;
                gestureCmd.type = DysektProcessor::CmdBeginGesture;
                processor.pushCommand(gestureCmd);
                dragMode = DragEdgeRight;
                dragSliceIdx = sel;
                dragPreviewStart = s.startSample;
                dragPreviewEnd = selEnd;
                dragOrigStart = s.startSample;
                dragOrigEnd = selEnd;

                // Find the slice whose start is flush with our end (link it)
                linkedSliceIdx = -1;
                for (int i = 0; i < ui.numSlices; ++i)
                {
                    if (i == sel || !ui.slices[(size_t)i].active) continue;
                    if (ui.slices[(size_t)i].startSample == selEnd)
                    {
                        linkedSliceIdx = i;
                        linkedPreviewStart = selEnd;
                        linkedPreviewEnd = processor.sliceManager.getEndForSlice(i, ui.sampleNumFrames);
                        break;
                    }
                }
                return;
            }
        }
    }

    // Click on waveform: select whichever slice contains this sample position
    for (int i = 0; i < num; ++i)
    {
        const auto& sl = ui.slices[(size_t)i];
        if (sl.active && samplePos >= sl.startSample
            && samplePos < processor.sliceManager.getEndForSlice(i, ui.sampleNumFrames))
        {
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdSelectSlice;
            cmd.intParam1 = i;
            processor.pushCommand(cmd);
            break;
        }
    }
}

// A dummy stub for filesDropped, since it's a required JUCE override, but not part of the core functionality shown here
bool WaveformView::isInterestedInFileDrag(const juce::StringArray&) { return true; }
void WaveformView::filesDropped(const juce::StringArray&, int, int) {  }
void WaveformView::mouseDrag(const juce::MouseEvent&) {}
void WaveformView::mouseUp(const juce::MouseEvent&) {}
void WaveformView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) {}
void WaveformView::enterTrimMode(int start, int end) { trimMode = true; trimInPoint = trimStart = start; trimOutPoint = trimEnd = end; }
void WaveformView::setTrimPoints(int inPt, int outPt) { trimInPoint = inPt; trimOutPoint = outPt; }
void WaveformView::exitTrimMode() { trimMode = false; }
void WaveformView::getTrimBounds(int& outStart, int& outEnd) const { outStart = trimInPoint; outEnd = trimOutPoint; }
void WaveformView::setTrimMode(bool active) { trimMode = active; }
void WaveformView::resetTrim() { trimInPoint = trimStart; trimOutPoint = trimEnd; }
