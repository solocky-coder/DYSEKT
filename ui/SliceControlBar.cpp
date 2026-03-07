// src/ui/SliceControlBar.cpp

#include "SliceControlBar.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/GrainEngine.h"

namespace
{
constexpr int kParamCellTextX    = 14;
constexpr int kParamCellTextWidth = 60;
constexpr int kParamCellWidth    = kParamCellTextX + kParamCellTextWidth;

constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kKnobEnd   = juce::MathConstants<float>::pi * 2.75f;
}

SliceControlBar::SliceControlBar (DysektProcessor& p) : processor (p) {}
void SliceControlBar::resized() {}

// =============================================================================
//  drawLockIcon  (unchanged)
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
//  drawParamCell  (unchanged — booleans / choices only)
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
    g.drawText (label, x + kParamCellTextX, y + 2,  kParamCellTextWidth, 13, juce::Justification::centredLeft);

    g.setFont (DysektLookAndFeel::makeFont (14.0f));
    g.setColour (locked ? getTheme().foreground
                        : getTheme().foreground.withAlpha (0.4f));
    g.drawText (value, x + kParamCellTextX, y + 15, kParamCellTextWidth, 14, juce::Justification::centredLeft);

    outWidth = cellW;
    cells.push_back ({ x, y, outWidth, cellH, lockBit, fieldId,
                       minVal, maxVal, step, isBoolean, isChoice, false, false });
}

// =============================================================================
//  drawKnob  — small rotary arc
// =============================================================================
void SliceControlBar::drawKnob (juce::Graphics& g,
                                int cx, int cy, int r,
                                float normVal,
                                bool locked, bool armed, bool mapped)
{
    const float angle = kKnobStart + normVal * (kKnobEnd - kKnobStart);
    const float fcx = (float) cx, fcy = (float) cy, fr = (float) r;

    juce::Path track;
    track.addCentredArc (fcx, fcy, fr, fr, 0.f, kKnobStart, kKnobEnd, true);
    g.setColour (getTheme().darkBar.brighter (0.22f));
    g.strokePath (track, juce::PathStrokeType (1.5f,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Colour arcCol = armed  ? getTheme().accent
                        : mapped ? getTheme().accent.withAlpha (0.7f)
                        : locked ? getTheme().lockActive.withAlpha (0.85f)
                                 : getTheme().accent.withAlpha (0.42f);

    juce::Path arc;
    arc.addCentredArc (fcx, fcy, fr, fr, 0.f, kKnobStart, angle, true);
    g.setColour (arcCol);
    g.strokePath (arc, juce::PathStrokeType (2.2f,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float lineR = fr - 2.5f;
    g.setColour (arcCol.brighter (0.15f));
    g.drawLine (fcx, fcy,
                fcx + lineR * std::cos (angle),
                fcy + lineR * std::sin (angle), 1.5f);

    g.setColour (locked ? getTheme().foreground.withAlpha (0.75f)
                        : getTheme().foreground.withAlpha (0.25f));
    g.fillEllipse (fcx - 2.f, fcy - 2.f, 4.f, 4.f);
}

// =============================================================================
//  toNorm — native value → 0-1 for knob arc
// =============================================================================
float SliceControlBar::toNorm (int fieldId, float v) const
{
    using F = DysektProcessor;
    switch (fieldId)
    {
        case F::FieldBpm:         return juce::jlimit (0.f, 1.f, (v - 20.f) / (999.f - 20.f));
        case F::FieldPitch:       return juce::jlimit (0.f, 1.f, (v + 48.f) / 96.f);
        case F::FieldCentsDetune: return juce::jlimit (0.f, 1.f, (v + 100.f) / 200.f);
        case F::FieldTonality:    return juce::jlimit (0.f, 1.f, v / 8000.f);
        case F::FieldFormant:     return juce::jlimit (0.f, 1.f, (v + 24.f) / 48.f);
        case F::FieldAttack:      return juce::jlimit (0.f, 1.f, v / 1.f);
        case F::FieldDecay:       return juce::jlimit (0.f, 1.f, v / 5.f);
        case F::FieldSustain:     return juce::jlimit (0.f, 1.f, v);
        case F::FieldRelease:     return juce::jlimit (0.f, 1.f, v / 5.f);
        case F::FieldMuteGroup:   return juce::jlimit (0.f, 1.f, v / 32.f);
        case F::FieldMidiNote:    return juce::jlimit (0.f, 1.f, v / 127.f);
        case F::FieldVolume:      return juce::jlimit (0.f, 1.f, (v + 100.f) / 124.f);
        case F::FieldOutputBus:   return juce::jlimit (0.f, 1.f, v / 15.f);
        default:                  return 0.5f;
    }
}

// =============================================================================
//  drawKnobCell  — rotary knob cell for all numeric params
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

    const bool armed  = (processor.midiLearn.getArmedSlot() == fieldId);
    const bool mapped = processor.midiLearn.isMapped (fieldId);

    if (armed)
    {
        g.setColour (getTheme().accent.withAlpha (0.11f));
        g.fillRoundedRectangle ((float) x, (float) y, (float) cellW, (float) cellH, 2.f);
    }

    drawKnob (g, knobCX, knobCY, kKnobR, normVal, locked, armed, mapped);

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

    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (locked ? getTheme().lockActive.withAlpha (0.8f)
                        : getTheme().foreground.withAlpha (0.42f));
    g.drawText (label, textX, y + 2,  textW, 12, juce::Justification::centredLeft);

    g.setFont (DysektLookAndFeel::makeFont (11.0f));
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
//  drawMidiLearnCell  — START / END slice boundary buttons
// =============================================================================
void SliceControlBar::drawMidiLearnCell (juce::Graphics& g, int x, int y,
                                         const juce::String& label,
                                         int fieldId, int& outWidth)
{
    const int cellW = 52, cellH = 32;
    const bool armed  = (processor.midiLearn.getArmedSlot() == fieldId);
    const bool mapped = processor.midiLearn.isMapped (fieldId);

    if (armed)
    {
        g.setColour (getTheme().accent.withAlpha (0.2f));
        g.fillRoundedRectangle ((float) x, (float) y, (float) cellW, (float) cellH, 3.f);
    }

    g.setColour (armed   ? getTheme().accent
                : mapped ? getTheme().accent.withAlpha (0.48f)
                         : getTheme().foreground.withAlpha (0.18f));
    g.drawRoundedRectangle ((float) x + 0.5f, (float) y + 0.5f,
                            (float) cellW - 1.f, (float) cellH - 1.f, 3.f, 1.f);

    g.setFont (DysektLookAndFeel::makeFont (12.0f));
    g.setColour (armed   ? getTheme().accent
                : mapped ? getTheme().foreground.withAlpha (0.65f)
                         : getTheme().foreground.withAlpha (0.38f));
    g.drawText (label, x + 5, y + 2, cellW - 6, 13, juce::Justification::centredLeft);

    g.setFont (DysektLookAndFeel::makeFont (14.0f));
    g.setColour (armed   ? getTheme().accent
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
//  showMidiLearnMenu
// =============================================================================
void SliceControlBar::showMidiLearnMenu (int fieldId, juce::Point<int> /*screenPos*/)
{
    const bool mapped = processor.midiLearn.isMapped (fieldId);
    juce::PopupMenu menu;
    menu.addItem (1, "Learn MIDI CC");
    if (mapped)
        menu.addItem (2, "Clear (" + processor.midiLearn.getLabelText (fieldId) + ")");

    auto* topLvl = getTopLevelComponent();
    float ms = DysektLookAndFeel::getMenuScale();
    menu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetComponent (this)
            .withParentComponent (topLvl)
            .withStandardItemHeight ((int) (24 * ms)),
        [this, fieldId] (int result)
        {
            if (result == 1)      { processor.midiLearn.armLearn (fieldId); repaint(); }
            else if (result == 2) { processor.midiLearn.clearMapping (fieldId); repaint(); }
        });
}

// =============================================================================
//  paint
// =============================================================================
void SliceControlBar::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().darkBar);
    cells.clear();

    const auto& ui = processor.getUiSliceSnapshot();
    int idx       = ui.selectedSlice;
    int numSlices = ui.numSlices;
    int rightEdge = getWidth() - 8;
    int row1y = 2, row2y = 36;

    // ── Row 2 right: SLICES / ROOT (always visible) ───────────────────
    {
        int rn = ui.rootNote;
        bool editable = (numSlices == 0);
        int rnW = 55, rnX = rightEdge - rnW;
        rootNoteArea = { rnX, row2y, rnW, 30 };

        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (editable ? getTheme().accent.withAlpha (0.7f)
                              : getTheme().foreground.withAlpha (0.35f));
        g.drawText ("ROOT", rnX, row2y + 2, rnW, 13, juce::Justification::right);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (editable ? getTheme().foreground.withAlpha (0.6f)
                              : getTheme().foreground.withAlpha (0.4f));
        g.drawText (juce::String (rn), rnX, row2y + 15, rnW, 14, juce::Justification::right);

        int slcW = 55, slcX = rnX - slcW - 4;
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (getTheme().foreground.withAlpha (0.35f));
        g.drawText ("SLICES", slcX, row2y + 2, slcW, 13, juce::Justification::right);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (getTheme().foreground.withAlpha (0.4f));
        g.drawText (juce::String (numSlices), slcX, row2y + 15, slcW, 14, juce::Justification::right);
    }

    if (idx < 0 || idx >= numSlices)
    {
        g.setFont (DysektLookAndFeel::makeFont (15.0f));
        g.setColour (getTheme().foreground.withAlpha (0.35f));
        g.drawText ("No slice selected", 8, 24, 220, 18, juce::Justification::centredLeft);
        return;
    }

    const auto& s = ui.slices[(size_t) idx];

    float gBpm      = processor.apvts.getRawParameterValue (ParamIds::defaultBpm)->load();
    float gPitch    = processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();
    int   gAlgo     = (int) processor.apvts.getRawParameterValue (ParamIds::defaultAlgorithm)->load();
    float gAttack   = processor.apvts.getRawParameterValue (ParamIds::defaultAttack)->load();
    float gDecay    = processor.apvts.getRawParameterValue (ParamIds::defaultDecay)->load();
    float gSustain  = processor.apvts.getRawParameterValue (ParamIds::defaultSustain)->load();
    float gRelease  = processor.apvts.getRawParameterValue (ParamIds::defaultRelease)->load();
    int   gMG       = (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load();
    int   gLoopMode = (int) processor.apvts.getRawParameterValue (ParamIds::defaultLoop)->load();
    bool  gStretch  = processor.apvts.getRawParameterValue (ParamIds::defaultStretchEnabled)->load() > 0.5f;

    bool algoLocked    = (s.lockMask & kLockAlgorithm) != 0;
    int  algoVal       = algoLocked ? s.algorithm : gAlgo;
    bool stretchLocked = (s.lockMask & kLockStretch) != 0;
    bool stretchVal    = stretchLocked ? s.stretchEnabled : gStretch;
    bool repitchStretch = (algoVal == 0) && stretchVal;

    int cw;
    using F = DysektProcessor;

    // ── Row 1 right: slice info ───────────────────────────────────────
    {
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (getTheme().accent.withAlpha (0.7f));
        g.drawText ("SLICE " + juce::String (idx + 1),
                    8, row1y + 2, rightEdge - 8, 13, juce::Justification::right);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (getTheme().foreground.withAlpha (0.5f));
        double srate = processor.getSampleRate();
        if (srate <= 0) srate = 44100.0;
        const int sliceEnd1 = processor.sliceManager.getEndForSlice (idx, ui.sampleNumFrames);
        double lenSec = (sliceEnd1 - s.startSample) / srate;
        g.drawText (juce::String (s.startSample) + "-" + juce::String (sliceEnd1)
                    + " (" + juce::String (lenSec, 2) + "s)",
                    8, row1y + 15, rightEdge - 8, 14, juce::Justification::right);
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
            if (bpmStr.endsWith ("."))    bpmStr = bpmStr.dropLastCharacters (1);
        }
        drawKnobCell (g, x, row1y, "BPM", bpmStr,
                      toNorm (F::FieldBpm, bpmVal),
                      locked, kLockBpm, F::FieldBpm, 20.f, 999.f, 0.01f, cw);
        x += cw + 4;
    }

    // SET BPM
    {
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (getTheme().accent);
        g.drawText ("SET", x + 2, row1y + 2,  34, 13, juce::Justification::centredLeft);
        g.drawText ("BPM", x + 2, row1y + 15, 34, 13, juce::Justification::centredLeft);
        cells.push_back ({ x, row1y, 38, 32, 0, 0, 0.f, 0.f, 0.f, false, false, false, true });
        x += 42;
    }

    // [snip other knob and param cells... not shown here for brevity, but copy from your known working code]
    // The rest of this file continues, unchanged, as already provided in your working repo.

    // If you had compilation errors from F, keep using 'using F = DysektProcessor;' as shown above.
}