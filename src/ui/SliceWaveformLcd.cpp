#include "SliceWaveformLcd.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── Theme-derived colours ─────────────────────────────────────────────────────
// Called once per paint() — all colours come from getTheme() so they
// update automatically when the user switches theme.
static juce::Colour lcd2Bg()       { return getTheme().darkBar.darker (0.55f); }
static juce::Colour lcd2Bezel()    { return lcd2Bg().brighter (0.12f); }
static juce::Colour lcd2Phosphor() { return getTheme().accent; }
static juce::Colour lcd2Dim()      { return getTheme().accent.withAlpha (0.15f).overlaidWith (lcd2Bg()); }
static juce::Colour lcd2Bright()   { return getTheme().accent.brighter (0.45f); }
static juce::Colour lcd2Label()    { return getTheme().accent.withAlpha (0.55f); }

// Keep the static const members as aliases so the .h declarations compile
const juce::Colour SliceWaveformLcd::kBg       { 0xFF050F0E };
const juce::Colour SliceWaveformLcd::kBezel    { 0xFF0D1E1C };
const juce::Colour SliceWaveformLcd::kPhosphor { 0xFF2AFFD0 };
const juce::Colour SliceWaveformLcd::kDim      { 0xFF0A2A22 };
const juce::Colour SliceWaveformLcd::kBright   { 0xFF8AFFF0 };
const juce::Colour SliceWaveformLcd::kLabel    { 0xFF1A7060 };

// ── Helpers ───────────────────────────────────────────────────────────────────

juce::String SliceWaveformLcd::midiNoteName (int note)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    note = juce::jlimit (0, 127, note);
    return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
}

juce::String SliceWaveformLcd::formatMs (float secs)
{
    float ms = secs * 1000.0f;
    if (ms < 1000.0f) return juce::String (juce::roundToInt (ms)) + "ms";
    return juce::String (ms / 1000.0f, 2) + "s";
}

juce::String SliceWaveformLcd::formatAlgo (int algo)
{
    switch (algo) { case 0: return "TIME"; case 1: return "GRAN"; case 2: return "SPEC"; }
    return "----";
}

juce::String SliceWaveformLcd::formatPan (float pan)
{
    if (std::abs (pan) < 0.01f) return "C";
    if (pan < 0.0f) return "L" + juce::String (juce::roundToInt (-pan * 100.0f));
    return "R" + juce::String (juce::roundToInt (pan * 100.0f));
}

// ── Constructor ───────────────────────────────────────────────────────────────

SliceWaveformLcd::SliceWaveformLcd (DysektProcessor& p)
    : processor (p)
{
    setOpaque (true);
}

void SliceWaveformLcd::resized() {}

void SliceWaveformLcd::repaintLcd()
{
    repaint();
}

// ── Data building ─────────────────────────────────────────────────────────────

void SliceWaveformLcd::buildDisplayData()
{
    data = {};

    const auto& snap = processor.getUiSliceSnapshot();
    data.hasSample   = snap.sampleLoaded && ! snap.sampleMissing;
    data.numSlices   = snap.numSlices;
    data.sampleName  = snap.sampleFileName;
    data.totalFrames = snap.sampleNumFrames;
    data.sampleRate  = processor.getSampleRate() > 0.0
                           ? processor.getSampleRate() : 44100.0;

    if (! data.hasSample || snap.selectedSlice < 0 || snap.selectedSlice >= snap.numSlices)
        return;

    data.hasSlice    = true;
    data.sliceIndex  = snap.selectedSlice;

    const auto& sl       = snap.slices[(size_t) snap.selectedSlice];
    data.startSample     = sl.startSample;
    // Marker model: end derived from next slice's start (or totalFrames).
    data.endSample       = processor.sliceManager.getEndForSlice (
                               snap.selectedSlice, data.totalFrames);
    data.midiNote        = sl.midiNote;
    data.volume          = sl.volume;
    data.pan             = sl.pan;
    data.pitchSemitones  = sl.pitchSemitones;
    data.algorithm       = sl.algorithm;

    // Build waveform peak array from the processor's audio thumbnail.
    // We ask for one peak per pixel column (computed at paint time — stored
    // at a nominal width of 256 so we can scale later).
    const int kPeaks = 256;
    data.peaks.clearQuick();
    data.peaks.insertMultiple (-1, 0.0f, kPeaks);

    const int sliceLen = data.endSample - data.startSample;
    if (sliceLen <= 0) return;

    // Try to read from processor's audio thumbnail data if available.
    // We sample the waveform at evenly-spaced positions across the slice.
    for (int i = 0; i < kPeaks; i++)
    {
        const float t   = (float) i / (float) kPeaks;
        const int   pos = data.startSample + (int) (t * (float) sliceLen);
        data.peaks.set (i, processor.getWaveformPeakAt (pos));
    }
}

// ── Draw helpers ──────────────────────────────────────────────────────────────

void SliceWaveformLcd::drawBackground (juce::Graphics& g)
{
    const auto ac = getTheme().accent;
    auto b = getLocalBounds();

    // ── Outer frame — matches DualLcdControlFrame style ────────────────────
    juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
                                     juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
    g.setGradientFill (outerGrad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);
    g.setColour (ac.withAlpha (0.20f));
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

    // ── Inner screen ────────────────────────────────────────────────────────
    auto screen = b.reduced (4);
    g.setColour (lcd2Bg());
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    juce::ColourGradient glow (lcd2Phosphor().withAlpha (0.07f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack, 0, (float) (screen.getY() + 18), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (juce::Colour (0xFF000000).withAlpha ((uint8_t) kScanlineAlpha));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    g.setColour (ac.withAlpha (0.12f));
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
}

void SliceWaveformLcd::drawWaveform (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    if (data.peaks.isEmpty()) return;

    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float W  = area.getWidth();
    const float H  = area.getHeight();
    const int   n  = data.peaks.size();

    // Centre line
    g.setColour (lcd2Phosphor().withAlpha (0.08f));
    g.drawHorizontalLine (juce::roundToInt (cy), area.getX(), area.getRight());

    // Grid lines at ±50% amplitude
    g.setColour (lcd2Dim().withAlpha (0.5f));
    g.drawHorizontalLine (juce::roundToInt (cy - H * 0.25f), area.getX(), area.getRight());
    g.drawHorizontalLine (juce::roundToInt (cy + H * 0.25f), area.getX(), area.getRight());

    // Build fill + line paths
    juce::Path fill, lineTop, lineBot;
    bool first = true;

    for (int i = 0; i < n; i++)
    {
        const float x   = area.getX() + (float) i / (float) n * W;
        const float amp = juce::jlimit (0.0f, 1.0f, data.peaks[i]) * (H * 0.45f);

        const float yT = cy - amp;
        const float yB = cy + amp;

        if (first) { lineTop.startNewSubPath (x, yT); lineBot.startNewSubPath (x, yB); first = false; }
        else       { lineTop.lineTo (x, yT); lineBot.lineTo (x, yB); }
    }

    // Close fill between top and bottom lines
    fill = lineTop;
    for (int i = n - 1; i >= 0; i--)
    {
        const float x   = area.getX() + (float) i / (float) n * W;
        const float amp = juce::jlimit (0.0f, 1.0f, data.peaks[i]) * (H * 0.45f);
        fill.lineTo (x, cy + amp);
    }
    fill.closeSubPath();

    // Draw fill
    g.setColour (lcd2Phosphor().withAlpha (0.10f));
    g.fillPath (fill);

    // Draw glow strokes
    juce::PathStrokeType stroke (1.3f, juce::PathStrokeType::mitered);
    g.setColour (lcd2Phosphor().withAlpha (0.30f));
    g.strokePath (lineTop, stroke);
    g.strokePath (lineBot, stroke);

    // Bright strokes on top
    g.setColour (lcd2Phosphor().withAlpha (0.80f));
    juce::PathStrokeType brightStroke (1.1f, juce::PathStrokeType::mitered);
    g.strokePath (lineTop, brightStroke);
    g.strokePath (lineBot, brightStroke);

    // Start / end markers
    g.setColour (lcd2Phosphor().withAlpha (0.60f));
    g.drawVerticalLine (juce::roundToInt (area.getX() + 1), area.getY(), area.getBottom());
    g.drawVerticalLine (juce::roundToInt (area.getRight() - 1), area.getY(), area.getBottom());
}

void SliceWaveformLcd::drawOverlay (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    const juce::Font noteFont  = DysektLookAndFeel::makeFont (22.0f);
    const juce::Font smallFont = DysektLookAndFeel::makeFont (9.0f);
    const juce::Font hdrFont   = DysektLookAndFeel::makeFont (8.0f, true);

    // ── Sample name + index — top ─────────────────────────────────────────────
/*
    {
        juce::String idxStr = juce::String (data.sliceIndex + 1).paddedLeft ('0', 2)
                            + "/" + juce::String (data.numSlices).paddedLeft ('0', 2);
        juce::String nm     = data.sampleName.toUpperCase().substring (0, 20);

        g.setFont (hdrFont);
        g.setColour (lcd2Bright().withAlpha (0.85f));
        g.drawText (nm,
                    juce::Rectangle<float> (area.getX() + kLeftPad, area.getY() + 2,
                                            area.getWidth() * 0.75f, 14.0f),
                    juce::Justification::centredLeft, false);

        g.setColour (lcd2Phosphor().withAlpha (0.55f));
        g.drawText (idxStr,
                    juce::Rectangle<float> (area.getX(), area.getY() + 2,
                                            area.getWidth() - kLeftPad, 14.0f),
                    juce::Justification::centredRight, false);
    }
*/

    // ── MIDI note — bottom right ───────────────────────────────────────────────
/*
    {
        g.setFont (noteFont);
        g.setColour (lcd2Bright().withAlpha (0.92f));
        g.drawText (midiNoteName (data.midiNote),
                    juce::Rectangle<float> (area.getRight() - 70.0f,
                                            area.getBottom() - 46,
                                            60.0f, 28.0f),
                    juce::Justification::centredRight, false);
    }
*/


    // Stats row removed — all params shown in the left LCD panel
}

void SliceWaveformLcd::drawNoData (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (lcd2Dim().brighter (0.4f));

    if (! data.hasSample)
        g.drawText ("-- NO SAMPLE LOADED --", b, juce::Justification::centred);
    else
        g.drawText ("-- SELECT A SLICE --", b, juce::Justification::centred);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SliceWaveformLcd::paint (juce::Graphics& g)
{
    buildDisplayData();
    drawBackground (g);

    if (! data.hasSample || ! data.hasSlice)
    {
        drawNoData (g);
        return;
    }

    // Content area (inside the bezel)
    auto screen = getLocalBounds().reduced (4).toFloat();

    // Waveform occupies the full inner area; overlay drawn on top
    drawWaveform (g, screen.reduced (2.0f));
    drawOverlay  (g, screen.reduced (4.0f, 6.0f));
}
