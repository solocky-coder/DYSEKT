#include "MixerStrip.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace
{
    constexpr int kKnobR = 8;     // knob radius
    constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kKnobEnd   = juce::MathConstants<float>::pi * 2.75f;
}

MixerStrip::MixerStrip (DysektProcessor& p, int idx)
    : processor (p), sliceIdx (idx)
{
    addAndMakeVisible (meter);
}

void MixerStrip::setSliceIndex (int idx) { sliceIdx = idx; repaint(); }
void MixerStrip::setSelected   (bool sel) { selected = sel; repaint(); }

void MixerStrip::setMeterLevels (float left, float right)
{
    meter.setLevels (left, right);
    meter.repaint();
}

// =============================================================================
void MixerStrip::buildLayout()
{
    const auto b  = getLocalBounds();
    const int  w  = b.getWidth();
    int y = 4;

    // ── Channel number label ───────────────────────────────────────────────
    channelLabelArea = { 0, y, w, 14 };
    y += 16;

    // ── Three knobs stacked: PAN, CUT, RES ────────────────────────────────
    // Each knob row = knob circle + label below
    const int knobDiam  = 22;
    const int knobLblH  = 11;
    const int knobRowH  = knobDiam + knobLblH + 3;
    const int knobX     = (w - knobDiam) / 2;

    knobs[KnobPan].bounds    = { knobX, y, knobDiam, knobDiam };  y += knobRowH;
    knobs[KnobCutoff].bounds = { knobX, y, knobDiam, knobDiam };  y += knobRowH;
    knobs[KnobRes].bounds    = { knobX, y, knobDiam, knobDiam };  y += knobRowH + 2;

    knobs[KnobVolume].bounds = { 0, 0, 0, 0 }; // volume uses fader only

    // ── Fader + meter side-by-side ─────────────────────────────────────────
    // Remaining height after knobs and bottom buttons
    const int bottomH   = 14 + 4 + 16 + 4; // M/S row + bus row
    const int faderAreaH = b.getHeight() - y - bottomH;
    const int meterW    = 8;
    const int faderW    = w - meterW - 6;

    faderTrack = { 3, y, faderW, faderAreaH };
    meter.setBounds (3 + faderW + 2, y, meterW, faderAreaH);
    y += faderAreaH + 4;

    // ── Mute / Solo ────────────────────────────────────────────────────────
    const int btnW = (w - 6) / 2;
    muteBtn = { 2,        y, btnW, 14 };
    soloBtn = { 4 + btnW, y, btnW, 14 };
    y += 18;

    // ── Output bus ─────────────────────────────────────────────────────────
    outputBusArea = { 2, y, w - 4, 14 };
}

// =============================================================================
void MixerStrip::resized()
{
    buildLayout();
}

// =============================================================================
float MixerStrip::logFreqToNorm (float hz) const
{
    return juce::jlimit (0.0f, 1.0f,
        (std::log2 (hz / 20.0f) / std::log2 (20000.0f / 20.0f)));
}

float MixerStrip::normToLogFreq (float norm) const
{
    return 20.0f * std::pow (20000.0f / 20.0f, juce::jlimit (0.0f, 1.0f, norm));
}

float MixerStrip::toNorm (KnobId id, float native) const
{
    switch (id)
    {
        case KnobVolume: return juce::jlimit (0.0f, 1.0f, (native + 100.0f) / 124.0f);
        case KnobPan:    return juce::jlimit (0.0f, 1.0f, (native + 1.0f) * 0.5f);
        case KnobCutoff: return logFreqToNorm (native);
        case KnobRes:    return juce::jlimit (0.0f, 1.0f, native);
        default:         return 0.0f;
    }
}

float MixerStrip::fromNorm (KnobId id, float norm) const
{
    const float n = juce::jlimit (0.0f, 1.0f, norm);
    switch (id)
    {
        case KnobVolume: return -100.0f + n * 124.0f;
        case KnobPan:    return n * 2.0f - 1.0f;
        case KnobCutoff: return normToLogFreq (n);
        case KnobRes:    return n;
        default:         return 0.0f;
    }
}

float MixerStrip::getSliceValue (KnobId id) const
{
    const auto& ui = processor.getUiSliceSnapshot();
    if (sliceIdx < 0 || sliceIdx >= ui.numSlices) return 0.0f;
    const auto& s = ui.slices[(size_t)sliceIdx];

    switch (id)
    {
        case KnobVolume: return s.volume;
        case KnobPan:    return s.pan;
        case KnobCutoff: return s.filterCutoff;
        case KnobRes:    return s.filterRes;
        default:         return 0.0f;
    }
}

void MixerStrip::pushSliceParam (KnobId id, float nativeVal)
{
    const auto& ui = processor.getUiSliceSnapshot();
    if (sliceIdx < 0 || sliceIdx >= ui.numSlices) return;

    // Select this slice so the command affects it
    {
        DysektProcessor::Command sel;
        sel.type      = DysektProcessor::CmdSelectSlice;
        sel.intParam1 = sliceIdx;
        processor.pushCommand (sel);
    }

    DysektProcessor::Command cmd;
    cmd.type = DysektProcessor::CmdSetSliceParam;
    switch (id)
    {
        case KnobVolume: cmd.intParam1 = DysektProcessor::FieldVolume;
                         cmd.floatParam1 = nativeVal; break;
        case KnobPan:    cmd.intParam1 = DysektProcessor::FieldPan;
                         cmd.floatParam1 = nativeVal; break;
        case KnobCutoff: cmd.intParam1 = DysektProcessor::FieldFilterCutoff;
                         cmd.floatParam1 = nativeVal; break;
        case KnobRes:    cmd.intParam1 = DysektProcessor::FieldFilterRes;
                         cmd.floatParam1 = nativeVal; break;
        default: return;
    }
    processor.pushCommand (cmd);
}

// =============================================================================
void MixerStrip::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const auto  b     = getLocalBounds();

    // ── Background ──────────────────────────────────────────────────────────
    juce::ColourGradient bg (theme.darkBar.brighter (0.06f), 0, 0,
                              theme.darkBar.darker   (0.1f), 0, (float) b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);

    // Border — accent when selected, subtle otherwise
    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.70f));
        g.drawRoundedRectangle (b.reduced (1).toFloat(), 4.0f, 1.5f);
        g.setColour (theme.accent.withAlpha (0.10f));
        g.fillRoundedRectangle (b.reduced (1).toFloat(), 4.0f);
    }
    else
    {
        g.setColour (theme.accent.withAlpha (0.18f));
        g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);
    }

    const auto& ui = processor.getUiSliceSnapshot();
    const bool hasSlice = (sliceIdx >= 0 && sliceIdx < ui.numSlices);

    // ── Channel label ────────────────────────────────────────────────────────
    {
        g.setFont (DysektLookAndFeel::makeFont (9.0f, true));
        juce::String label = hasSlice ? juce::String (sliceIdx + 1) : "-";
        g.setColour (selected ? theme.accent : theme.foreground.withAlpha (0.65f));
        g.drawText (label, channelLabelArea, juce::Justification::centred);
    }

    if (! hasSlice) return;

    const auto& sl = ui.slices[(size_t)sliceIdx];

    // ── Knobs: PAN, CUT, RES (top to bottom) ─────────────────────────────────
    struct KnobSpec { KnobId id; const char* label; LockBit lock; };
    KnobSpec specs[] = {
        { KnobPan,    "PAN", kLockPan    },
        { KnobCutoff, "CUT", kLockFilter },
        { KnobRes,    "RES", kLockFilter },
    };
    for (auto& s : specs)
    {
        const bool locked = (sl.lockMask & s.lock) != 0;
        const float norm  = toNorm (s.id, getSliceValue (s.id));
        KnobArea ka;
        ka.bounds = knobs[s.id].bounds;
        ka.value  = norm;
        drawKnob (g, ka, s.label, locked);
    }

    // ── Fader ────────────────────────────────────────────────────────────────
    {
        const bool  locked = (sl.lockMask & kLockVolume) != 0;
        const float norm   = toNorm (KnobVolume, sl.volume);
        drawFader (g, faderTrack, norm, locked);
    }

    // ── Mute button ──────────────────────────────────────────────────────────
    {
        g.setColour (theme.button.brighter (0.05f));
        g.fillRoundedRectangle (muteBtn.toFloat(), 2.0f);
        g.setColour (theme.accent.withAlpha (0.25f));
        g.drawRoundedRectangle (muteBtn.toFloat(), 2.0f, 0.8f);
        g.setColour (theme.foreground.withAlpha (0.65f));
        g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
        g.drawText ("M", muteBtn, juce::Justification::centred);
    }

    // ── Solo button ───────────────────────────────────────────────────────────
    {
        g.setColour (theme.button.brighter (0.05f));
        g.fillRoundedRectangle (soloBtn.toFloat(), 2.0f);
        g.setColour (theme.accent.withAlpha (0.25f));
        g.drawRoundedRectangle (soloBtn.toFloat(), 2.0f, 0.8f);
        g.setColour (theme.foreground.withAlpha (0.65f));
        g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
        g.drawText ("S", soloBtn, juce::Justification::centred);
    }

    // ── Output bus ────────────────────────────────────────────────────────────
    {
        g.setColour (theme.button);
        g.fillRoundedRectangle (outputBusArea.toFloat(), 2.0f);
        g.setColour (theme.foreground.withAlpha (0.55f));
        g.setFont (DysektLookAndFeel::makeFont (7.5f));
        g.drawText ("BUS " + juce::String (sl.outputBus + 1),
                    outputBusArea, juce::Justification::centred);
    }

    // ── Lock indicator ────────────────────────────────────────────────────────
    if (sl.lockMask & (kLockVolume | kLockPan | kLockFilter | kLockOutputBus))
    {
        g.setColour (theme.lockActive);
        g.fillEllipse ((float)(getWidth() - 7), 4.0f, 5.0f, 5.0f);
    }
}

// =============================================================================
void MixerStrip::drawKnob (juce::Graphics& g, const KnobArea& ka,
                            const juce::String& label, bool locked) const
{
    const auto& theme = getTheme();
    const auto  b     = ka.bounds;
    if (b.isEmpty()) return;

    const float cx = (float) b.getCentreX();
    const float cy = (float) b.getCentreY();
    const float r  = (float) (juce::jmin (b.getWidth(), b.getHeight()) / 2) - 1.5f;

    const float angle = kKnobStart + ka.value * (kKnobEnd - kKnobStart);

    // Knob body
    g.setColour (theme.button.brighter (0.15f));
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (theme.accent.withAlpha (0.20f));
    g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.0f);

    // Arc track
    juce::Path track;
    track.addCentredArc (cx, cy, r - 1.5f, r - 1.5f, 0.f, kKnobStart, kKnobEnd, true);
    g.setColour (theme.darkBar.brighter (0.4f));
    g.strokePath (track, juce::PathStrokeType (1.5f));

    // Filled arc
    juce::Path fill;
    fill.addCentredArc (cx, cy, r - 1.5f, r - 1.5f, 0.f, kKnobStart, angle, true);
    g.setColour (locked ? theme.lockActive : theme.accent);
    g.strokePath (fill, juce::PathStrokeType (2.0f));

    // Pointer
    const float pLen = r * 0.55f;
    g.setColour (theme.foreground.withAlpha (0.95f));
    g.drawLine (cx, cy,
                cx + std::sin (angle) * pLen,
                cy - std::cos (angle) * pLen, 1.5f);

    // Label below knob
    g.setColour (theme.foreground.withAlpha (0.50f));
    g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
    g.drawText (label,
                b.getX() - 4, b.getBottom() + 1, b.getWidth() + 8, 10,
                juce::Justification::centred);
}

// =============================================================================
void MixerStrip::drawFader (juce::Graphics& g, const juce::Rectangle<int>& track,
                              float norm, bool locked) const
{
    const auto& theme = getTheme();
    if (track.isEmpty()) return;

    // Track groove (narrow, centred)
    const int grooveX = track.getCentreX() - 2;
    g.setColour (theme.darkBar.darker (0.3f));
    g.fillRoundedRectangle ((float) grooveX, (float) track.getY(),
                             4.0f, (float) track.getHeight(), 2.0f);
    g.setColour (theme.accent.withAlpha (0.12f));
    g.drawRoundedRectangle ((float) grooveX, (float) track.getY(),
                             4.0f, (float) track.getHeight(), 2.0f, 0.8f);

    // 0 dB tick mark
    const int zeroDbY = track.getBottom() - juce::roundToInt (
        toNorm (KnobVolume, 0.0f) * (float) track.getHeight());
    g.setColour (theme.accent.withAlpha (0.35f));
    g.drawHorizontalLine (zeroDbY, (float)(grooveX - 4), (float)(grooveX + 8));

    // Fader thumb — wide pill like reference image
    const int thumbH = 16;
    const int thumbY = track.getBottom() - juce::roundToInt (norm * (float) track.getHeight()) - thumbH / 2;
    const int thumbX = track.getX() + 1;
    const int thumbW = track.getWidth() - 2;

    // Thumb shadow
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle ((float) thumbX + 1, (float) thumbY + 2,
                             (float) thumbW, (float) thumbH, 3.0f);

    // Thumb body
    juce::ColourGradient thumbGrad (
        theme.foreground.withAlpha (0.92f), (float) thumbX, (float) thumbY,
        theme.foreground.withAlpha (0.60f), (float) thumbX, (float)(thumbY + thumbH), false);
    g.setGradientFill (thumbGrad);
    g.fillRoundedRectangle ((float) thumbX, (float) thumbY,
                             (float) thumbW, (float) thumbH, 3.0f);

    // Thumb centre line
    g.setColour ((locked ? theme.lockActive : theme.accent).withAlpha (0.6f));
    g.drawHorizontalLine (thumbY + thumbH / 2,
                          (float)(thumbX + 4), (float)(thumbX + thumbW - 4));
}

// =============================================================================
// Mouse interactions
// =============================================================================
void MixerStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();

    // Select this strip
    DysektProcessor::Command sel;
    sel.type      = DysektProcessor::CmdSelectSlice;
    sel.intParam1 = sliceIdx;
    processor.pushCommand (sel);

    // Fader drag?
    if (faderTrack.contains (pt))
    {
        activeDrag = KnobVolume;
        knobs[KnobVolume].isDragging   = true;
        knobs[KnobVolume].dragStartY   = e.getScreenY();
        knobs[KnobVolume].dragStartVal = toNorm (KnobVolume, getSliceValue (KnobVolume));
        return;
    }

    // Knob drag?
    const KnobId ids[] = { KnobPan, KnobCutoff, KnobRes };
    for (KnobId kid : ids)
    {
        if (knobs[kid].bounds.contains (pt))
        {
            activeDrag = kid;
            knobs[kid].isDragging   = true;
            knobs[kid].dragStartY   = e.getScreenY();
            knobs[kid].dragStartVal = toNorm (kid, getSliceValue (kid));
            return;
        }
    }

    // Output bus click?
    if (outputBusArea.contains (pt))
    {
        const auto& ui = processor.getUiSliceSnapshot();
        if (sliceIdx >= 0 && sliceIdx < ui.numSlices)
        {
            const int curBus = ui.slices[(size_t)sliceIdx].outputBus;
            const int newBus = (curBus + 1) % 16;
            DysektProcessor::Command cmd;
            cmd.type        = DysektProcessor::CmdSetSliceParam;
            cmd.intParam1   = DysektProcessor::FieldOutputBus;
            cmd.floatParam1 = (float)newBus;
            processor.pushCommand (cmd);
        }
    }
}

void MixerStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag < 0) return;

    auto& ka = knobs[(size_t)activeDrag];
    if (! ka.isDragging) return;

    const float drag = (float)(ka.dragStartY - e.getScreenY()) / 150.0f;
    const float norm = juce::jlimit (0.0f, 1.0f, ka.dragStartVal + drag);
    pushSliceParam ((KnobId)activeDrag, fromNorm ((KnobId)activeDrag, norm));
    repaint();
}

void MixerStrip::mouseUp (const juce::MouseEvent&)
{
    if (activeDrag >= 0)
        knobs[(size_t)activeDrag].isDragging = false;
    activeDrag = -1;
}

void MixerStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    // Reset fader to 0 dB
    if (faderTrack.contains (pt))
    {
        pushSliceParam (KnobVolume, 0.0f);
        repaint();
    }
    // Reset pan to centre
    else if (knobs[KnobPan].bounds.contains (pt))
    {
        pushSliceParam (KnobPan, 0.0f);
        repaint();
    }
    // Reset cutoff to max
    else if (knobs[KnobCutoff].bounds.contains (pt))
    {
        pushSliceParam (KnobCutoff, 20000.0f);
        repaint();
    }
    // Reset resonance to 0
    else if (knobs[KnobRes].bounds.contains (pt))
    {
        pushSliceParam (KnobRes, 0.0f);
        repaint();
    }
}

void MixerStrip::mouseWheelMove (const juce::MouseEvent& e,
                                  const juce::MouseWheelDetails& wheel)
{
    const auto pt = e.getPosition();

    KnobId kid = (KnobId)-1;
    if (faderTrack.contains (pt))            kid = KnobVolume;
    else if (knobs[KnobPan].bounds.contains (pt))    kid = KnobPan;
    else if (knobs[KnobCutoff].bounds.contains (pt)) kid = KnobCutoff;
    else if (knobs[KnobRes].bounds.contains (pt))    kid = KnobRes;

    if ((int)kid >= 0)
    {
        const float delta = wheel.deltaY * 0.05f;
        const float norm  = juce::jlimit (0.0f, 1.0f,
            toNorm (kid, getSliceValue (kid)) + delta);
        pushSliceParam (kid, fromNorm (kid, norm));
        repaint();
    }
}
