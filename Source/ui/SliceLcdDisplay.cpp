#include "SliceLcdDisplay.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── Colour palette ─────────────────────────────────────────────────────────────
namespace LcdColours
{
    // Classic green LCD palette (MPC60 / SP-1200 inspired)
    static const juce::Colour background  { 0xFF0A1208 };   // near-black green tint
    static const juce::Colour bezel       { 0xFF141C12 };
    static const juce::Colour phosphor    { 0xFF6BFF4A };   // bright phosphor green
    static const juce::Colour dim         { 0xFF2A6020 };   // unlit segment colour
    static const juce::Colour highlight   { 0xFFB0FFB0 };   // selected row text
    static const juce::Colour labelCol    { 0xFF4BC035 };   // label text (slightly dimmer)
    static const juce::Colour scanline    { 0xFF000000 };
    static const juce::Colour cursor      { 0xFF6BFF4A };
    static const juce::Colour noDataCol   { 0xFF296020 };
    static const juce::Colour border      { 0xFF1E3018 };
    static const juce::Colour flagOn      { 0xFF6BFF4A };
    static const juce::Colour flagOff     { 0xFF213018 };
    static const juce::Colour flagBg      { 0xFF0D180B };
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
    auto b = getLocalBounds();

    // Outer bezel gradient
    juce::ColourGradient bezelGrad (LcdColours::bezel.brighter (0.08f), 0, 0,
                                     LcdColours::bezel.darker  (0.2f),  0, (float) b.getHeight(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);

    // Inner screen area
    auto screen = b.reduced (4);
    g.setColour (LcdColours::background);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    // Scanlines
    g.setColour (LcdColours::scanline.withAlpha ((uint8_t) kScanlineAlpha));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    // Subtle inner glow at top
    juce::ColourGradient glow (LcdColours::phosphor.withAlpha (0.06f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack,       0, (float) (screen.getY() + 20), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    // Border
    g.setColour (LcdColours::border);
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
}

void SliceLcdDisplay::drawRow (juce::Graphics& g, int row, const juce::String& label,
                                const juce::String& value, bool highlight)
{
    auto b = getLocalBounds().reduced (4);
    const int rowH = (b.getHeight() - 8) / kRows;
    int y = b.getY() + 4 + row * rowH;

    const juce::Font labelFont = DysektLookAndFeel::makeFont (10.0f, true);
    const juce::Font valueFont = DysektLookAndFeel::makeFont (11.0f);

    // Row background on selected
    if (highlight)
    {
        g.setColour (LcdColours::phosphor.withAlpha (0.10f));
        g.fillRect (b.getX(), y, b.getWidth(), rowH - 1);
    }

    // Label
    g.setFont (labelFont);
    g.setColour (highlight ? LcdColours::highlight : LcdColours::labelCol);
    g.drawText (label, b.getX() + kLeftPad, y, kLabelW, rowH,
                juce::Justification::centredLeft, false);

    // Value
    g.setFont (valueFont);
    g.setColour (highlight ? LcdColours::highlight : LcdColours::phosphor);
    g.drawText (value, b.getX() + kLeftPad + kLabelW, y,
                b.getWidth() - kLeftPad - kLabelW, rowH,
                juce::Justification::centredLeft, false);
}

void SliceLcdDisplay::drawFlagsRow (juce::Graphics& g, int row)
{
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

        g.setColour (f.on ? LcdColours::phosphor.withAlpha (0.15f) : LcdColours::flagBg);
        g.fillRoundedRectangle (box.toFloat(), 1.5f);
        g.setColour (f.on ? LcdColours::flagOn : LcdColours::flagOff);
        g.drawRoundedRectangle (box.toFloat(), 1.5f, 1.0f);
        g.setColour (f.on ? LcdColours::flagOn : LcdColours::flagOff);
        g.drawText (f.text, box.getX() + pad, box.getY(), box.getWidth() - pad * 2, box.getHeight(),
                    juce::Justification::centred, false);
        x += fw + 5;
    }

    // Filter info on the right
    if (data.filterCutoff < 19000.0f)
    {
        juce::String fStr;
        if (data.filterCutoff >= 1000.0f)
            fStr = "FLT:" + juce::String (data.filterCutoff / 1000.0f, 1) + "k";
        else
            fStr = "FLT:" + juce::String (juce::roundToInt (data.filterCutoff)) + "Hz";

        g.setColour (LcdColours::phosphor);
        g.setFont (flagFont);
        g.drawText (fStr, b.getRight() - 80, y, 70, rowH,
                    juce::Justification::centredRight, false);
    }
}

void SliceLcdDisplay::drawNoSliceScreen (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (LcdColours::noDataCol);

    // Sample name header
    if (data.hasSample && data.sampleName.isNotEmpty())
    {
        g.setColour (LcdColours::phosphor.withAlpha (0.6f));
        g.drawText (data.sampleName.toUpperCase(),
                    b.reduced (kLeftPad, 0), juce::Justification::centredTop);
        g.setColour (LcdColours::noDataCol);
    }

    g.setFont (DysektLookAndFeel::makeFont (11.0f));
    g.drawText ("-- NO SLICE SELECTED --", b, juce::Justification::centred);

    // Slice count hint
    if (data.numSlices > 0)
    {
        g.setFont (DysektLookAndFeel::makeFont (9.0f));
        g.setColour (LcdColours::dim);
        g.drawText (juce::String (data.numSlices) + " SLICES  |  SELECT A PAD",
                    b, juce::Justification::centredBottom);
    }
}

void SliceLcdDisplay::drawNoSampleScreen (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (11.0f));
    g.setColour (LcdColours::noDataCol);
    g.drawText ("-- NO SAMPLE LOADED --", b, juce::Justification::centred);

    g.setFont (DysektLookAndFeel::makeFont (9.0f));
    g.setColour (LcdColours::dim);
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
