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

// -- SNIP: All overlay, drawing, playback cursor, slice, etc. code unchanged from your previous working file --
// (see earlier posts for those full implementations if needed)

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

    // Middle-mouse drag: scroll+zoom
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

    // Trim mode: marker drag is set up here
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
        // (do not select slices or add slices in trim mode)
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

    // *** MPC/PC STYLE: Add slice with SINGLE CLICK ONLY ***
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

// Enable trim dragging (but not slice region drag)
void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
        return;

    int samplePos = std::max(0, std::min(pixelToSample(e.x), sampleSnap->buffer.getNumSamples()));

    // Only handle trim drag
    if (dragMode == DragTrimIn)
    {
        trimInPoint = juce::jlimit(0, trimOutPoint - 64, samplePos);
        processor.trimRegionStart.store(trimInPoint, std::memory_order_relaxed);
        repaint();
        return;
    }
    if (dragMode == DragTrimOut)
    {
        trimOutPoint = juce::jlimit(trimInPoint + 64, sampleSnap->buffer.getNumSamples(), samplePos);
        processor.trimRegionEnd.store(trimOutPoint, std::memory_order_relaxed);
        repaint();
        return;
    }
}

void WaveformView::mouseUp(const juce::MouseEvent&)
{
    // End any trim drag
    if (dragMode == DragTrimIn || dragMode == DragTrimOut)
    {
        dragMode = None;
        trimDragging = false;
    }
}

void WaveformView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) {}

// --- Required JUCE drag-n-drop and stubs ---
bool WaveformView::isInterestedInFileDrag(const juce::StringArray&) { return true; }
void WaveformView::filesDropped(const juce::StringArray&, int, int) {  }
void WaveformView::enterTrimMode(int start, int end) { trimMode = true; trimInPoint = trimStart = start; trimOutPoint = trimEnd = end; }
void WaveformView::setTrimPoints(int inPt, int outPt) { trimInPoint = inPt; trimOutPoint = outPt; }
void WaveformView::exitTrimMode() { trimMode = false; }
void WaveformView::getTrimBounds(int& outStart, int& outEnd) const { outStart = trimInPoint; outEnd = trimOutPoint; }
void WaveformView::setTrimMode(bool active) { trimMode = active; }
void WaveformView::resetTrim() { trimInPoint = trimStart; trimOutPoint = trimEnd; }