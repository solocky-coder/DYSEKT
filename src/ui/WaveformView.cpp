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
        paintTrimOverlay (g);
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

// ...[truncated for brevity: the rest of your original code stays unchanged up to...]

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    // --- ADD SLICE ON CLICK SUPPORT ---
    if (isAddSliceActive && isAddSliceActive())
    {
        int markerSample = pixelToSample(e.x);

        // Default slice region length (adjust as desired)
        int defaultLength = 2048; // Or set to taste
        int sliceEnd = markerSample + defaultLength;
        auto sampleSnap = processor.sampleData.getSnapshot();
        if (sampleSnap != nullptr)
            sliceEnd = std::min(sliceEnd, sampleSnap->buffer.getNumSamples());

        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdCreateSlice;
        cmd.intParam1 = markerSample;
        cmd.intParam2 = sliceEnd;
        processor.pushCommand(cmd);

        repaint();
        return;
    }
    // --- END ADD SLICE ON CLICK ---

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

    // ...[rest of your original mouseDown implementation continues unchanged]...
}

// ...[the remainder of your file stays as is: nothing else is changed]...