#include "SliceControlBar.h"
#include <cmath>
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/GrainEngine.h"
#include "../PluginEditor.h"

// ── Fixed ADSR knob colours — match LCD node colours, theme-independent ───────
static const juce::Colour kAdsrAttack  { 0xFF00FF87 };   // Toxic Lime
static const juce::Colour kAdsrDecay   { 0xFFFFE800 };   // Radioactive Yellow
static const juce::Colour kAdsrSustain { 0xFF00C8FF };   // Ice Blue
static const juce::Colour kAdsrRelease { 0xFFFF6B00 };   // Molten Orange

static juce::Colour adsrTintForField (int fieldId)
{
    using F = DysektProcessor;
    if (fieldId == F::FieldAttack)  return kAdsrAttack;
    if (fieldId == F::FieldDecay)   return kAdsrDecay;
    if (fieldId == F::FieldSustain) return kAdsrSustain;
    if (fieldId == F::FieldRelease) return kAdsrRelease;
    return {};  // invalid = use theme default
}

namespace
{
constexpr int kParamCellTextX    = 14;
constexpr int kParamCellTextWidth = 60;
constexpr int kParamCellWidth    = kParamCellTextX + kParamCellTextWidth;

constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kKnobEnd   = juce::MathConstants<float>::pi * 2.75f;
static constexpr int kKnobR = 7;   // knob radius (px)
}

SliceControlBar::SliceControlBar (DysektProcessor& p) : processor (p) {}
void SliceControlBar::resized() {}

// ... (other functions - drawKnobCell, drawPanSliderCell, etc.)

void SliceControlBar::drawMarkerSliderCell(juce::Graphics& g, int x, int y,
                                           int markerSample, int sampleNumFrames,
                                           bool locked, int& outWidth)
{
    const int cellW = kParamCellWidth;
    const int cellH = 32;
    const auto& theme = getTheme();
    const auto ac = theme.accent;

    const float norm = sampleNumFrames > 0 ?
        juce::jlimit(0.f, 1.f, (float)markerSample / (float)sampleNumFrames) : 0.f;

    const int trackY = y + 18;
    const int trackH = 6;
    const int trackX = x + 2;
    const int trackW = cellW - 4;
    const int thumbX = trackX + (int)(norm * (float)trackW);

    // Highlight if MIDI learn armed/mapped
    const bool armed  = (processor.midiLearn.getArmedSlot() == DysektProcessor::FieldSliceStart);
    const bool mapped = processor.midiLearn.isMapped(DysektProcessor::FieldSliceStart);

    if (armed)
    {
        g.setColour(ac.withAlpha(0.11f));
        g.fillRoundedRectangle((float)x, (float)y, (float)cellW, (float)cellH, 2.f);
    }

    // Label
    g.setFont(DysektLookAndFeel::makeFont(10.0f));
    g.setColour(locked ? theme.lockActive.withAlpha(0.8f)
                       : theme.foreground.withAlpha(0.42f));
    g.drawText("MARKER", x, y + 2, cellW, 12, juce::Justification::centredLeft);

    // MIDI-learn label
    if (armed || mapped)
    {
        g.setFont(DysektLookAndFeel::makeFont(8.0f));
        g.setColour(ac.withAlpha(armed ? 1.0f : 0.65f));
        g.drawText(armed ? "ARM" : processor.midiLearn.getLabelText(DysektProcessor::FieldSliceStart),
                   x, y + cellH - 10, cellW, 10, juce::Justification::centredLeft);
    }

    // Value text centered above track
    g.setFont(DysektLookAndFeel::makeMonoFont(10.0f));
    g.setColour(locked ? theme.foreground : theme.foreground.withAlpha(0.75f));
    g.drawText(juce::String(markerSample), trackX, trackY - 11, trackW, 10, juce::Justification::centred);

    // Track background
    g.setColour(theme.darkBar.darker(0.3f));
    g.fillRoundedRectangle((float)trackX, (float)trackY, (float)trackW, (float)trackH, 2.f);

    // Active portion fill
    g.setColour(locked ? theme.lockActive : ac.withAlpha(0.40f));
    if (thumbX > trackX)
        g.fillRect(trackX, trackY + 1, thumbX - trackX, trackH - 2);

    // Thumb
    g.setColour(locked ? theme.lockActive : (armed ? ac.brighter(0.4f) : ac));
    g.fillRoundedRectangle((float)(thumbX - 2), (float)(trackY - 1),
                          4.f, (float)(trackH + 2), 1.5f);

    outWidth = cellW;
    ParamCell c{};
    c.x = x; c.y = y; c.w = cellW; c.h = cellH;
    c.lockBit = 0;
    c.fieldId = DysektProcessor::FieldSliceStart;
    c.minVal = 0.f;
    c.maxVal = (float)sampleNumFrames;
    c.step   = 1.f;
    c.isKnob = true; // Keeps drag logic unchanged
    c.isMidiLearnable = true;
    c.knobNorm = norm;
    cells.push_back(c);
}

// ... (rest of necessary functions) ...

void SliceControlBar::paint (juce::Graphics& g)
{
    cells.clear();
    const auto& ui = processor.getUiSliceSnapshot();
    int idx       = ui.selectedSlice;
    int numSlices = ui.numSlices;
    int rightEdge = getWidth() - 8;
    int row1y = 2, row2y = 36;
    rootNoteArea = {};

    if (idx < 0 || idx >= numSlices)
    {
        g.setFont (DysektLookAndFeel::makeFont (15.0f));
        g.setColour (getTheme().foreground.withAlpha (0.35f));
        g.drawText ("No slice selected", 8, 24, 220, 18, juce::Justification::centredLeft);
        return;
    }
    int x = 8; // layout offset

    // ... The rest of paint() as per your UI design and layout ...

    // MARKER
    {
        g.setColour (getTheme().separator.withAlpha (0.5f));
        g.drawVerticalLine (x + 2, (float) row1y + 4, (float) row1y + 28);
        x += 8;
        const int liveIdx = processor.liveDragSliceIdx.load (std::memory_order_acquire);
        const int markerSample = (liveIdx == idx)
            ? processor.liveDragBoundsStart.load (std::memory_order_relaxed)
            : ui.slices[(size_t)juce::jmax(0, idx)].startSample;
        int cw;
        drawMarkerSliderCell(g, x, row1y, markerSample, ui.sampleNumFrames, false, cw);
        x += cw + 4;
    }
    // ... Other controls ...
}

// =========== MOUSE EVENT HANDLERS ===========

void SliceControlBar::mouseDown(const juce::MouseEvent& e)
{
    // Original logic as in your previous version
    // ... (inserted code for parameter cell detection, drag handling, etc.)
}

void SliceControlBar::mouseDrag(const juce::MouseEvent& e)
{
    // Original logic as in your previous version
    // ... (inserted code for gesture updating, drag handling, etc.)
}

void SliceControlBar::mouseUp(const juce::MouseEvent& e)
{
    // Original logic as in your previous version
    // ... (inserted code for gesture end, drag end, etc.)
}

void SliceControlBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Original logic as in your previous version
    // ... (inserted code for double click detection, text edit show, etc.)
}