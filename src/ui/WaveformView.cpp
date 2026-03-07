#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

WaveformView::WaveformView (DysektProcessor& p) : processor (p) {}

void WaveformView::setSliceDrawMode (bool active)
{
    sliceDrawMode = active;
    setMouseCursor (active ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);
}

bool WaveformView::hasActiveSlicePreview() const noexcept
{
    if (dragSliceIdx < 0)
        return false;

    return dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice;
}

bool WaveformView::getActiveSlicePreview (int& sliceIdx, int& startSample, int& endSample) const
{
    if (! hasActiveSlicePreview())
        return false;

    sliceIdx = dragSliceIdx;
    startSample = dragPreviewStart;
    endSample = dragPreviewEnd;
    return true;
}

bool WaveformView::isInteracting() const noexcept
{
    return dragMode != None || midDragging || shiftPreviewActive;
}

WaveformView::ViewState WaveformView::buildViewState (const SampleData::SnapshotPtr& sampleSnap) const
{
    ViewState state;
    if (sampleSnap == nullptr)
        return state;

    const int numFrames = sampleSnap->buffer.getNumSamples();
    const int width = getWidth();
    if (numFrames <= 0 || width <= 0)
        return state;

    const float z = std::max (1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visibleLen = juce::jlimit (1, numFrames, (int) (numFrames / z));
    const int maxStart = juce::jmax (0, numFrames - visibleLen);
    const int visibleStart = juce::jlimit (0, maxStart, (int) (sc * (float) maxStart));

    state.numFrames = numFrames;
    state.visibleStart = visibleStart;
    state.visibleLen = visibleLen;
    state.width = width;
    state.samplesPerPixel = (float) visibleLen / (float) width;
    state.valid = true;
    return state;
}

int WaveformView::pixelToSample (int px) const
{
    if (paintViewStateActive && cachedPaintViewState.valid)
    {
        return cachedPaintViewState.visibleStart
            + (int) ((float) px / (float) cachedPaintViewState.width * cachedPaintViewState.visibleLen);
    }

    const auto state = buildViewState (processor.sampleData.getSnapshot());
    if (! state.valid)
        return 0;
    return state.visibleStart + (int) ((float) px / (float) state.width * state.visibleLen);
}

int WaveformView::sampleToPixel (int sample) const
{
    if (paintViewStateActive && cachedPaintViewState.valid)
    {
        return (int) ((float) (sample - cachedPaintViewState.visibleStart)
                      / (float) cachedPaintViewState.visibleLen
                      * (float) cachedPaintViewState.width);
    }

    const auto state = buildViewState (processor.sampleData.getSnapshot());
    if (! state.valid)
        return 0;
    return (int) ((float) (sample - state.visibleStart) / (float) state.visibleLen * (float) state.width);
}

void WaveformView::rebuildCacheIfNeeded()
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    const auto view = buildViewState (sampleSnap);
    if (! view.valid)
        return;

    const CacheKey key { view.visibleStart, view.visibleLen, view.width, view.numFrames, sampleSnap.get() };
    if (key == prevCacheKey)
        return;

    cache.rebuild (sampleSnap->buffer, sampleSnap->peakMipmaps,
                   view.numFrames, processor.zoom.load(), processor.scroll.load(), view.width);
    prevCacheKey = key;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    g.fillAll (getTheme().waveformBg);

    // Grid lines
    int cy = getHeight() / 2;
    g.setColour (getTheme().gridLine.withAlpha (0.5f));
    g.drawHorizontalLine (cy, 0.0f, (float) getWidth());
    g.setColour (getTheme().gridLine.withAlpha (0.2f));
    g.drawHorizontalLine (getHeight() / 4, 0.0f, (float) getWidth());
    g.drawHorizontalLine (getHeight() * 3 / 4, 0.0f, (float) getWidth());

    if (sampleSnap != nullptr)
    {
        cachedPaintViewState = buildViewState (sampleSnap);
        paintViewStateActive = cachedPaintViewState.valid;
        rebuildCacheIfNeeded();
        drawWaveform (g);
        drawSlices (g);
        paintDrawSlicePreview (g);
        paintLazyChopOverlay (g);
        paintTransientMarkers (g);
        drawPlaybackCursors (g);
        if (trimMode) paintTrimOverlay (g);
        paintViewStateActive = false;
    }
    else
    {
        paintViewStateActive = false;
        g.setColour (getTheme().foreground.withAlpha (0.25f));
        g.setFont (DysektLookAndFeel::makeFont (22.0f));
        g.drawText ("DROP AUDIO FILE", getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformView::paintDrawSlicePreview (juce::Graphics& g)
{
    // Draw active slice region while dragging in +SLC mode
    if (dragMode == DrawSlice)
    {
        int x1 = sampleToPixel (std::min (drawStart, drawEnd));
        int x2 = sampleToPixel (std::max (drawStart, drawEnd));
        if (x2 > x1)
        {
            g.setColour (getTheme().accent.withAlpha (0.2f));
            g.fillRect (x1, 0, x2 - x1, getHeight());
            g.setColour (getTheme().accent.withAlpha (0.6f));
            g.drawVerticalLine (x1, 0.0f, (float) getHeight());
            g.drawVerticalLine (x2, 0.0f, (float) getHeight());
        }
    }

    // Draw ghost overlay for Ctrl-drag duplicate
    if (dragMode == DuplicateSlice)
    {
        int gx1 = sampleToPixel (ghostStart);
        int gx2 = sampleToPixel (ghostEnd);
        if (gx2 > gx1)
        {
            g.setColour (getTheme().accent.withAlpha (0.15f));
            g.fillRect (gx1, 0, gx2 - gx1, getHeight());
            juce::Path p;
            p.addRectangle ((float) gx1, 0.5f, (float)(gx2 - gx1), (float) getHeight() - 1.0f);
            float dl[] = { 4.0f, 4.0f };
            juce::PathStrokeType pst (1.0f);
            juce::Path dashed;
            pst.createDashedStroke (dashed, p, dl, 2);
            g.setColour (getTheme().accent.withAlpha (0.75f));
            g.strokePath (dashed, pst);
        }
    }
}

void WaveformView::paintLazyChopOverlay (juce::Graphics& g)
{
    if (! (processor.lazyChop.isActive() && processor.lazyChop.isPlaying()
           && processor.lazyChop.getChopPos() >= 0))
        return;

    int previewIdx = LazyChopEngine::getPreviewVoiceIndex();
    float playhead = processor.voicePool.voicePositions[previewIdx].load (std::memory_order_relaxed);
    if (playhead <= 0.0f)
        return;

    int chopSample = processor.lazyChop.getChopPos();
    int headSample = (int) playhead;
    int x1 = sampleToPixel (std::min (chopSample, headSample));
    int x2 = sampleToPixel (std::max (chopSample, headSample));
    if (x2 > x1)
    {
        g.setColour (juce::Colour (0xFFCC4444).withAlpha (0.15f));
        g.fillRect (x1, 0, x2 - x1, getHeight());
        g.setColour (juce::Colour (0xFFCC4444).withAlpha (0.5f));
        g.drawVerticalLine (sampleToPixel (chopSample), 0.0f, (float) getHeight());
    }
}

void WaveformView::paintTransientMarkers (juce::Graphics& g)
{
    if (transientPreviewPositions.empty())
        return;

    g.setColour (getTheme().accent.withAlpha (0.6f));
    float dashLengths[] = { 4.0f, 3.0f };
    for (int pos : transientPreviewPositions)
    {
        int px = sampleToPixel (pos);
        if (px >= 0 && px < getWidth())
        {
            juce::Path dashPath;
            dashPath.startNewSubPath ((float) px, 0.0f);
            dashPath.lineTo ((float) px, (float) getHeight());
            juce::PathStrokeType stroke (1.0f);
            juce::Path dashedPath;
            stroke.createDashedStroke (dashedPath, dashPath, dashLengths, 2);
            g.fillPath (dashedPath);
        }
    }
}

void WaveformView::drawWaveform (juce::Graphics& g)
{
    // Use the selected slice's palette colour as the waveform colour.
    // Falls back to theme waveform colour when no slice is selected.
    juce::Colour waveformColour = getTheme().waveform;
    {
        const auto& ui  = processor.getUiSliceSnapshot();
        const int   sel = ui.selectedSlice;
        if (sel >= 0 && sel < ui.numSlices && ui.slices[(size_t) sel].active)
            waveformColour = ui.slices[(size_t) sel].colour;
    }

    const int   cy    = getHeight() / 2;
    const float scale = (float) getHeight() * UILayout::waveformVerticalScale;

    auto& peaks    = cache.getPeaks();
    const int numPeaks = std::min (cache.getNumPeaks(), getWidth());
    if (numPeaks <= 0)
        return;

    float samplesPerPixel = 1.0f;
    if (paintViewStateActive && cachedPaintViewState.valid)
        samplesPerPixel = cachedPaintViewState.samplesPerPixel;
    else
    {
        const auto view = buildViewState (processor.sampleData.getSnapshot());
        if (view.valid)
            samplesPerPixel = view.samplesPerPixel;
    }

    // ── Build top/bottom path once (shared by both modes) ────────────────────
    juce::Path fillPath;
    if (samplesPerPixel >= 1.0f)
    {
        fillPath.startNewSubPath (0.0f, (float) cy - peaks[0].maxVal * scale);
        for (int px = 1; px < numPeaks; ++px)
            fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);
        for (int px = numPeaks - 1; px >= 0; --px)
            fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].minVal * scale);
        fillPath.closeSubPath();
    }

    // ─────────────────────────────────────────────────────────────────────────
    if (! softWaveform)
    {
        // ── HARD MODE (original flat-fill rendering) ──────────────────────────
        if (samplesPerPixel < 1.0f)
        {
            g.setColour (waveformColour.withAlpha (0.9f));
            juce::Path path;
            bool started = false;
            for (int px = 0; px < numPeaks; ++px)
            {
                float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                if (! started) { path.startNewSubPath ((float) px, y); started = true; }
                else             path.lineTo ((float) px, y);
            }
            g.strokePath (path, juce::PathStrokeType (1.5f));

            if (samplesPerPixel < 0.125f)
            {
                const float dotR = 2.5f;
                for (int px = 0; px < numPeaks; ++px)
                {
                    float exactPos = (float) pixelToSample (0) + (float) px * samplesPerPixel;
                    float frac = exactPos - std::floor (exactPos);
                    if (frac < samplesPerPixel)
                    {
                        float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                        g.fillEllipse ((float) px - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
                    }
                }
            }
        }
        else
        {
            g.setColour (waveformColour);
            g.fillPath (fillPath);

            if (samplesPerPixel < 8.0f)
            {
                juce::Path midPath;
                float midY0 = (float) cy - (peaks[0].maxVal + peaks[0].minVal) * 0.5f * scale;
                midPath.startNewSubPath (0.0f, midY0);
                for (int px = 1; px < numPeaks; ++px)
                {
                    float mid = (peaks[(size_t) px].maxVal + peaks[(size_t) px].minVal) * 0.5f;
                    midPath.lineTo ((float) px, (float) cy - mid * scale);
                }
                g.strokePath (midPath, juce::PathStrokeType (1.5f));
            }
        }
    }
    else
    {
        // ── SOFT MODE (TAL-style: gradient fill + bright outline stroke) ──────
        const juce::Colour waveCol  = waveformColour;
        const juce::Colour bgCol    = getTheme().waveformBg;
        const int h = getHeight();

        if (samplesPerPixel < 1.0f)
        {
            // Sub-sample zoom: single bright line, no fill needed
            g.setColour (waveCol.withAlpha (0.95f));
            juce::Path path;
            bool started = false;
            for (int px = 0; px < numPeaks; ++px)
            {
                float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                if (! started) { path.startNewSubPath ((float) px, y); started = true; }
                else             path.lineTo ((float) px, y);
            }
            g.strokePath (path, juce::PathStrokeType (1.8f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (samplesPerPixel < 0.125f)
            {
                g.setColour (waveCol);
                const float dotR = 3.0f;
                for (int px = 0; px < numPeaks; ++px)
                {
                    float exactPos = (float) pixelToSample (0) + (float) px * samplesPerPixel;
                    float frac = exactPos - std::floor (exactPos);
                    if (frac < samplesPerPixel)
                    {
                        float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                        g.fillEllipse ((float) px - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
                    }
                }
            }
        }
        else
        {
            // ── 1. Translucent gradient fill ──────────────────────────────────
            // The fill uses a vertical gradient: transparent at top/bottom
            // edges, semi-opaque near the centreline — giving depth without
            // the harsh solid look of hard mode.
            {
                juce::ColourGradient grad (
                    waveCol.withAlpha (0.0f),  0.0f, 0.0f,
                    waveCol.withAlpha (0.0f),  0.0f, (float) h,
                    false);
                // Add a brighter band at the centreline
                grad.addColour (0.35, waveCol.withAlpha (0.18f));
                grad.addColour (0.5,  waveCol.withAlpha (0.28f));
                grad.addColour (0.65, waveCol.withAlpha (0.18f));

                g.setGradientFill (grad);
                g.fillPath (fillPath);
            }

            // ── 2. Top outline (max envelope) ─────────────────────────────────
            {
                juce::Path topPath;
                topPath.startNewSubPath (0.0f, (float) cy - peaks[0].maxVal * scale);
                for (int px = 1; px < numPeaks; ++px)
                    topPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);

                // Faint halo pass (wider, more transparent)
                g.setColour (waveCol.withAlpha (0.25f));
                g.strokePath (topPath, juce::PathStrokeType (3.5f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                // Main bright line
                g.setColour (waveCol.withAlpha (0.90f));
                g.strokePath (topPath, juce::PathStrokeType (1.3f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // ── 3. Bottom outline (min envelope) ──────────────────────────────
            {
                juce::Path botPath;
                botPath.startNewSubPath (0.0f, (float) cy - peaks[0].minVal * scale);
                for (int px = 1; px < numPeaks; ++px)
                    botPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].minVal * scale);

                g.setColour (waveCol.withAlpha (0.25f));
                g.strokePath (botPath, juce::PathStrokeType (3.5f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                g.setColour (waveCol.withAlpha (0.90f));
                g.strokePath (botPath, juce::PathStrokeType (1.3f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // ── 4. Transition midline (close zoom) ────────────────────────────
            if (samplesPerPixel < 8.0f)
            {
                juce::Path midPath;
                float midY0 = (float) cy - (peaks[0].maxVal + peaks[0].minVal) * 0.5f * scale;
                midPath.startNewSubPath (0.0f, midY0);
                for (int px = 1; px < numPeaks; ++px)
                {
                    float mid = (peaks[(size_t) px].maxVal + peaks[(size_t) px].minVal) * 0.5f;
                    midPath.lineTo ((float) px, (float) cy - mid * scale);
                }
                g.setColour (waveCol.withAlpha (0.85f));
                g.strokePath (midPath, juce::PathStrokeType (1.5f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
}

void WaveformView::drawSlices (juce::Graphics& g)
{
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t) i];
        if (! s.active) continue;

        int drawStartSample = s.startSample;
        int drawEndSample = processor.sliceManager.getEndForSlice (i, ui.sampleNumFrames);
        if (i == sel && dragSliceIdx == i
            && (dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice))
        {
            drawStartSample = dragPreviewStart;
            drawEndSample = dragPreviewEnd;
        }
        else if (dragMode == None)
        {
            // Live preview from SliceControlBar knob drag (processor atomics)
            const int liveIdx = processor.liveDragSliceIdx.load (std::memory_order_acquire);
            if (liveIdx == i)
            {
                drawStartSample = processor.liveDragBoundsStart.load (std::memory_order_relaxed);
                drawEndSample   = processor.liveDragBoundsEnd.load   (std::memory_order_relaxed);
            }
        }

        int x1 = std::max (0, sampleToPixel (drawStartSample));
        int x2 = std::min (getWidth(), sampleToPixel (drawEndSample));
        int sw = x2 - x1;
        if (sw <= 0) continue;

        if (i == sel)
        {
            // Selected: purple overlay
            g.setColour (getTheme().selectionOverlay.withAlpha (0.22f));
            g.fillRect (x1, 0, sw, getHeight());

            // Markers with triangle handles at bottom
            g.setColour (getTheme().foreground.withAlpha (0.8f));
            g.drawVerticalLine (x1, 0.0f, (float) getHeight());
            g.drawVerticalLine (x2 - 1, 0.0f, (float) getHeight());

            // Triangle handles at bottom for start (larger/brighter when hovered)
            {
                bool hov = (hoveredEdge == HoveredEdge::Left);
                float tw = hov ? 10.0f : 7.0f;
                float th = hov ? 12.0f : 9.0f;
                float alpha = hov ? 1.0f : 0.9f;
                juce::Path triS;
                triS.addTriangle ((float) x1, (float) getHeight(),
                                  (float) x1 + tw, (float) getHeight(),
                                  (float) x1, (float) getHeight() - th);
                g.setColour (getTheme().foreground.withAlpha (alpha));
                g.fillPath (triS);
            }

            // Triangle handles at bottom for end (larger/brighter when hovered)
            {
                bool hov = (hoveredEdge == HoveredEdge::Right);
                float tw = hov ? 10.0f : 7.0f;
                float th = hov ? 12.0f : 9.0f;
                float alpha = hov ? 1.0f : 0.9f;
                juce::Path triE;
                triE.addTriangle ((float) (x2 - 1), (float) getHeight(),
                                  (float) (x2 - 1) - tw, (float) getHeight(),
                                  (float) (x2 - 1), (float) getHeight() - th);
                g.setColour (getTheme().foreground.withAlpha (alpha));
                g.fillPath (triE);
            }

            // "S" and "E" labels near handles
            g.setFont (DysektLookAndFeel::makeFont (10.0f, true));
            g.setColour (getTheme().foreground.withAlpha (0.7f));
            g.drawText ("S", x1 + 2, getHeight() - 24, 12, 12, juce::Justification::centredLeft);
            g.drawText ("E", x2 - 14, getHeight() - 24, 12, 12, juce::Justification::centredRight);

            // Label
            g.setColour (getTheme().foreground.withAlpha (0.85f));
            g.setFont (DysektLookAndFeel::makeFont (13.0f, true));
            g.drawText ("Slice " + juce::String (i + 1), x1 + 3, 3, 70, 14,
                         juce::Justification::centredLeft);
        }
        else
        {
            // Non-selected: thin vertical edge lines only (no overlay fill)
            g.setColour (s.colour.withAlpha (0.30f));
            g.drawVerticalLine (x1, 0.0f, (float) getHeight());
            g.drawVerticalLine (x2 - 1, 0.0f, (float) getHeight());
        }
    }
}

void WaveformView::drawPlaybackCursors (juce::Graphics& g)
{
    int previewIdx = LazyChopEngine::getPreviewVoiceIndex();
    for (int i = 0; i < VoicePool::kMaxVoices; ++i)
    {
        float pos = processor.voicePool.voicePositions[i].load (std::memory_order_relaxed);
        if (pos > 0.0f)
        {
            int px = sampleToPixel ((int) pos);
            if (px >= 0 && px < getWidth())
            {
                if (i == previewIdx && processor.lazyChop.isActive())
                    g.setColour (juce::Colour (0xFFCC4444));  // red for preview
                else
                    g.setColour (getTheme().accent.withAlpha (0.7f));  // yellow

                g.drawVerticalLine (px, 0.0f, (float) getHeight());
            }
        }
    }
}

void WaveformView::resized()
{
    prevCacheKey = {};  // force cache rebuild
}

void WaveformView::syncAltStateFromMods (const juce::ModifierKeys& mods)
{
    const bool alt = mods.isAltDown();
    if (alt == altModeActive)
        return;

    altModeActive = alt;
    hoveredEdge = HoveredEdge::None;

    if (alt)
        setMouseCursor (juce::MouseCursor::IBeamCursor);
    else if (dragMode != DrawSlice)
        setMouseCursor (juce::MouseCursor::NormalCursor);

    repaint();
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr) return;
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;
    HoveredEdge newEdge = HoveredEdge::None;

    if (sel >= 0 && sel < num && ! sliceDrawMode && ! altModeActive)
    {
        const auto& s = ui.slices[(size_t) sel];
        if (s.active)
        {
            int x1 = sampleToPixel (s.startSample);
            int x2 = sampleToPixel (processor.sliceManager.getEndForSlice (sel, ui.sampleNumFrames));
            if      (std::abs (e.x - x1) < 6) newEdge = HoveredEdge::Left;
            else if (std::abs (e.x - x2) < 6) newEdge = HoveredEdge::Right;
        }
    }
    if (altModeActive)
        setMouseCursor (juce::MouseCursor::IBeamCursor);
    else if (sliceDrawMode)
        setMouseCursor (juce::MouseCursor::IBeamCursor);
    else
        setMouseCursor (newEdge != HoveredEdge::None
            ? juce::MouseCursor::LeftRightResizeCursor
            : juce::MouseCursor::NormalCursor);

    if (newEdge != hoveredEdge) { hoveredEdge = newEdge; repaint(); }
}

void WaveformView::mouseEnter (const juce::MouseEvent& e) { mouseMove (e); }

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    if (hoveredEdge != HoveredEdge::None) { hoveredEdge = HoveredEdge::None; repaint(); }
}

void WaveformView::modifierKeysChanged (const juce::ModifierKeys& mods)
{
    syncAltStateFromMods (mods);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
        return;

    // Trim mode: intercept clicks for trim marker drag
    if (trimMode)
    {
        static constexpr int kTrimMarkerHitTolerance = 6;
        int inX  = sampleToPixel (trimInPoint);
        int outX = sampleToPixel (trimOutPoint);
        if (std::abs (e.x - inX) <= kTrimMarkerHitTolerance)
            { dragMode = DragTrimIn;  return; }
        if (std::abs (e.x - outX) <= kTrimMarkerHitTolerance)
            { dragMode = DragTrimOut; return; }
        return;  // all other trim-mode clicks are no-ops
    }

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

    // Shift+click: preview audio from pointer position
    if (e.mods.isShiftDown() && ! sliceDrawMode && ! altModeActive
        && ! processor.lazyChop.isActive())
    {
        shiftPreviewActive = true;
        processor.shiftPreviewRequest.store (samplePos, std::memory_order_relaxed);
        return;
    }

    if (sliceDrawMode || altModeActive)
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
                return;
            }

            if (e.x > x1 && e.x < x2)
            {
                DysektProcessor::Command gestureCmd;
                gestureCmd.type = DysektProcessor::CmdBeginGesture;
                processor.pushCommand (gestureCmd);

                dragSliceIdx = sel;
                dragOffset   = samplePos - s.startSample;
                dragSliceLen = selEnd - s.startSample;

                if (e.mods.isCtrlDown())
                {
                    dragMode   = DuplicateSlice;
                    ghostStart = s.startSample;
                    ghostEnd   = selEnd;
                }
                else
                {
                    dragMode = MoveSlice;
                    dragPreviewStart = s.startSample;
                    dragPreviewEnd = selEnd;
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

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragTrimIn || dragMode == DragTrimOut)
    {
        static constexpr int kMinTrimRegionSamples = 64;
        auto sn = processor.sampleData.getSnapshot();
        int totalFrames = sn ? sn->buffer.getNumSamples() : 1;
        int newPos = juce::jlimit (0, totalFrames, pixelToSample (e.x));
        if (dragMode == DragTrimIn)
        {
            newPos = juce::jlimit (0, trimOutPoint - kMinTrimRegionSamples, newPos);
            trimInPoint = newPos;
        }
        else
        {
            newPos = juce::jlimit (trimInPoint + kMinTrimRegionSamples, totalFrames, newPos);
            trimOutPoint = newPos;
        }
        repaint();
        return;
    }

    syncAltStateFromMods (e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
        return;

    // Middle-mouse drag: scroll+zoom
    if (midDragging)
    {
        int w = getWidth();
        if (w <= 0) return;

        float deltaY = (float) (e.y - midDragStartY);
        float newZoom = juce::jlimit (1.0f, 16384.0f, midDragStartZoom * UIHelpers::computeZoomFactor (deltaY));
        processor.zoom.store (newZoom);

        float newViewFrac = 1.0f / newZoom;
        float hDragFrac = -(float) (e.x - midDragStartX) / (float) w * newViewFrac;
        float newViewStart = midDragAnchorFrac - midDragAnchorPixelFrac * newViewFrac + hDragFrac;

        float maxScroll = 1.0f - newViewFrac;
        if (maxScroll > 0.0f)
            processor.scroll.store (juce::jlimit (0.0f, 1.0f, newViewStart / maxScroll));

        prevCacheKey = {};
        return;
    }

    int samplePos = std::max (0, std::min (pixelToSample (e.x), sampleSnap->buffer.getNumSamples()));

    if (dragMode == DrawSlice)
    {
        drawEnd = samplePos;
        return;
    }

    if (dragMode == DragEdgeLeft && dragSliceIdx >= 0)
    {
        if (processor.snapToZeroCrossing.load())
            samplePos = AudioAnalysis::findNearestZeroCrossing (sampleSnap->buffer, samplePos);
        dragPreviewStart = juce::jlimit (0, dragPreviewEnd - 64, samplePos);
    }
    else if (dragMode == DragEdgeRight && dragSliceIdx >= 0)
    {
        if (processor.snapToZeroCrossing.load())
            samplePos = AudioAnalysis::findNearestZeroCrossing (sampleSnap->buffer, samplePos);
        dragPreviewEnd = juce::jlimit (dragPreviewStart + 64,
                                       sampleSnap->buffer.getNumSamples(), samplePos);
    }
    else if (dragMode == MoveSlice && dragSliceIdx >= 0)
    {
        int newStart = samplePos - dragOffset;
        int newEnd   = newStart + dragSliceLen;
        int maxLen   = sampleSnap->buffer.getNumSamples();
        newStart = juce::jlimit (0, maxLen - dragSliceLen, newStart);
        newEnd   = newStart + dragSliceLen;
        dragPreviewStart = newStart;
        dragPreviewEnd   = newEnd;
    }

    // Push live bounds to the audio engine so note-ons during drag use the
    // current edge position. Written before the idx store (release) so the
    // audio thread sees consistent values after its acquire load on idx.
    if ((dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice)
        && dragSliceIdx >= 0)
    {
        processor.liveDragBoundsStart.store (dragPreviewStart, std::memory_order_relaxed);
        processor.liveDragBoundsEnd.store   (dragPreviewEnd,   std::memory_order_relaxed);
        processor.liveDragSliceIdx.store    (dragSliceIdx,     std::memory_order_release);
    }

    if (dragMode == DuplicateSlice && dragSliceIdx >= 0)
    {
        int maxLen   = sampleSnap->buffer.getNumSamples();
        int newStart = juce::jlimit (0, maxLen - dragSliceLen, samplePos - dragOffset);
        ghostStart   = newStart;
        ghostEnd     = newStart + dragSliceLen;
    }
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (dragMode == DragTrimIn || dragMode == DragTrimOut)
    {
        dragMode = None;
        repaint();
        return;
    }

    syncAltStateFromMods (e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();

    // Stop shift preview
    if (shiftPreviewActive)
    {
        shiftPreviewActive = false;
        processor.shiftPreviewRequest.store (-1, std::memory_order_relaxed);
        return;
    }

    if (midDragging)
    {
        midDragging = false;
        return;
    }

    if (dragMode == DrawSlice)
    {
        const bool altStillDown = e.mods.isAltDown();
        const int maxFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 0;
        int endPos = std::max (0, std::min (pixelToSample (e.x), maxFrames));
        if (sampleSnap != nullptr && processor.snapToZeroCrossing.load())
        {
            drawStart = AudioAnalysis::findNearestZeroCrossing (sampleSnap->buffer, drawStart);
            endPos = AudioAnalysis::findNearestZeroCrossing (sampleSnap->buffer, endPos);
        }
        if (std::abs (endPos - drawStart) >= 64)
        {
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdCreateSlice;
            cmd.intParam1 = drawStart;
            cmd.intParam2 = endPos;
            processor.pushCommand (cmd);
            if (! altModeActive)
            {
                sliceDrawMode = false;
                setMouseCursor (juce::MouseCursor::NormalCursor);
            }
        }

        if (drawStartedFromAlt && ! altStillDown)
        {
            sliceDrawMode = false;
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }

        // If click without dragging (< 64 samples), keep draw mode active
    }
    else if (dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice)
    {
        if (dragSliceIdx >= 0)
        {
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdSetSliceBounds;
            cmd.intParam1 = dragSliceIdx;
            cmd.intParam2 = dragPreviewStart;
            cmd.positions[0] = dragPreviewEnd;
            cmd.numPositions = 1;
            processor.pushCommand (cmd);
        }
    }
    else if (dragMode == DuplicateSlice)
    {
        if (sampleSnap != nullptr && processor.snapToZeroCrossing.load())
        {
            ghostStart = AudioAnalysis::findNearestZeroCrossing (sampleSnap->buffer, ghostStart);
            ghostEnd   = ghostStart + dragSliceLen;
        }
        DysektProcessor::Command cmd;
        cmd.type      = DysektProcessor::CmdDuplicateSlice;
        cmd.intParam1 = ghostStart;
        cmd.intParam2 = ghostEnd;
        processor.pushCommand (cmd);
    }

    // Deactivate live drag so the audio thread stops overriding slice bounds.
    // Must happen before dragSliceIdx is cleared so there's no window where
    // a stale idx could re-activate on the next block.
    processor.liveDragSliceIdx.store (-1, std::memory_order_release);

    dragMode = None;
    dragSliceIdx = -1;
    dragPreviewStart = 0;
    dragPreviewEnd = 0;
    drawStartedFromAlt = false;
}

void WaveformView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (w.deltaX != 0.0f)
    {
        float sc = processor.scroll.load();
        sc -= w.deltaX * 0.05f;
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, sc));
        prevCacheKey = {};
        return;
    }

    if (e.mods.isShiftDown())
    {
        // Scroll
        float sc = processor.scroll.load();
        sc -= w.deltaY * 0.05f;
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, sc));
    }
    else
    {
        // Cursor-anchored zoom
        int width = getWidth();
        float oldZoom = processor.zoom.load();
        float oldViewFrac = 1.0f / oldZoom;
        float oldScroll = processor.scroll.load();

        // Sample fraction under cursor
        float cursorPixelFrac = (width > 0) ? (float) e.x / (float) width : 0.5f;

        // Apply zoom change
        float newZoom = (w.deltaY > 0)
            ? std::min (16384.0f, oldZoom * 1.2f)
            : std::max (1.0f, oldZoom / 1.2f);
        processor.zoom.store (newZoom);

        // Recompute scroll so anchorFrac stays at same pixel position
        float newViewFrac = 1.0f / newZoom;
        float maxScroll = 1.0f - newViewFrac;
        if (maxScroll > 0.0f)
        {
            float oldViewStart = oldScroll * (1.0f - oldViewFrac);
            float anchorFrac = oldViewStart + cursorPixelFrac * oldViewFrac;
            float newViewStart = anchorFrac - cursorPixelFrac * newViewFrac;
            processor.scroll.store (juce::jlimit (0.0f, 1.0f, newViewStart / maxScroll));
        }
        else
            processor.scroll.store (0.0f);
    }
    prevCacheKey = {};  // force cache rebuild
}

bool WaveformView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav"  || ext == ".ogg"  || ext == ".aif"  ||
            ext == ".aiff" || ext == ".flac"  || ext == ".mp3"  ||
            ext == ".sf2"  || ext == ".sfz")
            return true;
    }
    return false;
}

void WaveformView::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;

    juce::File f (files[0]);
    auto ext = f.getFileExtension().toLowerCase();

    processor.zoom.store (1.0f);
    processor.scroll.store (0.0f);
    prevCacheKey = {};

    if (ext == ".sf2" || ext == ".sfz")
        processor.loadSoundFontAsync (f);
    else
        processor.loadFileAsync (f);
}

// =============================================================================
//  Trim mode entry points
// =============================================================================

// enterTrimMode — called by ActionPanel/PluginEditor when the user opens trim.
// start / end are the initial marker positions in samples (typically 0 and
// totalFrames so the markers sit at the extremes and the user drags inward).
void WaveformView::enterTrimMode (int start, int end)
{
    trimMode    = true;
    trimStart   = start;
    trimEnd     = end;
    trimInPoint = start;
    trimOutPoint = end;
    dragMode    = None;
    repaint();
}

// setTrimMode — toggle used by TrimDialog buttons (Apply / Cancel).
// When active=false the overlay is hidden; a repaint clears the markers.
void WaveformView::setTrimMode (bool active)
{
    trimMode = active;
    if (! active)
    {
        dragMode     = None;
        trimInPoint  = 0;
        trimOutPoint = 0;
    }
    repaint();
}

// ── paintTrimOverlay ──────────────────────────────────────────────────────────
void WaveformView::paintTrimOverlay (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    int inX  = sampleToPixel (trimInPoint);
    int outX = sampleToPixel (trimOutPoint);

    // Shade regions outside trim window
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (0, 0, inX,      h);
    g.fillRect (outX, 0, w - outX, h);

    // IN marker — cyan, right-pointing triangle
    g.setColour (juce::Colour (0xFF00FFFF));
    g.drawVerticalLine (inX, 0.0f, (float) h);
    juce::Path tri;
    tri.addTriangle ((float) inX, 4.f, (float) (inX + 8), 10.f, (float) inX, 16.f);
    g.fillPath (tri);
    g.setFont (DysektLookAndFeel::makeFont (9.0f));
    g.drawText ("IN",  inX + 2,  2, 24, 12, juce::Justification::left);

    // OUT marker — orange, left-pointing triangle
    g.setColour (juce::Colour (0xFFFF8C00));
    g.drawVerticalLine (outX, 0.0f, (float) h);
    juce::Path tri2;
    tri2.addTriangle ((float) outX, 4.f, (float) (outX - 8), 10.f, (float) outX, 16.f);
    g.fillPath (tri2);
    g.drawText ("OUT", outX - 28, 2, 28, 12, juce::Justification::right);
}
