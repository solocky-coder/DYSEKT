#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

WaveformView::WaveformView(DysektProcessor& p) : processor(p) {}

// ----- paint -----
void WaveformView::paint(juce::Graphics& g)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    g.fillAll(getTheme().waveformBg);

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
        paintDrawSlicePreview(g);
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

void WaveformView::resized() { prevCacheKey = {}; }

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    // --- ADD SLICE ON CLICK SUPPORT ---
    if (isAddSliceActive && isAddSliceActive())
    {
        int markerSample = pixelToSample(e.x);
        int defaultLength = 2048;
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

    syncAltStateFromMods(e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
        return;

    // ... [RE-INSERT your full original mouseDown event logic here if needed] ...
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    // ... [your real mouseDrag implementation here] ...
}

void WaveformView::mouseUp(const juce::MouseEvent& e)
{
    // ... [your real mouseUp implementation here] ...
}

void WaveformView::mouseMove(const juce::MouseEvent& e)
{
    // ... [your real mouseMove implementation here] ...
}

void WaveformView::mouseEnter(const juce::MouseEvent& e) { mouseMove(e); }
void WaveformView::mouseExit(const juce::MouseEvent&) { /* ... */ }
void WaveformView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // ... [your real mouseWheelMove implementation here] ...
}
void WaveformView::modifierKeysChanged(const juce::ModifierKeys& mods) { /* ... */ }

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
    auto ext = f.getFileExtension().toLowerCase();

    processor.zoom.store(1.0f);
    processor.scroll.store(0.0f);
    prevCacheKey = {};

    if (ext == ".sf2" || ext == ".sfz")
        processor.loadSoundFontAsync(f);
    else
        processor.loadFileAsync(f);
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

    cache.rebuild(
        sampleSnap->buffer, sampleSnap->peakMipmaps,
        view.numFrames, processor.zoom.load(), processor.scroll.load(), view.width);
    prevCacheKey = key;
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
        return cachedPaintViewState.visibleStart
            + (int)((float)px / (float)cachedPaintViewState.width * cachedPaintViewState.visibleLen);
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

// --- All the rest of your detailed, real paint/draw/interaction methods go here:

void WaveformView::drawWaveform(juce::Graphics& g)
{
    // [Insert your actual waveform drawing logic here]
}

void WaveformView::drawSlices(juce::Graphics& g)
{
    // [Insert your actual slice drawing logic here]
}

void WaveformView::paintDrawSlicePreview(juce::Graphics& g)
{
    // [Insert your actual slice preview drawing logic here]
}

void WaveformView::paintLazyChopOverlay(juce::Graphics& g)
{
    // [Insert your actual lazy chop overlay logic here]
}

void WaveformView::paintTransientMarkers(juce::Graphics& g)
{
    // [Insert your actual transient marker drawing logic here]
}

void WaveformView::paintTrimOverlay(juce::Graphics& g)
{
    // [Insert your actual trim overlay logic here]
}

void WaveformView::drawPlaybackCursors(juce::Graphics& g)
{
    // [Insert your playback drawing/cursor painting logic here]
}

bool WaveformView::hasActiveSlicePreview() const noexcept { 
    // [Your actual logic]
    return false;
}
bool WaveformView::getActiveSlicePreview(int&, int&, int&) const { /*...*/ return false; }
bool WaveformView::getLinkedSlicePreview(int&, int&, int&) const { /*...*/ return false; }
bool WaveformView::isInteracting() const noexcept { /*...*/ return false; }
void WaveformView::setSliceDrawMode(bool) { /*...*/ }
void WaveformView::setTrimMode(bool) { /*...*/ }
void WaveformView::resetTrim() { /*...*/ }
void WaveformView::exitTrimMode() { /*...*/ }
void WaveformView::getTrimBounds(int&, int&) const { /*...*/ }
void WaveformView::enterTrimMode(int, int) { /*...*/ }
void WaveformView::setTrimPoints(int, int) { /*...*/ }
void WaveformView::syncAltStateFromMods(const juce::ModifierKeys&) { /*...*/ }