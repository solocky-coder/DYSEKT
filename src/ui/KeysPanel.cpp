#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// 2 octaves: semitone offsets for each white key
const int KeysPanel::whiteOffsets[14] = {
    0,2,4,5,7,9,11,      // octave 0
    12,14,16,17,19,21,23  // octave 1
};

// afterWhite = index of white key this black key sits to the right of
// offset     = semitone offset from base note
const KeysPanel::BlackDef KeysPanel::blackDefs[10] = {
    {0,1},{1,3},{3,6},{4,8},{5,10},       // octave 0
    {7,13},{8,15},{10,18},{11,20},{12,22}  // octave 1
};

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

    startTimerHz (30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

void KeysPanel::setKeyzones (std::vector<Keyzone> zones) { keyzones = std::move (zones); repaint(); }
void KeysPanel::clearKeyzones() { keyzones.clear(); repaint(); }

void KeysPanel::autoScrollToZones()
{
    if (keyzones.empty()) return;
    int lo = 127, hi = 0;
    for (auto& z : keyzones) { lo = std::min (lo, z.loKey); hi = std::max (hi, z.hiKey); }
    baseOctave = juce::jlimit (0, 8, (lo + hi) / 2 / 12 - 1);
    resized(); repaint();
}

void KeysPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    kTransposeRowH = juce::jmin (22, h / 6);

    // Keyboard height: fixed minimum, capped so zone view gets the rest
    const int kKeyHMin = juce::jmin (60, h / 3);
    kKeyH = juce::jmax (kKeyHMin, juce::jmin (80, h / 3));

    // Zone view takes everything between transpose row and keyboard
    kZoneViewH = juce::jmax (0, h - kTransposeRowH - kKeyH);

    // Fill the full panel width
    kWhiteKeyW = juce::jmax (6, w / kNumWhite);
    kBlackKeyW = juce::roundToInt (kWhiteKeyW * 0.52f);
    kBlackKeyH = juce::roundToInt ((float) kKeyH * 0.62f);

    const int kbW       = kWhiteKeyW * kNumWhite;
    const int keyboardX = (w - kbW) / 2;
    const int keyboardY = h - kKeyH;   // keyboard always at bottom

    const int cx = w / 2;
    transposeDownBtn.setBounds (cx - 72, 2, 20, kTransposeRowH - 4);
    transposeUpBtn  .setBounds (cx + 52, 2, 20, kTransposeRowH - 4);

    keyRects.clear();
    for (int i = 0; i < kNumWhite; ++i)
    {
        KeyRect kr;
        kr.bounds  = { keyboardX + i * kWhiteKeyW, keyboardY, kWhiteKeyW - 1, kKeyH };
        kr.note    = baseOctave * 12 + whiteOffsets[i];
        kr.isBlack = false;
        keyRects.push_back (kr);
    }
    for (int i = 0; i < kNumBlack; ++i)
    {
        KeyRect kr;
        int bx     = keyboardX + (blackDefs[i].afterWhite + 1) * kWhiteKeyW - kBlackKeyW / 2;
        kr.bounds  = { bx, keyboardY, kBlackKeyW, kBlackKeyH };
        kr.note    = baseOctave * 12 + blackDefs[i].offset;
        kr.isBlack = true;
        keyRects.push_back (kr);
    }
}

void KeysPanel::paint (juce::Graphics& g)
{
    const auto& theme  = getTheme();
    const auto  accent = theme.accent;

    g.setColour (theme.darkBar.darker (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    for (auto* btn : { &transposeDownBtn, &transposeUpBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,   theme.darkBar.brighter (0.08f));
        btn->setColour (juce::TextButton::textColourOffId,  accent.withAlpha (0.80f));
        btn->setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.2f));
    }

    {
        const juce::String label = "C" + juce::String (baseOctave)
                                 + "  \xe2\x80\x93  C" + juce::String (baseOctave + 2);
        g.setFont (DysektLookAndFeel::makeFont (9.0f, true));
        g.setColour (accent.withAlpha (0.50f));
        g.drawText (label, 0, 2, getWidth(), kTransposeRowH - 2, juce::Justification::centred);
    }

    const auto& ui = processor.getUiSliceSnapshot();
    std::set<int> slicedNotes;
    for (int s = 0; s < ui.numSlices; ++s)
        slicedNotes.insert (ui.slices[(size_t) s].midiNote);

    const int kbW       = kWhiteKeyW * kNumWhite;
    const int keyboardX = keyRects.empty() ? 0 : keyRects.front().bounds.getX();

    // ── HALion-style zone view (above keyboard) ───────────────────────────────
    if (kZoneViewH > 0)
        drawZoneView (g, keyboardX, kTransposeRowH, kZoneViewH, kbW);

    for (const auto& kr : keyRects)
        if (! kr.isBlack)
        {
            const int n = kr.note;
            const bool midiActive = (n >= 0 && n < 128)
                ? ((n < 64 ? (sfzActiveSnap[0] >> n) : (sfzActiveSnap[1] >> (n - 64))) & 1) != 0
                : false;
            drawKey (g, kr, slicedNotes.count (n) > 0, n == hoveredNote, n == lastActiveNote || midiActive);
        }

    for (const auto& kr : keyRects)
        if (kr.isBlack)
        {
            const int n = kr.note;
            const bool midiActive = (n >= 0 && n < 128)
                ? ((n < 64 ? (sfzActiveSnap[0] >> n) : (sfzActiveSnap[1] >> (n - 64))) & 1) != 0
                : false;
            drawKey (g, kr, slicedNotes.count (n) > 0, n == hoveredNote, n == lastActiveNote || midiActive);
        }
}

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

        // Right-edge gap shadow
        g.setColour (juce::Colour (0xFF707070).withAlpha (0.40f));
        g.fillRect (b.getRight() - 1.0f, b.getY() + 4.0f, 1.0f, b.getHeight() - 8.0f);

        // Bottom cap highlight
        g.setColour (juce::Colours::white.withAlpha (active ? 0.55f : 0.18f));
        g.fillRoundedRectangle (b.getX() + 2.0f, b.getBottom() - 10.0f, b.getWidth() - 4.0f, 7.0f, 2.0f);

        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.80f));
            g.drawRoundedRectangle (b.reduced (1.0f, 0.5f), 3.0f, 1.2f);
        }

        if ((kr.note % 12) == 0)
        {
            g.setFont (DysektLookAndFeel::makeFont (6.5f));
            g.setColour (hasSlice ? accent.withAlpha (0.9f) : juce::Colour (0xFF999999));
            g.drawText (juce::MidiMessage::getMidiNoteName (kr.note, true, true, 3),
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

        // Gloss strip
        g.setColour (juce::Colours::white.withAlpha (active ? 0.22f : 0.09f));
        g.fillRoundedRectangle (b.getX() + 2.0f, b.getY() + 1.5f, b.getWidth() - 4.0f, 5.0f, 1.5f);

        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.85f));
            g.drawRoundedRectangle (b.reduced (0.8f), 2.5f, 1.0f);
        }

        // Outer shadow
        g.setColour (juce::Colours::black.withAlpha (0.60f));
        g.drawRoundedRectangle (b.expanded (0.5f), 3.0f, 0.8f);
    }
}

// =============================================================================
//  noteToX — maps a MIDI note to the LEFT EDGE pixel of its key on screen.
//  Black keys have their own visual position (centered between white keys);
//  they no longer collapse to the preceding white key's X.
// =============================================================================

float KeysPanel::noteToX (int note, int kbX) const
{
    // Per-semitone table: for white keys, number of white keys to the left within
    // the octave.  For black keys, encoded as -(afterWhite+1) so we can detect them.
    // afterWhite = the white key they sit to the right of (0-based within octave):
    //   C#=0, D#=1, F#=3, G#=4, A#=5
    static const int semitoneToWhiteOrBlack[12] =
    {
        /*C */ 0,
        /*C#*/ -(0+1),   // black, afterWhite=0
        /*D */ 1,
        /*D#*/ -(1+1),   // black, afterWhite=1
        /*E */ 2,
        /*F */ 3,
        /*F#*/ -(3+1),   // black, afterWhite=3
        /*G */ 4,
        /*G#*/ -(4+1),   // black, afterWhite=4
        /*A */ 5,
        /*A#*/ -(5+1),   // black, afterWhite=5
        /*B */ 6,
    };

    const int rel    = note - baseOctave * 12;
    const int octave = rel / 12;
    const int semi   = juce::jlimit (0, 11, ((rel % 12) + 12) % 12);
    const int enc    = semitoneToWhiteOrBlack[semi];

    const float octaveX = (float) kbX + (float)(octave * 7) * (float) kWhiteKeyW;

    if (enc >= 0)
    {
        // White key — left edge
        return octaveX + (float)(enc) * (float) kWhiteKeyW;
    }
    else
    {
        // Black key — centered between its two flanking white keys
        const int afterWhite = (-enc) - 1;   // recover afterWhite
        return octaveX + (float)(afterWhite + 1) * (float) kWhiteKeyW
               - (float) kBlackKeyW * 0.5f;
    }
}

// =============================================================================
//  noteKeyWidth — visual pixel width of the key for a given MIDI note.
// =============================================================================

float KeysPanel::noteKeyWidth (int note) const
{
    static const bool isBlack[12] =
        { false, true, false, true, false, false, true, false, true, false, true, false };
    const int semi = ((note % 12) + 12) % 12;
    return isBlack[semi] ? (float) kBlackKeyW : (float) kWhiteKeyW;
}

// =============================================================================
//  drawZoneView — HALion-style horizontal zone rectangles
// =============================================================================

void KeysPanel::drawZoneView (juce::Graphics& g,
                               int kbX, int zoneY, int zoneH, int kbW) const
{
    const auto& theme = getTheme();

    // ── Background ────────────────────────────────────────────────────────────
    g.setColour (theme.darkBar.darker (0.55f));
    g.fillRect (kbX, zoneY, kbW, zoneH);

    if (keyzones.empty())
    {
        g.setFont (DysektLookAndFeel::makeFont (9.0f));
        g.setColour (theme.foreground.withAlpha (0.18f));
        g.drawText ("No zones loaded", kbX, zoneY, kbW, zoneH,
                    juce::Justification::centred, false);
        g.setColour (theme.accent.withAlpha (0.15f));
        g.fillRect (kbX, zoneY + zoneH - 1, kbW, 1);
        return;
    }

    const int loNote = baseOctave * 12;
    const int hiNote = loNote + 24;

    // ── Subtle octave grid lines ──────────────────────────────────────────────
    g.setColour (theme.accent.withAlpha (0.08f));
    for (int note = loNote; note <= hiNote; note += 12)
    {
        const float x = noteToX (note, kbX);
        g.fillRect (x, (float) zoneY, 1.0f, (float) zoneH);
    }

    const float barTopPad = 2.0f;
    const float barH      = (float) zoneH - barTopPad - 2.0f;
    const float cornerR   = 2.5f;

    // Black semitone lookup
    static const bool semitoneIsBlack[12] =
        { false,true,false,true,false,false,true,false,true,false,true,false };

    auto isBlackNote = [] (int n) -> bool
    {
        return semitoneIsBlack[((n % 12) + 12) % 12];
    };

    // ── Draw one zone bar ─────────────────────────────────────────────────────
    auto drawBar = [&] (const Keyzone& z, float barX, float barW, bool blackKey)
    {
        if (barW < 1.0f) return;

        const float barY  = (float) zoneY + barTopPad;
        // Black-key bars are shorter (like real black keys), visually sitting on top.
        const float bH    = blackKey ? barH * 0.72f : barH;
        const float cx    = barX + barW * 0.5f;
        const float cy    = barY + bH  * 0.5f;

        juce::ColourGradient grad (
            z.colour.brighter (0.18f), barX, barY,
            z.colour.darker   (0.30f), barX, barY + bH,
            false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (barX, barY, barW, bH, cornerR);

        g.setColour (z.colour.brighter (0.55f).withAlpha (0.40f));
        g.fillRoundedRectangle (barX + 1.0f, barY + 1.0f,
                                barW - 2.0f, juce::jmin (4.0f, bH * 0.15f), 1.0f);

        g.setColour (z.colour.brighter (0.35f).withAlpha (0.80f));
        g.drawRoundedRectangle (barX + 0.5f, barY + 0.5f,
                                barW - 1.0f, bH - 1.0f, cornerR, 1.0f);

        if (barW >= 6.0f && bH >= 14.0f)
        {
            const juce::String label = z.name.isNotEmpty()
                ? z.name
                : (juce::MidiMessage::getMidiNoteName (z.loKey, true, true, 3)
                   + (z.loKey != z.hiKey
                        ? "-" + juce::MidiMessage::getMidiNoteName (z.hiKey, true, true, 3)
                        : juce::String()));

            const float fontSize = juce::jlimit (6.5f, 9.0f, barW * 0.75f);
            g.setFont (DysektLookAndFeel::makeFont (fontSize, true));
            g.setColour (z.colour.brighter (0.95f).withAlpha (0.95f));

            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (
                -juce::MathConstants<float>::halfPi, cx, cy));
            g.drawText (label,
                        juce::Rectangle<float> (cx - bH * 0.5f, cy - barW * 0.5f, bH, barW),
                        juce::Justification::centred, true);
            g.restoreState();
        }
    };

    // ── Pass 1: white-key zones and multi-note zones ──────────────────────────
    // A zone is "single-black-key" only when lo==hi and that note is a black key.
    // Everything else (white single-note zones, and all multi-note zones) draws here.
    for (const auto& z : keyzones)
    {
        if (z.hiKey < loNote || z.loKey > hiNote) continue;
        const int lo = juce::jmax (loNote, z.loKey);
        const int hi = juce::jmin (hiNote, z.hiKey);
        if (lo > hi) continue;
        if (lo == hi && isBlackNote (lo)) continue;  // deferred to pass 2

        const float x1 = noteToX (lo, kbX);
        const float x2 = noteToX (hi, kbX) + noteKeyWidth (hi);
        drawBar (z, x1, x2 - x1, false);
    }

    // ── Pass 2: single-black-key zones drawn on top (like black piano keys) ───
    for (const auto& z : keyzones)
    {
        if (z.hiKey < loNote || z.loKey > hiNote) continue;
        const int lo = juce::jmax (loNote, z.loKey);
        const int hi = juce::jmin (hiNote, z.hiKey);
        if (lo != hi || ! isBlackNote (lo)) continue;

        drawBar (z, noteToX (lo, kbX), noteKeyWidth (lo), true);
    }

    // ── Bottom separator line ─────────────────────────────────────────────────
    g.setColour (theme.accent.withAlpha (0.20f));
    g.fillRect (kbX, zoneY + zoneH - 1, kbW, 1);
}

juce::Colour KeysPanel::zoneColourForNote (int note) const
{
    for (const auto& z : keyzones)
        if (note >= z.loKey && note <= z.hiKey)
            return z.colour;
    return juce::Colour (0);
}

void KeysPanel::mouseDown (const juce::MouseEvent& e)
{
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

void KeysPanel::timerCallback()
{
    // Snapshot the SF2 active-note bitmask from the audio thread.
    const uint64_t lo = processor.sfzActiveNotes[0].load (std::memory_order_relaxed);
    const uint64_t hi = processor.sfzActiveNotes[1].load (std::memory_order_relaxed);
    if (lo != sfzActiveSnap[0] || hi != sfzActiveSnap[1])
    {
        sfzActiveSnap[0] = lo;
        sfzActiveSnap[1] = hi;
    }
    repaint();
}
