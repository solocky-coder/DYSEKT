#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

WaveformView::WaveformView(DysektProcessor& p) : processor(p) {}

void WaveformView::setAddSliceActiveGetter(std::function<bool()> fn) { isAddSliceActive = std::move(fn); }

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
    // Your remaining mouseDown logic goes here (if any)
}

void WaveformView::mouseDrag(const juce::MouseEvent&) { }
void WaveformView::mouseUp(const juce::MouseEvent&) { }
void WaveformView::mouseMove(const juce::MouseEvent&) { }
void WaveformView::mouseEnter(const juce::MouseEvent&) { }
void WaveformView::mouseExit(const juce::MouseEvent&) { }
void WaveformView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) { }
void WaveformView::modifierKeysChanged(const juce::ModifierKeys&) { }

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

void WaveformView::rebuildCacheIfNeeded() { }
bool WaveformView::hasActiveSlicePreview() const noexcept { return false; }
bool WaveformView::getActiveSlicePreview(int&, int&, int&) const { return false; }
bool WaveformView::getLinkedSlicePreview(int&, int&, int&) const { return false; }
bool WaveformView::isInteracting() const noexcept { return false; }
void WaveformView::setSliceDrawMode(bool) { }
void WaveformView::setTrimMode(bool) { }
void WaveformView::resetTrim() { }
void WaveformView::exitTrimMode() { }
void WaveformView::getTrimBounds(int&, int&) const { }
void WaveformView::enterTrimMode(int, int) { }
void WaveformView::setTrimPoints(int, int) { }
int WaveformView::pixelToSample(int) const { return 0; }
int WaveformView::sampleToPixel(int) const { return 0; }
WaveformView::ViewState WaveformView::buildViewState(const SampleData::SnapshotPtr&) const { return ViewState{}; }
void WaveformView::syncAltStateFromMods(const juce::ModifierKeys&) { }
void WaveformView::drawWaveform(juce::Graphics&) { }
void WaveformView::drawSlices(juce::Graphics&) { }
void WaveformView::drawPlaybackCursors(juce::Graphics&) { }
void WaveformView::paintDrawSlicePreview(juce::Graphics&) { }
void WaveformView::paintLazyChopOverlay(juce::Graphics&) { }
void WaveformView::paintTransientMarkers(juce::Graphics&) { }
void WaveformView::paintTrimOverlay(juce::Graphics&) { }