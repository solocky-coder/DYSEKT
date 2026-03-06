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

void SliceLcdDisplay::mouseWheelMove (const juce::MouseEvent&,
                                       const juce::MouseWheelDetails& w)
{
    auto b = getLocalBounds().reduced (4);
    const int rowH      = (b.getHeight() - 8) / kRows;
    const int contentH  = kRows * rowH;
    const int visibleH  = b.getHeight() - 8;
    const int maxScroll = juce::jmax (0, contentH - visibleH);

    scrollOffsetPx = juce::jlimit (0, maxScroll,
        scrollOffsetPx - juce::roundToInt (w.deltaY * (float) rowH * 2.5f));

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
    int y = b.getY() + 4 + row * rowH - scrollOffsetPx;

    // Skip rows fully outside the visible screen area
    if (y + rowH <= b.getY() + 4 || y >= b.getBottom() - 4) return;

    const juce::Font labelFont = DysektLookAndFeel::makeFont (10.0f, true);
    const juce::Font valueFont = DysektLookAndFeel::makeFont (11.0f);

    if (highlight)
    {
        g.setColour (pal.phosphor.withAlpha (0.10f));
        g.fillRect (b.getX(), y, b.getWidth(), rowH - 1);
    }

    // Header row: label left-aligned, value follows directly after a gap
    const int lx = b.getX() + kLeftPad;
    g.setFont (labelFont);
    g.setColour (highlight ? pal.highlight : pal.labelCol);
    g.drawText (label, lx, y, b.getWidth() / 3, rowH,
                juce::Justification::centredLeft, false);

    const int vx = lx + labelFont.getStringWidth (label) + 6;
    g.setFont (valueFont);
    g.setColour (highlight ? pal.highlight : pal.phosphor);
    g.drawText (value, vx, y, b.getRight() - vx - kLeftPad, rowH,
                juce::Justification::centredLeft, false);
}

// ── Two-item row ──────────────────────────────────────────────────────────────
// Left item  : label+value as one string, left-aligned in the left half
// Right item : label+value as one string, centred in the right half
void SliceLcdDisplay::drawRowPair (juce::Graphics& g, int row,
                                    const juce::String& leftStr,
                                    const juce::String& rightStr,
                                    bool highlight)
{
    const auto pal = LcdColours::fromTheme();
    auto b = getLocalBounds().reduced (4);
    const int rowH    = (b.getHeight() - 8) / kRows;
    const int y       = b.getY() + 4 + row * rowH - scrollOffsetPx;
    const juce::Font  f = DysektLookAndFeel::makeFont (10.5f);

    // Skip rows fully outside the visible screen area
    if (y + rowH <= b.getY() + 4 || y >= b.getBottom() - 4) return;

    if (highlight)
    {
        g.setColour (pal.phosphor.withAlpha (0.10f));
        g.fillRect (b.getX(), y, b.getWidth(), rowH - 1);
    }

    g.setFont (f);

    // ── Left item — left-aligned from the left edge ───────────────────────────
    const int halfW   = b.getWidth() / 2;
    const int leftX   = b.getX() + kLeftPad;
    const int leftW   = halfW - kLeftPad - 4;

    // Split leftStr into label (up to first space or ':') and rest for colouring
    const int colonPos = leftStr.indexOfChar (':');
    if (colonPos > 0)
    {
        juce::String lbl = leftStr.substring (0, colonPos + 1);
        juce::String val = leftStr.substring (colonPos + 1);
        g.setColour (highlight ? pal.highlight : pal.labelCol);
        g.drawText (lbl, leftX, y, f.getStringWidth (lbl) + 2, rowH,
                    juce::Justification::centredLeft, false);
        g.setColour (highlight ? pal.highlight : pal.phosphor);
        g.drawText (val, leftX + f.getStringWidth (lbl) + 2, y,
                    leftW - f.getStringWidth (lbl) - 2, rowH,
                    juce::Justification::centredLeft, false);
    }
    else
    {
        g.setColour (highlight ? pal.highlight : pal.phosphor);
        g.drawText (leftStr, leftX, y, leftW, rowH,
                    juce::Justification::centredLeft, false);
    }

    // ── Right item — centred in the right half ────────────────────────────────
    const int rightX  = b.getX() + halfW;
    const int rightW  = halfW - kLeftPad;

    const int rColonPos = rightStr.indexOfChar (':');
    if (rColonPos > 0)
    {
        // measure combined string, then centre the pair as a unit
        const int totalW = f.getStringWidth (rightStr);
        const int startX = rightX + (rightW - totalW) / 2;
        juce::String rlbl = rightStr.substring (0, rColonPos + 1);
        juce::String rval = rightStr.substring (rColonPos + 1);

        g.setColour (highlight ? pal.highlight : pal.labelCol);
        g.drawText (rlbl, startX, y, f.getStringWidth (rlbl) + 2, rowH,
                    juce::Justification::centredLeft, false);
        g.setColour (highlight ? pal.highlight : pal.phosphor);
        g.drawText (rval, startX + f.getStringWidth (rlbl) + 2, y,
                    totalW - f.getStringWidth (rlbl), rowH,
                    juce::Justification::centredLeft, false);
    }
    else
    {
        g.setColour (highlight ? pal.highlight : pal.phosphor);
        g.drawText (rightStr, rightX, y, rightW, rowH,
                    juce::Justification::centred, false);
    }
}

void SliceLcdDisplay::drawFlagsRow (juce::Graphics& g, int row)
{
    const auto pal = LcdColours::fromTheme();
    auto b = getLocalBounds().reduced (4);
    const int rowH = (b.getHeight() - 8) / kRows;
    int y  = b.getY() + 4 + row * rowH - scrollOffsetPx;
    int x  = b.getX() + kLeftPad;
    int pad = 3;

    // Skip if fully outside the visible screen area
    if (y + rowH <= b.getY() + 4 || y >= b.getBottom() - 4) return;

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

    // ── Clip all row drawing to the inner screen — prevents bleed on scroll ───
    auto screen = getLocalBounds().reduced (4);
    g.saveState();
    g.reduceClipRegion (screen);

    // ── Row 0:  Header — Slice index / total + filename ───────────────────────
    {
        juce::String sliceStr = "SL "
            + juce::String (data.sliceIndex + 1).paddedLeft ('0', 2)
            + " / "
            + juce::String (data.numSlices).paddedLeft ('0', 2);

        juce::String nameStr = data.sampleName.toUpperCase().substring (0, 18);
        drawRow (g, 0, sliceStr, nameStr, true);
    }

    // ── Row 1:  NOTE:Cx(nnn)  |  ROOT:Cx ─────────────────────────────────────
    {
        // Format: NOTE:C1(036)   ROOT:C1
        juce::String noteStr = "NOTE:" + midiNoteName (data.midiNote).trimEnd()
            + "(" + juce::String (data.midiNote).paddedLeft ('0', 3) + ")";
        juce::String rootStr = "ROOT:" + midiNoteName (data.rootNote).trimEnd();
        drawRowPair (g, 1, noteStr, rootStr);
    }

    // ── Row 2:  ST:nnnnn  |  END:nnnnn ───────────────────────────────────────
    {
        juce::String stStr  = "ST:"  + juce::String (data.startSample);
        juce::String endStr = "END:" + juce::String (data.endSample);
        drawRowPair (g, 2, stStr, endStr);
    }

    // ── Row 3:  LEN:xxxms  |  VOL:+x.xdB ────────────────────────────────────
    {
        const int   lenSamples = data.endSample - data.startSample;
        const float lenMs      = (float) lenSamples / (float) data.sampleRate * 1000.0f;
        juce::String lenStr = "LEN:" + (lenMs < 1000.0f
            ? juce::String (juce::roundToInt (lenMs)) + "ms"
            : juce::String (lenMs / 1000.0f, 2) + "s");

        const float vol = data.volume;
        juce::String volStr = juce::String ("VOL:") + (vol >= 0.0f ? "+" : "") + juce::String (vol, 1) + "dB";
        drawRowPair (g, 3, lenStr, volStr);
    }

    // ── Row 4:  PAN:x  |  PIT:+x.xst ────────────────────────────────────────
    {
        juce::String panStr = "PAN:" + formatPan (data.pan).trimEnd();
        const float  pit    = data.pitchSemitones;
        juce::String pitStr = juce::String ("PIT:") + (pit >= 0.0f ? "+" : "") + juce::String (pit, 1) + "st";
        drawRowPair (g, 4, panStr, pitStr);
    }

    // ── Row 5:  DET:+xct  |  ALGO:xxxxxxxx ───────────────────────────────────
    {
        const float det = data.centsDetune;
        juce::String detStr  = juce::String ("DET:") + (det >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (det)) + "ct";
        juce::String algoStr = "ALGO:" + formatAlgo (data.algorithm).trimEnd();
        drawRowPair (g, 5, detStr, algoStr);
    }

    // ── Row 6:  ATK/DEC  |  SUS/REL  +  badge flags ─────────────────────────
    {
        juce::String adsrLeft  = "A:" + formatMs (data.attackSec).trimEnd()
            + " D:" + formatMs (data.decaySec).trimEnd();
        juce::String adsrRight = "S:" + juce::String (juce::roundToInt (data.sustainLevel * 100.0f)) + "%"
            + " R:" + formatMs (data.releaseSec).trimEnd();
        drawRowPair (g, 6, adsrLeft, adsrRight);
    }

    // ── Floating flags — drawn over the bottom edge of row 6 ─────────────────
    drawFlagsRow (g, 6);

    g.restoreState();  // end clip region

    // ── Scroll indicator — right edge, only when scrolled ─────────────────────
    {
        const int rowH      = (screen.getHeight() - 8) / kRows;
        const int contentH  = kRows * rowH;
        const int visibleH  = screen.getHeight() - 8;

        if (contentH > visibleH && scrollOffsetPx > 0)
        {
            // Faint down-arrow at bottom-right to signal more content
            const auto ac = getTheme().accent;
            g.setColour (ac.withAlpha (0.45f));
            g.setFont (DysektLookAndFeel::makeFont (9.0f));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xbc"),  // ▼
                        screen.getRight() - 14, screen.getBottom() - 14, 12, 12,
                        juce::Justification::centred, false);
        }

        if (scrollOffsetPx > 0)
        {
            const auto ac = getTheme().accent;
            g.setColour (ac.withAlpha (0.45f));
            g.setFont (DysektLookAndFeel::makeFont (9.0f));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xb2"),  // ▲
                        screen.getRight() - 14, screen.getY() + 2, 12, 12,
                        juce::Justification::centred, false);
        }
    }
}
