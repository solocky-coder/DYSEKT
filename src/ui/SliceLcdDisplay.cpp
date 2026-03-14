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
        p.flagOff    = ac.withAlpha (0.45f);          // dim but visible
        p.flagBg     = bg.brighter (0.25f);           // visible pill outline against screen bg
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

    // ── Scrollbar setup ───────────────────────────────────────────────────────
    scrollBar.setAutoHide (false);
    scrollBar.addListener (this);
    addAndMakeVisible (scrollBar);
    lookAndFeelChanged();   // apply initial theme colours
    updateScrollBar();
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
    // Marker model: end derived from next slice's start (or sampleNumFrames).
    data.endSample   = processor.sliceManager.getEndForSlice (
                           snap.selectedSlice, snap.sampleNumFrames);
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
    data.filterCutoff    = sl.filterCutoff;
    data.filterRes       = sl.filterRes;
    data.sliceLocked     = (sl.lockMask == 0xFFFFFFFFu);
    // Extended fields for scroll rows 7-9
    data.stretchEnabled  = sl.stretchEnabled;
    data.tonalityHz      = sl.tonalityHz;
    data.formantSemitones = sl.formantSemitones;
    data.formantComp     = sl.formantComp;
    data.grainMode       = sl.grainMode;
    data.releaseTail     = sl.releaseTail;
    data.outputBus       = sl.outputBus;
    data.bpm             = sl.bpm;
}

// ── Repaint trigger ────────────────────────────────────────────────────────────

void SliceLcdDisplay::repaintLcd()
{
    updateScrollBar();
    repaint();
}

// ── Scrollbar helpers ─────────────────────────────────────────────────────────
void SliceLcdDisplay::updateScrollBar()
{
    const auto screen = getLocalBounds().reduced (4);
    const int contentH = kTotalRows * kRowH;
    const int visibleH = kVisibleRows * kRowH;

    scrollBar.setRangeLimits (0.0, contentH);
    scrollBar.setCurrentRange (scrollOffsetPx, visibleH);
    scrollBar.setVisible (contentH > visibleH);
}

void SliceLcdDisplay::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    const int contentH = kTotalRows * kRowH;
    const int visibleH = kVisibleRows * kRowH;
    const int maxScroll = juce::jmax (0, contentH - visibleH);
    scrollOffsetPx = juce::jlimit (0, maxScroll, (int) newRangeStart);
    repaint();
}

void SliceLcdDisplay::mouseWheelMove (const juce::MouseEvent&,
                                       const juce::MouseWheelDetails& w)
{
    const int contentH  = kTotalRows * kRowH;
    const int visibleH  = kVisibleRows * kRowH;
    const int maxScroll = juce::jmax (0, contentH - visibleH);

    scrollOffsetPx = juce::jlimit (0, maxScroll,
        scrollOffsetPx - juce::roundToInt (w.deltaY * (float) kRowH * 3.0f));

    scrollBar.setCurrentRange (scrollOffsetPx, visibleH);
    repaint();
}

// ── Resized ────────────────────────────────────────────────────────────────────

void SliceLcdDisplay::resized()
{
    auto screen = getLocalBounds().reduced (4);
    // Scrollbar: thin strip on right edge of inner screen
    scrollBar.setBounds (screen.getRight() - kScrollW, screen.getY() + 1,
                         kScrollW, screen.getHeight() - 2);
    updateScrollBar();
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
    const int rowH = kRowH;
    int y = b.getY() + 4 + row * rowH - scrollOffsetPx;

    // Skip rows fully outside the visible screen area
    if (y + rowH <= b.getY() + 4 || y >= b.getBottom() - 4) return;

    const juce::Font labelFont = DysektLookAndFeel::makeFont (11.0f, true);
    const juce::Font valueFont = DysektLookAndFeel::makeFont (12.0f);

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
    const int rowH    = kRowH;
    const int y       = b.getY() + 4 + row * rowH - scrollOffsetPx;
    const juce::Font  f = DysektLookAndFeel::makeFont (11.5f);

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

    // ── Right item — left-aligned from the right-column start ────────────────
    // Use 52% split so right column has enough room and never overflows.
    const int rightX  = b.getX() + (b.getWidth() * 52 / 100);
    const int rightW  = b.getRight() - rightX - kLeftPad - 42 - kScrollW; // leave space for flag column + scrollbar

    const int rColonPos = rightStr.indexOfChar (':');
    if (rColonPos > 0)
    {
        juce::String rlbl = rightStr.substring (0, rColonPos + 1);
        juce::String rval = rightStr.substring (rColonPos + 1);
        const int rlblW = f.getStringWidth (rlbl);

        g.setColour (highlight ? pal.highlight : pal.labelCol);
        g.drawText (rlbl, rightX, y, rlblW + 2, rowH,
                    juce::Justification::centredLeft, false);
        g.setColour (highlight ? pal.highlight : pal.phosphor);
        g.drawText (rval, rightX + rlblW + 2, y,
                    rightW - rlblW - 2, rowH,
                    juce::Justification::centredLeft, false);
    }
    else
    {
        g.setColour (highlight ? pal.highlight : pal.phosphor);
        g.drawText (rightStr, rightX, y, rightW, rowH,
                    juce::Justification::centredLeft, false);
    }
}

void SliceLcdDisplay::drawFlagsRow (juce::Graphics& g, int /*row*/)
{
    // ── Flags drawn as a VERTICAL column on the RIGHT edge of the screen ──────
    // Greyed out when inactive, full phosphor when active.
    const auto pal = LcdColours::fromTheme();
    auto screen = getLocalBounds().reduced (4);

    const juce::Font flagFont = DysektLookAndFeel::makeFont (8.5f, true);
    const int pad    = 3;
    const int flagW  = 34;   // fixed pill width
    const int flagH  = 12;   // pill height
    const int flagGap = 3;   // gap between pills
    const int numFlags = 4;
    const int totalFlagsH = numFlags * flagH + (numFlags - 1) * flagGap;

    // Centre the vertical stack in the screen height
    int fy = screen.getY() + (screen.getHeight() - totalFlagsH) / 2;
    const int fx = screen.getRight() - flagW - 4;  // right-edge inset

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
        juce::Rectangle<int> box (fx, fy, flagW, flagH);

        // Background: dark when off, faint phosphor when on
        g.setColour (f.on ? pal.phosphor.withAlpha (0.15f) : pal.flagBg);
        g.fillRoundedRectangle (box.toFloat(), 2.0f);

        // Border: very dim when off, full when on
        g.setColour (f.on ? pal.flagOn : pal.flagOff);
        g.drawRoundedRectangle (box.toFloat(), 2.0f, 1.0f);

        // Text: dim when off, bright when on
        g.setColour (f.on ? pal.flagOn : pal.flagOff);
        g.drawText (f.text, box.getX() + pad, box.getY(),
                    box.getWidth() - pad * 2, box.getHeight(),
                    juce::Justification::centred, false);

        fy += flagH + flagGap;
    }

    // Filter badge below flags (only when active)
    if (data.filterCutoff < 19000.0f)
    {
        juce::String fStr;
        if (data.filterCutoff >= 1000.0f)
            fStr = "FLT:" + juce::String (data.filterCutoff / 1000.0f, 1) + "k";
        else
            fStr = "FLT:" + juce::String (juce::roundToInt (data.filterCutoff)) + "Hz";

        juce::Rectangle<int> fbox (fx, fy + flagGap, flagW, flagH);
        g.setColour (pal.phosphor.withAlpha (0.15f));
        g.fillRoundedRectangle (fbox.toFloat(), 2.0f);
        g.setColour (pal.flagOn);
        g.drawRoundedRectangle (fbox.toFloat(), 2.0f, 1.0f);
        g.setColour (pal.flagOn);
        g.drawText (fStr, fbox.getX() + pad, fbox.getY(),
                    fbox.getWidth() - pad * 2, fbox.getHeight(),
                    juce::Justification::centred, false);
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

void SliceLcdDisplay::lookAndFeelChanged()
{
    // Re-apply scrollbar colours whenever the theme changes
    scrollBar.setColour (juce::ScrollBar::backgroundColourId,
                         juce::Colour (0x00000000));   // transparent bg
    scrollBar.setColour (juce::ScrollBar::thumbColourId,
                         getTheme().accent.withAlpha (0.60f));
    scrollBar.setColour (juce::ScrollBar::trackColourId,
                         getTheme().darkBar.darker (0.4f));
    scrollBar.repaint();
}

void SliceLcdDisplay::paint (juce::Graphics& g)
{
    buildDisplayData();
    drawLcdBackground (g);

    if (! data.hasSample)   { drawNoSampleScreen (g); return; }
    if (! data.hasSlice)    { drawNoSliceScreen  (g); return; }

    // ── Clip all row drawing to the inner screen — prevents bleed on scroll ───
    auto screen = getLocalBounds().reduced (4);
    // Clip to screen minus scrollbar strip on the right
    auto clipScreen = screen.withTrimmedRight (kScrollW + 1);
    g.saveState();
    g.reduceClipRegion (clipScreen);

    // ── Row 0:  Header — centred: "SL xx / xx  SAMPLENAME" ──────────────────
    {
        juce::String sliceStr = "SL "
            + juce::String (data.sliceIndex + 1).paddedLeft ('0', 2)
            + " / "
            + juce::String (data.numSlices).paddedLeft ('0', 2);

        juce::String nameStr = data.sampleName.toUpperCase().substring (0, 18);

        // Draw centred header: measure label+value as one unit, centre in screen
        auto   screen    = getLocalBounds().reduced (4);
        auto   pal       = LcdColours::fromTheme();
        const int rowH   = kRowH;
        const int y      = screen.getY() + 4 + 0 * rowH - scrollOffsetPx;

        // Highlight background
        g.setColour (pal.phosphor.withAlpha (0.10f));
        g.fillRect (screen.getX(), y, screen.getWidth(), rowH - 1);

        const juce::Font lblF = DysektLookAndFeel::makeFont (11.0f, true);
        const juce::Font valF = DysektLookAndFeel::makeFont (12.0f);
        const int lblW = lblF.getStringWidth (sliceStr);
        const int gap  = 8;
        const int valW = valF.getStringWidth (nameStr);
        const int totalW = lblW + gap + valW;
        const int startX = screen.getX() + (screen.getWidth() - totalW) / 2;

        g.setFont (lblF);
        g.setColour (pal.highlight);
        g.drawText (sliceStr, startX, y, lblW + 2, rowH,
                    juce::Justification::centredLeft, false);

        g.setFont (valF);
        g.setColour (pal.highlight);
        g.drawText (nameStr, startX + lblW + gap, y, valW + 2, rowH,
                    juce::Justification::centredLeft, false);

        // Lock badge: small pill on the far right when slice is locked
        if (data.sliceLocked)
        {
            const juce::Font lkF = DysektLookAndFeel::makeFont (8.0f, true);
            const juce::String lkStr = "LOCK";
            const int lkW = lkF.getStringWidth (lkStr) + 6;
            const int lkX = screen.getRight() - lkW - 6;
            const int lkY = y + 1;
            const int lkH = rowH - 3;
            g.setColour (pal.phosphor.withAlpha (0.25f));
            g.fillRoundedRectangle ((float) lkX, (float) lkY, (float) lkW, (float) lkH, 2.0f);
            g.setColour (pal.highlight);
            g.drawRoundedRectangle ((float) lkX, (float) lkY, (float) lkW, (float) lkH, 2.0f, 1.0f);
            g.setFont (lkF);
            g.setColour (pal.highlight);
            g.drawText (lkStr, lkX + 3, lkY, lkW - 6, lkH, juce::Justification::centred, false);
        }
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

    // ── Row 7:  FMNT:+x.xst  |  TONAL:xxxxHz ────────────────────────────────
    {
        const float fmnt = data.formantSemitones;
        juce::String fmntStr  = juce::String ("FMNT:") + (fmnt >= 0.0f ? "+" : "")
                              + juce::String (fmnt, 1) + "st";
        juce::String tonalStr = "TONAL:" + (data.tonalityHz < 1.0f
                              ? juce::String ("OFF")
                              : juce::String (juce::roundToInt (data.tonalityHz)) + "Hz");
        drawRowPair (g, 7, fmntStr, tonalStr);
    }

    // ── Row 8:  GRAIN:xxxxx  |  FRES:x.xx ───────────────────────────────────
    {
        juce::String grainStr;
        switch (data.grainMode)
        {
            case 0:  grainStr = "GRAIN:FAST";   break;
            case 1:  grainStr = "GRAIN:NORMAL"; break;
            case 2:  grainStr = "GRAIN:SMOOTH"; break;
            default: grainStr = "GRAIN:?";      break;
        }
        juce::String fresStr = "FRES:" + juce::String (data.filterRes, 2);
        drawRowPair (g, 8, grainStr, fresStr);
    }

    // ── Row 9:  OUT:xx  |  BPM:xxx.xx  +  toggle flags ───────────────────────
    {
        juce::String outStr = "OUT:" + juce::String (data.outputBus + 1);
        juce::String bpmStr = "BPM:" + juce::String (data.bpm, 1);
        drawRowPair (g, 9, outStr, bpmStr);
    }

    // ── Row 9 right-side toggle flags (STRETCH / TAIL / FMNT C) ──────────────
    // Drawn inline as small text badges after BPM on row 9
    {
        const auto pal2 = LcdColours::fromTheme();
        auto b2 = getLocalBounds().reduced (4);
        const int rowH2  = kRowH;
        const int y9     = b2.getY() + 4 + 9 * rowH2 - scrollOffsetPx;
        if (y9 < b2.getBottom() - 4 && y9 + rowH2 > b2.getY() + 4)
        {
            const juce::Font flagFont2 = DysektLookAndFeel::makeFont (8.5f, true);
            const int pad2 = 3;
            const int fh   = rowH2 - 4;
            int fx2 = b2.getX() + b2.getWidth() / 4;  // start after left column

            struct TFlag { juce::String text; bool on; };
            TFlag tflags[] = {
                { "STR", data.stretchEnabled },
                { "TAIL", data.releaseTail  },
                { "FMC", data.formantComp   },
            };
            g.setFont (flagFont2);
            for (auto& tf : tflags)
            {
                int fw2 = flagFont2.getStringWidth (tf.text) + pad2 * 2 + 4;
                juce::Rectangle<int> box2 (fx2, y9 + 2, fw2, fh);
                g.setColour (tf.on ? pal2.phosphor.withAlpha (0.15f) : pal2.flagBg);
                g.fillRoundedRectangle (box2.toFloat(), 1.5f);
                g.setColour (tf.on ? pal2.flagOn : pal2.flagOff);
                g.drawRoundedRectangle (box2.toFloat(), 1.5f, 1.0f);
                g.setColour (tf.on ? pal2.flagOn : pal2.flagOff);
                g.drawText (tf.text, box2.getX() + pad2, box2.getY(),
                            box2.getWidth() - pad2 * 2, box2.getHeight(),
                            juce::Justification::centred, false);
                fx2 += fw2 + 4;
            }
        }
    }

    // ── Floating flags — right-edge vertical column (always visible) ──────────
    drawFlagsRow (g, 6);

    g.restoreState();  // end clip region

    // Real ScrollBar widget handles scroll indication — no unicode arrows needed
}
