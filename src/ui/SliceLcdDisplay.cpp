#include "SliceLcdDisplay.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── Theme-derived LCD palette ──────────────────────────────────────────────────
// All colours are computed from getTheme() at paint time so they update when
// the user switches theme.  The helper is called once per paint() call.
namespace LcdColours
{
    struct Palette
    {
        juce::Colour background, bezel, phosphor, dim, highlight,
                     labelCol, scanline, cursor, noDataCol, border,
                     flagOn, flagOff, flagBg;
    };

    static Palette fromTheme()
    {
        const auto& t  = getTheme();
        const auto  ac = t.accent;                          // main phosphor colour
        const auto  bg = t.darkBar.darker (0.55f);          // near-black background

        Palette p;
        p.background = bg;
        p.bezel      = bg.brighter (0.12f);
        p.phosphor   = ac;
        p.dim        = ac.withAlpha (0.18f).withMultipliedSaturation (0.6f)
                        .overlaidWith (bg);
        p.highlight  = ac.brighter (0.5f);
        p.labelCol   = ac.withAlpha (0.70f);
        p.scanline   = juce::Colours::black;
        p.cursor     = ac;
        p.noDataCol  = ac.withAlpha (0.15f).overlaidWith (bg);
        p.border     = ac.withAlpha (0.12f).overlaidWith (bg);
        p.flagOn     = ac;
        p.flagOff    = ac.withAlpha (0.12f).overlaidWith (bg);
        p.flagBg     = bg.darker (0.3f);
        return p;
    }
}

// ── Helpers ────────────────────────────────────────────────────────────────────

juce::String SliceLcdDisplay::midiNoteName (int note)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    note = juce::jlimit (0, 127, note);
    juce::String s (names[note % 12]);
    s += juce::String (note / 12 - 2);   // C3 = 60 → octave = 60/12-2 = 3
    // Pad to 3 chars
    while (s.length() < 3) s += " ";
    return s;
}

juce::String SliceLcdDisplay::formatMs (float secs)
{
    float ms = secs * 1000.0f;
    if (ms < 10.0f)   return juce::String (ms, 1) + "ms";
    if (ms < 1000.0f) return juce::String (juce::roundToInt (ms)) + "ms";
    return juce::String (secs, 2) + "s ";
}

juce::String SliceLcdDisplay::formatAlgo (int algo)
{
    switch (algo)
    {
        case 0:  return "TIME-DOMAIN";
        case 1:  return "GRANULAR   ";
        case 2:  return "SPECTRAL   ";
        default: return "UNKNOWN    ";
    }
}

juce::String SliceLcdDisplay::formatPan (float pan)
{
    if (std::abs (pan) < 0.01f) return "C  ";
    if (pan < 0.0f)
    {
        int v = juce::roundToInt (-pan * 100.0f);
        return "L" + juce::String (v).paddedLeft ('0', 2);
    }
    int v = juce::roundToInt (pan * 100.0f);
    return "R" + juce::String (v).paddedLeft ('0', 2);
}

// ── Constructor ────────────────────────────────────────────────────────────────

SliceLcdDisplay::SliceLcdDisplay (DysektProcessor& p)
    : processor (p)
{
    setOpaque (true);
}

// ── Data building ──────────────────────────────────────────────────────────────

void SliceLcdDisplay::buildDisplayData()
{
    data = {};

    const auto& snap = processor.getUiSliceSnapshot();
    data.hasSample    = snap.sampleLoaded && ! snap.sampleMissing;
    data.numSlices    = snap.numSlices;
    data.rootNote     = snap.rootNote;
    data.sampleName   = snap.sampleFileName;
    data.sampleNumFrames = snap.sampleNumFrames;
    data.sampleRate   = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;

    if (! data.hasSample || snap.selectedSlice < 0 || snap.selectedSlice >= snap.numSlices)
    {
        data.hasSlice = false;
        return;
    }

    data.hasSlice    = true;
    data.sliceIndex  = snap.selectedSlice;

    const auto& sl   = snap.slices[(size_t) snap.selectedSlice];
    data.midiNote    = sl.midiNote;
    data.startSample = sl.startSample;
    data.endSample   = sl.endSample;
    data.volume      = sl.volume;
    data.pan         = sl.pan;
    data.pitchSemitones = sl.pitchSemitones;
    data.centsDetune = sl.centsDetune;
    data.algorithm   = sl.algorithm;
    data.attackSec   = sl.attackSec;
    data.decaySec    = sl.decaySec;
    data.sustainLevel = sl.sustainLevel;
    data.releaseSec  = sl.releaseSec;
    data.reverse     = sl.reverse;
    data.loopMode    = sl.loopMode;
    data.oneShot     = sl.oneShot;
    data.muteGroup   = sl.muteGroup;
    data.filterCutoff = sl.filterCutoff;
    data.filterRes   = sl.filterRes;
}

// ── Repaint trigger ────────────────────────────────────────────────────────────

void SliceLcdDisplay::repaintLcd()
{
    repaint();
}

// ── Paint ──────────────────────────────────────────────────────────────────────

void SliceLcdDisplay::drawLcdBackground (juce::Graphics& g)
{
    const auto pal = LcdColours::fromTheme();
    const auto ac  = getTheme().accent;
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
    g.setColour (pal.background);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (pal.scanline.withAlpha ((uint8_t) kScanlineAlpha));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    juce::ColourGradient glow (pal.phosphor.withAlpha (0.06f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack, 0, (float) (screen.getY() + 20), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (ac.withAlpha (0.12f));
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
}

void SliceLcdDisplay::drawRow (juce::Graphics& g, int row, const juce::String& label,
                                const juce::String& value, bool highlight)
{
    const auto pal = LcdColours::fromTheme();
    auto b = getLocalBounds().reduced (4);
    const int rowH = (b.getHeight() - 8) / kRows;
    int y = b.getY() + 4 + row * rowH;

    const juce::Font labelFont = DysektLookAndFeel::makeFont (10.0f, true);
    const juce::Font valueFont = DysektLookAndFeel::makeFont (11.0f);

    if (highlight)
    {
        g.setColour (pal.phosphor.withAlpha (0.10f));
        g.fillRect (b.getX(), y, b.getWidth(), rowH - 1);
    }

    // Label: right-aligned up to the horizontal centre of the screen
    const int centreX   = b.getX() + b.getWidth() / 2;
    const int labelX    = b.getX() + kLeftPad;
    const int labelW    = centreX - labelX - 4;

    g.setFont (labelFont);
    g.setColour (highlight ? pal.highlight : pal.labelCol);
    g.drawText (label, labelX, y, labelW, rowH,
                juce::Justification::centredRight, false);

    // Value: starts just right of centre; scrolls if wider than available space
    const int valueX = centreX + 4;
    const int valueW = b.getRight() - valueX - kLeftPad;

    g.setFont (valueFont);
    g.setColour (highlight ? pal.highlight : pal.phosphor);

    const int textW = valueFont.getStringWidth (value);

    if (textW <= valueW)
    {
        g.drawText (value, valueX, y, valueW, rowH,
                    juce::Justification::centredLeft, false);
    }
    else
    {
        // Horizontal marquee scroll — driven by wall-clock, no extra timer needed
        g.saveState();
        g.reduceClipRegion (valueX, y, valueW, rowH);

        const double totalScroll = (double) (textW - valueW + 10);
        const double period      = 2.5 + value.length() * 0.04; // seconds per cycle
        const double t           = (double) juce::Time::currentTimeMillis() / 1000.0;
        const double phase       = std::fmod (t, period) / period;

        double scrollX = 0.0;
        if      (phase < 0.25)  scrollX = 0.0;                               // hold start
        else if (phase < 0.70)  scrollX = ((phase - 0.25) / 0.45) * totalScroll;  // forward
        else if (phase < 0.85)  scrollX = totalScroll;                        // hold end
        else                    scrollX = ((1.0 - (phase - 0.85) / 0.15)) * totalScroll; // back

        const int baseline = y + (rowH + valueFont.getAscent() - valueFont.getDescent()) / 2;
        g.drawSingleLineText (value, valueX - (int) scrollX, baseline);

        g.restoreState();
    }
}

void SliceLcdDisplay::drawFlagsRow (juce::Graphics& g, int row)
{
    const auto pal = LcdColours::fromTheme();
    auto b = getLocalBounds().reduced (4);
    const int rowH = (b.getHeight() - 8) / kRows;
    int y  = b.getY() + 4 + row * rowH;
    int x  = b.getX() + kLeftPad;
    int pad = 3;

    const juce::Font flagFont = DysektLookAndFeel::makeFont (9.0f, true);

    struct Flag { juce::String text; bool on; };
    juce::String loopStr = data.loopMode == 1 ? "LOOP" : (data.loopMode == 2 ? "PING" : "LOOP");
    Flag flags[] = {
        { "REV",  data.reverse },
        { loopStr, data.loopMode > 0 },
        { "1SH",  data.oneShot },
        { "MUT:" + (data.muteGroup > 0 ? juce::String (data.muteGroup) : juce::String ("-")), data.muteGroup > 0 },
    };

    g.setFont (flagFont);
    for (auto& f : flags)
    {
        int fw = flagFont.getStringWidth (f.text) + pad * 2 + 4;
        juce::Rectangle<int> box (x, y + 1, fw, rowH - 3);

        g.setColour (f.on ? pal.phosphor.withAlpha (0.15f) : pal.flagBg);
        g.fillRoundedRectangle (box.toFloat(), 1.5f);
        g.setColour (f.on ? pal.flagOn : pal.flagOff);
        g.drawRoundedRectangle (box.toFloat(), 1.5f, 1.0f);
        g.setColour (f.on ? pal.flagOn : pal.flagOff);
        g.drawText (f.text, box.getX() + pad, box.getY(), box.getWidth() - pad * 2, box.getHeight(),
                    juce::Justification::centred, false);
        x += fw + 5;
    }

    if (data.filterCutoff < 19000.0f)
    {
        juce::String fStr;
        if (data.filterCutoff >= 1000.0f)
            fStr = "FLT:" + juce::String (data.filterCutoff / 1000.0f, 1) + "k";
        else
            fStr = "FLT:" + juce::String (juce::roundToInt (data.filterCutoff)) + "Hz";

        g.setColour (pal.phosphor);
        g.setFont (flagFont);
        g.drawText (fStr, b.getRight() - 80, y, 70, rowH,
                    juce::Justification::centredRight, false);
    }
}

void SliceLcdDisplay::drawNoSliceScreen (juce::Graphics& g)
{
    const auto pal = LcdColours::fromTheme();
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (pal.noDataCol);

    if (data.hasSample && data.sampleName.isNotEmpty())
    {
        g.setColour (pal.phosphor.withAlpha (0.6f));
        g.drawText (data.sampleName.toUpperCase(),
                    b.reduced (kLeftPad, 0), juce::Justification::centredTop);
        g.setColour (pal.noDataCol);
    }

    g.setFont (DysektLookAndFeel::makeFont (11.0f));
    g.drawText ("-- NO SLICE SELECTED --", b, juce::Justification::centred);

    if (data.numSlices > 0)
    {
        g.setFont (DysektLookAndFeel::makeFont (9.0f));
        g.setColour (pal.dim);
        g.drawText (juce::String (data.numSlices) + " SLICES  |  SELECT A PAD",
                    b, juce::Justification::centredBottom);
    }
}

void SliceLcdDisplay::drawNoSampleScreen (juce::Graphics& g)
{
    const auto pal = LcdColours::fromTheme();
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (11.0f));
    g.setColour (pal.noDataCol);
    g.drawText ("-- NO SAMPLE LOADED --", b, juce::Justification::centred);

    g.setFont (DysektLookAndFeel::makeFont (9.0f));
    g.setColour (pal.dim);
    g.drawText ("DROP A FILE OR USE THE BROWSER",
                b, juce::Justification::centredBottom);
}

void SliceLcdDisplay::paint (juce::Graphics& g)
{
    buildDisplayData();
    drawLcdBackground (g);

    if (! data.hasSample)   { drawNoSampleScreen (g); return; }
    if (! data.hasSlice)    { drawNoSliceScreen  (g); return; }

    // ── Row 0:  Header — Slice index / total + filename ───────────────────────
    {
        juce::String sliceStr = "SL "
            + juce::String (data.sliceIndex + 1).paddedLeft ('0', 2)
            + " / "
            + juce::String (data.numSlices).paddedLeft ('0', 2);

        juce::String nameStr = data.sampleName.toUpperCase().substring (0, 18);
        drawRow (g, 0, sliceStr, nameStr, true);
    }

    // ── Row 1:  MIDI note + root note ─────────────────────────────────────────
    {
        juce::String noteStr = midiNoteName (data.midiNote)
            + " [" + juce::String (data.midiNote).paddedLeft ('0', 3) + "]"
            + "  ROOT:" + midiNoteName (data.rootNote);
        drawRow (g, 1, "NOTE:", noteStr);
    }

    // ── Row 2:  Start / End samples ───────────────────────────────────────────
    {
        juce::String stStr = juce::String (data.startSample).paddedLeft (' ', 7)
            + "  END:"
            + juce::String (data.endSample).paddedLeft (' ', 7);
        drawRow (g, 2, "ST:", stStr);
    }

    // ── Row 3:  Length + Volume + Pan ─────────────────────────────────────────
    {
        const int    lenSamples = data.endSample - data.startSample;
        const float  lenMs      = (float) lenSamples / (float) data.sampleRate * 1000.0f;
        juce::String lenStr;
        if (lenMs < 1000.0f)
            lenStr = juce::String (juce::roundToInt (lenMs)) + "ms";
        else
            lenStr = juce::String (lenMs / 1000.0f, 2) + "s ";

        const float volDb = data.volume;
        juce::String volStr = (volDb >= 0.0f ? "+" : "") + juce::String (volDb, 1) + "dB";
        juce::String row = lenStr + "   VOL:" + volStr + "  PAN:" + formatPan (data.pan);
        drawRow (g, 3, "LEN:", row);
    }

    // ── Row 4:  Pitch + Detune + Algorithm ────────────────────────────────────
    {
        const float  pit = data.pitchSemitones;
        const float  det = data.centsDetune;
        juce::String pitStr = (pit >= 0.0f ? "+" : "") + juce::String (pit, 1) + "st";
        juce::String detStr = (det >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (det)) + "ct";
        juce::String row    = pitStr + " DET:" + detStr + "  " + formatAlgo (data.algorithm);
        drawRow (g, 4, "PIT:", row);
    }

    // ── Row 5:  ADSR ──────────────────────────────────────────────────────────
    {
        juce::String row = "A:" + formatMs (data.attackSec)
            + " D:" + formatMs (data.decaySec)
            + " S:" + juce::String (juce::roundToInt (data.sustainLevel * 100.0f)) + "% "
            + " R:" + formatMs (data.releaseSec);
        drawRow (g, 5, "ADSR:", row);
    }

    // ── Row 6:  Flags (REV / LOOP / 1SH / MUT) + Filter ──────────────────────
    drawFlagsRow (g, 6);
}
