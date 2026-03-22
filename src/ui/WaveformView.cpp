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

if (!active)
{
    // --- CLEAR ALL DRAG/PREVIEW/LIVE MARKER STATES ---
    dragMode     = None;
    dragSliceIdx = -1;
    linkedSliceIdx = -1;
    dragPreviewStart = 0;
    dragPreviewEnd = 0;
    drawStartedFromAlt = false;
    processor.liveDragSliceIdx.store(-1, std::memory_order_release);
    processor.liveDragBoundsStart.store(-1, std::memory_order_release); // <--- ADD THIS LINE
    processor.liveDragBoundsEnd.store(-1, std::memory_order_release);   // <--- ADD THIS LINE
    repaint();
}
}

bool WaveformView::hasActiveSlicePreview() const noexcept
{
    if (dragSliceIdx < 0)
        return false;
    return dragMode == DragEdgeLeft || dragMode == MoveSlice;
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

bool WaveformView::getLinkedSlicePreview (int& sliceIdx, int& startSample, int& endSample) const
{
    if (linkedSliceIdx < 0 || dragMode == None)
        return false;
    sliceIdx    = linkedSliceIdx;
    startSample = linkedPreviewStart;
    endSample   = linkedPreviewEnd;
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
        return cachedPaintViewState.visibleStart
            + (int) ((float) px / (float) cachedPaintViewState.width * cachedPaintViewState.visibleLen);
    const auto state = buildViewState (processor.sampleData.getSnapshot());
    if (! state.valid) return 0;
    return state.visibleStart + (int) ((float) px / (float) state.width * state.visibleLen);
}

int WaveformView::sampleToPixel (int sample) const
{
    if (paintViewStateActive && cachedPaintViewState.valid)
        return (int) ((float) (sample - cachedPaintViewState.visibleStart)
                      / (float) cachedPaintViewState.visibleLen
                      * (float) cachedPaintViewState.width);
    const auto state = buildViewState (processor.sampleData.getSnapshot());
    if (! state.valid) return 0;
    return (int) ((float) (sample - state.visibleStart) / (float) state.visibleLen * (float) state.width);
}

void WaveformView::rebuildCacheIfNeeded()
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    const auto view = buildViewState (sampleSnap);
    if (! view.valid) return;
    const CacheKey key { view.visibleStart, view.visibleLen, view.width, view.numFrames, sampleSnap.get() };
    if (key == prevCacheKey) return;
    cache.rebuild (sampleSnap->buffer, sampleSnap->peakMipmaps,
                   view.numFrames, processor.zoom.load(), processor.scroll.load(), view.width);
    prevCacheKey = key;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    g.fillAll (getTheme().waveformBg);
    int cy = getHeight() / 2;
    g.setColour (getTheme().gridLine.withAlpha (0.5f));
    g.drawHorizontalLine (cy, 0.0f, (float) getWidth());
    g.setColour (getTheme().gridLine.withAlpha (0.2f));
    g.drawHorizontalLine (getHeight() / 4, 0.0f, (float) getWidth());
    g.drawHorizontalLine (getHeight() * 3 / 4, 0.0f, (float) getWidth());
    if (sampleSnap != nullptr) {
        cachedPaintViewState = buildViewState (sampleSnap);
        paintViewStateActive = cachedPaintViewState.valid;
        rebuildCacheIfNeeded();
        drawWaveform (g);
        drawSlices (g);
        paintDrawSlicePreview (g);
        paintLazyChopOverlay (g);
        paintTransientMarkers (g);
        paintTrimOverlay (g);
        drawPlaybackCursors (g);
        paintViewStateActive = false;
    } else {
        paintViewStateActive = false;
        g.setColour (getTheme().foreground.withAlpha (0.25f));
        g.setFont (DysektLookAndFeel::makeFont (22.0f));
        g.drawText ("DROP AUDIO FILE", getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformView::paintDrawSlicePreview (juce::Graphics& g) {}

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
    if (x2 > x1) {
        g.setColour (juce::Colour (0xFFCC4444).withAlpha (0.15f));
        g.fillRect (x1, 0, x2 - x1, getHeight());
        g.setColour (juce::Colour (0xFFCC4444).withAlpha (0.5f));
        g.drawVerticalLine (sampleToPixel (chopSample), 0.0f, (float) getHeight());
    }
}

void WaveformView::paintTrimOverlay (juce::Graphics& g)
{
    if (! trimMode) return;
    const int w  = getWidth();
    const int h  = getHeight();

    auto sampleSnap = processor.sampleData.getSnapshot();
    int totalFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 1;

    int clampedTrimIn  = juce::jlimit(0, totalFrames - 1, trimInPoint);
    int clampedTrimOut = juce::jlimit(clampedTrimIn + 1, totalFrames, trimOutPoint);

    const int x1 = juce::jlimit (0, w - 1, sampleToPixel (clampedTrimIn));
    const int x2 = juce::jlimit (x1 + 1, w - 1, sampleToPixel (clampedTrimOut));
    const auto ac = getTheme().accent;

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    if (x1 > 0) g.fillRect (0, 0, x1, h);
    if (x2 < w - 1) g.fillRect (x2 + 1, 0, w - x2 - 1, h);

    g.setColour (ac.withAlpha (0.90f));
    g.drawVerticalLine (x1, 0.0f, (float) h);
    {
        juce::Path tri;
        tri.addTriangle ((float) x1, 0.0f, (float) x1 + 10.0f, 0.0f, (float) x1, 10.0f);
        g.fillPath (tri);
    }
    g.drawVerticalLine (x2, 0.0f, (float) h);
    {
        juce::Path tri;
        tri.addTriangle ((float) x2, 0.0f, (float) x2 - 10.0f, 0.0f, (float) x2, 10.0f);
        g.fillPath (tri);
    }
    g.setColour (ac.withAlpha (0.04f));
    if (x2 > x1) g.fillRect (x1, 0, x2 - x1, h);
}

void WaveformView::paintTransientMarkers (juce::Graphics& g)
{
    if (transientPreviewPositions.empty()) return;
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

    if (! softWaveform)
    {
        if (samplesPerPixel < 1.0f)
        {
            g.setColour (getTheme().waveform.withAlpha (0.9f));
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
            g.setColour (getTheme().waveform);
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
        const juce::Colour waveCol  = getTheme().waveform;
        const juce::Colour bgCol    = getTheme().waveformBg;
        const int h = getHeight();

        if (samplesPerPixel < 1.0f)
        {
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
            juce::ColourGradient grad (
                waveCol.withAlpha (0.0f),  0.0f, 0.0f,
                waveCol.withAlpha (0.0f),  0.0f, (float) h,
                false);
            grad.addColour (0.35, waveCol.withAlpha (0.18f));
            grad.addColour (0.5,  waveCol.withAlpha (0.28f));
            grad.addColour (0.65, waveCol.withAlpha (0.18f));
            g.setGradientFill (grad);
            g.fillPath (fillPath);

            juce::Path topPath;
            topPath.startNewSubPath (0.0f, (float) cy - peaks[0].maxVal * scale);
            for (int px = 1; px < numPeaks; ++px)
                topPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);

            g.setColour (waveCol.withAlpha (0.25f));
            g.strokePath (topPath, juce::PathStrokeType (3.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (waveCol.withAlpha (0.90f));
            g.strokePath (topPath, juce::PathStrokeType (1.3f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

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
    // --- Consume pending optimistic marker commit from processor (for MIDI/knob moves) ---
    int optIdx = processor.pendingUiOptimisticIdx.exchange(-1, std::memory_order_acq_rel);
    int optSample = -1;
    if (optIdx >= 0) {
        optSample = processor.pendingUiOptimisticSample.exchange(-1, std::memory_order_acq_rel);

        // Only set optimistic if different from snapshot
        const auto& uiSnap = processor.getUiSliceSnapshot();
        if (optIdx < (int)uiSnap.slices.size() && uiSnap.slices[(size_t)optIdx].startSample != optSample) {
            optimisticSliceIdx = optIdx;
            optimisticStartSample = optSample;
        } else {
            // It's already committed in the snapshot, so don't set
            optimisticSliceIdx = -1;
            optimisticStartSample = -1;
        }
    }

    // --- Clear optimistic state once snapshot reflects the real model ---
    if (optimisticSliceIdx >= 0) {
        const auto& snap = processor.getUiSliceSnapshot();
        if (optimisticSliceIdx < (int)snap.slices.size()) {
            const auto& optSlice = snap.slices[(size_t)optimisticSliceIdx];
            if (optSlice.startSample == optimisticStartSample || !optSlice.active) {
                optimisticSliceIdx = -1;
                optimisticStartSample = -1;
            }
        } else {
            optimisticSliceIdx = -1;
            optimisticStartSample = -1;
        }
    }

    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    int num = ui.numSlices;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t) i];
        if (! s.active) continue;

        int drawStartSample = s.startSample;
        if (i == optimisticSliceIdx && optimisticStartSample >= 0)
            drawStartSample = optimisticStartSample;

        int drawEndSample = processor.sliceManager.getEndForSlice(i, ui.sampleNumFrames);

        // Live preview during drag:
        if (i == sel && dragSliceIdx == i &&
            (dragMode == DragEdgeLeft || dragMode == MoveSlice))
        {
            drawStartSample = dragPreviewStart;
            drawEndSample = dragPreviewEnd;
        }
        else if (i == linkedSliceIdx && linkedSliceIdx >= 0 && dragMode != None)
        {
            drawStartSample = linkedPreviewStart;
            drawEndSample   = linkedPreviewEnd;
        }
        else if (dragMode == None)
        {
            const int liveIdx = processor.liveDragSliceIdx.load (std::memory_order_acquire);
            if (liveIdx == i)
            {
                const int liveStart = processor.liveDragBoundsStart.load (std::memory_order_relaxed);
                const int liveEnd   = processor.liveDragBoundsEnd.load   (std::memory_order_relaxed);
                if (liveStart >= 0 && liveEnd > liveStart && liveEnd <= ui.sampleNumFrames)
                {
                    drawStartSample = liveStart;
                    drawEndSample   = liveEnd;
                }
            }
        }

        int x1 = std::max (0, sampleToPixel (drawStartSample));
        int x2 = std::min (getWidth(), sampleToPixel (drawEndSample));
        int sw = x2 - x1;
        if (sw <= 0) continue;

        // --- CUBASE-STYLE SLICE OVERLAY ---
        // Soft colored region fill
        g.setColour(s.colour.withAlpha(0.11f));
        g.fillRect(x1, 0, sw, getHeight());

        // Strong colored borders: top & bottom
        g.setColour(s.colour.withAlpha(0.65f));
        g.drawHorizontalLine(0, (float)x1, (float)x2);                    // top
        g.drawHorizontalLine(getHeight() - 1, (float)x1, (float)x2);      // bottom

        // Bold colored vertical start marker
        g.setColour(s.colour.withAlpha(0.85f));
        g.fillRect(x1, 0, 2, getHeight());

        // (Optional) end marker: uncomment for Cubase-style region ending
        //g.setColour(s.colour.withAlpha(0.42f));
        //g.fillRect(x2 - 2, 0, 2, getHeight());

        // Selection highlight overlay
        if (i == sel) {
            g.setColour(s.colour.withAlpha(0.18f));
            g.fillRect(x1, 0, sw, getHeight());
        }

        // Optional: slice index label
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.setColour(s.colour.contrasting(0.8f));
        g.drawText(juce::String(i + 1), x1 + 4, 3, 16, 12, juce::Justification::left);
    }
}
}

void WaveformView::resized()
{
    prevCacheKey = {};
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap) {
        int totalFrames = sampleSnap->buffer.getNumSamples();
        trimInPoint  = juce::jlimit(0, totalFrames - 1, trimInPoint);
        trimOutPoint = juce::jlimit(trimInPoint + 1, totalFrames, trimOutPoint);
    }
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
    if (trimMode)
    {
        const int x1 = sampleToPixel (trimInPoint);
        const int x2 = sampleToPixel (trimOutPoint);
        if (std::abs (e.x - x1) < 8 || std::abs (e.x - x2) < 8)
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }
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
            if (std::abs (e.x - x1) < 6) newEdge = HoveredEdge::Left;
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
void WaveformView::mouseExit (const juce::MouseEvent&) { if (hoveredEdge != HoveredEdge::None) { hoveredEdge = HoveredEdge::None; repaint(); } }
void WaveformView::modifierKeysChanged (const juce::ModifierKeys& mods) { syncAltStateFromMods (mods); }

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
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

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    syncAltStateFromMods (e.mods);
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr) return;

    if (trimMode && trimDragging && (dragMode == DragTrimIn || dragMode == DragTrimOut))
    {
        int totalFrames = sampleSnap->buffer.getNumSamples();
        int minLen = 1;
        int newSample = juce::jlimit(0, totalFrames, pixelToSample(e.x));
        if (dragMode == DragTrimIn)
            trimInPoint = juce::jlimit(0, trimOutPoint - minLen, newSample);
        else if (dragMode == DragTrimOut)
            trimOutPoint = juce::jlimit(trimInPoint + minLen, totalFrames, newSample);
        repaint();
        return;
    }

    // Slice edge dragging (visual preview)
    if (dragMode == DragEdgeLeft && dragSliceIdx >= 0)
    {
        if (processor.snapToZeroCrossing.load())
            dragPreviewStart = AudioAnalysis::findNearestZeroCrossing (sampleSnap->buffer, pixelToSample(e.x));
        else
            dragPreviewStart = juce::jlimit(0, dragPreviewEnd - 64, pixelToSample(e.x));
        if (linkedSliceIdx >= 0)
            linkedPreviewEnd = dragPreviewStart;
        repaint();
    }
    // TODO: add MoveSlice/other modes if needed
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    // ---- TRIM MODE: commit new in/out points ----
    if (trimMode)
    {
        trimDragging = false;
        dragMode = None;

        // Only update internal trim points and visual state;
        // do NOT trigger 'onTrimApplied' here. Apply is done by 'APPLY' button.
        processor.trimRegionStart.store (trimInPoint, std::memory_order_relaxed);
        processor.trimRegionEnd.store   (trimOutPoint, std::memory_order_relaxed);
        repaint();
        return;
    }

    // ---- SLICE EDGE DRAG: commit marker move ----
    if ((dragMode == DragEdgeLeft || dragMode == MoveSlice) && dragSliceIdx >= 0)
    {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdSetSliceBounds;
        cmd.intParam1 = dragSliceIdx;
        cmd.intParam2 = dragPreviewStart;
        cmd.positions[0] = dragPreviewEnd;
        cmd.numPositions = 1;
        processor.pushCommand(cmd);

        // Also commit the linked adjacent slice so boundaries stay flush
        if (linkedSliceIdx >= 0)
        {
            DysektProcessor::Command lCmd;
            lCmd.type = DysektProcessor::CmdSetSliceBounds;
            lCmd.intParam1 = linkedSliceIdx;
            lCmd.intParam2 = linkedPreviewStart;
            lCmd.positions[0] = linkedPreviewEnd;
            lCmd.numPositions = 1;
            processor.pushCommand(lCmd);
        }

        // --- Optimistic update to prevent marker jump-back
        optimisticSliceIdx = dragSliceIdx;
        optimisticStartSample = dragPreviewStart;
    }

    // Deactivate live drag so the audio thread stops overriding slice bounds.
    processor.liveDragSliceIdx.store (-1, std::memory_order_release);

    // Clear drag state
    dragMode     = None;
    dragSliceIdx = -1;
    linkedSliceIdx = -1;
    trimDragging = false;
    dragPreviewStart = 0;
    dragPreviewEnd = 0;
    drawStartedFromAlt = false;
    repaint();
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
        float sc = processor.scroll.load();
        sc -= w.deltaY * 0.05f;
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, sc));
    }
    else
    {
        int width = getWidth();
        float oldZoom = processor.zoom.load();
        float oldViewFrac = 1.0f / oldZoom;
        float oldScroll = processor.scroll.load();
        float cursorPixelFrac = (width > 0) ? (float) e.x / (float) width : 0.5f;
        float newZoom = (w.deltaY > 0)
            ? std::min (16384.0f, oldZoom * 1.2f)
            : std::max (1.0f, oldZoom / 1.2f);
        processor.zoom.store (newZoom);
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
    prevCacheKey = {};
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

void WaveformView::enterTrimMode (int start, int end)
{
    trimMode     = true;
    trimStart    = start;
    trimEnd      = end;
    trimInPoint  = start;
    trimOutPoint = end;
    trimDragging = false;
    dragMode     = None;
    repaint();
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap) {
        int totalFrames = sampleSnap->buffer.getNumSamples();
        trimInPoint  = juce::jlimit(0, totalFrames - 1, trimInPoint);
        trimOutPoint = juce::jlimit(trimInPoint + 1, totalFrames, trimOutPoint);
    }
}

void WaveformView::setTrimPoints (int inPt, int outPt)
{
    trimInPoint  = inPt;
    trimOutPoint = outPt;
    repaint();
}

void WaveformView::setTrimMode (bool active)
{
    trimMode = active;
    if (! active)
    {
        dragMode = None;
    }
    repaint();
}
