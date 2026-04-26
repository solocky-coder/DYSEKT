#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// =============================================================================
// Full 128-note keyboard helpers
// =============================================================================
// MIDI note 0 = C-1.  For each semitone 0..11 in an octave:
//   white key index within octave (C=0,D=1,E=2,F=3,G=4,A=5,B=6), or -1 if black.
static const int semiToWhite[12] = { 0,-1,1,-1,2, 3,-1,4,-1,5,-1,6 };
// Number of white keys per octave = 7.
// Total white keys for MIDI 0-127:
//   MIDI 0 = C, MIDI 127 = G9.  Octaves 0-9 give 70 white keys (C..B × 10),
//   plus 3 more (G in octave 10 = G9 = MIDI 127 is actually in oct 10 but
//   MIDI 127 = G9, semi=7(G), octave 10 → 10*7=70, +4 = 74+1=75-1=74 whites? 
//   Let's count: 75 white keys total (A0 piano starts at C-1 in MIDI).

// Count total white keys from note 0 to 127
static int totalWhiteKeys()
{
    int count = 0;
    for (int n = 0; n < 128; ++n)
        if (semiToWhite[n % 12] >= 0) ++count;
    return count;   // = 75
}

// Return the white-key index (0-based from note 0) for any MIDI note.
// Returns -1 for black keys.
static int whiteIndexForNote (int note)
{
    if (semiToWhite[note % 12] < 0) return -1;
    int count = 0;
    for (int n = 0; n < note; ++n)
        if (semiToWhite[n % 12] >= 0) ++count;
    return count;
}

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
// ZoneMatrixContent::paint   — Mixer-channel row style
//
// Column layout (left → right):
//   [4px colour bar] [Name col, colour-tinted bg] | [Key range] [Root] [Vel badge] [Lp]
// =============================================================================

void KeysPanel::ZoneMatrixContent::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const int   w     = getWidth();
    const int   h     = getHeight();

    // ── Column geometry (mirrors MixerPanel name / data split) ────────────────
    constexpr int kStripeW  = 4;     // left colour bar  (mixer uses 3–4 px)
    constexpr int kNameColW = 94;    // colour-tinted name column
    constexpr int kNameX    = kStripeW + 2;
    constexpr int kNameW    = kNameColW - kStripeW - 4;
    // Right of name column: KEY | ROOT | VEL | LP
    constexpr int kKeyX     = kNameColW + 4;
    constexpr int kKeyW     = 66;
    constexpr int kRootX    = kKeyX + kKeyW + 2;
    constexpr int kRootW    = 34;
    constexpr int kVelX     = kRootX + kRootW + 2;
    constexpr int kVelW     = 54;
    constexpr int kLoopX    = kVelX + kVelW + 4;

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
        g.setColour (theme.darkBar.darker (0.25f));
        g.fillRect (0, 0, w, kHeaderH);

        // Subtle tint on name column header to match rows below
        g.setColour (juce::Colours::white.withAlpha (0.02f));
        g.fillRect (0, 0, kNameColW, kHeaderH);

        g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
        g.setColour (theme.foreground.withAlpha (0.30f));

        auto hdr = [&](const char* txt, int x, int cw,
                       juce::Justification j = juce::Justification::centredLeft)
        {
            g.drawText (txt, x, 0, cw, kHeaderH, j, false);
        };

        hdr ("NAME",  kNameX,       kNameW,  juce::Justification::centredLeft);
        hdr ("KEY",   kKeyX,        kKeyW,   juce::Justification::centredLeft);
        hdr ("ROOT",  kRootX,       kRootW,  juce::Justification::centred);
        hdr ("VEL",   kVelX,        kVelW,   juce::Justification::centred);
        if (w > kLoopX + 8)
            hdr ("LP", kLoopX, w - kLoopX,   juce::Justification::centred);

        g.setColour (theme.separator.withAlpha (0.45f));
        g.drawHorizontalLine (kHeaderH - 1, 0.f, (float) w);

        // Vertical divider between name col and data cols in header
        g.setColour (theme.separator.withAlpha (0.20f));
        g.drawVerticalLine (kNameColW - 1, 0.f, (float) kHeaderH);
    }

    // ── Rows ──────────────────────────────────────────────────────────────────
    const juce::Font fMain  = DysektLookAndFeel::makeFont (10.5f);
    const juce::Font fSmall = DysektLookAndFeel::makeFont (9.0f);
    const juce::Font fTiny  = DysektLookAndFeel::makeFont (8.0f);

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& r   = rows[(size_t) i];
        const int   ry  = kHeaderH + i * kRowH;
        const bool  sel = (i == selectedRow);
        const juce::Colour zc = r.zone.colour;

        // ── Row base: alternating dark tint ───────────────────────────────────
        if (i %

void KeysPanel::ZoneMatrixContent::highlightNote (int note)
{
    if (note < 0)
        return;   // don't clear selection on note-off — keep last highlighted

    // Find first row whose key range covers this note.
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& z = rows[(size_t) i].zone;
        if (note >= z.loKey && note <= z.hiKey)
        {
            if (selectedRow == i) return;   // already there — skip scroll + repaint
            selectedRow = i;

            // Ask the parent Viewport to reveal this row.
            // ZoneMatrixContent is the viewedComponent; the viewport is its parent.
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            {
                const int rowY = kHeaderH + i * kRowH;
                vp->setViewPosition (0, juce::jmax (0, rowY - vp->getHeight() / 2));
            }
            repaint();
            return;
        }
    }
}

// =============================================================================
// KeysPanel::highlightNoteInMatrix
// =============================================================================

void KeysPanel::highlightNoteInMatrix (int note)
{
    zoneMatrix.highlightNote (note);
}



void KeysPanel::ZoneMatrixContent::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < kHeaderH) return;

    const int clickedRow = (e.y - kHeaderH) / kRowH;
    if (clickedRow >= 0 && clickedRow < (int) rows.size())
    {
        selectedRow = clickedRow;
        repaint();

        // Audition: send note-on then immediately note-off after a short delay.
        // We do NOT set owner.lastActiveNote here — that field is only for
        // physical piano-key presses so that mouseUp can release correctly.
        const int note = rows[(size_t) clickedRow].zone.loKey;
        owner.processor.sfzUiNoteOnRequest.store (note, std::memory_order_relaxed);

        // Schedule an automatic release so the note never sticks.
        // Use a simple one-shot via the owner's timer mechanism:
        owner.scheduleNoteOff (note);
    }
}

// =============================================================================
// KeysPanel
// =============================================================================

KeysPanel::KeysPanel (DysektProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);

    // Transpose buttons kept invisible — octave-shift logic intact for future use.
    addAndMakeVisible (transposeDownBtn);
    addAndMakeVisible (transposeUpBtn);
    transposeDownBtn.setMouseCursor (juce::MouseCursor::NormalCursor);
    transposeUpBtn  .setMouseCursor (juce::MouseCursor::NormalCursor);
    transposeDownBtn.setVisible (false);
    transposeUpBtn  .setVisible (false);
    transposeDownBtn.onClick = [this] { if (baseOctave > 0) { --baseOctave; resized(); repaint(); } };
    transposeUpBtn  .onClick = [this] { if (baseOctave < 8) { ++baseOctave; resized(); repaint(); } };

    zoneViewport.setScrollBarsShown (true, false);
    zoneViewport.setViewedComponent (&zoneMatrix, false);
    zoneViewport.setScrollBarThickness (0);
    addAndMakeVisible (zoneViewport);

    startTimerHz (30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

// =============================================================================

void KeysPanel::scheduleNoteOff (int note)
{
    // Store the pending note-off note; the timer will send it on the next tick.
    pendingNoteOff = note;
}

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
    // With a full keyboard there is no scrolling — nothing to do here.
    repaint();
}

void KeysPanel::rebuildZoneMatrix()
{
    const int vpW = getWidth();
    zoneMatrix.rebuild (keyzones, 0, vpW, 0, kWhiteKeyW, kBlackKeyW, vpW);
}

// =============================================================================
// resized  — full 128-note keyboard
// =============================================================================

void KeysPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    kTransposeRowH = 0;

    // Key height: compact, matching the image reference (thinner keys → shorter).
    // Allow up to 50px tall, minimum 32px.
    kKeyH = juce::jlimit (32, 50, h / 3);

    kZoneViewH = juce::jmax (0, h - kKeyH);

    // Full keyboard: 75 white keys spanning the full component width.
    kNumWhite = kTotalWhite;   // 75
    kNumBlack = kTotalBlack;   // 53

    // Key width: fill the full component width with all 75 white keys.
    // Use float division so we can use fractional positions and avoid gaps.
    kWhiteKeyW = juce::jmax (4, w / kNumWhite);     // integer for hit-testing
    kWhiteKeyWf = (float) w / (float) kNumWhite;    // float for drawing
    kBlackKeyW  = juce::roundToInt (kWhiteKeyWf * 0.56f);
    kBlackKeyH  = juce::roundToInt ((float) kKeyH  * 0.60f);

    const int keyboardY = h - kKeyH;

    // ── Build keyRects for all 128 notes ──────────────────────────────────────
    keyRects.clear();
    keyRects.reserve (128);

    // White keys pass
    int wi = 0;   // running white-key index
    for (int note = 0; note < 128; ++note)
    {
        const int semi = note % 12;
        if (semiToWhite[semi] >= 0)
        {
            const int x = juce::roundToInt ((float) wi * kWhiteKeyWf);
            const int x2 = juce::roundToInt ((float)(wi + 1) * kWhiteKeyWf);
            KeyRect kr;
            kr.bounds  = { x, keyboardY, x2 - x - 1, kKeyH };
            kr.note    = note;
            kr.isBlack = false;
            keyRects.push_back (kr);
            ++wi;
        }
    }

    // Black keys pass — positioned at the right edge of the white key before them
    for (int note = 0; note < 128; ++note)
    {
        const int semi = note % 12;
        if (semiToWhite[semi] >= 0) continue;  // skip white keys

        // Find the white-key index of the white key just below this black key.
        // The white key below a black key at semitone s is the white key with
        // the largest note < note that is a white key.
        int prevWhite = -1;
        for (int n = note - 1; n >= 0; --n)
        {
            if (semiToWhite[n % 12] >= 0) { prevWhite = n; break; }
        }
        if (prevWhite < 0) continue;

        // The white-key slot index of prevWhite
        int pwi = whiteIndexForNote (prevWhite);
        if (pwi < 0) continue;

        // Black key sits centred on the right edge of prevWhite's slot
        const float rightEdge = (float)(pwi + 1) * kWhiteKeyWf;
        const int bx = juce::roundToInt (rightEdge - kBlackKeyW * 0.5f);

        KeyRect kr;
        kr.bounds  = { bx, keyboardY, kBlackKeyW, kBlackKeyH };
        kr.note    = note;
        kr.isBlack = true;
        keyRects.push_back (kr);
    }

    // ── Zone viewport ─────────────────────────────────────────────────────────
    if (kZoneViewH > 0)
    {
        zoneViewport.setBounds (0, 0, w, kZoneViewH);
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

    g.setColour (theme.darkBar.darker (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    // ── LCD-style frame around zone viewport ─────────────────────────────────
    if (kZoneViewH > 0)
    {
        const juce::Rectangle<float> frameRect (0.f, 0.f, (float) getWidth(), (float) kZoneViewH);

        juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0.f, 0.f,
                                        juce::Colour (0xFF0E0E0E), 0.f,
                                        (float) kZoneViewH, false);
        g.setGradientFill (outerGrad);
        g.fillRoundedRectangle (frameRect, 2.0f);

        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (frameRect.reduced (0.5f), 2.0f, 1.0f);

        const auto screen = frameRect.reduced (2.f);
        g.setColour (theme.darkBar.darker (0.55f));
        g.fillRoundedRectangle (screen, 1.5f);

        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (int y = (int) screen.getY(); y < (int) screen.getBottom(); y += 2)
            g.drawHorizontalLine (y, screen.getX(), screen.getRight());

        juce::ColourGradient glow (accent.withAlpha (0.05f), 0.f, screen.getY(),
                                   juce::Colours::transparentBlack, 0.f,
                                   screen.getY() + 16.f, false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (screen, 1.5f);

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

    // ── Root-pitch dots ───────────────────────────────────────────────────────
    for (const auto& z : keyzones)
    {
        if (z.rootPitch < 0) continue;
        const int n = z.rootPitch;
        if (n < 0 || n >= 128) continue;

        // Find the keyRect for this note
        for (const auto& kr : keyRects)
        {
            if (kr.note != n) continue;
            const auto  b   = kr.bounds.toFloat();
            const float dotX = b.getCentreX() - 2.5f;
            const float dotY = kr.isBlack ? b.getBottom() - 8.f : b.getBottom() - 9.f;
            g.setColour (z.colour.brighter (0.6f).withAlpha (0.90f));
            g.fillEllipse (dotX, dotY, 5.0f, 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.40f));
            g.drawEllipse (dotX, dotY, 5.0f, 5.0f, 0.8f);
            break;
        }
    }
}

// =============================================================================
// drawKey
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
        g.fillRoundedRectangle (b.reduced (0.5f, 0.f).withTrimmedBottom (-4.f), 2.0f);

        g.setColour (juce::Colour (0xFF707070).withAlpha (0.40f));
        g.fillRect  (b.getRight() - 1.0f, b.getY() + 2.0f, 1.0f, b.getHeight() - 4.0f);

        // Only draw note labels on C notes if key wide enough
        if ((kr.note % 12) == 0 && b.getWidth() >= 6.f)
        {
            g.setFont   (DysektLookAndFeel::makeFont (5.5f));
            g.setColour (hasSlice ? accent.withAlpha (0.9f) : juce::Colour (0xFF999999));
            g.drawText  (juce::MidiMessage::getMidiNoteName (kr.note, true, true, 3),
                         kr.bounds.getX(), kr.bounds.getBottom() - 10,
                         kr.bounds.getWidth(), 9, juce::Justification::centred);
        }

        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.80f));
            g.drawRoundedRectangle (b.reduced (0.8f, 0.5f), 2.0f, 1.0f);
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
        g.fillRoundedRectangle (b, 1.5f);

        g.setColour (juce::Colours::white.withAlpha (active ? 0.22f : 0.09f));
        g.fillRoundedRectangle (b.getX() + 1.0f, b.getY() + 1.0f,
                                b.getWidth() - 2.0f, 3.5f, 1.0f);

        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.85f));
            g.drawRoundedRectangle (b.reduced (0.6f), 1.5f, 0.8f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.60f));
        g.drawRoundedRectangle (b.expanded (0.4f), 2.0f, 0.7f);
    }
}

// =============================================================================
// Hit-testing helpers
// =============================================================================

float KeysPanel::noteToX (int note, int /*kbX*/) const
{
    for (const auto& kr : keyRects)
        if (kr.note == note)
            return (float) kr.bounds.getX();
    return 0.f;
}

float KeysPanel::noteKeyWidth (int note) const
{
    for (const auto& kr : keyRects)
        if (kr.note == note)
            return (float) kr.bounds.getWidth();
    return (float) kWhiteKeyW;
}

juce::Colour KeysPanel::zoneColourForNote (int note) const
{
    for (const auto& z : keyzones)
        if (note >= z.loKey && note <= z.hiKey)
            return z.colour;
    return juce::Colour (0);
}

// =============================================================================
// Mouse  — sticky-note fix
//
// Root cause of sticking:
//   1. ZoneMatrixContent::mouseDown set owner.lastActiveNote without an
//      associated mouseUp → note-off was never sent.  Fixed above by using
//      scheduleNoteOff() instead.
//   2. If the mouse is dragged off the component while a key is held, mouseUp
//      is never called on KeysPanel.  Fixed by also releasing in mouseExit.
//   3. mouseDrag was not implemented, so holding and moving the mouse after a
//      click would leave the old note active.
// =============================================================================

void KeysPanel::mouseDown (const juce::MouseEvent& e)
{
    // Zone viewport swallows its own clicks — skip piano hit testing.
    if (zoneViewport.isVisible() &&
        zoneViewport.getBounds().contains (e.getPosition()))
        return;

    // Release any previously held note before pressing a new one.
    releaseLastNote();

    // Black keys first (they sit on top)
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

void KeysPanel::mouseDrag (const juce::MouseEvent& e)
{
    // Slide to a new key while holding the mouse button.
    if (zoneViewport.isVisible() &&
        zoneViewport.getBounds().contains (e.getPosition()))
        return;

    int found = -1;
    // Black keys first
    for (auto it = keyRects.rbegin(); it != keyRects.rend(); ++it)
        if (it->isBlack && it->bounds.contains (e.getPosition())) { found = it->note; break; }

    if (found < 0)
        for (const auto& kr : keyRects)
            if (! kr.isBlack && kr.bounds.contains (e.getPosition())) { found = kr.note; break; }

    if (found >= 0 && found != lastActiveNote)
    {
        releaseLastNote();
        lastActiveNote = found;
        processor.sfzUiNoteOnRequest.store (lastActiveNote, std::memory_order_relaxed);
        repaint();
    }
}

void KeysPanel::mouseUp (const juce::MouseEvent&)
{
    releaseLastNote();
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
    // Release any held note if the cursor leaves the component.
    releaseLastNote();
    if (hoveredNote != -1) { hoveredNote = -1; repaint(); }
}

void KeysPanel::releaseLastNote()
{
    if (lastActiveNote >= 0)
    {
        processor.sfzUiNoteOffRequest.store (lastActiveNote, std::memory_order_relaxed);
        lastActiveNote = -1;
        repaint();
    }
}

// =============================================================================
// timerCallback
// =============================================================================

void KeysPanel::timerCallback()
{
    // Send any pending scheduled note-off (from zone-table clicks)
    if (pendingNoteOff >= 0)
    {
        processor.sfzUiNoteOffRequest.store (pendingNoteOff, std::memory_order_relaxed);
        pendingNoteOff = -1;
    }

    const uint64_t lo = processor.sfzActiveNotes[0].load (std::memory_order_relaxed);
    const uint64_t hi = processor.sfzActiveNotes[1].load (std::memory_order_relaxed);
    if (lo != sfzActiveSnap[0] || hi != sfzActiveSnap[1])
    {
        sfzActiveSnap[0] = lo;
        sfzActiveSnap[1] = hi;

        // Find the lowest active note and scroll the zone matrix to it.
        for (int n = 0; n < 128; ++n)
        {
            const uint64_t word = (n < 64) ? lo : hi;
            const int      bit  = (n < 64) ? n  : (n - 64);
            if ((word >> bit) & 1)
            {
                highlightNoteInMatrix (n);
                break;
            }
        }
    }

    // Also track UI mouse-pressed key
    if (lastActiveNote >= 0)
        highlightNoteInMatrix (lastActiveNote);

    repaint();
}


