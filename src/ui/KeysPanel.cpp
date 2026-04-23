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

    kTransposeRowH = juce::jmin (22, h / 5);
    kZoneBarH      = keyzones.empty() ? 0 : 6;
    kKeyH          = h - kTransposeRowH - kZoneBarH - 2;

    // Fill the full panel width — keys expand to fit, no upper cap
    kWhiteKeyW = juce::jmax (6, w / kNumWhite);
    kBlackKeyW = juce::roundToInt (kWhiteKeyW * 0.52f);
    kBlackKeyH = juce::roundToInt (kKeyH * 0.62f);

    const int kbW       = kWhiteKeyW * kNumWhite;
    const int keyboardX = (w - kbW) / 2;
    const int keyboardY = kTransposeRowH + kZoneBarH;

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

    if (kZoneBarH > 0 && ! keyRects.empty())
        drawZoneBar (g, keyRects.front().bounds.getX(), kTransposeRowH, kWhiteKeyW * kNumWhite);

    for (const auto& kr : keyRects)
        if (! kr.isBlack)
            drawKey (g, kr, slicedNotes.count (kr.note) > 0, kr.note == hoveredNote, kr.note == lastActiveNote);

    for (const auto& kr : keyRects)
        if (kr.isBlack)
            drawKey (g, kr, slicedNotes.count (kr.note) > 0, kr.note == hoveredNote, kr.note == lastActiveNote);
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

        // Zone color dot
        const auto zc = zoneColourForNote (kr.note);
        if (zc.getAlpha() > 0)
        {
            g.setColour (zc.withAlpha (0.88f));
            g.fillRoundedRectangle (b.getX() + 2.5f, b.getY() + 3.0f, b.getWidth() - 5.0f, 4.0f, 1.5f);
        }

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

        // Zone color dot at bottom
        const auto zc = zoneColourForNote (kr.note);
        if (zc.getAlpha() > 0)
        {
            g.setColour (zc.withAlpha (0.80f));
            g.fillRoundedRectangle (b.getX() + 2.0f, b.getBottom() - 7.0f, b.getWidth() - 4.0f, 4.0f, 1.0f);
        }

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

void KeysPanel::drawZoneBar (juce::Graphics& g, int keyboardX, int zoneBarY, int kbW) const
{
    g.setColour (getTheme().darkBar.darker (0.4f));
    g.fillRect (keyboardX, zoneBarY, kbW, kZoneBarH);

    const int loNote = baseOctave * 12;
    const int hiNote = loNote + 24;

    for (const auto& z : keyzones)
    {
        const int lo = juce::jlimit (loNote, hiNote, z.loKey);
        const int hi = juce::jlimit (loNote, hiNote, z.hiKey + 1);
        if (lo >= hi) continue;

        auto noteToX = [&] (int note) -> float
        {
            const int rel = note - loNote;
            static const int whiteCount[] = { 0,0,1,1,2,3,3,4,4,5,5,6,7 };
            const int octaves = rel / 12;
            const int semi    = rel % 12;
            return (float) keyboardX + (float)(octaves * 7 + (semi < 13 ? whiteCount[semi] : 7)) * (float) kWhiteKeyW;
        };

        const float x1 = noteToX (lo);
        const float x2 = noteToX (hi);
        if (x2 <= x1) continue;

        g.setColour (z.colour.withAlpha (0.78f));
        g.fillRoundedRectangle (x1 + 1.0f, (float) zoneBarY + 1.0f, x2 - x1 - 1.0f, (float) kZoneBarH - 2.0f, 1.5f);
    }

    g.setColour (getTheme().accent.withAlpha (0.12f));
    g.fillRect (keyboardX, zoneBarY + kZoneBarH - 1, kbW, 1);
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
        if (it->isBlack && it->bounds.contains (e.getPosition()))
            { lastActiveNote = it->note; repaint(); return; }
    for (const auto& kr : keyRects)
        if (! kr.isBlack && kr.bounds.contains (e.getPosition()))
            { lastActiveNote = kr.note; repaint(); return; }
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
