#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// 2 octaves: semitone offsets for each white key
const int KeysPanel::whiteOffsets[14] = {
    0,2,4,5,7,9,11,      // octave 0
    12,14,16,17,19,21,23  // octave 1
};

const KeysPanel::BlackDef KeysPanel::blackDefs[10] = {
    {0,1},{1,3},{3,6},{4,8},{5,10},       // octave 0
    {7,13},{8,15},{10,18},{11,20},{12,22}  // octave 1
};

// =============================================================================
//  ZoneMatrixContent
// =============================================================================

void KeysPanel::ZoneMatrixContent::rebuild (const std::vector<Keyzone>& zones,
                                             int kbX, int kbW,
                                             int baseOctave,
                                             int whiteKeyW, int blackKeyW)
{
    kbX_        = kbX;
    kbW_        = kbW;
    baseOctave_ = baseOctave;

    rows.clear();
    rows.reserve (zones.size());

    for (const auto& z : zones)
    {
        Row r;
        r.zone = z;

        // Compute pixel X range on the keyboard axis.
        // We delegate to the owner's noteToX / noteKeyWidth helpers.
        const float x1 = owner.noteToX (z.loKey, kbX);
        const float x2 = owner.noteToX (z.hiKey, kbX) + owner.noteKeyWidth (z.hiKey);
        r.barX = x1;
        r.barW = juce::jmax (2.0f, x2 - x1);
        rows.push_back (r);
    }

    // Total height = one row per zone (minimum fills the viewport)
    const int totalH = juce::jmax (1, (int) rows.size()) * kRowH;
    setSize (kbW, totalH);

    // If selected row went out of range, reset it
    if (selectedRow >= (int) rows.size())
        selectedRow = -1;

    repaint();
}

void KeysPanel::ZoneMatrixContent::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const int   w     = getWidth();
    const int   h     = getHeight();

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

    // ── Alternating row backgrounds ───────────────────────────────────────────
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const int rowY = i * kRowH;
        const bool isSelected = (i == selectedRow);

        if (isSelected)
            g.setColour (theme.accent.withAlpha (0.18f));
        else if (i % 2 == 0)
            g.setColour (theme.darkBar.darker (0.4f));
        else
            g.setColour (theme.darkBar.darker (0.55f));

        g.fillRect (0, rowY, w, kRowH);
    }

    // ── Subtle vertical octave grid lines ─────────────────────────────────────
    const int loNote = baseOctave_ * 12;
    const int hiNote = loNote + 24;
    g.setColour (theme.accent.withAlpha (0.07f));
    for (int note = loNote; note <= hiNote; note += 12)
    {
        const float x = owner.noteToX (note, kbX_) - kbX_;
        g.fillRect (x, 0.0f, 1.0f, (float) h);
    }

    // ── Zone bars ─────────────────────────────────────────────────────────────
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& r    = rows[(size_t) i];
        const float rowY = (float)(i * kRowH);
        const float pad  = 2.0f;
        const float barY = rowY + pad;
        const float barH = (float) kRowH - pad * 2.0f;

        // Bar X is relative to keyboard, but content is already positioned at kbX=0
        // because the Viewport is placed at kbX in KeysPanel.
        const float bx = r.barX - (float) kbX_;
        const float bw = r.barW;

        if (bw < 1.0f) continue;

        // Fill
        juce::ColourGradient grad (
            r.zone.colour.brighter (0.15f), bx, barY,
            r.zone.colour.darker   (0.25f), bx, barY + barH, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bx, barY, bw, barH, 2.0f);

        // Gloss strip
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (bx + 1.0f, barY + 0.5f,
                                bw - 2.0f, juce::jmin (2.5f, barH * 0.3f), 1.0f);

        // Border
        const bool sel = (i == selectedRow);
        g.setColour (sel ? theme.accent.brighter (0.5f) : r.zone.colour.brighter (0.4f).withAlpha (0.7f));
        g.drawRoundedRectangle (bx + 0.5f, barY + 0.5f, bw - 1.0f, barH - 1.0f, 2.0f, sel ? 1.2f : 0.8f);

        // Label — left-aligned inside the bar, clipped to bar width
        if (bw >= 16.0f && barH >= 7.0f)
        {
            const juce::String label = r.zone.name.isNotEmpty()
                ? r.zone.name
                : (juce::MidiMessage::getMidiNoteName (r.zone.loKey, true, true, 3)
                   + (r.zone.loKey != r.zone.hiKey
                        ? "-" + juce::MidiMessage::getMidiNoteName (r.zone.hiKey, true, true, 3)
                        : juce::String()));

            g.setFont (DysektLookAndFeel::makeFont (juce::jmin (8.5f, barH - 1.0f), true));
            g.setColour (r.zone.colour.brighter (1.0f).withAlpha (0.95f));

            // Clip to the bar
            g.saveState();
            g.reduceClipRegion (juce::Rectangle<float> (bx + 3.0f, barY, bw - 6.0f, barH).toNearestInt());
            g.drawText (label,
                        juce::Rectangle<float> (bx + 3.0f, barY, bw - 6.0f, barH),
                        juce::Justification::centredLeft, true);
            g.restoreState();
        }
    }

    // ── Row separator lines ───────────────────────────────────────────────────
    g.setColour (theme.darkBar.darker (0.2f).withAlpha (0.5f));
    for (int i = 1; i < (int) rows.size(); ++i)
        g.fillRect (0, i * kRowH, w, 1);
}

void KeysPanel::ZoneMatrixContent::mouseDown (const juce::MouseEvent& e)
{
    const int clickedRow = e.y / kRowH;
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
//  KeysPanel
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

    // Set up the zone viewport
    zoneViewport.setScrollBarsShown (true, false);   // vertical scroll only
    zoneViewport.setViewedComponent (&zoneMatrix, false);
    zoneViewport.setScrollBarThickness (6);
    addAndMakeVisible (zoneViewport);

    startTimerHz (30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

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

    zoneMatrix.rebuild (keyzones, kbX, kbW, baseOctave, kWhiteKeyW, kBlackKeyW);
}

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

    // ── Position the zone viewport ────────────────────────────────────────────
    if (kZoneViewH > 0)
    {
        zoneViewport.setBounds (keyboardX, kTransposeRowH, kbW, kZoneViewH);
        zoneViewport.setVisible (true);
        rebuildZoneMatrix();
    }
    else
    {
        zoneViewport.setVisible (false);
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

        g.setColour (juce::Colour (0xFF707070).withAlpha (0.40f));
        g.fillRect (b.getRight() - 1.0f, b.getY() + 4.0f, 1.0f, b.getHeight() - 8.0f);

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

float KeysPanel::noteToX (int note, int kbX) const
{
    static const int semitoneToWhiteOrBlack[12] =
    {
        /*C */ 0,
        /*C#*/ -(0+1),
        /*D */ 1,
        /*D#*/ -(1+1),
        /*E */ 2,
        /*F */ 3,
        /*F#*/ -(3+1),
        /*G */ 4,
        /*G#*/ -(4+1),
        /*A */ 5,
        /*A#*/ -(5+1),
        /*B */ 6,
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
