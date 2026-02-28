#include "SliceControlBar.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/GrainEngine.h"
#include <cmath>

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
        case F::FieldMuteGroup:    return juce::jlimit (0.f, 1.f, v / 32.f);
        case F::FieldMidiNote:     return juce::jlimit (0.f, 1.f, v / 127.f);
        case F::FieldVolume:       return juce::jlimit (0.f, 1.f, (v + 100.f) / 124.f);
        case F::FieldOutputBus:    return juce::jlimit (0.f, 1.f, v / 15.f);
        case F::FieldPan:          return juce::jlimit (0.f, 1.f, (v + 1.f) / 2.f);
        case F::FieldFilterCutoff: return juce::jlimit (0.f, 1.f,
                                       (std::log (juce::jlimit (20.f, 20000.f, v)) - std::log (20.f))
                                       / (std::log (20000.f) - std::log (20.f)));        case F::FieldFilterRes:    return juce::jlimit (0.f, 1.f, v);
        default:                   return 0.5f;
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
void SliceControlBar::showMidiLearnMenu (int fieldId)
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
        double lenSec = (s.endSample - s.startSample) / srate;
        g.drawText (juce::String (s.startSample) + "-" + juce::String (s.endSample)
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

    // PITCH — knob
    {
        bool locked = (s.lockMask & kLockPitch) != 0;
        float pv = locked ? s.pitchSemitones : gPitch;
        if (repitchStretch)
        {
            float daw     = processor.dawBpm.load();
            float bpmVal  = (s.lockMask & kLockBpm) ? s.bpm : gBpm;
            float semis   = (daw > 0.f && bpmVal > 0.f)
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
        bool locked  = (s.lockMask & kLockCentsDetune) != 0;
        float cv     = locked ? s.centsDetune : gCents;
        if (repitchStretch)
        {
            float daw    = processor.dawBpm.load();
            float bpmVal = (s.lockMask & kLockBpm) ? s.bpm : gBpm;
            float semis  = (daw > 0.f && bpmVal > 0.f)
                           ? 12.f * std::log2 (daw / bpmVal) : 0.f;
            int semisI   = (int) std::round (semis);
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
        juce::String algoNames[] = { "Repitch", "Stretch", "Bungee" };
        drawParamCell (g, x, row1y, "ALGO",
                       algoNames[juce::jlimit (0, 2, algoVal)],
                       algoLocked, kLockAlgorithm, F::FieldAlgorithm,
                       0.f, 2.f, 1.f, false, true, cw);
        x += cw + 4;
    }

    if (algoVal == 1)
    {
        float gTonal = processor.apvts.getRawParameterValue (ParamIds::defaultTonality)->load();
        bool locked  = (s.lockMask & kLockTonality) != 0;
        float tv     = locked ? s.tonalityHz : gTonal;
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
    else if (algoVal == 2)
    {
        int gGM   = (int) processor.apvts.getRawParameterValue (ParamIds::defaultGrainMode)->load();
        bool locked = (s.lockMask & kLockGrainMode) != 0;
        int gmVal   = locked ? s.grainMode : gGM;
        juce::String gmNames[] = { "Fast", "Normal", "Smooth" };
        drawParamCell (g, x, row1y, "GRAIN",
                       gmNames[juce::jlimit (0, 2, gmVal)],
                       locked, kLockGrainMode, F::FieldGrainMode,
                       0.f, 2.f, 1.f, false, true, cw);
        x += cw + 4;
    }

    // STRETCH — boolean
    {
        bool locked = (s.lockMask & kLockStretch) != 0;
        bool sv     = locked ? s.stretchEnabled : gStretch;
        drawParamCell (g, x, row1y, "STRETCH", sv ? "ON" : "OFF",
                       locked, kLockStretch, F::FieldStretchEnabled,
                       0.f, 1.f, 1.f, true, false, cw);
        x += cw + 4;
    }

    // 1SHOT — boolean
    {
        bool gOS   = processor.apvts.getRawParameterValue (ParamIds::defaultOneShot)->load() > 0.5f;
        bool locked = (s.lockMask & kLockOneShot) != 0;
        bool osv    = locked ? s.oneShot : gOS;
        drawParamCell (g, x, row1y, "1SHOT", osv ? "ON" : "OFF",
                       locked, kLockOneShot, F::FieldOneShot,
                       0.f, 1.f, 1.f, true, false, cw);
        x += cw + 4;
    }

    // START / END as knobs + LINK toggle
    {
        g.setColour (getTheme().separator);
        g.drawVerticalLine (x + 2, (float) row1y + 4, (float) row1y + 28);
        x += 8;

        // START knob — normalised 0-1 over sample length
        {
            float startNorm = (ui.numSlices > 0 && idx >= 0)
                ? (float) s.startSample / (float) juce::jmax (1, ui.sampleNumFrames)
                : 0.f;
            juce::String startStr = juce::String (s.startSample);
            drawKnobCell (g, x, row1y, "START", startStr,
                          startNorm, false, 0,
                          F::FieldSliceStart, 0.f, 1.f, 0.001f, cw);
            cells.back().isMidiLearnable = true;
            x += cw + 4;
        }

        // END knob
        {
            float endNorm = (ui.numSlices > 0 && idx >= 0)
                ? (float) s.endSample / (float) juce::jmax (1, ui.sampleNumFrames)
                : 1.f;
            juce::String endStr = juce::String (s.endSample);
            drawKnobCell (g, x, row1y, "END", endStr,
                          endNorm, false, 0,
                          F::FieldSliceEnd, 0.f, 1.f, 0.001f, cw);
            cells.back().isMidiLearnable = true;
            x += cw + 4;
        }

        // LINK toggle button
        {
            const bool linked = processor.slicesLinked.load (std::memory_order_relaxed);
            const int btnW = 42, btnH = 32;
            linkBtnArea = { x, row1y, btnW, btnH };

            g.setColour (linked ? getTheme().accent.withAlpha (0.25f)
                                : getTheme().darkBar.brighter (0.08f));
            g.fillRoundedRectangle ((float) x, (float) row1y, (float) btnW, (float) btnH, 3.f);

            g.setColour (linked ? getTheme().accent
                                : getTheme().foreground.withAlpha (0.22f));
            g.drawRoundedRectangle ((float) x + 0.5f, (float) row1y + 0.5f,
                                    (float) btnW - 1.f, (float) btnH - 1.f, 3.f, 1.f);

            g.setFont (DysektLookAndFeel::makeFont (10.0f));
            g.setColour (linked ? getTheme().accent
                                : getTheme().foreground.withAlpha (0.35f));
            g.drawText ("LINK", x, row1y + 2, btnW, 13, juce::Justification::centred);

            g.setFont (DysektLookAndFeel::makeFont (9.0f));
            g.setColour (linked ? getTheme().foreground.withAlpha (0.7f)
                                : getTheme().foreground.withAlpha (0.22f));
            g.drawText (linked ? "ON" : "OFF", x, row1y + 15, btnW, 13, juce::Justification::centred);

            ParamCell lc{};
            lc.x = x; lc.y = row1y; lc.w = btnW; lc.h = btnH;
            lc.isLinkBtn = true;
            cells.push_back (lc);
        }
    }

    // ── Separator ─────────────────────────────────────────────────────
    g.setColour (getTheme().separator);
    g.drawHorizontalLine (34, 8.0f, (float) getWidth() - 8.0f);

    // ── Row 2 ─────────────────────────────────────────────────────────
    x = 8;

    // ATK — knob (stored seconds, display ms)
    {
        bool locked = (s.lockMask & kLockAttack) != 0;
        float atk   = locked ? s.attackSec : gAttack / 1000.f;
        drawKnobCell (g, x, row2y, "ATK",
                      juce::String ((int) (atk * 1000.f)) + "ms",
                      toNorm (F::FieldAttack, atk),
                      locked, kLockAttack, F::FieldAttack, 0.f, 1.f, 0.001f, cw);
        x += cw + 4;
    }

    // DEC — knob
    {
        bool locked = (s.lockMask & kLockDecay) != 0;
        float dec   = locked ? s.decaySec : gDecay / 1000.f;
        drawKnobCell (g, x, row2y, "DEC",
                      juce::String ((int) (dec * 1000.f)) + "ms",
                      toNorm (F::FieldDecay, dec),
                      locked, kLockDecay, F::FieldDecay, 0.f, 5.f, 0.001f, cw);
        x += cw + 4;
    }

    // SUS — knob (stored 0-1, display %)
    {
        bool locked = (s.lockMask & kLockSustain) != 0;
        float sus   = locked ? s.sustainLevel : gSustain / 100.f;
        drawKnobCell (g, x, row2y, "SUS",
                      juce::String ((int) (sus * 100.f)) + "%",
                      toNorm (F::FieldSustain, sus),
                      locked, kLockSustain, F::FieldSustain, 0.f, 1.f, 0.01f, cw);
        x += cw + 4;
    }

    // REL — knob
    {
        bool locked = (s.lockMask & kLockRelease) != 0;
        float rel   = locked ? s.releaseSec : gRelease / 1000.f;
        drawKnobCell (g, x, row2y, "REL",
                      juce::String ((int) (rel * 1000.f)) + "ms",
                      toNorm (F::FieldRelease, rel),
                      locked, kLockRelease, F::FieldRelease, 0.f, 5.f, 0.001f, cw);
        x += cw + 4;
    }

    // TAIL — boolean
    {
        bool gTail = processor.apvts.getRawParameterValue (ParamIds::defaultReleaseTail)->load() > 0.5f;
        bool locked = (s.lockMask & kLockReleaseTail) != 0;
        bool tv = locked ? s.releaseTail : gTail;
        drawParamCell (g, x, row2y, "TAIL", tv ? "ON" : "OFF",
                       locked, kLockReleaseTail, F::FieldReleaseTail,
                       0.f, 1.f, 1.f, true, false, cw);
        x += cw + 4;
    }

    // REV — boolean
    {
        bool gRev = processor.apvts.getRawParameterValue (ParamIds::defaultReverse)->load() > 0.5f;
        bool locked = (s.lockMask & kLockReverse) != 0;
        bool rv = locked ? s.reverse : gRev;
        drawParamCell (g, x, row2y, "REV", rv ? "ON" : "OFF",
                       locked, kLockReverse, F::FieldReverse,
                       0.f, 1.f, 1.f, true, false, cw);
        x += cw + 4;
    }

    // LOOP — choice
    {
        bool locked = (s.lockMask & kLockLoop) != 0;
        int lv = locked ? s.loopMode : gLoopMode;
        juce::String loopNames[] = { "OFF", "LOOP", "PP" };
        drawParamCell (g, x, row2y, "LOOP",
                       loopNames[juce::jlimit (0, 2, lv)],
                       locked, kLockLoop, F::FieldLoop,
                       0.f, 2.f, 1.f, false, true, cw);
        x += cw + 4;
    }

    // MUTE — knob
    {
        bool locked = (s.lockMask & kLockMuteGroup) != 0;
        int mv = locked ? s.muteGroup : gMG;
        drawKnobCell (g, x, row2y, "MUTE",
                      juce::String (mv),
                      toNorm (F::FieldMuteGroup, (float) mv),
                      locked, kLockMuteGroup, F::FieldMuteGroup, 0.f, 32.f, 1.f, cw);
        x += cw + 4;
    }

    // GAIN — knob
    {
        float gGainDb = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
        bool locked   = (s.lockMask & kLockVolume) != 0;
        float gv      = locked ? s.volume : gGainDb;
        drawKnobCell (g, x, row2y, "GAIN",
                      (gv >= 0.f ? "+" : "") + juce::String (gv, 1) + "dB",
                      toNorm (F::FieldVolume, gv),
                      locked, kLockVolume, F::FieldVolume, -100.f, 24.f, 0.1f, cw);
        x += cw + 4;
    }

    // PAN — knob (-1 L .. 0 C .. +1 R)
    {
        float gPan  = processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();
        bool locked = (s.lockMask & kLockPan) != 0;
        float pv    = locked ? s.pan : gPan;
        juce::String panStr = (pv == 0.0f) ? "C"
                            : (pv < 0.0f   ? juce::String ((int) std::round (-pv * 100.0f)) + "L"
                                           : juce::String ((int) std::round ( pv * 100.0f)) + "R");
        drawKnobCell (g, x, row2y, "PAN", panStr,
                      (pv + 1.0f) * 0.5f,          // normalise -1..+1 → 0..1
                      locked, kLockPan, F::FieldPan, -1.f, 1.f, 0.01f, cw);
        x += cw + 4;
    }

    // FCUT — filter cutoff knob (Hz, log feel)
    {
        float gFC   = processor.apvts.getRawParameterValue (ParamIds::defaultFilterCutoff)->load();
        bool locked = (s.lockMask & kLockFilter) != 0;
        float fv    = locked ? s.filterCutoff : gFC;
        juce::String fcStr = (fv >= 10000.0f) ? juce::String ((int) (fv / 1000.0f)) + "k"
                                               : juce::String ((int) fv) + "Hz";
        // Normalise on a log scale for display
        const float logMin = std::log (20.0f), logMax = std::log (20000.0f);
        float fcNorm = (std::log (juce::jlimit (20.0f, 20000.0f, fv)) - logMin) / (logMax - logMin);
        drawKnobCell (g, x, row2y, "FCUT", fcStr,
                      fcNorm,
                      locked, kLockFilter, F::FieldFilterCutoff, 20.f, 20000.f, 1.f, cw);
        x += cw + 4;
    }

    // FRES — filter resonance knob (0..1)
    {
        float gFR   = processor.apvts.getRawParameterValue (ParamIds::defaultFilterRes)->load();
        bool locked = (s.lockMask & kLockFilter) != 0;
        float rv    = locked ? s.filterRes : gFR;
        drawKnobCell (g, x, row2y, "FRES",
                      juce::String ((int) (rv * 100.0f)) + "%",
                      rv,
                      locked, kLockFilter, F::FieldFilterRes, 0.f, 1.f, 0.01f, cw);
        x += cw + 4;
    }

    // OUT — knob
    {
        bool locked = (s.lockMask & kLockOutputBus) != 0;
        int ov      = locked ? s.outputBus : 0;
        drawKnobCell (g, x, row2y, "OUT",
                      juce::String (ov + 1),
                      toNorm (F::FieldOutputBus, (float) ov),
                      locked, kLockOutputBus, F::FieldOutputBus, 0.f, 15.f, 1.f, cw);
        x += cw + 4;
    }

    // MIDI — knob (always per-slice, lockBit=0)
    {
        drawKnobCell (g, x, row2y, "MIDI",
                      juce::String (s.midiNote),
                      toNorm (F::FieldMidiNote, (float) s.midiNote),
                      true, 0, F::FieldMidiNote, 0.f, 127.f, 1.f, cw);
    }
}

// =============================================================================
//  mouseDown
// =============================================================================
void SliceControlBar::mouseDown (const juce::MouseEvent& e)
{
    if (textEditor != nullptr) textEditor.reset();
    activeDragCell = -1;
    auto pos = e.getPosition();
    const auto& ui = processor.getUiSliceSnapshot();

    for (int i = 0; i < (int) cells.size(); ++i)
    {
        const auto& cell = cells[(size_t) i];
        if (! juce::Rectangle<int> (cell.x, cell.y, cell.w, cell.h).contains (pos)) continue;

        // MIDI Learn boundary button
        if (cell.isMidiLearnBtn)
        {
            if (e.mods.isRightButtonDown()) showMidiLearnMenu (cell.fieldId);
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
            showMidiLearnMenu (cell.fieldId); return;
        }

        if (cell.isSetBpm) { showSetBpmPopup(); return; }

        // LINK toggle
        if (cell.isLinkBtn)
        {
            bool current = processor.slicesLinked.load (std::memory_order_relaxed);
            processor.slicesLinked.store (! current, std::memory_order_relaxed);
            repaint(); return;
        }

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
            activeDragCell = i; dragStartY = pos.y;

            // Notify host of gesture start for APVTS-backed params (enables Quick Controls / MIDI Learn)
            {
                using F = DysektProcessor;
                juce::String pid;
                switch (cell.fieldId)
                {
                    case F::FieldBpm:         pid = ParamIds::defaultBpm;           break;
                    case F::FieldPitch:       pid = ParamIds::defaultPitch;         break;
                    case F::FieldCentsDetune: pid = ParamIds::defaultCentsDetune;   break;
                    case F::FieldTonality:    pid = ParamIds::defaultTonality;      break;
                    case F::FieldFormant:     pid = ParamIds::defaultFormant;       break;
                    case F::FieldAttack:      pid = ParamIds::defaultAttack;        break;
                    case F::FieldDecay:       pid = ParamIds::defaultDecay;         break;
                    case F::FieldSustain:     pid = ParamIds::defaultSustain;       break;
                    case F::FieldRelease:       pid = ParamIds::defaultRelease;       break;
                    case F::FieldMuteGroup:     pid = ParamIds::defaultMuteGroup;     break;
                    case F::FieldVolume:        pid = ParamIds::masterVolume;         break;
                    case F::FieldSliceStart:    pid = ParamIds::sliceStart;           break;
                    case F::FieldSliceEnd:      pid = ParamIds::sliceEnd;             break;
                    case F::FieldPan:           pid = ParamIds::defaultPan;           break;
                    case F::FieldFilterCutoff:  pid = ParamIds::defaultFilterCutoff;  break;
                    case F::FieldFilterRes:     pid = ParamIds::defaultFilterRes;     break;
                    default:                    break;
                }
                if (pid.isNotEmpty())
                    if (auto* p = processor.apvts.getParameter (pid))
                        p->beginChangeGesture();
            }

            // Activate live drag for slice boundary knobs
            if (cell.fieldId == DysektProcessor::FieldSliceStart || cell.fieldId == DysektProcessor::FieldSliceEnd)
            {
                int liveSel = ui.selectedSlice;
                if (liveSel >= 0 && liveSel < ui.numSlices)
                {
                    processor.liveDragBoundsStart.store (ui.slices[(size_t) liveSel].startSample, std::memory_order_relaxed);
                    processor.liveDragBoundsEnd.store   (ui.slices[(size_t) liveSel].endSample,   std::memory_order_relaxed);
                    processor.liveDragSliceIdx.store    (liveSel, std::memory_order_release);
                }
            }

            int sIdx = ui.selectedSlice;
            if (sIdx >= 0 && sIdx < ui.numSlices)
            {
                const auto& sl = ui.slices[(size_t) sIdx];
                using F = DysektProcessor;
                switch (cell.fieldId)
                {
                    case F::FieldBpm:         dragStartValue = (sl.lockMask & kLockBpm)         ? sl.bpm                  : processor.apvts.getRawParameterValue (ParamIds::defaultBpm)->load();              break;
                    case F::FieldPitch:       dragStartValue = (sl.lockMask & kLockPitch)       ? sl.pitchSemitones       : processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();            break;
                    case F::FieldCentsDetune: dragStartValue = (sl.lockMask & kLockCentsDetune) ? sl.centsDetune          : processor.apvts.getRawParameterValue (ParamIds::defaultCentsDetune)->load();      break;
                    case F::FieldTonality:    dragStartValue = (sl.lockMask & kLockTonality)    ? sl.tonalityHz           : processor.apvts.getRawParameterValue (ParamIds::defaultTonality)->load();         break;
                    case F::FieldFormant:     dragStartValue = (sl.lockMask & kLockFormant)     ? sl.formantSemitones     : processor.apvts.getRawParameterValue (ParamIds::defaultFormant)->load();          break;
                    case F::FieldAttack:      dragStartValue = (sl.lockMask & kLockAttack)      ? sl.attackSec            : processor.apvts.getRawParameterValue (ParamIds::defaultAttack)->load()  / 1000.f; break;
                    case F::FieldDecay:       dragStartValue = (sl.lockMask & kLockDecay)       ? sl.decaySec             : processor.apvts.getRawParameterValue (ParamIds::defaultDecay)->load()   / 1000.f; break;
                    case F::FieldSustain:     dragStartValue = (sl.lockMask & kLockSustain)     ? sl.sustainLevel         : processor.apvts.getRawParameterValue (ParamIds::defaultSustain)->load() / 100.f;  break;
                    case F::FieldRelease:     dragStartValue = (sl.lockMask & kLockRelease)     ? sl.releaseSec           : processor.apvts.getRawParameterValue (ParamIds::defaultRelease)->load() / 1000.f; break;
                    case F::FieldMuteGroup:   dragStartValue = (float)((sl.lockMask & kLockMuteGroup)  ? sl.muteGroup  : (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load());   break;
                    case F::FieldSliceStart:  dragStartValue = (float) sl.startSample; break;
                    case F::FieldSliceEnd:    dragStartValue = (float) sl.endSample;   break;
                    case F::FieldMidiNote:    dragStartValue = (float) sl.midiNote;              break;
                    case F::FieldVolume:      dragStartValue = (sl.lockMask & kLockVolume)      ? sl.volume               : processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();            break;
                    case F::FieldOutputBus:   dragStartValue = (float)((sl.lockMask & kLockOutputBus) ? sl.outputBus : 0); break;
                    case F::FieldPan:         dragStartValue = (sl.lockMask & kLockPan)    ? sl.pan          : processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();           break;
                    case F::FieldFilterCutoff: dragStartValue = (sl.lockMask & kLockFilter) ? sl.filterCutoff : processor.apvts.getRawParameterValue (ParamIds::defaultFilterCutoff)->load(); break;
                    case F::FieldFilterRes:   dragStartValue = (sl.lockMask & kLockFilter) ? sl.filterRes    : processor.apvts.getRawParameterValue (ParamIds::defaultFilterRes)->load();    break;
                    default:                  dragStartValue = 0.f; break;
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
                bool currentVal  = false;
                using F = DysektProcessor;
                if      (cell.fieldId == F::FieldStretchEnabled) currentVal = sliceLocked ? sl.stretchEnabled : (processor.apvts.getRawParameterValue (ParamIds::defaultStretchEnabled)->load() > 0.5f);
                else if (cell.fieldId == F::FieldFormantComp)   currentVal = sliceLocked ? sl.formantComp    : (processor.apvts.getRawParameterValue (ParamIds::defaultFormantComp)->load()    > 0.5f);
                else if (cell.fieldId == F::FieldReleaseTail)   currentVal = sliceLocked ? sl.releaseTail    : (processor.apvts.getRawParameterValue (ParamIds::defaultReleaseTail)->load()    > 0.5f);
                else if (cell.fieldId == F::FieldReverse)       currentVal = sliceLocked ? sl.reverse        : (processor.apvts.getRawParameterValue (ParamIds::defaultReverse)->load()        > 0.5f);
                else if (cell.fieldId == F::FieldOneShot)       currentVal = sliceLocked ? sl.oneShot        : (processor.apvts.getRawParameterValue (ParamIds::defaultOneShot)->load()        > 0.5f);
                DysektProcessor::Command cmd;
                cmd.type = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1 = cell.fieldId; cmd.floatParam1 = currentVal ? 0.f : 1.f;
                processor.pushCommand (cmd); repaint();
            }
            return;
        }

        // Choice cycle
        if (cell.isChoice)
        {
            int sIdx = ui.selectedSlice;
            if (sIdx >= 0 && sIdx < ui.numSlices)
            {
                const auto& sl = ui.slices[(size_t) sIdx];
                int current = 0;
                using F = DysektProcessor;
                if      (cell.fieldId == F::FieldAlgorithm) current = (sl.lockMask & kLockAlgorithm) ? sl.algorithm : (int) processor.apvts.getRawParameterValue (ParamIds::defaultAlgorithm)->load();
                else if (cell.fieldId == F::FieldGrainMode) current = (sl.lockMask & kLockGrainMode) ? sl.grainMode : (int) processor.apvts.getRawParameterValue (ParamIds::defaultGrainMode)->load();
                else if (cell.fieldId == F::FieldLoop)      current = (sl.lockMask & kLockLoop)      ? sl.loopMode  : (int) processor.apvts.getRawParameterValue (ParamIds::defaultLoop)->load();
                int next = (current + 1) > (int) cell.maxVal ? 0 : current + 1;
                DysektProcessor::Command cmd;
                cmd.type = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1 = cell.fieldId; cmd.floatParam1 = (float) next;
                processor.pushCommand (cmd); repaint();
            }
            return;
        }
    }
}

// =============================================================================
//  mouseDrag
// =============================================================================
void SliceControlBar::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDragCell < 0 || activeDragCell >= (int) cells.size()) return;
    const auto& cell = cells[(size_t) activeDragCell];
    if (! cell.isKnob) return;

    float deltaY = (float) (dragStartY - e.y);
    using F = DysektProcessor;

    // ── Slice boundary knobs: live drag in sample space ───────────────────
    if (cell.fieldId == F::FieldSliceStart || cell.fieldId == F::FieldSliceEnd)
    {
        const auto& ui2 = processor.getUiSliceSnapshot();
        int liveSel = ui2.selectedSlice;
        if (liveSel >= 0 && liveSel < ui2.numSlices && ui2.sampleNumFrames > 1)
        {
            // Scale: drag 300px = full sample length, shift = fine mode
            float sensitivity = (float) ui2.sampleNumFrames / 300.f;
            if (e.mods.isShiftDown()) sensitivity *= 0.05f;

            const auto& sl = ui2.slices[(size_t) liveSel];
            int delta = (int) (deltaY * sensitivity);

            if (cell.fieldId == F::FieldSliceStart)
            {
                int newStart = juce::jlimit (0, sl.endSample - 64,
                                             (int) dragStartValue + delta);
                processor.liveDragBoundsStart.store (newStart,       std::memory_order_relaxed);
                processor.liveDragBoundsEnd.store   (sl.endSample,   std::memory_order_relaxed);
            }
            else
            {
                int newEnd = juce::jlimit (sl.startSample + 64, ui2.sampleNumFrames,
                                           (int) dragStartValue + delta);
                processor.liveDragBoundsStart.store (sl.startSample, std::memory_order_relaxed);
                processor.liveDragBoundsEnd.store   (newEnd,         std::memory_order_relaxed);
            }
            processor.liveDragSliceIdx.store (liveSel, std::memory_order_release);
        }
        repaint(); return;
    }

    // ── All other knobs: CmdSetSliceParam ─────────────────────────────────
    bool isAdsr = (cell.fieldId == F::FieldAttack
                || cell.fieldId == F::FieldDecay
                || cell.fieldId == F::FieldSustain
                || cell.fieldId == F::FieldRelease);
    bool isBpm  = (cell.fieldId == F::FieldBpm);

    float newNative;
    if (isAdsr || isBpm)
    {
        float ds = dragStartValue, dmin = cell.minVal, dmax = cell.maxVal;
        if (cell.fieldId == F::FieldAttack  ||
            cell.fieldId == F::FieldDecay   ||
            cell.fieldId == F::FieldRelease)
        { ds *= 1000.f; dmin *= 1000.f; dmax *= 1000.f; }
        else if (cell.fieldId == F::FieldSustain)
        { ds *= 100.f; dmin *= 100.f; dmax *= 100.f; }

        float snap = e.mods.isShiftDown() ? 5.f : 1.f;
        float dv   = juce::jlimit (dmin, dmax, std::round ((ds + deltaY * 0.25f) / snap) * snap);

        if (cell.fieldId == F::FieldAttack  ||
            cell.fieldId == F::FieldDecay   ||
            cell.fieldId == F::FieldRelease)
            newNative = dv / 1000.f;
        else if (cell.fieldId == F::FieldSustain)
            newNative = dv / 100.f;
        else
            newNative = dv;
    }
    else
    {
        newNative = UIHelpers::computeDragValue (dragStartValue, deltaY,
                                                 cell.minVal, cell.maxVal,
                                                 e.mods.isShiftDown());
    }

    DysektProcessor::Command cmd;
    cmd.type = F::CmdSetSliceParam;
    cmd.intParam1 = cell.fieldId; cmd.floatParam1 = newNative;
    processor.pushCommand (cmd); repaint();
}

// =============================================================================
//  mouseUp  — commit slice bounds and deactivate live drag
// =============================================================================
void SliceControlBar::mouseUp (const juce::MouseEvent& /*e*/)
{
    using F = DysektProcessor;

    if (activeDragCell >= 0 && activeDragCell < (int) cells.size())
    {
        const auto& cell = cells[(size_t) activeDragCell];

        // Commit slice boundary drags
        if (cell.fieldId == F::FieldSliceStart || cell.fieldId == F::FieldSliceEnd)
        {
            const int liveIdx = processor.liveDragSliceIdx.load (std::memory_order_acquire);
            if (liveIdx >= 0)
            {
                F::Command cmd;
                cmd.type         = F::CmdSetSliceBounds;
                cmd.intParam1    = liveIdx;
                cmd.intParam2    = processor.liveDragBoundsStart.load (std::memory_order_relaxed);
                cmd.positions[0] = processor.liveDragBoundsEnd.load   (std::memory_order_relaxed);
                cmd.numPositions = 1;
                processor.pushCommand (cmd);
            }
        }

        // End host gesture for APVTS-backed params (enables Quick Controls / MIDI Learn)
        {
            using F = DysektProcessor;
            juce::String pid;
            switch (cell.fieldId)
            {
                case F::FieldBpm:         pid = ParamIds::defaultBpm;           break;
                case F::FieldPitch:       pid = ParamIds::defaultPitch;         break;
                case F::FieldCentsDetune: pid = ParamIds::defaultCentsDetune;   break;
                case F::FieldTonality:    pid = ParamIds::defaultTonality;      break;
                case F::FieldFormant:     pid = ParamIds::defaultFormant;       break;
                case F::FieldAttack:         pid = ParamIds::defaultAttack;        break;
                case F::FieldDecay:          pid = ParamIds::defaultDecay;         break;
                case F::FieldSustain:        pid = ParamIds::defaultSustain;       break;
                case F::FieldRelease:        pid = ParamIds::defaultRelease;       break;
                case F::FieldMuteGroup:      pid = ParamIds::defaultMuteGroup;     break;
                case F::FieldVolume:         pid = ParamIds::masterVolume;         break;
                case F::FieldSliceStart:     pid = ParamIds::sliceStart;           break;
                case F::FieldSliceEnd:       pid = ParamIds::sliceEnd;             break;
                case F::FieldPan:            pid = ParamIds::defaultPan;           break;
                case F::FieldFilterCutoff:   pid = ParamIds::defaultFilterCutoff;  break;
                case F::FieldFilterRes:      pid = ParamIds::defaultFilterRes;     break;
                default:                     break;
            }
            if (pid.isNotEmpty())
                if (auto* p = processor.apvts.getParameter (pid))
                    p->endChangeGesture();
        }
    }

    // Deactivate live drag (mirrors WaveformView::mouseUp)
    processor.liveDragSliceIdx.store (-1, std::memory_order_release);
    activeDragCell   = -1;
}

// =============================================================================
//  mouseDoubleClick
// =============================================================================
void SliceControlBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    const auto& ui = processor.getUiSliceSnapshot();

    for (int i = 0; i < (int) cells.size(); ++i)
    {
        const auto& cell = cells[(size_t) i];
        if (! juce::Rectangle<int> (cell.x, cell.y, cell.w, cell.h).contains (pos)) continue;
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
                case F::FieldBpm:         currentVal = (sl.lockMask & kLockBpm)         ? sl.bpm              : processor.apvts.getRawParameterValue (ParamIds::defaultBpm)->load();          break;
                case F::FieldPitch:       currentVal = (sl.lockMask & kLockPitch)       ? sl.pitchSemitones   : processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();        break;
                case F::FieldCentsDetune: currentVal = (sl.lockMask & kLockCentsDetune) ? sl.centsDetune      : processor.apvts.getRawParameterValue (ParamIds::defaultCentsDetune)->load();  break;
                case F::FieldTonality:    currentVal = (sl.lockMask & kLockTonality)    ? sl.tonalityHz       : processor.apvts.getRawParameterValue (ParamIds::defaultTonality)->load();     break;
                case F::FieldFormant:     currentVal = (sl.lockMask & kLockFormant)     ? sl.formantSemitones : processor.apvts.getRawParameterValue (ParamIds::defaultFormant)->load();      break;
                case F::FieldAttack:      currentVal = ((sl.lockMask & kLockAttack)  ? sl.attackSec  : processor.apvts.getRawParameterValue (ParamIds::defaultAttack)->load()  / 1000.f) * 1000.f; break;
                case F::FieldDecay:       currentVal = ((sl.lockMask & kLockDecay)   ? sl.decaySec   : processor.apvts.getRawParameterValue (ParamIds::defaultDecay)->load()   / 1000.f) * 1000.f; break;
                case F::FieldSustain:     currentVal = ((sl.lockMask & kLockSustain) ? sl.sustainLevel : processor.apvts.getRawParameterValue (ParamIds::defaultSustain)->load() / 100.f) * 100.f;  break;
                case F::FieldRelease:     currentVal = ((sl.lockMask & kLockRelease) ? sl.releaseSec  : processor.apvts.getRawParameterValue (ParamIds::defaultRelease)->load()  / 1000.f) * 1000.f; break;
                case F::FieldMuteGroup:   currentVal = (float)((sl.lockMask & kLockMuteGroup)  ? sl.muteGroup  : (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load()); break;
                case F::FieldMidiNote:    currentVal = (float) sl.midiNote;  break;
                case F::FieldVolume:       currentVal = (sl.lockMask & kLockVolume)     ? sl.volume       : processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();       break;
                case F::FieldOutputBus:    currentVal = (float)((sl.lockMask & kLockOutputBus) ? sl.outputBus : 0); break;
                case F::FieldPan:          currentVal = (sl.lockMask & kLockPan)    ? sl.pan          : processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();           break;
                case F::FieldFilterCutoff: currentVal = (sl.lockMask & kLockFilter) ? sl.filterCutoff : processor.apvts.getRawParameterValue (ParamIds::defaultFilterCutoff)->load(); break;
                case F::FieldFilterRes:    currentVal = (sl.lockMask & kLockFilter) ? sl.filterRes    : processor.apvts.getRawParameterValue (ParamIds::defaultFilterRes)->load();    break;
                default: break;
            }
        }
        showTextEditor (cell, currentVal); return;
    }
}

// =============================================================================
//  showTextEditor  (unchanged logic)
// =============================================================================
void SliceControlBar::showTextEditor (const ParamCell& cell, float currentValue)
{
    textEditor = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (*textEditor);
    textEditor->setBounds (cell.x + kParamCellTextX, cell.y + 14,
                           cell.w - kParamCellTextX - 2, 16);
    textEditor->setFont (DysektLookAndFeel::makeFont (14.0f));
    textEditor->setColour (juce::TextEditor::backgroundColourId, getTheme().darkBar.brighter (0.15f));
    textEditor->setColour (juce::TextEditor::textColourId,       getTheme().foreground);
    textEditor->setColour (juce::TextEditor::outlineColourId,    getTheme().accent);

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

    int   fieldId = cell.fieldId;
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
//  showSetBpmPopup  (unchanged)
// =============================================================================
void SliceControlBar::showSetBpmPopup()
{
    juce::PopupMenu menu;
    menu.addItem (1, "16 bars"); menu.addItem (2, "8 bars");   menu.addItem (3, "4 bars");
    menu.addItem (4, "2 bars");  menu.addItem (5, "1 bar");    menu.addItem (6, "1/2 note");
    menu.addItem (7, "1/4 note"); menu.addItem (8, "1/8 note"); menu.addItem (9, "1/16 note");

    auto* topLvl = getTopLevelComponent();
    float ms = DysektLookAndFeel::getMenuScale();
    menu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetComponent (this).withParentComponent (topLvl)
            .withStandardItemHeight ((int) (24 * ms)),
        [this] (int result) {
            if (result <= 0 || result > 9) return;
            const float bars[] = { 0.f,16.f,8.f,4.f,2.f,1.f,0.5f,0.25f,0.125f,0.0625f };
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdStretch;
            cmd.floatParam1 = bars[result];
            processor.pushCommand (cmd); repaint();
        });
}
