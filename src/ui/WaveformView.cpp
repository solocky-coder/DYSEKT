#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

WaveformView::WaveformView (DysektAudioProcessor& p) : processor (p) {}

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
    const int cy = getHeight() / 2;
    const float scale = (float) getHeight() * 0.9f;

    auto& peaks = cache.getPeaks();
    const int numPeaks = std::min (cache.getNumPeaks(), getWidth());
    if (numPeaks <= 0)
        return;

    juce::Path fillPath;
    fillPath.startNewSubPath (0.0f, (float) cy - peaks.maxVal * scale);
    for (int px = 1; px < numPeaks; ++px)
        fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);
    for (int px = numPeaks - 1; px >= 0; --px)
        fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].minVal * scale);
    fillPath.closeSubPath();

    g.setColour (getTheme().waveform);
    g.fillPath (fillPath);
}

void WaveformView::drawSlices (juce::Graphics& g) 
{
    // Implementation for slice drawing
}

void WaveformView::drawPlaybackCursors (juce::Graphics& g) 
{
    // Implementation for cursor drawing
}
