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
    const WaveformCache::CacheKey key{view.visibleStart, view.visibleLen, view.width, view.numFrames, sampleSnap.get()};
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
        // Draw overlays, playback cursors, etc. as in your functional build
        // e.g.: paintDrawSlicePreview(g); paintLazyChopOverlay(g); paintTransientMarkers(g); paintTrimOverlay(g); drawPlaybackCursors(g); 
        paintViewStateActive = false;
    } else {
        paintViewStateActive = false;
        g.setColour (getTheme().foreground.withAlpha (0.25f));
        g.setFont (DysektLookAndFeel::makeFont (22.0f));
        g.drawText ("DROP AUDIO FILE", getLocalBounds(), juce::Justification::centred);
    }
}

// ---- Draw waveform data ----
void WaveformView::drawWaveform(juce::Graphics& g)
{
    const int cy = getHeight() / 2;
    const float scale = (float) getHeight() * 0.48f;
    auto& peaks = cache.getPeaks();
    const int numPeaks = std::min (cache.getNumPeaks(), getWidth());
    if (numPeaks <= 0) return;

    juce::Path fillPath;
    fillPath.startNewSubPath (0.0f, (float) cy - peaks[0].maxVal * scale);
    for (int px = 1; px < numPeaks; ++px)
        fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);
    for (int px = numPeaks-1; px >= 0; --px)
        fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].minVal * scale);
    fillPath.closeSubPath();

    g.setColour (getTheme().waveform);
    g.fillPath (fillPath);
}

// ---- Draw slice overlays/markers ----
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
        int x1 = std::max(0, sampleToPixel(drawStartSample));
        int x2 = std::min(getWidth(), sampleToPixel(drawEndSample));
        if (i == sel) {
            g.setColour (getTheme().selectionOverlay.withAlpha (0.22f));
            g.fillRect (x1, 0, x2-x1, getHeight());
            g.setColour (getTheme().foreground.withAlpha (0.7f));
            g.drawVerticalLine (x1, 0.0f, (float) getHeight());
            g.drawVerticalLine (x2-1, 0.0f, (float) getHeight());
        }
        else {
            g.setColour (s.colour.withAlpha (0.30f));
            g.drawVerticalLine (x1, 0.0f, (float) getHeight());
            g.drawVerticalLine (x2-1, 0.0f, (float) getHeight());
        }
    }
}

// ---- Draw playback heads/cursors ----
void WaveformView::drawPlaybackCursors(juce::Graphics& g)
{
    for (int i = 0; i < VoicePool::kMaxVoices; ++i)
    {
        float pos = processor.voicePool.voicePositions[i].load (std::memory_order_relaxed);
        if (pos > 0.0f) {
            int px = sampleToPixel((int)pos);
            if (px >= 0 && px < getWidth()) {
                g.setColour (getTheme().accent.withAlpha (i == 0 ? 1.0f : 0.6f));
                g.drawVerticalLine(px, 0.0f, (float) getHeight());
            }
        }
    }
}

// ---- Trim mode API ----
void WaveformView::enterTrimMode(int start, int end)
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
        trimInPoint  = juce::jlimit(0, totalFrames-1, trimInPoint);
        trimOutPoint = juce::jlimit(trimInPoint+1, totalFrames, trimOutPoint);
    }
}
void WaveformView::setTrimPoints(int inPt, int outPt)
{
    trimInPoint  = inPt;
    trimOutPoint = outPt;
    repaint();
}
void WaveformView::exitTrimMode()
{
    setTrimMode(false);
}
void WaveformView::getTrimBounds(int& outStart, int& outEnd) const
{
    outStart = trimInPoint;
    outEnd   = trimOutPoint;
}
void WaveformView::setTrimMode(bool active)
{
    trimMode = active;
    if (!active)
        dragMode = None;
    repaint();
}
void WaveformView::resetTrim()
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap) {
        int totalFrames = sampleSnap->buffer.getNumSamples();
        trimInPoint = 0;
        trimOutPoint = totalFrames;
    }
    repaint();
}

// ---- File drag/drop (JUCE FileDragAndDropTarget) ----
bool WaveformView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav"  || ext == ".ogg"  || ext == ".aif"  ||
            ext == ".aiff" || ext == ".flac" || ext == ".mp3"  ||
            ext == ".sf2"  || ext == ".sfz")
            return true;
    }
    return false;
}

void WaveformView::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    juce::File f(files[0]);
    if (onLoadRequest)
        onLoadRequest(f);
}

// ---- All JUCE event handlers ----
void WaveformView::resized() {}
void WaveformView::mouseDown(const juce::MouseEvent& e) {}
void WaveformView::mouseDrag(const juce::MouseEvent& e) {}
void WaveformView::mouseUp(const juce::MouseEvent& e) {}
void WaveformView::mouseMove(const juce::MouseEvent& e) {}
void WaveformView::mouseEnter(const juce::MouseEvent& e) {}
void WaveformView::mouseExit(const juce::MouseEvent& e) {}
void WaveformView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {}
void WaveformView::modifierKeysChanged(const juce::ModifierKeys& mods) {}