#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

const int KeysPanel::whiteOffsets[14] = { 0,2,4,5,7,9,11, 12,14,16,17,19,21,23 };
const KeysPanel::BlackDef KeysPanel::blackDefs[10] = {
    {0,1},{1,3},{3,6},{4,8},{5,10},
    {7,13},{8,15},{10,18},{11,20},{12,22}
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
    transposeUpBtn  .onClick = [this] { if (baseOctave < 7) { ++baseOctave; resized(); repaint(); } };

    startTimerHz (30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

// ── Keyzone API ───────────────────────────────────────────────────────────────

void KeysPanel::setKeyzones (std::vector<Keyzone> zones)
{
    keyzones = std::move (zones);
    repaint();
}

void KeysPanel::clearKeyzones()
{
    keyzones.clear();
    repaint();
}

void KeysPanel::autoScrollToZones()
{
    if (keyzones.empty()) return;
    // Find the midpoint of all zones and centre the view there
    int lo = 127, hi = 0;
    for (auto& z : keyzones) { lo = std::min (lo, z.loKey); hi = std::max (hi, z.hiKey); }
    int mid = (lo + hi) / 2;
    // Each octave = 7 white keys; view = 2 octaves = 14 white keys centred on mid
    int octave = juce::jlimit (0, 7, mid / 12 - 1);
    baseOctave = octave;
    resized();
    repaint();
}

// ── Layout ────────────────────────────────────────────────────────────────────

void KeysPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Derive key dimensions from available height
    kTransposeRowH = juce::jmin (24, h / 5);
    kZoneBarH      = keyzones.empty() ? 0 : 8;
    kKeyH          = h - kTransposeRowH - kZoneBarH - 2;
    kWhiteKeyW     = juce::jmax (8, w / kNumWhite);
    kBlackKeyW     = juce::roundToInt (kWhiteKeyW * 0.58f);
    kBlackKeyH     = juce::roundToInt (kKeyH * 0.60f);

    const int kbW       = kWhiteKeyW * kNumWhite;
    const int keyboardX = (w - kbW) / 2;
    const int keyboardY = kTransposeRowH + kZoneBarH;

    // Transpose buttons flanking the octave label
    const int cx = w / 2;
    transposeDownBtn.setBounds (cx - 80, 2, 22, kTransposeRowH - 4);
    transposeUpBtn  .setBounds (cx + 58, 2, 22, kTransposeRowH - 4);

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

// ── Paint ─────────────────────────────────────────────────────────────────────

void KeysPanel::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const auto  accent = theme.accent;

    // Background
    g.setColour (theme.darkBar.darker (0.3f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    // Re-theme transpose buttons inline
    for (auto* btn : { &transposeDownBtn, &transposeUpBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  theme.darkBar.brighter (0.1f));
        btn->setColour (juce::TextButton::textColourOffId, accent.withAlpha (0.85f));
        btn->setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.2f));
    }

    // Octave label
    {
        const juce::String label = "C" + juce::String (baseOctave)
                                 + "  \xe2\x80\x93  C" + juce::String (baseOctave + 2);
        g.setFont (DysektLookAndFeel::makeFont (9.5f, true));
        g.setColour (accent.withAlpha (0.55f));
        g.drawText (label, 0, 2, getWidth(), kTransposeRowH - 2, juce::Justification::centred);
    }

    // Active MIDI notes from slice engine
    const auto& ui = processor.getUiSliceSnapshot();
    std::set<int> slicedNotes;
    for (int s = 0; s < ui.numSlices; ++s)
        slicedNotes.insert (ui.slices[(size_t) s].midiNote);

    // Zone bar
    if (kZoneBarH > 0 && ! keyRects.empty())
    {
        const int kbW       = kWhiteKeyW * kNumWhite;
        const int keyboardX = keyRects.front().bounds.getX();
        drawZoneBar (g, keyboardX, kTransposeRowH, kbW);
    }

    // White keys
    for (const auto& kr : keyRects)
    {
        if (kr.isBlack) continue;
        const bool hasSlice = slicedNotes.count (kr.note) > 0;
        const bool hovered  = (kr.note == hoveredNote);
        const bool active   = (kr.note == lastActiveNote);
        drawKey (g, kr, hasSlice, hovered, active);
    }

    // Black keys (drawn on top)
    for (const auto& kr : keyRects)
    {
        if (! kr.isBlack) continue;
        const bool hasSlice = slicedNotes.count (kr.note) > 0;
        const bool hovered  = (kr.note == hoveredNote);
        const bool active   = (kr.note == lastActiveNote);
        drawKey (g, kr, hasSlice, hovered, active);
    }
}

void KeysPanel::drawKey (juce::Graphics& g, const KeyRect& kr,
                          bool hasSlice, bool hovered, bool active) const
{
    const auto& theme  = getTheme();
    const auto  accent = theme.accent;
    const auto  bounds = kr.bounds.toFloat();

    if (! kr.isBlack)
    {
        // ── White key ─────────────────────────────────────────────────────────
        // Body gradient: warm near-white top, cream bottom
        juce::ColourGradient grad (
            active  ? accent.withAlpha (0.55f).withBrightness (0.95f)
            : hovered ? juce::Colour (0xFFF0F0F0)
                      : juce::Colour (0xFFE8E8E8),
            0.f, bounds.getY(),
            active  ? accent.withAlpha (0.30f)
            : hovered ? juce::Colour (0xFFD8D8D8)
                      : juce::Colour (0xFFC8C8C8),
            0.f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds.reduced (0.5f, 0.0f)
                                      .withTrimmedBottom (-3.0f), 4.0f);

        // Side shadow lines (gives depth between adjacent keys)
        g.setColour (juce::Colour (0xFF888888).withAlpha (0.6f));
        g.fillRect (bounds.getRight() - 1.0f, bounds.getY() + 2.0f, 1.0f, bounds.getHeight() - 4.0f);

        // Bottom rounded cap highlight
        g.setColour (juce::Colours::white.withAlpha (active ? 0.6f : 0.25f));
        g.fillRoundedRectangle (bounds.getX() + 3.0f,
                                 bounds.getBottom() - 14.0f,
                                 bounds.getWidth() - 6.0f, 10.0f, 3.0f);

        // Zone colour dot at top of key
        const auto zc = zoneColourForNote (kr.note);
        if (zc.getAlpha() > 0)
        {
            g.setColour (zc.withAlpha (0.85f));
            g.fillRoundedRectangle (bounds.getX() + 3.0f, bounds.getY() + 3.0f,
                                     bounds.getWidth() - 6.0f, 5.0f, 2.0f);
        }

        // Slice assigned marker — subtle accent border
        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (bounds.reduced (1.5f, 0.5f), 4.0f, 1.5f);
        }

        // Note label (C notes only to avoid clutter)
        const int semitone = kr.note % 12;
        if (semitone == 0)
        {
            g.setFont (DysektLookAndFeel::makeFont (7.0f));
            g.setColour (hasSlice ? accent : juce::Colour (0xFF888888));
            g.drawText (juce::MidiMessage::getMidiNoteName (kr.note, true, true, 3),
                        kr.bounds.getX(), kr.bounds.getBottom() - 13,
                        kr.bounds.getWidth(), 11, juce::Justification::centred);
        }
    }
    else
    {
        // ── Black key ─────────────────────────────────────────────────────────
        // Glossy dark body
        juce::ColourGradient grad (
            active  ? accent.darker (0.2f)
            : hovered ? juce::Colour (0xFF3A3A3A)
                      : juce::Colour (0xFF2A2A2A),
            0.f, bounds.getY(),
            active  ? accent.darker (0.6f)
            : hovered ? juce::Colour (0xFF1A1A1A)
                      : juce::Colour (0xFF0D0D0D),
            0.f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, 3.5f);

        // Gloss highlight strip at top
        g.setColour (juce::Colours::white.withAlpha (active ? 0.25f : 0.12f));
        g.fillRoundedRectangle (bounds.getX() + 3.0f, bounds.getY() + 2.0f,
                                 bounds.getWidth() - 6.0f, 7.0f, 2.0f);

        // Zone colour dot
        const auto zc = zoneColourForNote (kr.note);
        if (zc.getAlpha() > 0)
        {
            g.setColour (zc.withAlpha (0.80f));
            g.fillRoundedRectangle (bounds.getX() + 3.0f,
                                     bounds.getBottom() - 8.0f,
                                     bounds.getWidth() - 6.0f, 4.0f, 1.5f);
        }

        // Slice marker
        if (hasSlice)
        {
            g.setColour (accent.withAlpha (0.80f));
            g.drawRoundedRectangle (bounds.reduced (1.0f), 3.5f, 1.5f);
        }

        // Outer edge shadow
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds.expanded (0.5f), 4.0f, 1.0f);
    }
}

void KeysPanel::drawZoneBar (juce::Graphics& g, int keyboardX, int zoneBarY, int kbW) const
{
    // Full-width dim background
    g.setColour (getTheme().darkBar.darker (0.4f));
    g.fillRect (keyboardX, zoneBarY, kbW, kZoneBarH);

    const int loNote = baseOctave * 12;
    const int hiNote = loNote + 24;

    for (const auto& z : keyzones)
    {
        const int lo = juce::jlimit (loNote, hiNote, z.loKey);
        const int hi = juce::jlimit (loNote, hiNote, z.hiKey + 1);
        if (lo >= hi) continue;

        // Map note → x pixel (approximate — white-key spacing)
        auto noteToX = [&] (int note) -> float
        {
            const int rel = note - loNote;
            // Count white keys up to this semitone
            static const int whiteCount[] = { 0,0,1,1,2,3,3,4,4,5,5,6,7 };
            const int octaves = rel / 12;
            const int semi    = rel % 12;
            const float wx    = (float)(octaves * 7 + (semi < 13 ? whiteCount[semi] : 7));
            return (float) keyboardX + wx * (float) kWhiteKeyW;
        };

        const float x1 = noteToX (lo);
        const float x2 = noteToX (hi);
        if (x2 <= x1) continue;

        g.setColour (z.colour.withAlpha (0.75f));
        g.fillRoundedRectangle (x1 + 1.0f, (float) zoneBarY + 1.0f,
                                 x2 - x1 - 1.0f, (float) kZoneBarH - 2.0f, 2.0f);
    }

    // Thin separator line
    g.setColour (getTheme().accent.withAlpha (0.15f));
    g.fillRect (keyboardX, zoneBarY + kZoneBarH - 1, kbW, 1);
}

juce::Colour KeysPanel::zoneColourForNote (int note) const
{
    for (const auto& z : keyzones)
        if (note >= z.loKey && note <= z.hiKey)
            return z.colour;
    return juce::Colour (0);
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void KeysPanel::mouseDown (const juce::MouseEvent& e)
{
    // Black keys first (on top)
    for (auto it = keyRects.rbegin(); it != keyRects.rend(); ++it)
    {
        if (it->isBlack && it->bounds.contains (e.getPosition()))
        {
            lastActiveNote = it->note;
            repaint();
            return;
        }
    }
    for (const auto& kr : keyRects)
    {
        if (! kr.isBlack && kr.bounds.contains (e.getPosition()))
        {
            lastActiveNote = kr.note;
            repaint();
            return;
        }
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

void KeysPanel::timerCallback() { repaint(); }
