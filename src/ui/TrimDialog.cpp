#include "TrimDialog.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"

TrimDialog::TrimDialog (DysektProcessor& proc, WaveformView& wv)
    : processor (proc), waveformView (wv)
{
    applyBtn.setColour (juce::TextButton::buttonColourId,  getTheme().accent.withAlpha (0.8f));
    applyBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    applyBtn.onClick = [this] { onApply(); };
    addAndMakeVisible (applyBtn);

    cancelBtn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
    cancelBtn.setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    cancelBtn.onClick = [this] { onCancel(); };
    addAndMakeVisible (cancelBtn);

    startTimerHz (30);
}

TrimDialog::~TrimDialog()
{
    stopTimer();
}

void TrimDialog::timerCallback()
{
    // Repaint so the IN/OUT knobs stay in sync with MIDI-driven changes
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Draw a single trim knob cell (IN or OUT)
// ─────────────────────────────────────────────────────────────────────────────
void TrimDialog::drawTrimKnob (juce::Graphics& g, juce::Rectangle<int> cell,
                                const char* label, int sampleVal, int totalFrames)
{
    const auto& theme = getTheme();
    const float norm = (totalFrames > 0)
        ? juce::jlimit (0.0f, 1.0f, (float) sampleVal / (float) totalFrames)
        : 0.0f;

    // Background
    g.setColour (theme.darkBar.withAlpha (0.6f));
    g.fillRoundedRectangle (cell.toFloat(), 3.0f);
    g.setColour (theme.separator);
    g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 3.0f, 1.0f);

    // Knob arc
    const int kR = 10;
    const int cx = cell.getCentreX();
    const int arcY = cell.getY() + 4;
    const juce::Point<float> centre ((float)cx, (float)(arcY + kR));
    const float startA = juce::MathConstants<float>::pi * 0.75f;
    const float endA   = juce::MathConstants<float>::pi * 2.25f;
    const float curA   = startA + norm * (endA - startA);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, (float)kR, (float)kR, 0.0f, startA, endA, true);
    g.setColour (theme.separator);
    g.strokePath (track, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    juce::Path fill;
    fill.addCentredArc (centre.x, centre.y, (float)kR, (float)kR, 0.0f, startA, curA, true);
    g.setColour (theme.accent);
    g.strokePath (fill, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Tick
    const float tickLen = 5.0f;
    const float tx = centre.x + (float)(kR + 2) * std::cos (curA - juce::MathConstants<float>::halfPi);
    const float ty = centre.y + (float)(kR + 2) * std::sin (curA - juce::MathConstants<float>::halfPi);
    const float tx2 = centre.x + (float)(kR + 2 - tickLen) * std::cos (curA - juce::MathConstants<float>::halfPi);
    const float ty2 = centre.y + (float)(kR + 2 - tickLen) * std::sin (curA - juce::MathConstants<float>::halfPi);
    g.setColour (theme.accent.brighter (0.4f));
    g.drawLine (tx, ty, tx2, ty2, 1.5f);

    // Label (top)
    g.setFont (DysektLookAndFeel::makeFont (7.5f));
    g.setColour (theme.accent.withAlpha (0.75f));
    g.drawText (label, cell.getX(), cell.getY() + 1, cell.getWidth(), 10,
                juce::Justification::centred);

    // Value (bottom)
    g.setFont (DysektLookAndFeel::makeFont (7.0f));
    g.setColour (theme.foreground.withAlpha (0.7f));
    g.drawText (juce::String (sampleVal), cell.getX(), cell.getBottom() - 11,
                cell.getWidth(), 10, juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────────
void TrimDialog::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().header);
    g.setColour (getTheme().accent.withAlpha (0.3f));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);

    const int total = processor.sampleData.getNumFrames();
    const int inPt  = processor.trimRegionStart.load (std::memory_order_relaxed);
    const int outPt = processor.trimRegionEnd  .load (std::memory_order_relaxed);

    drawTrimKnob (g, inCell,  "IN",  inPt,  total);
    drawTrimKnob (g, outCell, "OUT", outPt, total);
}

void TrimDialog::resized()
{
    auto b = getLocalBounds().reduced (6, 4);
    const int btnW  = 76;
    const int knobW = 40;
    const int knobH = b.getHeight();
    const int gap   = 4;

    cancelBtn.setBounds (b.removeFromRight (btnW));
    b.removeFromRight (gap);
    applyBtn.setBounds (b.removeFromRight (btnW));
    b.removeFromRight (gap * 3);

    outCell = b.removeFromRight (knobW).withHeight (knobH);
    b.removeFromRight (gap);
    inCell  = b.removeFromRight (knobW).withHeight (knobH);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse handling — drag IN/OUT knobs
// ─────────────────────────────────────────────────────────────────────────────
void TrimDialog::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    if (inCell.contains (pos))
    {
        activeDrag   = 0;
        dragStartY   = pos.y;
        dragStartVal = processor.trimRegionStart.load (std::memory_order_relaxed);
    }
    else if (outCell.contains (pos))
    {
        activeDrag   = 1;
        dragStartY   = pos.y;
        dragStartVal = processor.trimRegionEnd.load (std::memory_order_relaxed);
    }
}

void TrimDialog::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag < 0) return;

    const int total = processor.sampleData.getNumFrames();
    if (total <= 0) return;

    float sensitivity = (float) total / 300.f;
    if (e.mods.isShiftDown()) sensitivity *= 0.05f;

    const int delta = (int) ((dragStartY - e.getPosition().y) * sensitivity);

    if (activeDrag == 0)
    {
        const int curEnd = processor.trimRegionEnd.load (std::memory_order_relaxed);
        const int newIn  = juce::jlimit (0, curEnd - 64, dragStartVal + delta);
        processor.trimRegionStart.store (newIn, std::memory_order_relaxed);
        waveformView.setTrimPoints (newIn, curEnd);
    }
    else
    {
        const int curIn  = processor.trimRegionStart.load (std::memory_order_relaxed);
        const int newOut = juce::jlimit (curIn + 64, total, dragStartVal + delta);
        processor.trimRegionEnd.store (newOut, std::memory_order_relaxed);
        waveformView.setTrimPoints (curIn, newOut);
    }

    repaint();
}

void TrimDialog::mouseUp (const juce::MouseEvent&)
{
    activeDrag = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
void TrimDialog::onApply()
{
    const int inPt  = processor.trimRegionStart.load (std::memory_order_relaxed);
    const int outPt = processor.trimRegionEnd  .load (std::memory_order_relaxed);
    if (waveformView.onTrimApplied)
        waveformView.onTrimApplied (inPt, outPt);
}

void TrimDialog::onCancel()
{
    if (waveformView.onTrimCancelled)
        waveformView.onTrimCancelled();
}
