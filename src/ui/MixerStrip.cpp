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
    int y = 2;

    // Channel number label
    channelLabelArea = { 0, y, w, 16 };
    y += 18;

    // VU Meter (narrow strip)
    const int meterH = 60;
    meter.setBounds (2, y, w - 4, meterH);
    y += meterH + 4;

    // Volume fader track (taller — main control)
    const int faderH = 80;
    faderTrack = { 6, y, w - 12, faderH };
    y += faderH + 6;

    // Knobs: Pan, Cutoff, Res  (3 small knobs in a row)
    const int knobDiam = kKnobR * 2 + 4;
    const int knobSpacing = (w - 3 * knobDiam) / 4;
    for (int i = 0; i < 3; ++i)
    {
        knobs[(size_t)(KnobPan + i)].bounds = {
            knobSpacing + i * (knobDiam + knobSpacing), y, knobDiam, knobDiam };
    }
    // Volume knob uses fader, skip KnobVolume bounds for now
    knobs[KnobVolume].bounds = { 0, 0, 0, 0 };
    y += knobDiam + 18;  // +18 for label

    // Mute / Solo buttons
    const int btnW = (w - 6) / 2;
    const int btnH = 18;
    muteBtn = { 2,       y, btnW, btnH };
    soloBtn = { 4 + btnW, y, btnW, btnH };
    y += btnH + 4;

    // Output bus selector
    outputBusArea = { 2, y, w - 4, 16 };
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

    // Background
    g.setColour (selected ? theme.button.brighter (0.12f) : theme.darkBar);
    g.fillRoundedRectangle (b.toFloat(), 3.0f);

    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.6f));
        g.drawRoundedRectangle (b.reduced (1).toFloat(), 3.0f, 1.5f);
    }
    else
    {
        g.setColour (theme.separator);
        g.drawRoundedRectangle (b.toFloat(), 3.0f, 1.0f);
    }

    const auto& ui = processor.getUiSliceSnapshot();
    const bool hasSlice = (sliceIdx >= 0 && sliceIdx < ui.numSlices);

    // Channel label
    {
        g.setFont (DysektLookAndFeel::makeFont (9.0f, true));
        juce::String label = hasSlice ? juce::String (sliceIdx + 1) : "-";
        g.setColour (selected ? theme.accent : theme.foreground.withAlpha (0.7f));
        g.drawText (label, channelLabelArea, juce::Justification::centred);
    }

    if (! hasSlice) return;

    const auto& sl = ui.slices[(size_t)sliceIdx];

    // Draw fader track
    {
        const bool locked = (sl.lockMask & kLockVolume) != 0;
        const float norm = toNorm (KnobVolume, sl.volume);
        drawFader (g, faderTrack, norm, locked);
    }

    // Draw knobs: Pan, Cutoff, Res
    const KnobId ids[] = { KnobPan, KnobCutoff, KnobRes };
    const char* labels[] = { "PAN", "CUT", "RES" };
    const LockBit lockBits[] = { kLockPan, kLockFilter, kLockFilter };

    for (int i = 0; i < 3; ++i)
    {
        KnobId kid = ids[i];
        const bool locked = (sl.lockMask & lockBits[i]) != 0;
        const float norm = toNorm (kid, getSliceValue (kid));
        KnobArea ka;
        ka.bounds = knobs[kid].bounds;
        ka.value  = norm;
        drawKnob (g, ka, labels[i], locked);
    }

    // Mute button
    {
        // "muted" state not stored in Slice yet, so just show stub
        g.setColour (theme.button);
        g.fillRoundedRectangle (muteBtn.toFloat(), 2.0f);
        g.setColour (theme.foreground.withAlpha (0.7f));
        g.setFont (DysektLookAndFeel::makeFont (8.0f));
        g.drawText ("M", muteBtn, juce::Justification::centred);
    }

    // Solo button
    {
        g.setColour (theme.button);
        g.fillRoundedRectangle (soloBtn.toFloat(), 2.0f);
        g.setColour (theme.foreground.withAlpha (0.7f));
        g.setFont (DysektLookAndFeel::makeFont (8.0f));
        g.drawText ("S", soloBtn, juce::Justification::centred);
    }

    // Output bus
    {
        g.setColour (theme.button);
        g.fillRoundedRectangle (outputBusArea.toFloat(), 2.0f);
        g.setColour (theme.foreground.withAlpha (0.6f));
        g.setFont (DysektLookAndFeel::makeFont (8.0f));
        g.drawText ("BUS " + juce::String (sl.outputBus + 1),
                    outputBusArea, juce::Justification::centred);
    }

    // Lock indicator (small dot top-right)
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

    const int cx = b.getCentreX();
    const int cy = b.getCentreY();
    const int r  = juce::jmin (b.getWidth(), b.getHeight()) / 2 - 1;

    const float angle = kKnobStart + ka.value * (kKnobEnd - kKnobStart);

    // Track arc
    juce::Path track;
    track.addArc ((float)(cx - r), (float)(cy - r),
                  (float)(r * 2), (float)(r * 2),
                  kKnobStart, kKnobEnd, true);
    g.setColour (theme.lockInactive.withAlpha (0.4f));
    g.strokePath (track, juce::PathStrokeType (1.5f));

    // Fill arc
    if (ka.value > 0.0f)
    {
        juce::Path fill;
        fill.addArc ((float)(cx - r), (float)(cy - r),
                     (float)(r * 2), (float)(r * 2),
                     kKnobStart, angle, true);
        g.setColour (locked ? theme.lockActive : theme.accent);
        g.strokePath (fill, juce::PathStrokeType (2.0f));
    }

    // Pointer line
    const float pLen = (float)r * 0.6f;
    g.setColour (theme.foreground.withAlpha (0.9f));
    g.drawLine ((float)cx, (float)cy,
                (float)cx + std::sin (angle) * pLen,
                (float)cy - std::cos (angle) * pLen, 1.5f);

    // Label below
    g.setColour (theme.foreground.withAlpha (0.5f));
    g.setFont (DysektLookAndFeel::makeFont (7.5f));
    g.drawText (label, b.withTop (b.getBottom()).withHeight (10),
                juce::Justification::centred);
}

// =============================================================================
void MixerStrip::drawFader (juce::Graphics& g, const juce::Rectangle<int>& track,
                              float norm, bool locked) const
{
    const auto& theme = getTheme();
    if (track.isEmpty()) return;

    // Track background
    g.setColour (theme.waveformBg.darker (0.2f));
    g.fillRoundedRectangle (track.reduced (3, 0).toFloat(), 2.0f);

    // Fill level
    const int fillH = juce::roundToInt (norm * (float)track.getHeight());
    if (fillH > 0)
    {
        auto fillRect = track.reduced (3, 0)
                             .withTop (track.getBottom() - fillH);
        g.setColour (locked ? theme.lockActive.withAlpha (0.7f)
                            : theme.accent.withAlpha (0.6f));
        g.fillRoundedRectangle (fillRect.toFloat(), 2.0f);
    }

    // Fader thumb
    const int thumbY = track.getBottom() - fillH - 4;
    g.setColour (theme.foreground.withAlpha (0.9f));
    g.fillRoundedRectangle ((float)(track.getX() + 1), (float)thumbY,
                             (float)(track.getWidth() - 2), 8.0f, 2.0f);

    // "0 dB" marker line
    const int zeroDbY = track.getBottom() - juce::roundToInt (
        toNorm (KnobVolume, 0.0f) * (float)track.getHeight());
    g.setColour (theme.gridLine.brighter (0.5f));
    g.drawHorizontalLine (zeroDbY, (float)track.getX(), (float)track.getRight());
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
