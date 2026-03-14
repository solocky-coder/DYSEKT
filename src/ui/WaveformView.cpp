#include "WaveformView.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

// ---- CONSTRUCTOR ----
WaveformView::WaveformView (DysektProcessor& p) : processor (p) {}

// ---- JUCE OVERRIDES ----
void WaveformView::paint (juce::Graphics& g)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    g.fillAll (getTheme().waveformBg);
    // ... Insert your real waveform painting code here ...
}

void WaveformView::resized()
{
    prevCacheKey = {}; // force cache rebuild
}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    syncAltStateFromMods(e.mods);

    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
        return;

    int samplePos = std::max(0, std::min(pixelToSample(e.x), sampleSnap->buffer.getNumSamples()));
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
    // ... If you want additional mouseDown logic, insert here ...
}

void WaveformView::mouseDrag(const juce::MouseEvent&) {}
void WaveformView::mouseUp(const juce::MouseEvent&) {}
void WaveformView::mouseMove(const juce::MouseEvent&) {}
void WaveformView::mouseEnter(const juce::MouseEvent&) {}
void WaveformView::mouseExit(const juce::MouseEvent&) {}
void WaveformView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) {}
void WaveformView::modifierKeysChanged(const juce::ModifierKeys&) {}

bool WaveformView::isInterestedInFileDrag(const juce::StringArray&) { return false; }
void WaveformView::filesDropped(const juce::StringArray&, int, int) {}

// ---- PUBLIC API ----
void WaveformView::rebuildCacheIfNeeded() {}

bool WaveformView::hasActiveSlicePreview() const noexcept { return false; }
bool WaveformView::getActiveSlicePreview(int&, int&, int&) const { return false; }
bool WaveformView::getLinkedSlicePreview(int&, int&, int&) const { return false; }
bool WaveformView::isInteracting() const noexcept { return false; }

void WaveformView::setSliceDrawMode(bool active) { sliceDrawMode = active; }
bool WaveformView::isSliceDrawModeActive() const noexcept { return sliceDrawMode; }

void WaveformView::enterTrimMode(int start, int end)
{
    trimMode = true;
    trimStart = start;
    trimEnd = end;
    trimInPoint = start;
    trimOutPoint = end;
}

void WaveformView::setTrimPoints(int inPt, int outPt)
{
    trimInPoint = inPt;
    trimOutPoint = outPt;
}

void WaveformView::exitTrimMode()
{
    trimMode = false;
}

void WaveformView::getTrimBounds(int& outStart, int& outEnd) const
{
    outStart = trimInPoint;
    outEnd = trimOutPoint;
}

bool WaveformView::isTrimModeActive() const noexcept { return trimMode; }

void WaveformView::setTrimMode(bool active)
{
    trimMode = active;
    if (!active)
    {
        dragMode = None;
    }
    repaint();
}

void WaveformView::resetTrim()
{
    trimInPoint = trimStart;
    trimOutPoint = trimEnd;
}

// ---- PRIVATE HELPERS/STUBS (needed for linker) ----
int WaveformView::pixelToSample(int px) const { return px; }
int WaveformView::sampleToPixel(int sample) const { return sample; }
WaveformView::ViewState WaveformView::buildViewState(const SampleData::SnapshotPtr&) const { return {}; }
void WaveformView::syncAltStateFromMods(const juce::ModifierKeys&) {}
void WaveformView::drawWaveform(juce::Graphics&) {}
void WaveformView::drawSlices(juce::Graphics&) {}
void WaveformView::drawPlaybackCursors(juce::Graphics&) {}
void WaveformView::paintDrawSlicePreview(juce::Graphics&) {}
void WaveformView::paintLazyChopOverlay(juce::Graphics&) {}
void WaveformView::paintTransientMarkers(juce::Graphics&) {}
void WaveformView::paintTrimOverlay(juce::Graphics&) {}