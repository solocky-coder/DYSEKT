#include "SliceWaveformLcd.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── Colour palette (teal / cyan LCD) ─────────────────────────────────────────
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
    data.endSample       = sl.endSample;
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
    auto b = getLocalBounds();

    // Outer bezel
    juce::ColourGradient grad (kBezel.brighter (0.1f), 0, 0,
                                kBezel.darker  (0.3f), 0, (float) b.getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);

    // Inner screen
    auto screen = b.reduced (4);
    g.setColour (kBg);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    // Top glow
    juce::ColourGradient glow (kPhosphor.withAlpha (0.07f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack,  0, (float) (screen.getY() + 18), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    // Scanlines
    g.setColour (juce::Colour (0xFF000000).withAlpha ((uint8_t) kScanlineAlpha));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    // Border
    g.setColour (kDim.brighter (0.2f));
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
    g.setColour (kPhosphor.withAlpha (0.08f));
    g.drawHorizontalLine (juce::roundToInt (cy),
                          area.getX(), area.getRight());

    // Grid lines at ±50% amplitude
    g.setColour (kDim.withAlpha (0.5f));
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
    g.setColour (kPhosphor.withAlpha (0.10f));
    g.fillPath (fill);

    // Draw glow strokes
    juce::PathStrokeType stroke (1.3f, juce::PathStrokeType::mitered);
    g.setColour (kPhosphor.withAlpha (0.30f));
    g.strokePath (lineTop, stroke);
    g.strokePath (lineBot, stroke);

    // Bright strokes on top
    g.setColour (kPhosphor.withAlpha (0.80f));
    juce::PathStrokeType brightStroke (1.1f, juce::PathStrokeType::mitered);
    g.strokePath (lineTop, brightStroke);
    g.strokePath (lineBot, brightStroke);

    // Start / end markers
    g.setColour (kPhosphor.withAlpha (0.60f));
    g.drawVerticalLine (juce::roundToInt (area.getX() + 1), area.getY(), area.getBottom());
    g.drawVerticalLine (juce::roundToInt (area.getRight() - 1), area.getY(), area.getBottom());
}

void SliceWaveformLcd::drawOverlay (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    const juce::Font noteFont  = DysektLookAndFeel::makeFont (22.0f);
    const juce::Font smallFont = DysektLookAndFeel::makeFont (9.0f);
    const juce::Font hdrFont   = DysektLookAndFeel::makeFont (8.0f, true);

    // ── Sample name + index — top ─────────────────────────────────────────────
    {
        juce::String idxStr = juce::String (data.sliceIndex + 1).paddedLeft ('0', 2)
                            + "/" + juce::String (data.numSlices).paddedLeft ('0', 2);
        juce::String nm     = data.sampleName.toUpperCase().substring (0, 20);

        g.setFont (hdrFont);
        g.setColour (kBright.withAlpha (0.85f));
        g.drawText (nm,
                    juce::Rectangle<float> (area.getX() + kLeftPad, area.getY() + 2,
                                            area.getWidth() * 0.75f, 14.0f),
                    juce::Justification::centredLeft, false);

        g.setColour (kPhosphor.withAlpha (0.55f));
        g.drawText (idxStr,
                    juce::Rectangle<float> (area.getX(), area.getY() + 2,
                                            area.getWidth() - kLeftPad, 14.0f),
                    juce::Justification::centredRight, false);
    }

    // ── MIDI note — bottom left ────────────────────────────────────────────────
    {
        g.setFont (noteFont);
        g.setColour (kBright.withAlpha (0.92f));
        g.drawText (midiNoteName (data.midiNote),
                    juce::Rectangle<float> (area.getX() + kLeftPad,
                                            area.getBottom() - 32,
                                            60.0f, 28.0f),
                    juce::Justification::centredLeft, false);
    }

    // ── Stats — bottom right ──────────────────────────────────────────────────
    const int   lenSamples = data.endSample - data.startSample;
    const float lenSecs    = (float) lenSamples / (float) data.sampleRate;
    const float lenMs      = lenSecs * 1000.0f;
    juce::String lenStr    = lenMs < 1000.0f
                             ? juce::String (juce::roundToInt (lenMs)) + "ms"
                             : juce::String (lenSecs, 2) + "s";

    juce::String volStr  = (data.volume >= 0.0f ? "+" : "") + juce::String (data.volume, 1);
    juce::String panStr  = formatPan (data.pan);
    juce::String pitStr  = (data.pitchSemitones >= 0.0f ? "+" : "")
                         + juce::String (data.pitchSemitones, 1);
    juce::String algStr  = formatAlgo (data.algorithm);

    struct Stat { juce::String label; juce::String value; };
    Stat stats[] = { {"LEN", lenStr}, {"VOL", volStr}, {"PAN", panStr},
                     {"PIT", pitStr}, {"ALG", algStr} };

    float sx = area.getRight() - kLeftPad;
    g.setFont (smallFont);

    for (int i = (int) std::size (stats) - 1; i >= 0; i--)
    {
        const float valW = 36.0f;
        const float lblW = 22.0f;
        const float colH = 22.0f;
        const float cx2  = sx - valW;
        const float ty   = area.getBottom() - colH;

        g.setColour (kLabel.withAlpha (0.8f));
        g.drawText (stats[i].label,
                    juce::Rectangle<float> (cx2 - lblW, ty, lblW, 12.0f),
                    juce::Justification::centredRight, false);

        g.setColour (kPhosphor.withAlpha (0.75f));
        g.drawText (stats[i].value,
                    juce::Rectangle<float> (cx2, ty + 10, valW, 14.0f),
                    juce::Justification::centredLeft, false);

        sx -= (valW + lblW + 4.0f);
    }
}

void SliceWaveformLcd::drawNoData (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (kDim.brighter (0.4f));

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
