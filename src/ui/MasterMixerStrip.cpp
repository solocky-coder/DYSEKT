#include "MasterMixerStrip.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../params/ParamIds.h"
#include <cmath>

namespace
{
    constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kKnobEnd   = juce::MathConstants<float>::pi * 2.75f;
}

MasterMixerStrip::MasterMixerStrip (DysektProcessor& p)
    : processor (p)
{
    addAndMakeVisible (meter);
}

void MasterMixerStrip::setMeterLevels (float left, float right)
{
    meter.setLevels (left, right);
    meter.repaint();
}

// =============================================================================
float MasterMixerStrip::getMasterVolNorm() const
{
    float raw = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
    return juce::jlimit (0.0f, 1.0f, (raw + 100.0f) / 124.0f);
}

float MasterMixerStrip::getMasterPanNorm() const
{
    float raw = processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();
    return juce::jlimit (0.0f, 1.0f, (raw + 1.0f) * 0.5f);
}

void MasterMixerStrip::setMasterVol (float norm)
{
    const float native = -100.0f + juce::jlimit (0.0f, 1.0f, norm) * 124.0f;
    if (auto* param = processor.apvts.getParameter (ParamIds::masterVolume))
        param->setValueNotifyingHost (param->convertTo0to1 (native));
}

void MasterMixerStrip::setMasterPan (float norm)
{
    const float native = juce::jlimit (0.0f, 1.0f, norm) * 2.0f - 1.0f;
    if (auto* param = processor.apvts.getParameter (ParamIds::defaultPan))
        param->setValueNotifyingHost (param->convertTo0to1 (native));
}

// =============================================================================
void MasterMixerStrip::resized()
{
    const auto b = getLocalBounds();
    const int w  = b.getWidth();

    int y = 2;
    headerArea = { 0, y, w, 16 };
    y += 18;

    const int meterH = 60;
    meter.setBounds (2, y, w - 4, meterH);
    y += meterH + 4;

    const int faderH = 80;
    faderTrack = { 6, y, w - 12, faderH };
    y += faderH + 6;

    const int knobDiam = 20;
    panKnobBounds = { (w - knobDiam) / 2, y, knobDiam, knobDiam };
}

// =============================================================================
void MasterMixerStrip::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const auto  b     = getLocalBounds();

    g.setColour (theme.darkBar.brighter (0.05f));
    g.fillRoundedRectangle (b.toFloat(), 3.0f);
    g.setColour (theme.accent.withAlpha (0.4f));
    g.drawRoundedRectangle (b.toFloat(), 3.0f, 1.5f);

    // Header
    g.setFont (DysektLookAndFeel::makeFont (9.0f, true));
    g.setColour (theme.accent);
    g.drawText ("MASTER", headerArea, juce::Justification::centred);

    // Fader
    drawFader (g, faderTrack, getMasterVolNorm());

    // Pan knob
    drawKnob (g, panKnobBounds, getMasterPanNorm(), "PAN");
}

// =============================================================================
void MasterMixerStrip::drawFader (juce::Graphics& g,
                                   const juce::Rectangle<int>& track,
                                   float norm) const
{
    const auto& theme = getTheme();
    if (track.isEmpty()) return;

    g.setColour (theme.waveformBg.darker (0.2f));
    g.fillRoundedRectangle (track.reduced (3, 0).toFloat(), 2.0f);

    const int fillH = juce::roundToInt (norm * (float)track.getHeight());
    if (fillH > 0)
    {
        auto fillRect = track.reduced (3, 0).withTop (track.getBottom() - fillH);
        g.setColour (theme.accent.withAlpha (0.7f));
        g.fillRoundedRectangle (fillRect.toFloat(), 2.0f);
    }

    const int thumbY = track.getBottom() - fillH - 4;
    g.setColour (theme.foreground.withAlpha (0.9f));
    g.fillRoundedRectangle ((float)(track.getX() + 1), (float)thumbY,
                             (float)(track.getWidth() - 2), 8.0f, 2.0f);

    // 0 dB marker
    const float zeroNorm = (0.0f + 100.0f) / 124.0f;
    const int zeroDbY = track.getBottom() - juce::roundToInt (zeroNorm * (float)track.getHeight());
    g.setColour (theme.gridLine.brighter (0.5f));
    g.drawHorizontalLine (zeroDbY, (float)track.getX(), (float)track.getRight());
}

void MasterMixerStrip::drawKnob (juce::Graphics& g,
                                   const juce::Rectangle<int>& bounds,
                                   float norm,
                                   const juce::String& label) const
{
    const auto& theme = getTheme();
    if (bounds.isEmpty()) return;

    const int cx = bounds.getCentreX();
    const int cy = bounds.getCentreY();
    const int r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2 - 1;
    const float angle = kKnobStart + norm * (kKnobEnd - kKnobStart);

    juce::Path track;
    track.addArc ((float)(cx - r), (float)(cy - r),
                  (float)(r * 2), (float)(r * 2),
                  kKnobStart, kKnobEnd, true);
    g.setColour (theme.lockInactive.withAlpha (0.4f));
    g.strokePath (track, juce::PathStrokeType (1.5f));

    if (norm > 0.0f)
    {
        juce::Path fill;
        fill.addArc ((float)(cx - r), (float)(cy - r),
                     (float)(r * 2), (float)(r * 2),
                     kKnobStart, angle, true);
        g.setColour (theme.accent);
        g.strokePath (fill, juce::PathStrokeType (2.0f));
    }

    const float pLen = (float)r * 0.6f;
    g.setColour (theme.foreground.withAlpha (0.9f));
    g.drawLine ((float)cx, (float)cy,
                (float)cx + std::sin (angle) * pLen,
                (float)cy - std::cos (angle) * pLen, 1.5f);

    g.setColour (theme.foreground.withAlpha (0.5f));
    g.setFont (DysektLookAndFeel::makeFont (7.5f));
    g.drawText (label, bounds.withTop (bounds.getBottom()).withHeight (10),
                juce::Justification::centred);
}

// =============================================================================
void MasterMixerStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    draggingFader = draggingPan = false;

    if (faderTrack.contains (pt))
    {
        draggingFader = true;
        dragStartY    = e.getScreenY();
        dragStartVal  = getMasterVolNorm();
    }
    else if (panKnobBounds.contains (pt))
    {
        draggingPan  = true;
        dragStartY   = e.getScreenY();
        dragStartVal = getMasterPanNorm();
    }
}

void MasterMixerStrip::mouseDrag (const juce::MouseEvent& e)
{
    const float delta = (float)(dragStartY - e.getScreenY()) / 150.0f;
    if (draggingFader)
    {
        setMasterVol (juce::jlimit (0.0f, 1.0f, dragStartVal + delta));
        repaint();
    }
    else if (draggingPan)
    {
        setMasterPan (juce::jlimit (0.0f, 1.0f, dragStartVal + delta));
        repaint();
    }
}

void MasterMixerStrip::mouseUp (const juce::MouseEvent&)
{
    draggingFader = draggingPan = false;
}

void MasterMixerStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (faderTrack.contains (pt))    { setMasterVol (0.808f); repaint(); }  // 0 dB
    else if (panKnobBounds.contains (pt)) { setMasterPan (0.5f); repaint(); }   // centre
}

void MasterMixerStrip::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& wheel)
{
    const auto pt = e.getPosition();
    if (faderTrack.contains (pt))
        { setMasterVol (juce::jlimit (0.0f, 1.0f, getMasterVolNorm() + wheel.deltaY * 0.05f)); repaint(); }
    else if (panKnobBounds.contains (pt))
        { setMasterPan (juce::jlimit (0.0f, 1.0f, getMasterPanNorm() + wheel.deltaY * 0.05f)); repaint(); }
}
