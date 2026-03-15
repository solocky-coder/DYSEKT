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

    const int x1 = juce::jlimit(0, w, sampleToPixel (clampedTrimIn));
    const int x2 = juce::jlimit(0, w, sampleToPixel (clampedTrimOut));
    const auto ac = getTheme().accent;

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    if (x1 > 0) g.fillRect (0, 0, x1, h);
    if (x2 < w) g.fillRect (x2, 0, w - x2, h);

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

// [SNIP] drawWaveform, drawSlices, drawPlaybackCursors: unchanged -- provided above

// ...truncated for length. All logic matches the previously delivered working version
// with the only difference: mouseUp DOES NOT call onTrimApplied or setTrimMode(false) in trim mode!

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    // ---- TRIM MODE: only stop dragging ----
    if (trimMode)
    {
        trimDragging = false;
        dragMode = None;
        // Do NOT fire onTrimApplied nor close trim mode here!
        repaint();
        return;
    }

    // ---- SLICE EDGE DRAG: commit marker move ----
    if ((dragMode == DragEdgeLeft || dragMode == DragEdgeRight || dragMode == MoveSlice) && dragSliceIdx >= 0)
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
    }

    processor.liveDragSliceIdx.store (-1, std::memory_order_release);

    dragMode     = None;
    dragSliceIdx = -1;
    linkedSliceIdx = -1;
    trimDragging = false;
    dragPreviewStart = 0;
    dragPreviewEnd = 0;
    drawStartedFromAlt = false;
    repaint();
}

// ...[all other handlers unchanged]...

void WaveformView::setTrimMode (bool active)
{
    trimMode = active;
    if (! active)
        dragMode = None;
    repaint();
}
