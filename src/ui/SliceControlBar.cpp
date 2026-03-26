#include "SliceControlBar.h"

// Synthetic field ID for the GLIDE cell — not in DysektProcessor::SliceParamField enum
// because it maps directly to VoicePool::legatoGlideMs (global, not per-slice).
static constexpr int kFieldGlide = 9998;
#include <juce_gui_basics/juce_gui_basics.h>
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/GrainEngine.h"
#include "../PluginEditor.h"

// ── Fixed ADSR knob colours — match LCD node colours, theme-independent ───────
static const juce::Colour kAdsrAttack { 0xFF00FF87 }; // Toxic Lime
static const juce::Colour kAdsrDecay { 0xFFFFE800 }; // Radioactive Yellow
static const juce::Colour kAdsrSustain { 0xFF00C8FF }; // Ice Blue
static const juce::Colour kAdsrRelease { 0xFFFF6B00 }; // Molten Orange

static juce::Colour adsrTintForField (int fieldId)
{
 using F = DysektProcessor;
 if (fieldId == F::FieldAttack) return kAdsrAttack;
 if (fieldId == F::FieldDecay) return kAdsrDecay;
 if (fieldId == F::FieldSustain) return kAdsrSustain;
 if (fieldId == F::FieldRelease) return kAdsrRelease;
 return {}; // invalid = use theme default
}

namespace
{
constexpr int kParamCellTextX = 14;
constexpr int kParamCellTextWidth = 60;
constexpr int kParamCellWidth = kParamCellTextX + kParamCellTextWidth;

constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kKnobEnd = juce::MathConstants<float>::pi * 2.75f;
}

SliceControlBar::SliceControlBar (DysektProcessor& p) : processor (p) {}
void SliceControlBar::resized() {}

// =============================================================================
// drawLockIcon (unchanged)
// =============================================================================
void SliceControlBar::drawLockIcon (juce::Graphics& g, int x, int y, bool locked)
{
 if (locked)
 {
 g.setColour (getTheme().lockActive);
 g.fillRect (x, y, 10, 10);
 }
 else
 {
 g.setColour (getTheme().lockInactive.withAlpha (0.6f));
 g.drawRect (x, y, 10, 10, 1);
 }
}

// =============================================================================
// drawParamCell (unchanged — booleans / choices only)
// =============================================================================
void SliceControlBar::drawParamCell (juce::Graphics& g, int x, int y,
 const juce::String& label, const juce::String& value,
 bool locked, uint32_t lockBit,
 int fieldId, float minVal, float maxVal, float step,
 bool isBoolean, bool isChoice, int& outWidth)
{
 const int cellH = 32;
 const int cellW = kParamCellWidth;

 g.setFont (DysektLookAndFeel::makeFont (12.0f));
 g.setColour (locked ? getTheme().lockActive.withAlpha (0.8f)
 : getTheme().foreground.withAlpha (0.45f));
 g.drawText (label, x + kParamCellTextX, y + 2, kParamCellTextWidth, 13, juce::Justification::centredLeft);

 g.setFont (DysektLookAndFeel::makeMonoFont (13.0f));
 g.setColour (locked ? getTheme().foreground
 : getTheme().foreground.withAlpha (0.4f));
 g.drawText (value, x + kParamCellTextX, y + 15, kParamCellTextWidth, 14, juce::Justification::centredLeft);

 outWidth = cellW;
 cells.push_back ({ x, y, outWidth, cellH, lockBit, fieldId,
 minVal, maxVal, step, isBoolean, isChoice, false, false });
}

// =============================================================================
// drawKnob — small rotary arc
// =============================================================================
void SliceControlBar::drawKnob (juce::Graphics& g,
 int cx, int cy, int r,
 float normVal,
 bool locked, bool armed, bool mapped,
 juce::Colour tintOverride)
{
 const float angle = kKnobStart + normVal * (kKnobEnd - kKnobStart);
 const float fcx = (float) cx, fcy = (float) cy, fr = (float) r;

 juce::Path track;
 track.addCentredArc (fcx, fcy, fr, fr, 0.f, kKnobStart, kKnobEnd, true);
 g.setColour (getTheme().darkBar.brighter (0.22f));
 g.strokePath (track, juce::PathStrokeType (1.5f,
 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

 // Base colour: fixed tint if provided, otherwise theme accent
 const juce::Colour base = tintOverride.isTransparent() ? getTheme().accent
 : tintOverride;

 // For ADSR knobs (tintOverride set), always keep the fixed colour — even when
 // locked. Lock state is shown via the lock icon; the colour identity must not
 // change to a theme colour.
 const bool hasTint = ! tintOverride.isTransparent();

 juce::Colour arcCol = armed ? base
 : mapped ? base.withAlpha (0.7f)
 : locked ? (hasTint ? base.withAlpha (0.75f)
 : getTheme().lockActive.withAlpha (0.85f))
 : base.withAlpha (0.55f);

 juce::Path arc;
 arc.addCentredArc (fcx, fcy, fr, fr, 0.f, kKnobStart, angle, true);
 g.setColour (arcCol);
 g.strokePath (arc, juce::PathStrokeType (2.2f,
 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

 // The arc uses JUCE's convention (0 = 12 o'clock, clockwise).
 // std::cos/sin uses standard maths (0 = 3 o'clock, counter-clockwise).
 // Offset by -π/2 so the indicator line aligns with the arc end.
 const float lineAngle = angle - juce::MathConstants<float>::halfPi;
 float lineR = fr - 2.5f;
 g.setColour (arcCol.brighter (0.15f));
 g.drawLine (fcx, fcy,
 fcx + lineR * std::cos (lineAngle),
 fcy + lineR * std::sin (lineAngle), 1.5f);

 g.setColour (locked ? getTheme().foreground.withAlpha (0.75f)
 : getTheme().foreground.withAlpha (0.25f));
 g.fillEllipse (fcx - 2.f, fcy - 2.f, 4.f, 4.f);
}

// =============================================================================
// toNorm — native value → 0-1 for knob arc
// =============================================================================
float SliceControlBar::toNorm (int fieldId, float v) const
{
 using F = DysektProcessor;
 switch (fieldId)
 {
 case F::FieldBpm: return juce::jlimit (0.f, 1.f, (v - 20.f) / (999.f - 20.f));
 case F::FieldPitch: return juce::jlimit (0.f, 1.f, (v + 48.f) / 96.f);
 case F::FieldCentsDetune: return juce::jlimit (0.f, 1.f, (v + 100.f) / 200.f);
 case F::FieldTonality: return juce::jlimit (0.f, 1.f, v / 8000.f);
 case F::FieldFormant: return juce::jlimit (0.f, 1.f, (v + 24.f) / 48.f);
 case F::FieldAttack: return juce::jlimit (0.f, 1.f, v / 1.f);
 case F::FieldDecay: return juce::jlimit (0.f, 1.f, v / 5.f);
 case F::FieldSustain: return juce::jlimit (0.f, 1.f, v);
 case F::FieldRelease: return juce::jlimit (0.f, 1.f, v / 5.f);
 case F::FieldMuteGroup: return juce::jlimit (0.f, 1.f, v / 32.f);
 case F::FieldMidiNote: return juce::jlimit (0.f, 1.f, v / 127.f);
 case F::FieldVolume: return juce::jlimit (0.f, 1.f, (v + 100.f) / 124.f);
 case F::FieldOutputBus: return juce::jlimit (0.f, 1.f, v / 15.f);
 case F::FieldPan: return juce::jlimit (0.f, 1.f, (v + 1.f) * 0.5f);
 case F::FieldFilterCutoff: return juce::jlimit (0.f, 1.f,
 (std::log2 (juce::jmax (20.f, v) / 20.f) / std::log2 (20000.f / 20.f)));
 case F::FieldFilterRes: return juce::jlimit (0.f, 1.f, v);
    case kFieldGlide:       return juce::jlimit (0.f, 1.f, v / 200.f);
 default: return 0.5f;
 }
}

// =============================================================================
// drawKnobCell — rotary knob cell for all numeric params
// =============================================================================
void SliceControlBar::drawKnobCell (juce::Graphics& g, int x, int y,
 const juce::String& label,
 const juce::String& valueText,
 float normVal,
 bool locked, uint32_t lockBit,
 int fieldId,
 float minVal, float maxVal, float step,
 int& outWidth)
{
 const int cellW = kParamCellWidth;
 const int cellH = 32;
 const int knobCX = x + kKnobR + 3;
 const int knobCY = y + cellH / 2;

 const bool armed = (processor.midiLearn.getArmedSlot() == fieldId);
 const bool mapped = processor.midiLearn.isMapped (fieldId);

 if (armed)
 {
 g.setColour (getTheme().accent.withAlpha (0.11f));
 g.fillRoundedRectangle ((float) x, (float) y, (float) cellW, (float) cellH, 2.f);
 }

 drawKnob (g, knobCX, knobCY, kKnobR, normVal, locked, armed, mapped,
 adsrTintForField (fieldId));

 if (armed || mapped)
 {
 g.setFont (DysektLookAndFeel::makeFont (8.0f));
 g.setColour (getTheme().accent.withAlpha (armed ? 1.0f : 0.65f));
 g.drawText (armed ? "ARM" : processor.midiLearn.getLabelText (fieldId),
 x, y + cellH - 10, kKnobR * 2 + 6, 10,
 juce::Justification::centred);
 }

 const int textX = knobCX + kKnobR + 4;
 const int textW = cellW - (textX - x) - 1;
 const juce::Colour adsr = adsrTintForField (fieldId);
 const bool hasAdsr = ! adsr.isTransparent();

 g.setFont (DysektLookAndFeel::makeFont (10.0f));
 // ADSR label always uses the fixed ADSR colour — even when locked.
 // Non-ADSR locked params use lockActive; everything else uses foreground.
 g.setColour (locked && ! hasAdsr ? getTheme().lockActive.withAlpha (0.8f)
 : hasAdsr ? adsr.withAlpha (0.70f)
 : getTheme().foreground.withAlpha (0.42f));
 g.drawText (label, textX, y + 2, textW, 12, juce::Justification::centredLeft);

 g.setFont (DysektLookAndFeel::makeMonoFont (11.0f));
 g.setColour (locked ? getTheme().foreground
 : getTheme().foreground.withAlpha (0.38f));
 g.drawText (valueText, textX, y + 14, textW, 14, juce::Justification::centredLeft);

 outWidth = cellW;

 ParamCell c{};
 c.x = x; c.y = y; c.w = cellW; c.h = cellH;
 c.lockBit = lockBit; c.fieldId = fieldId;
 c.minVal = minVal; c.maxVal = maxVal; c.step = step;
 c.isKnob = true; c.isMidiLearnable = true;
 c.knobNorm = normVal;
 cells.push_back (c);
}

// =============================================================================
// drawPanSliderCell — horizontal bipolar slider for PAN
// =============================================================================
void SliceControlBar::drawPanSliderCell (juce::Graphics& g, int x, int y,
 float panValue, // -1..+1
 bool locked, int& outWidth)
{
 const int cellW = kParamCellWidth;
 const int cellH = 32;
 const auto& theme = getTheme();
 const auto ac = theme.accent;

 const bool armed = (processor.midiLearn.getArmedSlot() == DysektProcessor::FieldPan);
 const bool mapped = processor.midiLearn.isMapped (DysektProcessor::FieldPan);

 if (armed)
 {
 g.setColour (ac.withAlpha (0.11f));
 g.fillRoundedRectangle ((float) x, (float) y, (float) cellW, (float) cellH, 2.f);
 }

 // ── Label ──────────────────────────────────────────────────────────────
 g.setFont (DysektLookAndFeel::makeFont (10.0f));
 g.setColour (locked ? theme.lockActive.withAlpha (0.8f)
 : theme.foreground.withAlpha (0.42f));
 g.drawText ("PAN", x, y + 2, cellW, 12, juce::Justification::centredLeft);

 // MIDI-learn label
 if (armed || mapped)
 {
 g.setFont (DysektLookAndFeel::makeFont (8.0f));
 g.setColour (ac.withAlpha (armed ? 1.0f : 0.65f));
 g.drawText (armed ? "ARM" : processor.midiLearn.getLabelText (DysektProcessor::FieldPan),
 x, y + cellH - 10, cellW, 10, juce::Justification::centredLeft);
 }

 // ── Slider track ───────────────────────────────────────────────────────
 const int trackY = y + 18;
 const int trackH = 6;
 const int trackX = x + 2;
 const int trackW = cellW - 4;

 // ── Value text — centred above the slider track ────────────────────────────
 const int pct = juce::jlimit (-100, 100, (int) std::round (panValue * 100.f));
 juce::String panStr = (pct == 0) ? "C"
 : (pct < 0) ? ("L" + juce::String (-pct))
 : ("R" + juce::String ( pct));
 g.setFont (DysektLookAndFeel::makeMonoFont (10.0f));
 g.setColour (locked ? theme.foreground : theme.foreground.withAlpha (0.75f));
 g.drawText (panStr, trackX, trackY - 11, trackW, 10, juce::Justification::centred);

 // Track background
 g.setColour (theme.darkBar.darker (0.3f));
 g.fillRoundedRectangle ((float) trackX, (float) trackY,
 (float) trackW, (float) trackH, 2.f);

 // Centre line
 const int centreX = trackX + trackW / 2;
 g.setColour (theme.foreground.withAlpha (0.18f));
 g.drawVerticalLine (centreX, (float) trackY, (float) (trackY + trackH));

 // Fill from centre toward current position
 const float norm = juce::jlimit (0.f, 1.f, (panValue + 1.f) * 0.5f);
 const int thumbX = trackX + (int) (norm * (float) trackW);
 const juce::Colour fillCol = locked ? theme.lockActive : ac;

 if (std::abs (panValue) > 0.005f)
 {
 const int fillX = (panValue < 0.f) ? thumbX : centreX;
 const int fillW = std::abs (thumbX - centreX);
 if (fillW > 0)
 {
 g.setColour (fillCol.withAlpha (locked ? 0.55f : 0.40f));
 g.fillRect (fillX, trackY + 1, fillW, trackH - 2);
 }
 }

 // Thumb
 g.setColour (locked ? theme.lockActive : (armed ? ac.brighter (0.4f) : ac));
 g.fillRoundedRectangle ((float) (thumbX - 2), (float) (trackY - 1),
 4.f, (float) (trackH + 2), 1.5f);

 // ── Register cell ──────────────────────────────────────────────────────
 outWidth = cellW;
 ParamCell c{};
 c.x = x; c.y = y; c.w = cellW; c.h = cellH;
 c.lockBit = kLockPan; c.fieldId = DysektProcessor::FieldPan;
 c.minVal = -1.f; c.maxVal = 1.f; c.step = 0.01f;
 c.isKnob = true; c.isMidiLearnable = true;
 c.knobNorm = norm;
 cells.push_back (c);
}


// =============================================================================
// drawMarkerSliderCell — flat LCD-style slider for MARKER (matches TrimDialog)
// =============================================================================
void SliceControlBar::drawMarkerSliderCell (juce::Graphics& g, int x, int y,
                                            int sampleVal, int totalFrames,
                                            int& outWidth)
{
    const int cellW = kParamCellWidth;
    const int cellH = 32;
    const auto& T = getTheme();
    auto cell = juce::Rectangle<int> (x, y, cellW, cellH);

    // Background + accent border — matches TrimDialog style
    g.setColour (T.darkBar);
    g.fillRoundedRectangle (cell.toFloat(), 3.0f);
    g.setColour (T.accent.withAlpha (0.55f));
    g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 3.0f, 1.0f);

    // Progress bar along bottom edge (shows marker position in file)
    const float frac = (totalFrames > 0)
        ? juce::jlimit (0.0f, 1.0f, (float) sampleVal / (float) totalFrames)
        : 0.0f;
    {
        auto bar = cell.removeFromBottom (3).toFloat();
        g.setColour (T.separator);
        g.fillRect (bar);
        g.setColour (T.accent);
        g.fillRect (bar.withWidth (bar.getWidth() * frac));
    }

    // Label — top half
    const int midY = cell.getY() + cell.getHeight() / 2;
    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (T.accent.withAlpha (0.65f));
    g.drawText ("MARKER",
                cell.getX(), cell.getY() + 2,
                cell.getWidth(), midY - cell.getY() - 2,
                juce::Justification::centred, false);

    // Value — bottom half
    g.setFont (DysektLookAndFeel::makeMonoFont (10.0f));
    g.setColour (T.foreground);
    g.drawText (juce::String (sampleVal),
                cell.getX(), midY,
                cell.getWidth(), cell.getBottom() - midY - 3,
                juce::Justification::centred, false);

    outWidth = cellW;

    ParamCell c{};
    c.x = x; c.y = y; c.w = cellW; c.h = cellH;
    c.lockBit = 0; c.fieldId = DysektProcessor::FieldSliceStart;
    c.minVal = 0.f; c.maxVal = 1.f; c.step = 0.001f;
    c.isKnob = true; c.isMidiLearnable = true;
    c.knobNorm = frac;
    cells.push_back (c);
}

// =============================================================================
// drawMidiLearnCell — START / END slice boundary buttons
// =============================================================================
void SliceControlBar::drawMidiLearnCell (juce::Graphics& g, int x, int y,
 const juce::String& label,
 int fieldId, int& outWidth)
{
 const int cellW = 52, cellH = 32;
 const bool armed = (processor.midiLearn.getArmedSlot() == fieldId);
 const bool mapped = processor.midiLearn.isMapped (fieldId);

 if (armed)
 {
 g.setColour (getTheme().accent.withAlpha (0.2f));
 g.fillRoundedRectangle ((float) x, (float) y, (float) cellW, (float) cellH, 3.f);
 }

 g.setColour (armed ? getTheme().accent
 : mapped ? getTheme().accent.withAlpha (0.48f)
 : getTheme().foreground.withAlpha (0.18f));
 g.drawRoundedRectangle ((float) x + 0.5f, (float) y + 0.5f,
 (float) cellW - 1.f, (float) cellH - 1.f, 3.f, 1.f);

 g.setFont (DysektLookAndFeel::makeFont (12.0f));
 g.setColour (armed ? getTheme().accent
 : mapped ? getTheme().foreground.withAlpha (0.65f)
 : getTheme().foreground.withAlpha (0.38f));
 g.drawText (label, x + 5, y + 2, cellW - 6, 13, juce::Justification::centredLeft);

 g.setFont (DysektLookAndFeel::makeMonoFont (13.0f));
 g.setColour (armed ? getTheme().accent
 : mapped ? getTheme().foreground
 : getTheme().foreground.withAlpha (0.28f));
 g.drawText (processor.midiLearn.getLabelText (fieldId),
 x + 5, y + 15, cellW - 6, 14, juce::Justification::centredLeft);

 outWidth = cellW;

 ParamCell c{};
 c.x = x; c.y = y; c.w = cellW; c.h = cellH;
 c.fieldId = fieldId;
 c.isMidiLearnBtn = true; c.isMidiLearnable = true;
 cells.push_back (c);
}

// =============================================================================
// showMidiLearnMenu
// =============================================================================
void SliceControlBar::showMidiLearnMenu (int fieldId, juce::Point<int> screenPos)
{
 const bool mapped = processor.midiLearn.isMapped (fieldId);
 juce::PopupMenu menu;
 menu.addItem (1, "Learn MIDI CC");
 if (mapped)
 menu.addItem (2, "Clear (" + processor.midiLearn.getLabelText (fieldId) + ")");

 menu.addSeparator();
 menu.addItem(1000, "Open MIDI Learn Dialog...");

 auto* topLvl = getTopLevelComponent();
 float ms = DysektLookAndFeel::getMenuScale();
 menu.showMenuAsync (
 juce::PopupMenu::Options()
 .withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1))
 .withParentComponent(topLvl)
 .withStandardItemHeight((int)(24 * ms)),
 [this, fieldId] (int result) {
 if (result == 1) { processor.midiLearn.armLearn (fieldId); repaint(); }
 else if (result == 2) { processor.midiLearn.clearMapping (fieldId); repaint(); }
 else if (result == 1000)
 {
 if (auto* editor = findParentComponentOfClass<DysektEditor>())
 editor->keyPressed(juce::KeyPress('M', juce::ModifierKeys::commandModifier, 0));
 }
 }
 );
}

// =============================================================================
// paint
// =============================================================================
void SliceControlBar::paint (juce::Graphics& g)
{
 // ── LCD-style frame — matches waveform + LCD screen aesthetic ────────────
 {
 const auto ac = getTheme().accent;
 auto b = getLocalBounds();

 juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
 juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
 g.setGradientFill (outerGrad);
 g.fillRoundedRectangle (b.toFloat(), 4.0f);

 g.setColour (ac.withAlpha (0.20f));
 g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

 auto screen = b.reduced (4);
 g.setColour (getTheme().darkBar.darker (0.55f));
 g.fillRoundedRectangle (screen.toFloat(), 2.0f);

 g.setColour (juce::Colours::black.withAlpha (0.18f));
 for (int y = screen.getY(); y < screen.getBottom(); y += 2)
 g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

 juce::ColourGradient glow (ac.withAlpha (0.06f), 0, (float) screen.getY(),
 juce::Colours::transparentBlack, 0, (float) (screen.getY() + 20), false);
 g.setGradientFill (glow);
 g.fillRoundedRectangle (screen.toFloat(), 2.0f);

 g.setColour (ac.withAlpha (0.12f));
 g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
 }

 cells.clear();

 const auto& ui = processor.getUiSliceSnapshot();
 int idx = ui.selectedSlice;
 int numSlices = ui.numSlices;
 int rightEdge = getWidth() - 8;
 int row1y = 2, row2y = 36;
 rootNoteArea = {}; // no longer drawn — LCD shows ROOT and SLICES

 if (idx < 0 || idx >= numSlices)
 {
 g.setFont (DysektLookAndFeel::makeFont (15.0f));
 g.setColour (getTheme().foreground.withAlpha (0.35f));
 g.drawText ("No slice selected", 8, 24, 220, 18, juce::Justification::centredLeft);
 return;
 }

 // Read live slice values directly from sliceManager — not the UI snapshot.
 // The snapshot lags by one processBlock cycle; sliceManager has the current
 // committed value which the smoother writes to immediately.
 const auto& s = (processor.sliceManager.getNumSlices() > idx && idx >= 0)
 ? processor.sliceManager.getSlice (idx)
 : ui.slices[(size_t) juce::jmax (0, idx)];

 float gBpm = processor.apvts.getRawParameterValue (ParamIds::defaultBpm)->load();
 float gPitch = processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();
 int gAlgo = (int) processor.apvts.getRawParameterValue (ParamIds::defaultAlgorithm)->load();
 float gAttack = processor.apvts.getRawParameterValue (ParamIds::defaultAttack)->load();
 float gDecay = processor.apvts.getRawParameterValue (ParamIds::defaultDecay)->load();
 float gSustain = processor.apvts.getRawParameterValue (ParamIds::defaultSustain)->load();
 float gRelease = processor.apvts.getRawParameterValue (ParamIds::defaultRelease)->load();
 int gMG = (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load();
 int gLoopMode = (int) processor.apvts.getRawParameterValue (ParamIds::defaultLoop)->load();
 bool gStretch = processor.apvts.getRawParameterValue (ParamIds::defaultStretchEnabled)->load() > 0.5f;

 bool algoLocked = (s.lockMask & kLockAlgorithm) != 0;
 int algoVal = algoLocked ? s.algorithm : gAlgo;
 bool stretchLocked = (s.lockMask & kLockStretch) != 0;
 bool stretchVal = stretchLocked ? s.stretchEnabled : gStretch;
 bool repitchStretch = (algoVal == 0) && stretchVal;

 int cw;
 using F = DysektProcessor;

 int filterGroupX1 = 0, filterGroupX2 = 0; // FCUT / FRES bracket

 // ── Row 1 right: slice index label ───────────────────────────────────────
 {
 g.setFont (DysektLookAndFeel::makeFont (12.0f));
 g.setColour (getTheme().accent.withAlpha (0.7f));
 g.drawText ("SLICE " + juce::String (idx + 1),
 8, row1y + 2, rightEdge - 8, 13, juce::Justification::right);
 // Sample range + length is shown in full on the LCD (ST / END / LEN rows)
 // so the duplicate read-only text here has been removed.
 }

 // ── Row 1 params ──────────────────────────────────────────────────
 int x = 8;

 // BPM — knob
 {
 bool locked = (s.lockMask & kLockBpm) != 0;
 float bpmVal = locked ? s.bpm : gBpm;
 juce::String bpmStr = juce::String (bpmVal, 2);
 if (bpmStr.contains ("."))
 {
 while (bpmStr.endsWith ("0")) bpmStr = bpmStr.dropLastCharacters (1);
 if (bpmStr.endsWith (".")) bpmStr = bpmStr.dropLastCharacters (1);
 }
 drawKnobCell (g, x, row1y, "BPM", bpmStr,
 toNorm (F::FieldBpm, bpmVal),
 locked, kLockBpm, F::FieldBpm, 20.f, 999.f, 0.01f, cw);
 x += cw + 4;
 }

 // PITCH — knob
 {
 bool locked = (s.lockMask & kLockPitch) != 0;
 float pv = locked ? s.pitchSemitones : gPitch;
 if (repitchStretch)
 {
 float daw = processor.dawBpm.load();
 float bpmVal = (s.lockMask & kLockBpm) ? s.bpm : gBpm;
 float semis = (daw > 0.f && bpmVal > 0.f)
 ? 12.f * std::log2 (daw / bpmVal) : 0.f;
 pv = (float) std::round (semis);
 }
 int pvi = (int) std::round (pv);
 drawKnobCell (g, x, row1y, "PITCH",
 (pvi >= 0 ? "+" : "") + juce::String (pvi) + "st",
 toNorm (F::FieldPitch, pv),
 locked, kLockPitch, F::FieldPitch, -48.f, 48.f, 0.1f, cw);
 if (repitchStretch) cells.back().isReadOnly = true;
 x += cw + 4;
 }

 // TUNE — knob
 {
 float gCents = processor.apvts.getRawParameterValue (ParamIds::defaultCentsDetune)->load();
 bool locked = (s.lockMask & kLockCentsDetune) != 0;
 float cv = locked ? s.centsDetune : gCents;
 if (repitchStretch)
 {
 float daw = processor.dawBpm.load();
 float bpmVal = (s.lockMask & kLockBpm) ? s.bpm : gBpm;
 float semis = (daw > 0.f && bpmVal > 0.f)
 ? 12.f * std::log2 (daw / bpmVal) : 0.f;
 int semisI = (int) std::round (semis);
 cv = (semis - (float) semisI) * 100.f;
 }
 int cvi = juce::jlimit (-100, 100, (int) std::round (cv));
 drawKnobCell (g, x, row1y, "TUNE",
 (cvi >= 0 ? "+" : "") + juce::String (cvi) + "ct",
 toNorm (F::FieldCentsDetune, cv),
 locked, kLockCentsDetune, F::FieldCentsDetune, -100.f, 100.f, 0.1f, cw);
 if (repitchStretch) cells.back().isReadOnly = true;
 x += cw + 4;
 }

 // ALGO — choice
 {
 juce::String algoNames[] = { "Repitch", "Stretch" };
 drawParamCell (g, x, row1y, "ALGO",
 algoNames[juce::jlimit (0, 1, algoVal)],
 algoLocked, kLockAlgorithm, F::FieldAlgorithm,
 0.f, 1.f, 1.f, false, true, cw);
 x += cw + 4;
 }

 if (algoVal == 1)
 {
 float gTonal = processor.apvts.getRawParameterValue (ParamIds::defaultTonality)->load();
 bool locked = (s.lockMask & kLockTonality) != 0;
 float tv = locked ? s.tonalityHz : gTonal;
 drawKnobCell (g, x, row1y, "TONAL",
 juce::String ((int) tv) + "Hz",
 toNorm (F::FieldTonality, tv),
 locked, kLockTonality, F::FieldTonality, 0.f, 8000.f, 100.f, cw);
 x += cw + 4;

 float gFmnt = processor.apvts.getRawParameterValue (ParamIds::defaultFormant)->load();
 locked = (s.lockMask & kLockFormant) != 0;
 float fv = locked ? s.formantSemitones : gFmnt;
 drawKnobCell (g, x, row1y, "FMNT",
 (fv >= 0.f ? "+" : "") + juce::String (fv, 1),
 toNorm (F::FieldFormant, fv),
 locked, kLockFormant, F::FieldFormant, -24.f, 24.f, 0.1f, cw);
 x += cw + 4;

 bool gFmntC = processor.apvts.getRawParameterValue (ParamIds::defaultFormantComp)->load() > 0.5f;
 locked = (s.lockMask & kLockFormantComp) != 0;
 bool fmntCVal = locked ? s.formantComp : gFmntC;
 drawParamCell (g, x, row1y, "FMNT C", fmntCVal ? "ON" : "OFF",
 locked, kLockFormantComp, F::FieldFormantComp,
 0.f, 1.f, 1.f, true, false, cw);
 x += cw + 4;
 }
 // STRETCH — boolean
 {
 bool locked = (s.lockMask & kLockStretch) != 0;
 bool sv = locked ? s.stretchEnabled : gStretch;
 drawParamCell (g, x, row1y, "STRETCH", sv ? "ON" : "OFF",
 locked, kLockStretch, F::FieldStretchEnabled,
 0.f, 1.f, 1.f, true, false, cw);
 x += cw + 4;
 }

 // MARKER — slice boundary knob in row 1
 // During MIDI CC movement, read from live drag atomics so the readout
 // tracks the encoder in real time instead of waiting for idle commit.
 {
 g.setColour (getTheme().separator.withAlpha (0.5f));
 g.drawVerticalLine (x + 2, (float) row1y + 4, (float) row1y + 28);
 x += 8;

 const int liveIdx = processor.liveDragSliceIdx.load (std::memory_order_acquire);
 const int markerSample = (liveIdx == idx)
 ? processor.liveDragBoundsStart.load (std::memory_order_relaxed)
 : s.startSample;
 drawMarkerSliderCell (g, x, row1y, markerSample, ui.sampleNumFrames, cw);
 x += cw + 4;
 }

 // GAIN, PAN, OUT — mix group in row 1
 {
 g.setColour (getTheme().separator.withAlpha (0.5f));
 g.drawVerticalLine (x + 2, (float) row1y + 4, (float) row1y + 28);
 x += 8;

 // GAIN
 {
 float gGainDb = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
 bool locked = (s.lockMask & kLockVolume) != 0;
 float gv = locked ? s.volume : gGainDb;
 drawKnobCell (g, x, row1y, "GAIN",
 (gv >= 0.f ? "+" : "") + juce::String (gv, 1) + "dB",
 toNorm (F::FieldVolume, gv),
 locked, kLockVolume, F::FieldVolume, -100.f, 24.f, 0.1f, cw);
 x += cw + 4;
 }

 // PAN
 {
 float gPanVal = processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();
 bool locked = (s.lockMask & kLockPan) != 0;
 float pv = locked ? s.pan : gPanVal;
 drawPanSliderCell (g, x, row1y, pv, locked, cw);
 x += cw + 4;
 }

 // OUT
 {
 bool locked = (s.lockMask & kLockOutputBus) != 0;
 int ov = locked ? s.outputBus : 0;
 const juce::String outLabel = (ov == 0) ? juce::String ("MAIN") : ("AUX " + juce::String (ov));
 drawParamCell (g, x, row1y, "OUT", outLabel,
 locked, kLockOutputBus, F::FieldOutputBus, 0.f, 15.f, 1.f,
 false, true, cw);
 x += cw + 4;
 }
 }
 g.setColour (getTheme().separator);
 g.drawHorizontalLine (34, 8.0f, (float) getWidth() - 8.0f);

 // ── Row 2 ─────────────────────────────────────────────────────────
 x = 8;
 int adsrGroupX1 = x, adsrGroupX2 = x;

 // ATK — knob (stored seconds, display ms)
 {
 adsrGroupX1 = x;
 bool locked = (s.lockMask & kLockAttack) != 0;
 float atk = locked ? s.attackSec : gAttack / 1000.f;
 drawKnobCell (g, x, row2y, "ATK",
 juce::String ((int) (atk * 1000.f)) + "ms",
 toNorm (F::FieldAttack, atk),
 locked, kLockAttack, F::FieldAttack, 0.f, 1.f, 0.001f, cw);
 x += cw + 4;
 }

 // DEC — knob
 {
 bool locked = (s.lockMask & kLockDecay) != 0;
 float dec = locked ? s.decaySec : gDecay / 1000.f;
 drawKnobCell (g, x, row2y, "DEC",
 juce::String ((int) (dec * 1000.f)) + "ms",
 toNorm (F::FieldDecay, dec),
 locked, kLockDecay, F::FieldDecay, 0.f, 5.f, 0.001f, cw);
 x += cw + 4;
 }

 // SUS — knob (stored 0-1, display %)
 {
 bool locked = (s.lockMask & kLockSustain) != 0;
 float sus = locked ? s.sustainLevel : gSustain / 100.f;
 drawKnobCell (g, x, row2y, "SUS",
 juce::String ((int) (sus * 100.f)) + "%",
 toNorm (F::FieldSustain, sus),
 locked, kLockSustain, F::FieldSustain, 0.f, 1.f, 0.01f, cw);
 x += cw + 4;
 }

 // REL — knob
 {
 bool locked = (s.lockMask & kLockRelease) != 0;
 float rel = locked ? s.releaseSec : gRelease / 1000.f;
 drawKnobCell (g, x, row2y, "REL",
 juce::String ((int) (rel * 1000.f)) + "ms",
 toNorm (F::FieldRelease, rel),
 locked, kLockRelease, F::FieldRelease, 0.f, 5.f, 0.001f, cw);
 x += cw + 4;
 adsrGroupX2 = x - 4;
 }

 // FCUT — filter cutoff knob (log-scaled, 20–20000 Hz)
 {
 filterGroupX1 = x;
 float gFCut = processor.apvts.getRawParameterValue (ParamIds::defaultFilterCutoff)->load();
 bool locked = (s.lockMask & kLockFilter) != 0;
 float fv = locked ? s.filterCutoff : gFCut;
 juce::String fStr = (fv >= 1000.f)
 ? (juce::String (fv / 1000.f, 1) + "k")
 : (juce::String ((int) fv) + "Hz");
 drawKnobCell (g, x, row2y, "FCUT", fStr,
 toNorm (F::FieldFilterCutoff, fv),
 locked, kLockFilter, F::FieldFilterCutoff,
 20.f, 20000.f, 1.f, cw);
 x += cw + 4;
 }

 // FRES — filter resonance knob (0–1, display as 0–100%)
 {
 float gFRes = processor.apvts.getRawParameterValue (ParamIds::defaultFilterRes)->load();
 bool locked = (s.lockMask & kLockFilter) != 0;
 float rv = locked ? s.filterRes : gFRes;
 drawKnobCell (g, x, row2y, "FRES",
 juce::String ((int) (rv * 100.f)) + "%",
 toNorm (F::FieldFilterRes, rv),
 locked, kLockFilter, F::FieldFilterRes,
 0.f, 1.f, 0.01f, cw);
 x += cw + 4;
 filterGroupX2 = x - 4;
 }

    // GLIDE — global legato glide time (0-200ms)
    {
        const float glideMs = processor.voicePool.legatoGlideMs.load (std::memory_order_relaxed);
        const juce::String glideStr = (glideMs < 1.0f) ? "0ms"
                                                        : juce::String ((int) glideMs) + "ms";
        drawKnobCell (g, x, row2y, "GLIDE", glideStr,
                      juce::jlimit (0.f, 1.f, glideMs / 200.f),
                      false, 0, kFieldGlide,
                      0.f, 200.f, 1.f, cw);
        x += cw + 4;
    }

 // METER — waveform activity pulse after FRES
 // Shows peak level as a glowing fill + playback cursor for the selected slice
 if (idx >= 0 && idx < DysektProcessor::kMaxMeterSlices)
 {
 const int meterX = x + 4;
 const int meterW = juce::jmax (40, rightEdge - meterX - 4);
 const int meterY = row2y + 4;
 const int meterH = 22;

 // Background track
 g.setColour (juce::Colour (0xFF080808));
 g.fillRect (meterX, meterY, meterW, meterH);
 g.setColour (juce::Colour (0xFF1A1A1A));
 g.drawRect (meterX, meterY, meterW, meterH);

 // Peak level bars (L top half, R bottom half)
 const float pkL = processor.slicePeakL[(size_t) idx].load (std::memory_order_relaxed);
 const float pkR = processor.slicePeakR[(size_t) idx].load (std::memory_order_relaxed);
 const int barH = (meterH - 2) / 2;

 auto phosphorCol = [&] (float pos) -> juce::Colour
 {
 const auto& sl = processor.sliceManager.getSlice(idx);
 const auto base = sl.colour;
 if (pos < 0.70f) return base.withAlpha (0.25f + (pos / 0.70f) * 0.60f);
 if (pos < 0.85f) return base.interpolatedWith (juce::Colour (0xFFFFE000),
 (pos - 0.70f) / 0.15f).withAlpha (0.88f);
 return juce::Colour (0xFFFF2222).withAlpha (0.80f);
 };

 auto drawBar = [&] (int barY, float pk)
 {
 const float fill = std::sqrt (juce::jlimit (0.0f, 1.0f, pk));
 const int litW = juce::roundToInt (fill * (float)(meterW - 4));
 if (litW <= 0) return;
 for (int px = 0; px < litW; ++px)
 {
 const float pos = (float) px / (float)(meterW - 4);
 g.setColour (phosphorCol (pos));
 g.fillRect (meterX + 2 + px, barY, 1, barH);
 }
 };

 drawBar (meterY + 1, pkL);
 drawBar (meterY + 1 + barH + 1, pkR);

 // Playback cursor — scan all voices for one playing this slice
 const int sliceStart = s.startSample;
 const int sliceEnd = processor.sliceManager.getEndForSlice (idx, ui.sampleNumFrames);
 const int sliceLen = juce::jmax (1, sliceEnd - sliceStart);

 for (int vi = 0; vi < VoicePool::kMaxVoices; ++vi)
 {
 const float vpos = processor.voicePool.voicePositions[vi].load (std::memory_order_relaxed);
 if (vpos <= 0.0f) continue;
 const int ipos = (int) vpos;
 if (ipos < sliceStart || ipos >= sliceEnd) continue;

 // Map position within slice to meter width
 const float frac = (float)(ipos - sliceStart) / (float) sliceLen;
 const int cursorX = meterX + 2 + juce::roundToInt (frac * (float)(meterW - 4));

 // Bright glowing cursor line
 g.setColour (getTheme().foreground.withAlpha (0.90f));
 g.fillRect (cursorX, meterY + 1, 1, meterH - 2);
 g.setColour (getTheme().foreground.withAlpha (0.25f));
 g.fillRect (cursorX - 1, meterY + 1, 1, meterH - 2);
 g.fillRect (cursorX + 1, meterY + 1, 1, meterH - 2);
 break; // first active voice wins
 }

 // Label
 g.setFont (DysektLookAndFeel::makeFont (7.0f, true));
 g.setColour (getTheme().foreground.withAlpha (0.30f));
 g.drawText ("", meterX, row2y, 22, 8, juce::Justification::centredLeft);
 }
 {
 g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
 g.setColour (getTheme().foreground.withAlpha (0.40f));

 auto drawGroupLabel = [&] (int x1, int x2, const char* label)
 {
 // Draw label centred over the group, sitting on the separator line
 g.drawText (label, x1, 26, x2 - x1, 9,
 juce::Justification::centred);
 };

 if (adsrGroupX2 > adsrGroupX1) drawGroupLabel (adsrGroupX1, adsrGroupX2, "");
 if (filterGroupX2 > filterGroupX1) drawGroupLabel (filterGroupX1, filterGroupX2, "");
 }
}

// =============================================================================
// mouseDown
// =============================================================================
void SliceControlBar::mouseDown (const juce::MouseEvent& e)
{
 // ── Lock guard: block all param changes if selected slice is fully locked ─
 {
 const auto& snap = processor.getUiSliceSnapshot();
 if (snap.selectedSlice >= 0 && snap.selectedSlice < snap.numSlices)
 {
 const auto& sl = snap.slices[(size_t) snap.selectedSlice];
 if (sl.lockMask == 0xFFFFFFFFu)
 {
 repaint();
 return;
 }
 }
 }

 if (textEditor != nullptr) textEditor.reset();
 activeDragCell = -1; draggingRootNote = false;
 auto pos = e.getPosition();
 const auto& ui = processor.getUiSliceSnapshot();

 if (ui.numSlices == 0 && rootNoteArea.contains (pos))
 {
 DysektProcessor::Command gc; gc.type = DysektProcessor::CmdBeginGesture;
 processor.pushCommand (gc);
 draggingRootNote = true; dragStartY = pos.y;
 dragStartValue = (float) ui.rootNote; return;
 }

 for (int i = 0; i < (int) cells.size(); ++i)
 {
 const auto& cell = cells[(size_t) i];
 if (! juce::Rectangle (cell.x, cell.y, cell.w, cell.h).contains (pos)) continue;

 // MIDI Learn boundary button
 if (cell.isMidiLearnBtn)
 {
 if (e.mods.isRightButtonDown()) showMidiLearnMenu (cell.fieldId, e.getScreenPosition());
 else
 {
 if (processor.midiLearn.getArmedSlot() == cell.fieldId)
 processor.midiLearn.armLearn (-1);
 else
 processor.midiLearn.armLearn (cell.fieldId);
 repaint();
 }
 return;
 }

 // Knob right-click → MIDI Learn menu
 if (cell.isKnob && e.mods.isRightButtonDown())
 {
 showMidiLearnMenu (cell.fieldId, e.getScreenPosition()); return;
 }

 if (cell.isSetBpm) { return; } // SET BPM removed

 if (cell.isReadOnly) return;

 // Knob left-click
 if (cell.isKnob)
 {
 if (processor.midiLearn.getArmedSlot() == cell.fieldId)
 {
 processor.midiLearn.armLearn (-1); repaint(); return;
 }
 DysektProcessor::Command gc; gc.type = DysektProcessor::CmdBeginGesture;
 processor.pushCommand (gc);
 activeDragCell = i;
 // Pan and Marker sliders are horizontal - store x; all others store y
 dragStartY = (cell.fieldId == DysektProcessor::FieldPan
            || cell.fieldId == DysektProcessor::FieldSliceStart) ? pos.x : pos.y;

 // Activate live drag for slice boundary knobs
 if (cell.fieldId == DysektProcessor::FieldSliceStart)
 {
 int liveSel = ui.selectedSlice;
 if (liveSel >= 0 && liveSel < ui.numSlices)
 {
 processor.liveDragBoundsStart.store (ui.slices[(size_t) liveSel].startSample, std::memory_order_relaxed);
 processor.liveDragBoundsEnd.store (processor.sliceManager.getEndForSlice (liveSel, ui.sampleNumFrames), std::memory_order_relaxed);
 processor.liveDragSliceIdx.store (liveSel, std::memory_order_release);
 }
 }

 int sIdx = ui.selectedSlice;
 if (sIdx >= 0 && sIdx < ui.numSlices)
 {
 const auto& sl = ui.slices[(size_t) sIdx];
 using F = DysektProcessor;
 switch (cell.fieldId)
 {
 case F::FieldBpm: dragStartValue = (sl.lockMask & kLockBpm) ? sl.bpm : processor.apvts.getRawParameterValue (ParamIds::defaultBpm)->load(); break;
 case F::FieldPitch: dragStartValue = (sl.lockMask & kLockPitch) ? sl.pitchSemitones : processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load(); break;
 case F::FieldCentsDetune: dragStartValue = (sl.lockMask & kLockCentsDetune) ? sl.centsDetune : processor.apvts.getRawParameterValue (ParamIds::defaultCentsDetune)->load(); break;
 case F::FieldTonality: dragStartValue = (sl.lockMask & kLockTonality) ? sl.tonalityHz : processor.apvts.getRawParameterValue (ParamIds::defaultTonality)->load(); break;
 case F::FieldFormant: dragStartValue = (sl.lockMask & kLockFormant) ? sl.formantSemitones : processor.apvts.getRawParameterValue (ParamIds::defaultFormant)->load(); break;
 case F::FieldAttack: dragStartValue = (sl.lockMask & kLockAttack) ? sl.attackSec : processor.apvts.getRawParameterValue (ParamIds::defaultAttack)->load() / 1000.f; break;
 case F::FieldDecay: dragStartValue = (sl.lockMask & kLockDecay) ? sl.decaySec : processor.apvts.getRawParameterValue (ParamIds::defaultDecay)->load() / 1000.f; break;
 case F::FieldSustain: dragStartValue = (sl.lockMask & kLockSustain) ? sl.sustainLevel : processor.apvts.getRawParameterValue (ParamIds::defaultSustain)->load() / 100.f; break;
 case F::FieldRelease: dragStartValue = (sl.lockMask & kLockRelease) ? sl.releaseSec : processor.apvts.getRawParameterValue (ParamIds::defaultRelease)->load() / 1000.f; break;
 case F::FieldMuteGroup: dragStartValue = (float)((sl.lockMask & kLockMuteGroup) ? sl.muteGroup : (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load()); break;
 case F::FieldSliceStart: dragStartValue = (float) sl.startSample; break;
 case F::FieldMidiNote: dragStartValue = (float) sl.midiNote; break;
 case F::FieldVolume: dragStartValue = (sl.lockMask & kLockVolume) ? sl.volume : processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load(); break;
 case F::FieldOutputBus: dragStartValue = (float)((sl.lockMask & kLockOutputBus) ? sl.outputBus : 0); break;
 case F::FieldPan: dragStartValue = (sl.lockMask & kLockPan) ? sl.pan : processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load(); break;
 case F::FieldFilterCutoff: dragStartValue = (sl.lockMask & kLockFilter) ? sl.filterCutoff : processor.apvts.getRawParameterValue (ParamIds::defaultFilterCutoff)->load(); break;
 case F::FieldFilterRes: dragStartValue = (sl.lockMask & kLockFilter) ? sl.filterRes : processor.apvts.getRawParameterValue (ParamIds::defaultFilterRes)->load(); break;
        case kFieldGlide:
            dragStartValue = processor.voicePool.legatoGlideMs.load (std::memory_order_relaxed);
            break;
 default: dragStartValue = 0.f; break;
 }
 }
 return;
 }

 // Boolean toggle
 if (cell.isBoolean)
 {
 int sIdx = ui.selectedSlice;
 if (sIdx >= 0 && sIdx < ui.numSlices)
 {
 const auto& sl = ui.slices[(size_t) sIdx];
 bool sliceLocked = (sl.lockMask & cell.lockBit) != 0;
 bool currentVal = false;
 using F = DysektProcessor;
 if (cell.fieldId == F::FieldStretchEnabled) currentVal = sliceLocked ? sl.stretchEnabled : (processor.apvts.getRawParameterValue (ParamIds::defaultStretchEnabled)->load() > 0.5f);
 else if (cell.fieldId == F::FieldFormantComp) currentVal = sliceLocked ? sl.formantComp : (processor.apvts.getRawParameterValue (ParamIds::defaultFormantComp)->load() > 0.5f);
 else if (cell.fieldId == F::FieldReleaseTail) currentVal = sliceLocked ? sl.releaseTail : (processor.apvts.getRawParameterValue (ParamIds::defaultReleaseTail)->load() > 0.5f);
 else if (cell.fieldId == F::FieldReverse) currentVal = sliceLocked ? sl.reverse : (processor.apvts.getRawParameterValue (ParamIds::defaultReverse)->load() > 0.5f);
 else if (cell.fieldId == F::FieldOneShot)
 {
 // Two-pill: left=ONE SHOT(1), right=HOLD(0) — pick by click X vs cell centre
 const bool clickOneShot = (e.x < (cell.x + cell.w / 2));
 DysektProcessor::Command pCmd;
 pCmd.type = DysektProcessor::CmdSetSliceParam;
 pCmd.intParam1 = F::FieldOneShot;
 pCmd.floatParam1 = clickOneShot ? 1.f : 0.f;
 processor.pushCommand (pCmd); repaint();
 return;
 }
 DysektProcessor::Command cmd;
 cmd.type = DysektProcessor::CmdSetSliceParam;
 cmd.intParam1 = cell.fieldId; cmd.floatParam1 = currentVal ? 0.f : 1.f;
 processor.pushCommand (cmd); repaint();
 }
 return;
 }

 // Choice — show PopupMenu for discrete params
 if (cell.isChoice)
 {
 int sIdx = ui.selectedSlice;
 if (sIdx < 0 || sIdx >= ui.numSlices) return;

 using F = DysektProcessor;
 const auto& sl = ui.slices[(size_t) sIdx];

 juce::PopupMenu menu;
 const int fieldId = cell.fieldId;

 auto addItems = [&] (const juce::StringArray& names, int currentVal)
 {
 for (int i = 0; i < names.size(); ++i)
 {
 const bool ticked = (i == currentVal);
 menu.addItem (i + 1, names[i], true, ticked);
 }
 };

 if (fieldId == F::FieldAlgorithm)
 {
 int cur = (sl.lockMask & kLockAlgorithm) ? sl.algorithm
 : (int) processor.apvts.getRawParameterValue (ParamIds::defaultAlgorithm)->load();
 addItems ({ "Standard", "Tonal", "Formant", "Formant Comp", "Grain" }, cur);
 }
 else if (fieldId == F::FieldLoop)
 {
 int cur = (sl.lockMask & kLockLoop) ? sl.loopMode
 : (int) processor.apvts.getRawParameterValue (ParamIds::defaultLoop)->load();
 addItems ({ "Off", "Loop", "Ping-Pong" }, cur);
 }
 else if (fieldId == F::FieldMuteGroup)
 {
 int cur = (sl.lockMask & kLockMuteGroup) ? sl.muteGroup
 : (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load();
 juce::StringArray names; names.add ("Off");
 for (int i = 1; i <= 32; ++i) names.add ("Group " + juce::String (i));
 addItems (names, cur);
 }
 else if (fieldId == F::FieldOutputBus)
 {
 int cur = (sl.lockMask & kLockOutputBus) ? sl.outputBus : 0;
 juce::StringArray names; names.add ("Main");
 for (int i = 1; i <= 15; ++i) names.add ("Aux " + juce::String (i));
 addItems (names, cur);
 }
 else
 {
 // Fallback: old-style cycle
 int current = 0;
 if (fieldId == F::FieldGrainMode) current = (sl.lockMask & kLockGrainMode) ? sl.grainMode : (int) processor.apvts.getRawParameterValue (ParamIds::defaultGrainMode)->load();
 int next = (current + 1) > (int) cell.maxVal ? 0 : current + 1;
 DysektProcessor::Command cmd;
 cmd.type = DysektProcessor::CmdSetSliceParam;
 cmd.intParam1 = fieldId; cmd.floatParam1 = (float) next;
 processor.pushCommand (cmd); repaint();
 return;
 }

 const auto cellScreenRect = localAreaToGlobal (
 juce::Rectangle (cell.x, cell.y, cell.w, cell.h));
 menu.showMenuAsync (juce::PopupMenu::Options()
 .withTargetScreenArea (cellScreenRect)
 .withParentComponent (getTopLevelComponent()),
 [this, fieldId] (int result)
 {
 if (result <= 0) return;
 DysektProcessor::Command cmd;
 cmd.type = DysektProcessor::CmdSetSliceParam;
 cmd.intParam1 = fieldId;
 cmd.floatParam1 = (float)(result - 1);
 processor.pushCommand (cmd);
 repaint();
 });
 return;
 }
 }
}

// =============================================================================
// mouseDrag
// =============================================================================
void SliceControlBar::mouseDrag (const juce::MouseEvent& e)
{
 // ── Lock guard: block all param changes if selected slice is fully locked ─
 {
 const auto& snap = processor.getUiSliceSnapshot();
 if (snap.selectedSlice >= 0 && snap.selectedSlice < snap.numSlices)
 {
 const auto& sl = snap.slices[(size_t) snap.selectedSlice];
 if (sl.lockMask == 0xFFFFFFFFu)
 {
 repaint();
 return;
 }
 }
 }

 if (draggingRootNote)
 {
 float deltaY = (float) (dragStartY - e.y);
 int newVal = juce::jlimit (0, 127, (int) (dragStartValue + deltaY * (127.f / 200.f)));
 DysektProcessor::Command cmd;
 cmd.type = DysektProcessor::CmdSetRootNote; cmd.intParam1 = newVal;
 processor.pushCommand (cmd); repaint(); return;
 }

 if (activeDragCell < 0 || activeDragCell >= (int) cells.size()) return;
 const auto& cell = cells[(size_t) activeDragCell];
 if (! cell.isKnob) return;

 float deltaY = (float) (dragStartY - e.y);
 using F = DysektProcessor;

 // ── Slice boundary knobs: live drag in sample space ───────────────────
 if (cell.fieldId == F::FieldSliceStart)
 {
 const auto& ui2 = processor.getUiSliceSnapshot();
 int liveSel = ui2.selectedSlice;
 if (liveSel >= 0 && liveSel < ui2.numSlices && ui2.sampleNumFrames > 1)
 {
 // Scale: drag 300px = full sample length, shift = fine mode
 float sensitivity = (float) ui2.sampleNumFrames / 300.f;
 if (e.mods.isShiftDown()) sensitivity *= 0.05f;

 const auto& sl = ui2.slices[(size_t) liveSel];
  int delta = (int) ((e.x - dragStartY) * sensitivity); // horizontal drag

 if (cell.fieldId == F::FieldSliceStart)
 {
 const int liveSliceEnd = processor.sliceManager.getEndForSlice (liveSel, ui2.sampleNumFrames);
 int newStart = juce::jlimit (0, liveSliceEnd - 64,
 (int) dragStartValue + delta);
 processor.liveDragBoundsStart.store (newStart, std::memory_order_relaxed);
 processor.liveDragBoundsEnd.store (liveSliceEnd, std::memory_order_relaxed);
 }
 else
 {
 int newEnd = juce::jlimit (sl.startSample + 64, ui2.sampleNumFrames,
 (int) dragStartValue + delta);
 processor.liveDragBoundsStart.store (sl.startSample, std::memory_order_relaxed);
 processor.liveDragBoundsEnd.store (newEnd, std::memory_order_relaxed);
 }
 processor.liveDragSliceIdx.store (liveSel, std::memory_order_release);
 }
 repaint(); return;
 }

    // ── GLIDE: write directly to VoicePool’s legatoGlideMs atomic ──────────────
    if (cell.fieldId == kFieldGlide)
    {
        const float sensitivity = e.mods.isShiftDown() ? 0.2f : 1.0f; // ms/pixel
        float newMs = juce::jlimit (0.0f, 200.0f, dragStartValue - deltaY * sensitivity);
        processor.voicePool.legatoGlideMs.store (newMs, std::memory_order_relaxed);
        repaint(); return;
    }

 // ── All other knobs: CmdSetSliceParam ─────────────────────────────────
 bool isAdsr = (cell.fieldId == F::FieldAttack
 || cell.fieldId == F::FieldDecay
 || cell.fieldId == F::FieldSustain
 || cell.fieldId == F::FieldRelease);
 bool isBpm = (cell.fieldId == F::FieldBpm);

 float newNative;
 if (isAdsr || isBpm)
 {
 // Sensitivity in display units per pixel:
 // Attack: 2 ms/px (range 0-1000ms → 500px full sweep)
 // Decay: 10 ms/px (range 0-5000ms → 500px full sweep)
 // Release: 10 ms/px
 // Sustain: 0.5 %/px (range 0-100% → 200px full sweep)
 // BPM: 2 bpm/px
 // Shift = fine mode (÷10)
 float sensitivity = 1.0f;
 if (cell.fieldId == F::FieldAttack) sensitivity = 2.0f;
 else if (cell.fieldId == F::FieldDecay) sensitivity = 10.0f;
 else if (cell.fieldId == F::FieldRelease) sensitivity = 10.0f;
 else if (cell.fieldId == F::FieldSustain) sensitivity = 0.5f;
 else if (cell.fieldId == F::FieldBpm) sensitivity = 2.0f;
 if (e.mods.isShiftDown()) sensitivity *= 0.1f;

 float ds = dragStartValue, dmin = cell.minVal, dmax = cell.maxVal;
 // Convert to display units
 if (cell.fieldId == F::FieldAttack ||
 cell.fieldId == F::FieldDecay ||
 cell.fieldId == F::FieldRelease)
 { ds *= 1000.f; dmin *= 1000.f; dmax *= 1000.f; }
 else if (cell.fieldId == F::FieldSustain)
 { ds *= 100.f; dmin *= 100.f; dmax *= 100.f; }

 float dv = juce::jlimit (dmin, dmax, ds + deltaY * sensitivity);

 if (cell.fieldId == F::FieldAttack ||
 cell.fieldId == F::FieldDecay ||
 cell.fieldId == F::FieldRelease)
 newNative = dv / 1000.f;
 else if (cell.fieldId == F::FieldSustain)
 newNative = dv / 100.f;
 else
 newNative = dv;
 }
 else
 {
 // Pan slider responds to horizontal drag — all other params use vertical
 const float delta = (cell.fieldId == F::FieldPan)
 ? (float)(e.x - dragStartY) // dragStartY stores startX for pan
 : deltaY;
 const float sensitivity = (cell.fieldId == F::FieldPan)
 ? (e.mods.isShiftDown() ? 0.002f : 0.01f) // 100px = full L→R sweep, shift=fine
 : 1.0f;
 if (cell.fieldId == F::FieldPan)
 newNative = juce::jlimit (-1.f, 1.f, dragStartValue + delta * sensitivity);
 else
 newNative = UIHelpers::computeDragValue (dragStartValue, delta,
 cell.minVal, cell.maxVal,
 e.mods.isShiftDown());
 }

 DysektProcessor::Command cmd;
 cmd.type = F::CmdSetSliceParam;
 cmd.intParam1 = cell.fieldId; cmd.floatParam1 = newNative;
 processor.pushCommand (cmd); repaint();
}

// =============================================================================
// mouseUp — commit slice bounds and deactivate live drag
// =============================================================================
void SliceControlBar::mouseUp (const juce::MouseEvent& /*e*/)
{
 using F = DysektProcessor;

 if (activeDragCell >= 0 && activeDragCell < (int) cells.size())
 {
 const auto& cell = cells[(size_t) activeDragCell];
 if (cell.fieldId == F::FieldSliceStart)
 {
 const int liveIdx = processor.liveDragSliceIdx.load (std::memory_order_acquire);
 if (liveIdx >= 0)
 {
 F::Command cmd;
 cmd.type = F::CmdSetSliceBounds;
 cmd.intParam1 = liveIdx;
 cmd.intParam2 = processor.liveDragBoundsStart.load (std::memory_order_relaxed);
 cmd.positions[0] = processor.liveDragBoundsEnd.load (std::memory_order_relaxed);
 cmd.numPositions = 1;
 processor.pushCommand (cmd);
 }
 }
 }

 // Deactivate live drag (mirrors WaveformView::mouseUp)
 processor.liveDragSliceIdx.store (-1, std::memory_order_release);
 activeDragCell = -1;
 draggingRootNote = false;
}

// =============================================================================
// mouseDoubleClick
// =============================================================================
void SliceControlBar::mouseDoubleClick (const juce::MouseEvent& e)
{
 auto pos = e.getPosition();
 const auto& ui = processor.getUiSliceSnapshot();

 if (ui.numSlices == 0 && rootNoteArea.contains (pos))
 {
 textEditor = std::make_unique<juce::TextEditor>();
 addAndMakeVisible (*textEditor);
 textEditor->setBounds (rootNoteArea.getX(), rootNoteArea.getY() + 15,
 rootNoteArea.getWidth(), 16);
 textEditor->setFont (DysektLookAndFeel::makeFont (14.0f));
 textEditor->setColour (juce::TextEditor::backgroundColourId, getTheme().darkBar.brighter (0.15f));
 textEditor->setColour (juce::TextEditor::textColourId, getTheme().foreground);
 textEditor->setColour (juce::TextEditor::outlineColourId, getTheme().accent);
 textEditor->setText (juce::String (ui.rootNote), false);
 textEditor->selectAll(); textEditor->grabKeyboardFocus();
 textEditor->onReturnKey = [this] {
 if (! textEditor) return;
 int val = juce::jlimit (0, 127, textEditor->getText().getIntValue());
 DysektProcessor::Command cmd;
 cmd.type = DysektProcessor::CmdSetRootNote; cmd.intParam1 = val;
 processor.pushCommand (cmd); textEditor.reset(); repaint();
 };
 textEditor->onEscapeKey = [this] { textEditor.reset(); repaint(); };
 textEditor->onFocusLost = [this] { textEditor.reset(); repaint(); };
 return;
 }

 for (int i = 0; i < (int) cells.size(); ++i)
 {
 const auto& cell = cells[(size_t) i];
 if (! juce::Rectangle (cell.x, cell.y, cell.w, cell.h).contains (pos)) continue;
 if (cell.isMidiLearnBtn || cell.isBoolean || cell.isChoice || cell.isReadOnly) return;
 if (! cell.isKnob) return;

 float currentVal = 0.f;
 int sIdx = ui.selectedSlice;
 if (sIdx >= 0 && sIdx < ui.numSlices)
 {
 const auto& sl = ui.slices[(size_t) sIdx];
 using F = DysektProcessor;
 // Values in display units (ms, %, raw)
 switch (cell.fieldId)
 {
 case F::FieldBpm: currentVal = (sl.lockMask & kLockBpm) ? sl.bpm : processor.apvts.getRawParameterValue (ParamIds::defaultBpm)->load(); break;
 case F::FieldPitch: currentVal = (sl.lockMask & kLockPitch) ? sl.pitchSemitones : processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load(); break;
 case F::FieldCentsDetune: currentVal = (sl.lockMask & kLockCentsDetune) ? sl.centsDetune : processor.apvts.getRawParameterValue (ParamIds::defaultCentsDetune)->load(); break;
 case F::FieldTonality: currentVal = (sl.lockMask & kLockTonality) ? sl.tonalityHz : processor.apvts.getRawParameterValue (ParamIds::defaultTonality)->load(); break;
 case F::FieldFormant: currentVal = (sl.lockMask & kLockFormant) ? sl.formantSemitones : processor.apvts.getRawParameterValue (ParamIds::defaultFormant)->load(); break;
 case F::FieldAttack: currentVal = ((sl.lockMask & kLockAttack) ? sl.attackSec : processor.apvts.getRawParameterValue (ParamIds::defaultAttack)->load() / 1000.f) * 1000.f; break;
 case F::FieldDecay: currentVal = ((sl.lockMask & kLockDecay) ? sl.decaySec : processor.apvts.getRawParameterValue (ParamIds::defaultDecay)->load() / 1000.f) * 1000.f; break;
 case F::FieldSustain: currentVal = ((sl.lockMask & kLockSustain) ? sl.sustainLevel : processor.apvts.getRawParameterValue (ParamIds::defaultSustain)->load() / 100.f) * 100.f; break;
 case F::FieldRelease: currentVal = ((sl.lockMask & kLockRelease) ? sl.releaseSec : processor.apvts.getRawParameterValue (ParamIds::defaultRelease)->load() / 1000.f) * 1000.f; break;
 case F::FieldMuteGroup: currentVal = (float)((sl.lockMask & kLockMuteGroup) ? sl.muteGroup : (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load()); break;
 case F::FieldMidiNote: currentVal = (float) sl.midiNote; break;
 case F::FieldVolume: currentVal = (sl.lockMask & kLockVolume) ? sl.volume : processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load(); break;
 case F::FieldOutputBus: currentVal = (float)((sl.lockMask & kLockOutputBus) ? sl.outputBus : 0); break;
 case F::FieldPan: currentVal = (sl.lockMask & kLockPan) ? sl.pan : processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load(); break;
 case F::FieldFilterCutoff: currentVal = (sl.lockMask & kLockFilter) ? sl.filterCutoff : processor.apvts.getRawParameterValue (ParamIds::defaultFilterCutoff)->load(); break;
 case F::FieldFilterRes: currentVal = (sl.lockMask & kLockFilter) ? sl.filterRes : processor.apvts.getRawParameterValue (ParamIds::defaultFilterRes)->load(); break;
 default: break;
 }
 }
 showTextEditor (cell, currentVal); return;
 }
}

// =============================================================================
// showTextEditor (unchanged logic)
// =============================================================================
void SliceControlBar::showTextEditor (const ParamCell& cell, float currentValue)
{
 textEditor = std::make_unique<juce::TextEditor>();
 addAndMakeVisible (*textEditor);
 textEditor->setBounds (cell.x + kParamCellTextX, cell.y + 14,
 cell.w - kParamCellTextX - 2, 16);
 textEditor->setFont (DysektLookAndFeel::makeFont (14.0f));
 textEditor->setColour (juce::TextEditor::backgroundColourId, getTheme().darkBar.brighter (0.15f));
 textEditor->setColour (juce::TextEditor::textColourId, getTheme().foreground);
 textEditor->setColour (juce::TextEditor::outlineColourId, getTheme().accent);

 using F = DysektProcessor;
 juce::String displayVal;
 if (cell.fieldId == F::FieldAttack || cell.fieldId == F::FieldDecay || cell.fieldId == F::FieldRelease)
 displayVal = juce::String ((int) currentValue);
 else if (cell.fieldId == F::FieldSustain)
 displayVal = juce::String ((int) currentValue);
 else if (cell.fieldId == F::FieldVolume || cell.fieldId == F::FieldBpm)
 displayVal = juce::String (currentValue, 2);
 else if (cell.step >= 1.f)
 displayVal = juce::String ((int) currentValue);
 else
 displayVal = juce::String (currentValue, 1);

 textEditor->setText (displayVal, false);
 textEditor->selectAll(); textEditor->grabKeyboardFocus();

 int fieldId = cell.fieldId;
 float minV = cell.minVal, maxV = cell.maxVal;

 textEditor->onReturnKey = [this, fieldId, minV, maxV] {
 if (! textEditor) return;
 float val = textEditor->getText().getFloatValue();
 using F2 = DysektProcessor;
 if (fieldId == F2::FieldAttack || fieldId == F2::FieldDecay || fieldId == F2::FieldRelease)
 val /= 1000.f;
 else if (fieldId == F2::FieldSustain)
 val /= 100.f;
 val = juce::jlimit (minV, maxV, val);
 DysektProcessor::Command cmd;
 cmd.type = DysektProcessor::CmdSetSliceParam;
 cmd.intParam1 = fieldId; cmd.floatParam1 = val;
 processor.pushCommand (cmd); textEditor.reset(); repaint();
 };
 textEditor->onEscapeKey = [this] { textEditor.reset(); repaint(); };
 textEditor->onFocusLost = [this] { textEditor.reset(); repaint(); };
}

// =============================================================================
// showSetBpmPopup removed — SET BPM functionality accessible via BPM knob