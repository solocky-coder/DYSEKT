#include "MixerPanel.h"
#include "../PluginProcessor.h"
static constexpr int kMaxMeterSlices = DysektProcessor::kMaxMeterSlices;
#include "DysektLookAndFeel.h"
#include "../audio/Slice.h"
#include "../params/ParamIds.h"
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Ctor / Dtor
// ─────────────────────────────────────────────────────────────────────────────
MixerPanel::MixerPanel (DysektProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (false);
    startTimerHz (30);
}

MixerPanel::~MixerPanel() = default;

// ─────────────────────────────────────────────────────────────────────────────
//  updateFromSnapshot
// ─────────────────────────────────────────────────────────────────────────────
void MixerPanel::updateFromSnapshot()
{
    const uint32_t v = (uint32_t) processor.getUiSliceSnapshotVersion();
    if (v != cachedVersion)
    {
        cachedVersion = v;
        repaint();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout helpers
// ─────────────────────────────────────────────────────────────────────────────
int MixerPanel::colX (Col col) const
{
    return kNameColW + (int)col * kKnobColW;
}

int MixerPanel::rowY (int sliceIdx) const
{
    return kHeaderH + sliceIdx * kRowH - scrollPixels;
}

int MixerPanel::masterRowY() const
{
    const auto& snap = processor.getUiSliceSnapshot();
    return kHeaderH + snap.numSlices * kRowH - scrollPixels;
}

MixerPanel::Cell MixerPanel::hitTest (juce::Point<int> pos) const
{
    Cell c;
    if (pos.y < kHeaderH) return c;          // header — no hit

    const auto& snap = processor.getUiSliceSnapshot();
    [[maybe_unused]] const int totalRowsH = snap.numSlices * kRowH + kMasterH;
    const int contentTop = kHeaderH;

    // Which logical row?
    const int relY = pos.y - contentTop + scrollPixels;
    const int logicalRow = relY / kRowH;

    if (logicalRow < snap.numSlices)
    {
        c.row = logicalRow;
        c.isMaster = false;
    }
    else if (relY >= snap.numSlices * kRowH &&
             relY <  snap.numSlices * kRowH + kMasterH)
    {
        c.row = -1;
        c.isMaster = true;
    }
    else return c;  // below content

    // Which column?
    const int cx = pos.x;
    if (cx < kNameColW) { c.row = -2; return c; }  // name column (row select only)
    int colIdx = (cx - kNameColW) / kKnobColW;
    if (colIdx < 0 || colIdx >= kNumCols) return c;
    c.col = (Col) colIdx;

    const int rowTop  = c.isMaster ? (kHeaderH + snap.numSlices * kRowH - scrollPixels)
                                   : rowY (c.row);
    const int rowHt   = c.isMaster ? kMasterH : kRowH;
    c.bounds = { kNameColW + colIdx * kKnobColW, rowTop, kKnobColW, rowHt };
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Format helpers
// ─────────────────────────────────────────────────────────────────────────────
juce::String MixerPanel::fmtGain (float db) const
{
    return (db >= 0.f ? "+" : "") + juce::String (db, 1) + "dB";
}
juce::String MixerPanel::fmtPan (float pan) const
{
    if (std::abs (pan) < 0.02f) return "C";
    return pan < 0.f ? "L" + juce::String ((int)(-pan * 100.f))
                     : "R" + juce::String ((int)( pan * 100.f));
}
juce::String MixerPanel::fmtFcut (float hz) const
{
    return hz >= 999.f ? juce::String ((int)(hz / 1000.f)) + "k"
                       : juce::String ((int) hz);
}
juce::String MixerPanel::fmtPres (float res) const
{
    return juce::String ((int)(res * 100.f)) + "%";
}
juce::String MixerPanel::fmtOut (int bus) const
{
    return juce::String (bus + 1);
}
juce::String MixerPanel::fmtMute (int mg) const
{
    return juce::String (mg);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Norm helpers
// ─────────────────────────────────────────────────────────────────────────────
float MixerPanel::toNormGain (float db)  const { return juce::jlimit (0.f, 1.f, (db + 100.f) / 124.f); }
float MixerPanel::toNormPan  (float pan) const { return juce::jlimit (0.f, 1.f, (pan + 1.f) * 0.5f); }
float MixerPanel::toNormFcut (float hz)  const
{
    return juce::jlimit (0.f, 1.f,
        std::log2 (juce::jmax (20.f, hz) / 20.f) / std::log2 (20000.f / 20.f));
}
float MixerPanel::toNormPres (float res) const { return juce::jlimit (0.f, 1.f, res); }
float MixerPanel::toNormOut  (int bus)   const { return juce::jlimit (0.f, 1.f, bus / 15.f); }

float MixerPanel::fromNormFcut (float n) const
{
    return 20.f * std::pow (2.f, n * std::log2 (20000.f / 20.f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Drawing
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int kKnobR = 7;   // knob radius (px) — slightly smaller than SCB

void MixerPanel::drawKnobInRow (juce::Graphics& g, int cx, int cy,
                                 float norm, bool locked, bool isMaster) const
{
    const auto& theme = getTheme();
    const float r = (float) kKnobR;

    // Track arc (background)
    const float startA  = juce::MathConstants<float>::pi * 0.75f;
    const float arcLen  = juce::MathConstants<float>::pi * 1.5f;
    juce::Path track;
    track.addCentredArc ((float)cx, (float)cy, r, r, 0.f, startA, startA + arcLen, true);
    g.setColour (theme.separator.withAlpha (0.6f));
    g.strokePath (track, juce::PathStrokeType (1.5f));

    // Fill arc
    if (norm > 0.001f)
    {
        const float endAngle = startA + arcLen * juce::jlimit (0.f, 1.f, norm);
        juce::Path fill;
        fill.addCentredArc ((float)cx, (float)cy, r, r, 0.f, startA, endAngle, true);
        g.setColour (locked ? theme.lockActive : (isMaster ? theme.accent.brighter (0.15f) : theme.accent));
        g.strokePath (fill, juce::PathStrokeType (1.5f));
    }

    // Centre dot
    g.setColour (locked ? theme.lockActive.withAlpha (0.7f) : theme.accent.withAlpha (0.55f));
    g.fillEllipse ((float)cx - 2.f, (float)cy - 2.f, 4.f, 4.f);
}

void MixerPanel::drawMuteBadge (juce::Graphics& g, int cx, int cy,
                                  int muteGroup, bool locked) const
{
    const auto& theme = getTheme();
    const int bw = 18, bh = 16;
    const juce::Rectangle<float> r ((float)(cx - bw/2), (float)(cy - bh/2), (float)bw, (float)bh);

    const bool active = (muteGroup > 0);
    g.setColour (active ? (locked ? theme.lockActive.withAlpha (0.15f)
                                  : theme.accent.withAlpha (0.10f))
                        : theme.separator.withAlpha (0.3f));
    g.fillRoundedRectangle (r, 2.5f);

    g.setColour (active ? (locked ? theme.lockActive : theme.accent)
                        : theme.foreground.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 2.5f, 0.8f);

    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (active ? (locked ? theme.lockActive : theme.accent)
                        : theme.foreground.withAlpha (0.3f));
    g.drawText (juce::String (muteGroup), r.toNearestInt(), juce::Justification::centred);
}

void MixerPanel::drawHeader (juce::Graphics& g) const
{
    const auto& theme = getTheme();

    g.setColour (theme.darkBar.darker (0.3f));
    g.fillRect (0, 0, getWidth(), kHeaderH);

    g.setColour (theme.separator);
    g.drawHorizontalLine (kHeaderH - 1, 0.f, (float) getWidth());

    // Slice column
    g.setFont (DysektLookAndFeel::makeFont (7.5f));
    g.setColour (theme.accent.withAlpha (0.5f));
    g.drawText ("SLICE", 10, 0, kNameColW - 10, kHeaderH, juce::Justification::centredLeft);

    // Knob column headers
    const char* labels[kNumCols] = { "GAIN", "PAN", "FCUT", "PRES", "MUTE GRP", "CHRO", "OUT" };
    for (int i = 0; i < kNumCols; ++i)
    {
        g.setColour (i < 4 ? theme.accent.withAlpha (0.55f)
                           : theme.foreground.withAlpha (0.28f));
        g.drawText (labels[i],
                    colX ((Col)i), 0, kKnobColW, kHeaderH,
                    juce::Justification::centred);
    }
}

void MixerPanel::drawChroBadge (juce::Graphics& g, int cx, int cy, int channel, bool locked) const
{
    const auto& theme = getTheme();
    const int bw = 18, bh = 16;
    const juce::Rectangle<float> r ((float)(cx - bw/2), (float)(cy - bh/2), (float)bw, (float)bh);

    const bool active = (channel > 0);
    g.setColour (active ? (locked ? theme.lockActive.withAlpha (0.15f) : theme.accent.withAlpha (0.15f))
                        : theme.separator.withAlpha (0.3f));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (active ? (locked ? theme.lockActive : theme.accent)
                        : (locked ? theme.lockActive.withAlpha (0.5f) : theme.foreground.withAlpha (0.25f)));
    g.drawRoundedRectangle (r, 2.5f, 0.8f);

    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (active ? (locked ? theme.lockActive : theme.accent)
                        : (locked ? theme.lockActive.withAlpha (0.5f) : theme.foreground.withAlpha (0.3f)));
    g.drawText (active ? juce::String (channel) : "-", r.toNearestInt(), juce::Justification::centred);
}

void MixerPanel::drawMeter (juce::Graphics& g,
                             int x, int y, int w, int h,
                             float peakL, float peakR,
                             juce::Colour tint, int si) const
{
    // ── Phosphor hairline meter ───────────────────────────────────────────
    // Two channels (L top, R bottom), each a single 1px fill bar plus a
    // glowing 1px hold marker.  Colours shift green → yellow → red like
    // a phosphor CRT trace.

    const int gap    = 2;
    const int barH   = (h - gap) / 2;   // height of each channel bar
    const int holdW  = 2;               // hold marker width in px

    // Update hold registers
    const int si2 = juce::jlimit (0, kMaxHoldSlices - 1, si);
    if (peakL > holdL[si2]) holdL[si2] = peakL;
    if (peakR > holdR[si2]) holdR[si2] = peakR;

    // Perceptual mapping: sqrt gives better visual resolution in the low end
    auto toFill = [] (float pk) -> float
    {
        return std::sqrt (juce::jlimit (0.0f, 1.0f, pk));
    };

    // Phosphor colour at normalised position 0-1 along bar
    auto phosphorCol = [&] (float pos, float /*pk*/) -> juce::Colour
    {
        if (pos < 0.70f)
        {
            // Green zone: dim base → bright phosphor green
            const float t = pos / 0.70f;
            return tint.withAlpha (0.25f + t * 0.65f);
        }
        else if (pos < 0.85f)
        {
            // Yellow zone
            const float t = (pos - 0.70f) / 0.15f;
            return tint.interpolatedWith (juce::Colour (0xFFFFE000), t)
                       .withAlpha (0.88f);
        }
        else
        {
            // Red zone
            const float t = (pos - 0.85f) / 0.15f;
            return juce::Colour (0xFFFF2222).withAlpha (0.75f + t * 0.20f);
        }
    };

    auto drawBar = [&] (int barY, float pk, float hold)
    {
        const float fill = toFill (pk);
        const int   litW = juce::roundToInt (fill * (float)(w - holdW - 2));

        // Dark background track
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillRect (x, barY, w, barH);

        // Thin border
        g.setColour (juce::Colour (0xFF1E1E1E));
        g.drawRect (x, barY, w, barH);

        // Gradient fill — drawn as a single gradient rect for performance
        if (litW > 0)
        {
            juce::ColourGradient grad (phosphorCol (0.0f,  pk), (float) x,           (float) barY,
                                       phosphorCol (fill,  pk), (float)(x + litW),   (float) barY,
                                       false);
            // add midpoint anchors for accurate colour mapping
            grad.addColour (0.70 / (double) juce::jmax (fill, 0.01f),
                            phosphorCol (0.70f * fill, pk));
            grad.addColour (juce::jlimit (0.0, 1.0, 0.85 / (double) juce::jmax (fill, 0.01f)),
                            phosphorCol (0.85f * fill, pk));
            g.setGradientFill (grad);
            g.fillRect (x + 1, barY + 1, litW, barH - 2);
        }

        // Hold marker — bright hairline with soft glow
        const float hFill = toFill (hold);
        const int   hx    = x + 1 + juce::roundToInt (hFill * (float)(w - holdW - 2));
        if (hFill > 0.01f && hx < x + w - 1)
        {
            // Outer glow
            g.setColour (phosphorCol (hFill, hold).withAlpha (0.25f));
            g.fillRect  (hx - 1, barY + 1, holdW + 2, barH - 2);
            // Core bright line
            g.setColour (phosphorCol (hFill, hold).withAlpha (0.95f));
            g.fillRect  (hx, barY + 1, holdW, barH - 2);
        }
    };

    drawBar (y,            peakL, holdL[si2]);
    drawBar (y + barH + gap, peakR, holdR[si2]);
}

void MixerPanel::drawSliceRow (juce::Graphics& g, int ry, int idx, bool selected) const
{
    const auto& theme = getTheme();
    const auto& snap  = processor.getUiSliceSnapshot();
    const auto& sl    = snap.slices[(size_t) idx];

    // Row background
    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.06f));
        g.fillRect (2, ry, getWidth() - 2, kRowH);
        g.setColour (theme.accent.withAlpha (0.55f));
        g.fillRect (0, ry, 2, kRowH);
    }
    else if (idx % 2 == 1)
    {
        g.setColour (juce::Colour (0xFF000000).withAlpha (0.12f));
        g.fillRect (0, ry, getWidth(), kRowH);
    }

    // Row bottom divider
    g.setColour (theme.separator.withAlpha (0.35f));
    g.drawHorizontalLine (ry + kRowH - 1, (float) kNameColW * 0.3f, (float) getWidth());

    // ── Slice name column — tinted with slice colour ─────────────────────
    const juce::Colour dot = sl.colour;

    // Colour bar on left edge (thicker, more visible than old dot)
    g.setColour (dot.withAlpha (0.85f));
    g.fillRect (0, ry, 3, kRowH);

    // Colour tint on name column background
    g.setColour (dot.withAlpha (0.07f));
    g.fillRect (3, ry, kNameColW - 4, kRowH);

    // Slice number — larger font, brighter
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (dot.withAlpha (selected ? 0.95f : 0.65f));
    g.drawText (juce::String (idx + 1).paddedLeft ('0', 2),
                8, ry, 30, kRowH, juce::Justification::centredLeft);

    // Duration
    const double srate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    const int end = processor.sliceManager.getEndForSlice (idx, snap.sampleNumFrames);
    const double lenSec = (end - sl.startSample) / srate;
    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (theme.foreground.withAlpha (0.30f));
    g.drawText (juce::String (lenSec, 2) + "s",
                40, ry, kNameColW - 42, kRowH, juce::Justification::centredLeft);

    // ── Knob columns ────────────────────────────────────────────────────
    const int kcy = ry + kRowH / 2;

    static constexpr int kTextOffX = kKnobR + 5;

    auto drawCol = [&] (Col col, float norm, bool locked,
                         const juce::String& valStr)
    {
        const int x = colX (col);
        const int cx = x + kKnobR + 8;
        drawKnobInRow (g, cx, kcy, norm, locked);

        const int tx = cx + kKnobR + 4;
        const int tw = kKnobColW - (tx - x) - 2;
        g.setFont (DysektLookAndFeel::makeFont (10.f));
        g.setColour (locked ? theme.foreground.withAlpha (0.90f)
                            : theme.foreground.withAlpha (0.40f));
        g.drawText (valStr, tx, ry + 1, tw, kRowH - 2, juce::Justification::centredLeft);
    };

    // GAIN
    const bool gainLocked = (sl.lockMask & kLockVolume) != 0;
    drawCol (ColGain, toNormGain (sl.volume), gainLocked, fmtGain (sl.volume));

    // PAN
    const bool panLocked = (sl.lockMask & kLockPan) != 0;
    drawCol (ColPan, toNormPan (sl.pan), panLocked, fmtPan (sl.pan));

    // FCUT
    const bool filterLocked = (sl.lockMask & kLockFilter) != 0;
    drawCol (ColFcut, toNormFcut (sl.filterCutoff), filterLocked, fmtFcut (sl.filterCutoff));

    // PRES
    drawCol (ColPres, toNormPres (sl.filterRes), filterLocked, fmtPres (sl.filterRes));

    // MUTE GRP
    {
        const bool mg_locked = (sl.lockMask & kLockMuteGroup) != 0;
        const int x = colX (ColMute);
        const int cx = x + kKnobColW / 2;
        drawMuteBadge (g, cx, kcy, sl.muteGroup, mg_locked);
    }

    // CHRO — chromatic MIDI channel badge
    {
        const bool chromaLocked = (sl.lockMask & kLockChromaticChannel) != 0;
        const int x  = colX (ColChro);
        const int cx = x + kKnobColW / 2;
        drawChroBadge (g, cx, kcy, sl.chromaticChannel, chromaLocked);
    }

    // OUT
    {
        const bool outLocked = (sl.lockMask & kLockOutputBus) != 0;
        const int x = colX (ColOut);
        const int cx = x + kKnobColW / 2 - 6;
        g.setFont (DysektLookAndFeel::makeFont (10.f));
        g.setColour (outLocked ? theme.foreground.withAlpha (0.85f)
                               : theme.foreground.withAlpha (0.32f));
        g.drawText (fmtOut (sl.outputBus), cx, ry, kKnobColW - 4, kRowH,
                    juce::Justification::centredLeft);
    }

    // METER — horizontal peak bar after OUT, tinted with slice colour
    {
        const int mx = colX (ColOut) + kKnobColW + 4;
        const int mw = getWidth() - mx - 6;
        if (mw > 20)
        {
            const int si = idx < kMaxMeterSlices ? idx : 0;
            const float pkL = processor.slicePeakL[si].load (std::memory_order_relaxed);
            const float pkR = processor.slicePeakR[si].load (std::memory_order_relaxed);
            drawMeter (g, mx, ry + 4, mw, kRowH - 8, pkL, pkR, dot, si);
        }
    }
}

void MixerPanel::drawMasterRow (juce::Graphics& g, int ry) const
{
    const auto& theme = getTheme();

    g.setColour (theme.accent.withAlpha (0.03f));
    g.fillRect (0, ry, getWidth(), kMasterH);
    g.setColour (theme.accent.withAlpha (0.12f));
    g.drawHorizontalLine (ry, 0.f, (float) getWidth());

    // Label
    g.setFont (DysektLookAndFeel::makeFont (8.0f, true));
    g.setColour (theme.accent.withAlpha (0.6f));
    g.drawText ("MASTER", 10, ry, kNameColW - 10, kMasterH, juce::Justification::centredLeft);

    const float masterDb  = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
    const float masterPan = processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();

    const int kcy = ry + kMasterH / 2;

    auto drawMasterCol = [&] (Col col, float norm, const juce::String& valStr)
    {
        const int x  = colX (col);
        const int cx = x + kKnobR + 8;
        drawKnobInRow (g, cx, kcy, norm, false, true);
        const int tx = cx + kKnobR + 4;
        const int tw = kKnobColW - (tx - x) - 2;
        g.setFont (DysektLookAndFeel::makeFont (9.f));
        g.setColour (theme.accent.withAlpha (0.55f));
        g.drawText (valStr, tx, ry + 1, tw, kMasterH - 2, juce::Justification::centredLeft);
    };

    drawMasterCol (ColGain, toNormGain (masterDb),  fmtGain (masterDb));
    drawMasterCol (ColPan,  toNormPan  (masterPan), fmtPan  (masterPan));

    // Remaining columns — dimmed dashes
    g.setFont (DysektLookAndFeel::makeFont (8.f));
    g.setColour (theme.foreground.withAlpha (0.15f));
    for (int i = ColFcut; i < kNumCols; ++i)
        g.drawText ("—", colX ((Col)i), ry, kKnobColW, kMasterH, juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────────
//  paint
// ─────────────────────────────────────────────────────────────────────────────
void MixerPanel::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const auto& snap  = processor.getUiSliceSnapshot();

    // ── LCD-style frame — matches waveform + LCD screen aesthetic ────────────
    {
        const auto ac = theme.accent;
        auto b = getLocalBounds();

        juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
                                         juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
        g.setGradientFill (outerGrad);
        g.fillRoundedRectangle (b.toFloat(), 4.0f);

        g.setColour (ac.withAlpha (0.20f));
        g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

        auto screen = b.reduced (4);
        g.setColour (theme.darkBar.darker (0.55f));
        g.fillRoundedRectangle (screen.toFloat(), 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (int y = screen.getY(); y < screen.getBottom(); y += 2)
            g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

        juce::ColourGradient glow (ac.withAlpha (0.06f), 0, (float) screen.getY(),
                                    juce::Colours::transparentBlack, 0, (float) (screen.getY() + 20), false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (screen.toFloat(), 2.0f);

        g.setColour (ac.withAlpha (0.12f));
        g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
    }

    // Clip ALL content to inner screen rect so the frame border is never overwritten
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (4));

    for (int i = 0; i < snap.numSlices; ++i)
        drawSliceRow (g, rowY (i), i, i == snap.selectedSlice);

    drawMasterRow (g, masterRowY());

    // Column dividers
    g.setColour (theme.accent.withAlpha (0.12f));
    g.drawVerticalLine (kNameColW - 1, 0.f, (float) getHeight());
    for (int i = 1; i < kNumCols; ++i)
        g.drawVerticalLine (colX ((Col)i) - 1, (float) kHeaderH, (float) getHeight());

    // Meter column separator
    {
        const int mx = colX (ColOut) + kKnobColW;
        g.setColour (theme.accent.withAlpha (0.12f));
        g.drawVerticalLine (mx + 2, (float) kHeaderH, (float) getHeight());
        g.setFont (DysektLookAndFeel::makeFont (7.5f));
        g.setColour (theme.foreground.withAlpha (0.22f));
        g.drawText ("METER", mx + 6, 0, kMeterColW - 8, kHeaderH, juce::Justification::centredLeft);
    }

    drawHeader (g);

    g.restoreState();
}

void MixerPanel::resized() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse
// ─────────────────────────────────────────────────────────────────────────────
void MixerPanel::mouseDown (const juce::MouseEvent& e)
{
    if (textEditor) { textEditor.reset(); repaint(); }

    const auto& snap = processor.getUiSliceSnapshot();
    const Cell c = hitTest (e.getPosition());

    // Click on name column (c.row == -2) or anywhere in a valid row → select slice
    if (c.row >= 0 && c.row < snap.numSlices)
    {
        // Always select the clicked row
        DysektProcessor::Command sel;
        sel.type = DysektProcessor::CmdSelectSlice;
        sel.intParam1 = c.row;
        processor.pushCommand (sel);

        // If click was in the name column, don't start a drag
        if (c.row == -2) { repaint(); return; }
    }

    if (!c.isMaster && (c.row < 0 || c.row >= snap.numSlices)) return;

    // Mute group — cycle on click (no drag)
    if (c.col == ColMute)
    {
        if (!c.isMaster)
        {
            const auto& sl = snap.slices[(size_t) c.row];
            int next = (sl.muteGroup + 1);
            if (next > 32) next = 0;
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdSetSliceParam;
            cmd.intParam1 = DysektProcessor::FieldMuteGroup;
            cmd.floatParam1 = (float) next;
            processor.pushCommand (cmd);
        }
        repaint(); return;
    }

    // CHRO — cycle channel 0→1→2→...→16→0 on click
    if (c.col == ColChro)
    {
        if (!c.isMaster)
        {
            const auto& sl = snap.slices[(size_t) c.row];
            if (e.mods.isRightButtonDown())
            {
                // Right-click: toggle chromatic channel lock
                DysektProcessor::Command cmd;
                cmd.type      = DysektProcessor::CmdToggleLock;
                cmd.intParam1 = (int) kLockChromaticChannel;
                processor.pushCommand (cmd);
            }
            else
            {
                // Left-click: cycle channel 0→1→...→16→0
                int next = (sl.chromaticChannel + 1) % 17;
                DysektProcessor::Command cmd;
                cmd.type = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1 = DysektProcessor::FieldChromaticChannel;
                cmd.floatParam1 = (float) next;
                processor.pushCommand (cmd);
            }
        }
        repaint(); return;
    }

    // OUT — cycle on click
    if (c.col == ColOut)
    {
        if (!c.isMaster)
        {
            const auto& sl = snap.slices[(size_t) c.row];
            int next = (sl.outputBus + 1) % 16;
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdSetSliceParam;
            cmd.intParam1 = DysektProcessor::FieldOutputBus;
            cmd.floatParam1 = (float) next;
            processor.pushCommand (cmd);
        }
        repaint(); return;
    }

    // Begin knob drag
    drag.active   = true;
    drag.isMaster = c.isMaster;
    drag.sliceIdx = c.row;
    drag.col      = c.col;
    drag.startY   = e.getScreenPosition().y;

    if (c.isMaster)
    {
        drag.startVal = (c.col == ColGain)
            ? processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load()
            : processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();
    }
    else
    {
        const auto& sl = snap.slices[(size_t) c.row];
        switch (c.col)
        {
            case ColGain: drag.startVal = sl.volume;       break;
            case ColPan:  drag.startVal = sl.pan;          break;
            case ColFcut: drag.startVal = sl.filterCutoff; break;
            case ColPres: drag.startVal = sl.filterRes;    break;
            default:      drag.startVal = 0.f;             break;
        }
    }
}

void MixerPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (!drag.active) return;

    const float dy       = (float)(drag.startY - e.getScreenPosition().y);
    const bool  fine     = e.mods.isShiftDown();
    const float fineMult = fine ? 0.1f : 1.0f;

    float newVal = drag.startVal;

    if (drag.isMaster)
    {
        if (drag.col == ColGain)
        {
            newVal = juce::jlimit (-100.f, 24.f, drag.startVal + dy * 0.5f * fineMult);
            processor.apvts.getRawParameterValue (ParamIds::masterVolume)->store (newVal);
        }
        else if (drag.col == ColPan)
        {
            newVal = juce::jlimit (-1.f, 1.f, drag.startVal + dy * 0.01f * fineMult);
            processor.apvts.getRawParameterValue (ParamIds::defaultPan)->store (newVal);
        }
        repaint(); return;
    }

    DysektProcessor::Command cmd;
    cmd.type = DysektProcessor::CmdSetSliceParam;

    switch (drag.col)
    {
        case ColGain:
            newVal = juce::jlimit (-100.f, 24.f, drag.startVal + dy * 0.5f * fineMult);
            cmd.intParam1 = DysektProcessor::FieldVolume;
            break;
        case ColPan:
            newVal = juce::jlimit (-1.f, 1.f, drag.startVal + dy * 0.01f * fineMult);
            cmd.intParam1 = DysektProcessor::FieldPan;
            break;
        case ColFcut:
        {
            // Log-space drag: convert to norm, adjust, convert back
            float normStart = toNormFcut (drag.startVal);
            float normNew   = juce::jlimit (0.f, 1.f, normStart + dy * 0.005f * fineMult);
            newVal = fromNormFcut (normNew);
            cmd.intParam1 = DysektProcessor::FieldFilterCutoff;
            break;
        }
        case ColPres:
            newVal = juce::jlimit (0.f, 1.f, drag.startVal + dy * 0.005f * fineMult);
            cmd.intParam1 = DysektProcessor::FieldFilterRes;
            break;
        default: return;
    }

    cmd.floatParam1 = newVal;
    processor.pushCommand (cmd);
    repaint();
}

void MixerPanel::mouseUp (const juce::MouseEvent&)
{
    drag.active = false;
}

void MixerPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const Cell c = hitTest (e.getPosition());
    if (c.col == ColMute || c.col == ColOut) return;
    if (!c.isMaster && (c.row < 0)) return;
    if (c.isMaster && c.col >= ColFcut) return;

    // Get current value as display string
    float currentVal = 0.f;
    juce::String suffix;

    if (c.isMaster)
    {
        currentVal = (c.col == ColGain)
            ? processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load()
            : processor.apvts.getRawParameterValue (ParamIds::defaultPan)->load();
    }
    else
    {
        const auto& snap = processor.getUiSliceSnapshot();
        if (c.row < 0 || c.row >= snap.numSlices) return;
        const auto& sl = snap.slices[(size_t) c.row];
        switch (c.col)
        {
            case ColGain: currentVal = sl.volume;        break;
            case ColPan:  currentVal = sl.pan;           break;
            case ColFcut: currentVal = sl.filterCutoff;  break;
            case ColPres: currentVal = sl.filterRes * 100.f; break;
            default: return;
        }
    }

    const juce::Rectangle<int> cellBounds = c.bounds;

    textEditor = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (*textEditor);
    textEditor->setBounds (cellBounds.getX() + kKnobR * 2 + 12,
                           cellBounds.getY() + cellBounds.getHeight() / 2 - 8,
                           cellBounds.getWidth() - kKnobR * 2 - 16, 16);
    textEditor->setFont (DysektLookAndFeel::makeFont (9.0f));
    const auto& theme = getTheme();
    textEditor->setColour (juce::TextEditor::backgroundColourId, theme.darkBar.brighter (0.15f));
    textEditor->setColour (juce::TextEditor::textColourId,       theme.foreground);
    textEditor->setColour (juce::TextEditor::outlineColourId,    theme.accent);
    textEditor->setText (juce::String (currentVal, c.col == ColPres ? 0 : 2), false);
    textEditor->selectAll();
    textEditor->grabKeyboardFocus();

    const bool  isMaster = c.isMaster;
    const Col   col      = c.col;
    const int   rowIdx   = c.row;

    textEditor->onReturnKey = [this, isMaster, col, rowIdx]
    {
        if (!textEditor) return;
        float v = textEditor->getText().getFloatValue();
        textEditor.reset();

        if (isMaster)
        {
            if (col == ColGain)
            {
                v = juce::jlimit (-100.f, 24.f, v);
                processor.apvts.getRawParameterValue (ParamIds::masterVolume)->store (v);
            }
            else if (col == ColPan)
            {
                v = juce::jlimit (-1.f, 1.f, v);
                processor.apvts.getRawParameterValue (ParamIds::defaultPan)->store (v);
            }
            repaint(); return;
        }

        // Select row first
        DysektProcessor::Command sel;
        sel.type = DysektProcessor::CmdSelectSlice;
        sel.intParam1 = rowIdx;
        processor.pushCommand (sel);

        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdSetSliceParam;
        switch (col)
        {
            case ColGain: cmd.intParam1 = DysektProcessor::FieldVolume;       cmd.floatParam1 = juce::jlimit (-100.f, 24.f, v);    break;
            case ColPan:  cmd.intParam1 = DysektProcessor::FieldPan;          cmd.floatParam1 = juce::jlimit (-1.f, 1.f, v);       break;
            case ColFcut: cmd.intParam1 = DysektProcessor::FieldFilterCutoff; cmd.floatParam1 = juce::jlimit (20.f, 20000.f, v);   break;
            case ColPres: cmd.intParam1 = DysektProcessor::FieldFilterRes;    cmd.floatParam1 = juce::jlimit (0.f, 1.f, v / 100.f); break;
            default: repaint(); return;
        }
        processor.pushCommand (cmd);
        repaint();
    };
    textEditor->onEscapeKey = [this] { textEditor.reset(); repaint(); };
    textEditor->onFocusLost = [this] { textEditor.reset(); repaint(); };
}

void MixerPanel::mouseWheelMove (const juce::MouseEvent&,
                                   const juce::MouseWheelDetails& wheel)
{
    const auto& snap = processor.getUiSliceSnapshot();
    const int contentH = snap.numSlices * kRowH + kMasterH;
    const int visibleH = getHeight() - kHeaderH;
    const int maxScroll = juce::jmax (0, contentH - visibleH);

    scrollPixels = juce::jlimit (0, maxScroll,
        scrollPixels - (int)(wheel.deltaY * 30.f));
    repaint();
}
