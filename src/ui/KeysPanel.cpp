#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// 2 octaves: semitone offsets for each white key
const int KeysPanel::whiteOffsets[14] = {
    0,2,4,5,7,9,11,      // octave 0
    12,14,16,17,19,21,23 // octave 1
};

const KeysPanel::BlackDef KeysPanel::blackDefs[10] = {
    {0,1},{1,3},{3,6},{4,8},{5,10},    // octave 0
    {7,13},{8,15},{10,18},{11,20},{12,22} // octave 1
};

// =============================================================================
// ZoneMatrixContent::rebuild
// =============================================================================

void KeysPanel::ZoneMatrixContent::rebuild (const std::vector<Keyzone>& zones,
                                             int kbX, int kbW,
                                             int baseOctave,
                                             int whiteKeyW, int blackKeyW,
                                             int componentWidth)
{
    kbX_        = kbX;
    kbW_        = kbW;
    baseOctave_ = baseOctave;
    contentW_   = componentWidth;

    rows.clear();
    rows.reserve (zones.size());

    for (const auto& z : zones)
    {
        Row r;
        r.zone = z;
        rows.push_back (r);
    }

    // Height = header + one row per zone (minimum 1 row tall for empty state)
    const int totalH = kHeaderH + juce::jmax (1, (int) rows.size()) * kRowH;
    setSize (componentWidth, totalH);

    if (selectedRow >= (int) rows.size())
        selectedRow = -1;

    repaint();
}

// =============================================================================
// ZoneMatrixContent::paint   — HALion-style text-column table
//
// Column layout (left → right):
//   [3px colour stripe] [#  ] [Name       ] [KeyRange ] [Root ] [Vel    ] [Lp]
//    col stripe          idx   name col      key col     root    vel col   loop
// =============================================================================

void KeysPanel::ZoneMatrixContent::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const int   w     = getWidth();
    const int   h     = getHeight();

    // ── Column x-positions (all relative to component left) ───────────────────
    // Stripe: 0..2 (3px)
    // #:      3..18 (16px)
    // Name:   19..94 (75px)
    // Key:    95..170 (75px)
    // Root:   171..206 (35px)
    // Vel:    207..262 (55px)
    // Loop:   263..w

    constexpr int kStripeW  = 3;
    constexpr int kIdxX     = 3;   constexpr int kIdxW  = 16;
    constexpr int kNameX    = 19;  constexpr int kNameW = 75;
    constexpr int kKeyX     = 95;  constexpr int kKeyW  = 75;
    constexpr int kRootX    = 171; constexpr int kRootW = 35;
    constexpr int kVelX     = 207; constexpr int kVelW  = 55;
    constexpr int kLoopX    = 263;

    // ── Background ────────────────────────────────────────────────────────────
    g.setColour (theme.darkBar.darker (0.55f));
    g.fillRect (0, 0, w, h);

    if (rows.empty())
    {
        g.setFont (DysektLookAndFeel::makeFont (9.0f));
        g.setColour (theme.foreground.withAlpha (0.18f));
        g.drawText ("No zones loaded", 0, 0, w, h, juce::Justification::centred, false);
        return;
    }

    // ── Column header ─────────────────────────────────────────────────────────
    {
        const int hy = 0;
        g.setColour (theme.darkBar.darker (0.15f));
        g.fillRect (0, hy, w, kHeaderH);

        g.setFont (DysektLookAndFeel::makeFont (8.0f, true));
        g.setColour (theme.foreground.withAlpha (0.28f));

        auto hdr = [&](const char* txt, int x, int cw, juce::Justification j = juce::Justification::centredLeft)
        {
            g.drawText (txt, x + 2, hy, cw - 4, kHeaderH, j, false);
        };

        hdr ("#",     kIdxX,  kIdxW,  juce::Justification::centred);
        hdr ("NAME",  kNameX, kNameW);
        hdr ("KEY",   kKeyX,  kKeyW);
        hdr ("ROOT",  kRootX, kRootW);
        hdr ("VEL",   kVelX,  kVelW);
        if (w > kLoopX + 10)
            hdr ("LP", kLoopX, w - kLoopX);

        // Header bottom separator
        g.setColour (theme.separator.withAlpha (0.50f));
        g.drawHorizontalLine (kHeaderH - 1, 0.f, (float) w);
    }

    // ── Rows ──────────────────────────────────────────────────────────────────
    const juce::Font fMain = DysektLookAndFeel::makeFont (10.0f);
    const juce::Font fSmall = DysektLookAndFeel::makeFont (9.0f);

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& r  = rows[(size_t) i];
        const int   ry = kHeaderH + i * kRowH;
        const bool  sel = (i == selectedRow);
        const juce::Colour zc = r.zone.colour;

        // Alternating row tint
        if (i % 2 == 1)
        {
            g.setColour (juce::Colour (0xFF000000).withAlpha (0.10f));
            g.fillRect (0, ry, w, kRowH);
        }

        // Selected row highlight wash
        if (sel)
        {
            g.setColour (theme.accent.withAlpha (0.08f));
            g.fillRect (kStripeW, ry, w - kStripeW, kRowH);
        }

        // ── 3px colour stripe ──────────────────────────────────────────────────
        g.setColour (zc.withAlpha (sel ? 1.0f : 0.80f));
        g.fillRect (0, ry, kStripeW, kRowH);

        // ── Text helper: draws centred-vertically in this row ─────────────────
        auto cell = [&](const juce::String& txt, int cx, int cw,
                        juce::Colour col,
                        juce::Justification just = juce::Justification::centredLeft)
        {
            g.setColour (col);
            g.drawText (txt, cx + 2, ry, cw - 4, kRowH, just, false);
        };

        // ── # (zone index, 1-based) ────────────────────────────────────────────
        g.setFont (fSmall);
        cell (juce::String (i + 1), kIdxX, kIdxW,
              theme.foreground.withAlpha (0.25f), juce::Justification::centred);

        // ── Name ───────────────────────────────────────────────────────────────
        {
            const juce::String name = r.zone.name.isNotEmpty()
                ? r.zone.name.toUpperCase().substring (0, 10)
                : ("ZN" + juce::String (i + 1));
            g.setFont (fMain);
            cell (name, kNameX, kNameW,
                  zc.withAlpha (sel ? 0.95f : 0.70f));
        }

        // ── Key range ─────────────────────────────────────────────────────────
        {
            const juce::String keyStr =
                juce::MidiMessage::getMidiNoteName (r.zone.loKey,  true, true, 3)
                + "-"
                + juce::MidiMessage::getMidiNoteName (r.zone.hiKey, true, true, 3);
            g.setFont (fSmall);
            cell (keyStr, kKeyX, kKeyW,
                  theme.foreground.withAlpha (sel ? 0.85f : 0.55f));
        }

        // ── Root pitch ────────────────────────────────────────────────────────
        {
            const juce::String rootStr = (r.zone.rootPitch >= 0)
                ? juce::MidiMessage::getMidiNoteName (r.zone.rootPitch, true, true, 3)
                : "-";
            g.setFont (fSmall);
            cell (rootStr, kRootX, kRootW,
                  theme.foreground.withAlpha (sel ? 0.80f : 0.45f),
                  juce::Justification::centred);
        }

        // ── Velocity ──────────────────────────────────────────────────────────
        {
            const bool isFull = (r.zone.loVel == 0 && r.zone.hiVel == 127);
            const juce::String velStr = "v" + juce::String (r.zone.loVel)
                                        + "-" + juce::String (r.zone.hiVel);
            g.setFont (fSmall);
            cell (velStr, kVelX, kVelW,
                  isFull ? theme.foreground.withAlpha (0.28f)
                         : theme.accent.withAlpha (sel ? 0.90f : 0.65f),
                  juce::Justification::centred);
        }

        // ── Loop flag ─────────────────────────────────────────────────────────
        if (w > kLoopX + 10 && r.zone.isLooped)
        {
            g.setFont (fSmall);
            cell ("\xe2\x86\xba", kLoopX, w - kLoopX,   // UTF-8 ↺
                  theme.accent.withAlpha (0.60f),
                  juce::Justification::centred);
        }

        // ── Row bottom divider ─────────────────────────────────────────────────
        g.setColour (theme.separator.withAlpha (0.20f));
        g.drawHorizontalLine (ry + kRowH - 1, (float) kStripeW, (float) w);
    }

    // ── Vertical column separators (subtle) ───────────────────────────────────
    g.setColour (theme.separator.withAlpha (0.18f));
    for (int cx : { kKeyX - 1, kRootX - 1, kVelX - 1 })
        if (cx < w)
            g.drawVerticalLine (cx, (float) kHeaderH, (float) h);
}

// =============================================================================
// ZoneMatrixContent::mouseDown
// =============================================================================

void KeysPanel::ZoneMatrixContent::mouseDown (const juce::MouseEvent& e)
{
    // Clicks in the header row are ignored
    if (e.y < kHeaderH)
        return;

    const int clickedRow = (e.y - kHeaderH) / kRowH;
    if (clickedRow >= 0 && clickedRow < (int) rows.size())
    {
        selectedRow = clickedRow;
        repaint();

        // Audition: trigger note-on for this zone's loKey
        const int note = rows[(size_t) clickedRow].zone.loKey;
        owner.processor.sfzUiNoteOnRequest.store (note, std::memory_order_relaxed);
        owner.lastActiveNote = note;
    }
}

// =============================================================================
// KeysPanel
// =============================================================================

KeysPanel::KeysPanel (DysektProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);

    addAndMakeVisible (transposeDownBtn);
    addAndMakeVisible (transposeUpBtn);
    transposeDownBtn.setMouseCursor (juce::MouseCursor::NormalCursor);
    transposeUpBtn  .setMouseCursor (juce::MouseCursor::NormalCursor);

    transposeDownBtn.onClick = [this] { if (baseOctave > 0) { --baseOctave; resized(); repaint(); } };
    transposeUpBtn  .onClick = [this] { if (baseOctave < 8) { ++baseOctave; resized(); repaint(); } };

    zoneViewport.setScrollBarsShown (true, false);
    zoneViewport.setViewedComponent (&zoneMatrix, false);
    zoneViewport.setScrollBarThickness (6);
    addAndMakeVisible (zoneViewport);

    startTimerHz (30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

// =============================================================================

void KeysPanel::setKeyzones (std::vector<Keyzone> zones)
{
    keyzones = std::move (zones);
    rebuildZoneMatrix();
    repaint();
}

void KeysPanel::clearKeyzones()
{
    keyzones.clear();
    rebuildZoneMatrix();
    repaint();
}

void KeysPanel::autoScrollToZones()
{
    if (keyzones.empty()) return;
    int lo = 127, hi = 0;
    for (auto& z : keyzones) { lo = std::min (lo, z.loKey); hi = std::max (hi, z.hiKey); }
    baseOctave = juce::jlimit (0, 8, (lo + hi) / 2 / 12 - 1);
    resized();
    repaint();
}

void KeysPanel::rebuildZoneMatrix()
{
    if (keyRects.empty()) return;

    const int kbX = keyRects.front().bounds.getX();
    const int kbW = kWhiteKeyW * kNumWhite;

    // Viewport spans full panel width for the name column to sit cleanly left
    const int vpW = getWidth();

    zoneMatrix.rebuild (keyzones, kbX, kbW, baseOctave,
                        kWhiteKeyW, kBlackKeyW, vpW);
}

// =============================================================================
// resized
// =============================================================================

void KeysPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    kTransposeRowH = juce::jmin (22, h / 6);

    const int kKeyHMin = juce::jmin (60, h / 3);
    kKeyH = juce::jmax (kKeyHMin, juce::jmin (80, h / 3));

    kZoneViewH = juce::jmax (0, h - kTransposeRowH - kKeyH);

    kWhiteKeyW = juce::jmax (6, w / kNumWhite);
    kBlackKeyW = juce::roundToInt (kWhiteKeyW * 0.52f);
    kBlackKeyH = juce::roundToInt ((float) kKeyH * 0.62f);

    const int kbW       = kWhiteKeyW * kNumWhite;
    const int keyboardX = (w - kbW) / 2;
    const int keyboardY = h - kKeyH;

    const int cx = w / 2;
    transposeDownBtn.setBounds (cx - 72, 2, 20, kTransposeRowH - 4);
    transposeUpBtn  .setBounds (cx + 52, 2, 20, kTransposeRowH - 4);

    keyRects.clear();
    for (int i = 0; i < kNumWhite; ++i)
    {
        KeyRect kr;
        kr.bounds = { keyboardX + i * kWhiteKeyW, keyboardY, kWhiteKeyW - 1, kKeyH };
        kr.note   = baseOctave * 12 + whiteOffsets[i];
        kr.isBlack = false;
        keyRects.push_back (kr);
    }
    for (int i = 0; i < kNumBlack; ++i)
    {
        KeyRect kr;
        int bx = keyboardX + (blackDefs[i].afterWhite + 1) * kWhiteKeyW - kBlackKeyW / 2;
        kr.bounds  = { bx, keyboardY, kBlackKeyW, kBlackKeyH };
        kr.note    = baseOctave * 12 + blackDefs[i].offset;
        kr.isBlack = true;
        keyRects.push_back (kr);
    }

    // ── Position the zone viewport — full width, below transpose row ──────────
    if (kZoneViewH > 0)
    {
        zoneViewport.setBounds (0, kTransposeRowH, w, kZoneViewH);
        zoneViewport.setVisible (true);
        rebuildZoneMatrix();
    }
    else
    {
        zoneViewport.setVisible (false);
    }
}

// =============================================================================
// paint
// =============================================================================

void KeysPanel::paint (juce::Graphics& g)
{
    const auto& theme  = getTheme();
    const auto  accent = theme.accent;

    // ── Outer panel background ────────────────────────────────────────────────
    g.setColour (theme.darkBar.darker (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    // ── Button colours ────────────────────────────────────────────────────────
    for (auto* btn : { &transposeDownBtn, &transposeUpBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  theme.darkBar.brighter (0.08f));
        btn->setColour (juce::TextButton::textColourOffId, accent.withAlpha (0.80f));
        btn->setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.2f));
    }

    // ── Transpose / info strip ────────────────────────────────────────────────
    {
        // If a zone row is selected, show that zone's info; otherwise show octave range
        juce::String label;
        const int selRow = zoneMatrix.selectedRow;

        if (selRow >= 0 && selRow < (int) keyzones.size())
        {
            const auto& z = keyzones[(size_t) selRow];
            label = z.name.isNotEmpty() ? z.name.toUpperCase() : "ZONE";

            if (z.rootPitch >= 0)
                label += "  root:" + juce::MidiMessage::getMidiNoteName (z.rootPitch, true, true, 3);

            label += "  v" + juce::String (z.loVel) + "-" + juce::String (z.hiVel);

            if (z.isLooped) label += "  \xe2\x86\xba";
        }
        else
        {
            label = "C" + juce::String (baseOctave)
                    + " \xe2\x80\x93 C" + juce::String (baseOctave + 2);
        }

        g.setFont   (DysektLookAndFeel::makeFont (9.0f, true));
        g.setColour (accent.withAlpha (0.50f));
        g.drawText  (label, 0, 2, getWidth(), kTransposeRowH - 2, juce::Justification::centred);
    }

    // ── LCD-style frame around zone viewport ─────────────────────────────────
    if (kZoneViewH > 0)
    {
        const juce::Rectangle<float> frameRect (
            0.f, (float) kTransposeRowH, (float) getWidth(), (float) kZoneViewH);

        // Outer gradient fill
        juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, (float) kTransposeRowH,
                                        juce::Colour (0xFF0E0E0E), 0,
                                        (float)(kTransposeRowH + kZoneViewH), false);
        g.setGradientFill (outerGrad);
        g.fillRoundedRectangle (frameRect, 2.0f);

        // Accent border
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (frameRect.reduced (0.5f), 2.0f, 1.0f);

        // Inner screen area
        const auto screen = frameRect.reduced (2.f);
        g.setColour (theme.darkBar.darker (0.55f));
        g.fillRoundedRectangle (screen, 1.5f);

        // Scanlines
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (int y = (int) screen.getY(); y < (int) screen.getBottom(); y += 2)
            g.drawHorizontalLine (y, screen.getX(), screen.getRight());

        // Top glow
        juce::ColourGradient glow (accent.withAlpha (0.05f), 0, screen.getY(),
                                   juce::Colours::transparentBlack, 0,
                                   screen.getY() + 16.f, false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (screen, 1.5f);

        // Inner border
        g.setColour (accent.withAlpha (0.10f));
        g.drawRoundedRectangle (screen.expanded (0.5f), 1.5f, 1.0f);
    }

    // ── Keyboard ──────────────────────────────────────────────────────────────
    const auto& ui = processor.getUiSliceSnapshot();
    std::set<int> slicedNotes;
    for (int s = 0; s < ui.numSlices; ++s)
        slicedNotes.insert (ui.slices[(size_t) s].midiNote);

    // White keys first
    for (const auto& kr : keyRects)
    {
        if (kr.isBlack) continue;
        const int n = kr.note;
        const bool midiActive = (n >= 0 && n < 128)
            ? ((n < 64 ? (sfzActiveSnap[0] >> n) : (sfzActiveSnap[1] >> (n - 64))) & 1) != 0
            : false;
        drawKey (g, kr, slicedNotes.count (n) > 0, n == hoveredNote, n == lastActiveNote || midiActive);
    }
    // Black keys on top
    for (const auto& kr : keyRects)
    {
        if (! kr.isBlack) continue;
        const int n = kr.note;
        const bool midiActive = (n >= 0 && n < 128)
            ? ((n < 64 ? (sfzActiveSnap[0] >> n) : (sfzActiveSnap[1] >> (n - 64))) & 1) != 0
            : false;
        drawKey (g, kr, slicedNotes.count (n) > 0, n == hoveredNote, n == lastActiveNote || midiActive);
    }

    // ── Root-pitch dots on keyboard ───────────────────────────────────────────
    // One small coloured ellipse per zone at its root note
    for (const auto& z : keyzones)
    {
        if (z.rootPitch < 0) continue;

        const int n = z.rootPitch;
        if (n < baseOctave * 12 || n >= (baseOctave + 2) * 12) continue;

        const int kbX = keyRects.empty() ? 0 : keyRects.front().bounds.getX();
        const float rx = noteToX (n, kbX);
        const int   keyboardY = getHeight() - kKeyH;

        static const bool isBlackSemi[12] =
            { false,true,false,true,false,false,true,false,true,false,true,false };
        const bool bk = isBlackSemi[((n % 12) + 12) % 12];

        const float dotY = bk ? (float)(keyboardY + kBlackKeyH - 7)
                               : (float)(keyboardY + kKeyH    - 9);

        g.setColour (z.colour.brighter (0.6f).withAlpha (0.90f));
        g.fillEllipse (rx + 2.0f, dotY, 5.0f, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.drawEllipse (rx + 2.0f, dotY, 5.0f, 5.0f, 0.8f);
    }
}

// =============================================================================
// drawKey — unchanged from original
// =============================================================================

void KeysPanel::drawKey (juce::Graphics& g, const KeyRect& kr,
                          bool hasSlice, bool hovered, bool active) const
{
    const auto& theme  = getTheme();
    const auto  accent = theme.accent;
    const auto  b      = kr.bounds.toFloat();

    if (! kr.isBlack)
    {
        const juce::Colour topCol = active  ? accent.withBrightness (0.92f).withAlpha (0.9f)
                                   : hovered ? juce::Colour (0xFFF4F4F4)
                                             : juce::Colour (0xFFECECEC);
        const juce::Colour botCol = active  ? accent.withBrightness (0.75f).withAlpha (0.7f)
                                   : hovered ? juce::Colour (0xFFDCDCDC)
                                             : juce::Colour (0xFFD0D0D0);

        juce::ColourGradient grad (topCol, 0.f, b.getY(), botCol, 0.f, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b.reduced (0.5f, 0.f).withTrimmedBottom (-4.f), 3.0f);

        g.setColour (juce::Colour (0xFF707070).withAlpha (0.40f));
        g.fillRect  (b.getRight() - 1.0f, b.getY() + 4.0f, 1.0f, b.getHeight() - 8.0f);

        g.setColour (juce::Colours::white.withAlpha (active ? 0.55f : 0.18f));
        g.fillRoundedRectangle (b.getX() + 2.0f, b.getBottom() - 10.0f,
                                b.getWidth() - 4.0f, 7.0f, 2.0f);

        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.80f));
            g.drawRoundedRectangle (b.reduced (1.0f, 0.5f), 3.0f, 1.2f);
        }

        if ((kr.note % 12) == 0)
        {
            g.setFont   (DysektLookAndFeel::makeFont (6.5f));
            g.setColour (hasSlice ? accent.withAlpha (0.9f) : juce::Colour (0xFF999999));
            g.drawText  (juce::MidiMessage::getMidiNoteName (kr.note, true, true, 3),
                         kr.bounds.getX(), kr.bounds.getBottom() - 12,
                         kr.bounds.getWidth(), 11, juce::Justification::centred);
        }
    }
    else
    {
        const juce::Colour topCol = active  ? accent.darker (0.1f)
                                   : hovered ? juce::Colour (0xFF383838)
                                             : juce::Colour (0xFF222222);
        const juce::Colour botCol = active  ? accent.darker (0.5f)
                                   : hovered ? juce::Colour (0xFF181818)
                                             : juce::Colour (0xFF0A0A0A);

        juce::ColourGradient grad (topCol, 0.f, b.getY(), botCol, 0.f, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 2.5f);

        g.setColour (juce::Colours::white.withAlpha (active ? 0.22f : 0.09f));
        g.fillRoundedRectangle (b.getX() + 2.0f, b.getY() + 1.5f, b.getWidth() - 4.0f, 5.0f, 1.5f);

        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.85f));
            g.drawRoundedRectangle (b.reduced (0.8f), 2.5f, 1.0f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.60f));
        g.drawRoundedRectangle (b.expanded (0.5f), 3.0f, 0.8f);
    }
}

// =============================================================================
// noteToX — unchanged
// =============================================================================

float KeysPanel::noteToX (int note, int kbX) const
{
    static const int semitoneToWhiteOrBlack[12] =
    {
        /* C  */  0,
        /* C# */ -(0+1),
        /* D  */  1,
        /* D# */ -(1+1),
        /* E  */  2,
        /* F  */  3,
        /* F# */ -(3+1),
        /* G  */  4,
        /* G# */ -(4+1),
        /* A  */  5,
        /* A# */ -(5+1),
        /* B  */  6,
    };

    const int rel    = note - baseOctave * 12;
    const int octave = rel / 12;
    const int semi   = juce::jlimit (0, 11, ((rel % 12) + 12) % 12);
    const int enc    = semitoneToWhiteOrBlack[semi];

    const float octaveX = (float) kbX + (float)(octave * 7) * (float) kWhiteKeyW;

    if (enc >= 0)
        return octaveX + (float)(enc) * (float) kWhiteKeyW;
    else
    {
        const int afterWhite = (-enc) - 1;
        return octaveX + (float)(afterWhite + 1) * (float) kWhiteKeyW
               - (float) kBlackKeyW * 0.5f;
    }
}

float KeysPanel::noteKeyWidth (int note) const
{
    static const bool isBlack[12] =
        { false, true, false, true, false, false, true, false, true, false, true, false };
    const int semi = ((note % 12) + 12) % 12;
    return isBlack[semi] ? (float) kBlackKeyW : (float) kWhiteKeyW;
}

juce::Colour KeysPanel::zoneColourForNote (int note) const
{
    for (const auto& z : keyzones)
        if (note >= z.loKey && note <= z.hiKey)
            return z.colour;
    return juce::Colour (0);
}

// =============================================================================
// Mouse
// =============================================================================

void KeysPanel::mouseDown (const juce::MouseEvent& e)
{
    // Don't steal clicks meant for the zone viewport
    if (zoneViewport.isVisible() &&
        zoneViewport.getBounds().contains (e.getPosition()))
        return;

    for (auto it = keyRects.rbegin(); it != keyRects.rend(); ++it)
    {
        if (it->isBlack && it->bounds.contains (e.getPosition()))
        {
            lastActiveNote = it->note;
            processor.sfzUiNoteOnRequest.store (lastActiveNote, std::memory_order_relaxed);
            repaint();
            return;
        }
    }
    for (const auto& kr : keyRects)
    {
        if (! kr.isBlack && kr.bounds.contains (e.getPosition()))
        {
            lastActiveNote = kr.note;
            processor.sfzUiNoteOnRequest.store (lastActiveNote, std::memory_order_relaxed);
            repaint();
            return;
        }
    }
}

void KeysPanel::mouseUp (const juce::MouseEvent&)
{
    if (lastActiveNote >= 0)
    {
        processor.sfzUiNoteOffRequest.store (lastActiveNote, std::memory_order_relaxed);
        lastActiveNote = -1;
        repaint();
    }
}

void KeysPanel::mouseMove (const juce::MouseEvent& e)
{
    int found = -1;
    for (auto it = keyRects.rbegin(); it != keyRects.rend(); ++it)
        if (it->bounds.contains (e.getPosition())) { found = it->note; break; }
    if (found != hoveredNote) { hoveredNote = found; repaint(); }
}

void KeysPanel::mouseExit (const juce::MouseEvent&)
{
    if (hoveredNote != -1) { hoveredNote = -1; repaint(); }
}

// =============================================================================
// timerCallback
// =============================================================================

void KeysPanel::timerCallback()
{
    const uint64_t lo = processor.sfzActiveNotes[0].load (std::memory_order_relaxed);
    const uint64_t hi = processor.sfzActiveNotes[1].load (std::memory_order_relaxed);
    if (lo != sfzActiveSnap[0] || hi != sfzActiveSnap[1])
    {
        sfzActiveSnap[0] = lo;
        sfzActiveSnap[1] = hi;
    }
    repaint();
}
