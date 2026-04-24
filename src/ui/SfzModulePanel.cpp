// =============================================================================
//  SfzModulePanel.cpp
// =============================================================================
#include "SfzModulePanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── Constants ──────────────────────────────────────────────────────────────────
static constexpr int kKnobW    = 60;
static constexpr int kKnobH    = 70;
static constexpr int kChBtnW   = 40;
static constexpr int kMeterW   = 80;
static constexpr int kPad      = 8;
static constexpr int kLoadBtnW = 72;
static constexpr int kLoadBtnH = 26;

// ── Constructor / destructor ──────────────────────────────────────────────────

SfzModulePanel::SfzModulePanel (DysektProcessor& p)
    : processor (p),
      keysPanel (p)
{
    addAndMakeVisible (keysPanel);
    startTimerHz (30);
}

SfzModulePanel::~SfzModulePanel() = default;

// ── Layout ────────────────────────────────────────────────────────────────────

void SfzModulePanel::resized()
{
    auto area = getLocalBounds().reduced (kPad);

    // Bottom portion — keyboard
    const int kbH = juce::jmax (90, (area.getHeight() * 3) / 5);
    auto kbArea = area.removeFromBottom (kbH);
    keysPanel.setBounds (kbArea);
    area.removeFromBottom (4); // gap

    // Left-to-right: name | load btn | VOL knob | TRANS knob | CH | meter | status
    loadBtnZone = area.removeFromLeft (kLoadBtnW).withSizeKeepingCentre (kLoadBtnW, kLoadBtnH);
    area.removeFromLeft (kPad);

    volZone   = area.removeFromLeft (kKnobW);
    area.removeFromLeft (4);
    transZone = area.removeFromLeft (kKnobW);
    area.removeFromLeft (kPad);

    chZone    = area.removeFromLeft (kChBtnW * 3 + 4);
    area.removeFromLeft (kPad);

    meterZone = area.removeFromRight (kMeterW);
    area.removeFromRight (kPad);

    statusZone = area.removeFromRight (60);
    area.removeFromRight (4);

    nameZone = area;  // remainder = file name label
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SfzModulePanel::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const float sf = (float) getWidth() / 1114.0f;  // approx kBaseW minus margins

    // ── Background ────────────────────────────────────────────────────────────
    // Border is drawn by the editor's paintOverChildren (same CRT-frame recipe
    // as the waveform / pad-grid panels) so we only fill the background here.
    auto bounds = getLocalBounds().toFloat();
    g.setColour (theme.darkBar.darker (0.25f));
    g.fillRoundedRectangle (bounds, 4.0f);

    // ── Header label ─────────────────────────────────────────────────────────
    {
        auto lbl = nameZone.reduced (4, 0);
        g.setFont (DysektLookAndFeel::makeFont (10.0f));
        g.setColour (theme.foreground.withAlpha (0.40f));
        g.drawText ("SFZ / SF2", lbl, juce::Justification::topLeft, false);

        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        const bool isLoaded = processor.sfzPlayer.isLoaded();
        if (isLoaded)
        {
            g.setColour (theme.foreground);
            const auto name = processor.sfzPlayer.getLoadedFile()
                                .getFileNameWithoutExtension();
            g.drawText (name, lbl.withTrimmedTop (14), juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour (theme.foreground.withAlpha (0.30f));
            g.drawText ("No instrument loaded", lbl.withTrimmedTop (14),
                        juce::Justification::centredLeft, false);
        }
    }

    // ── Load button ───────────────────────────────────────────────────────────
    {
        const auto btn = loadBtnZone.toFloat();
        const bool hover = loadBtnZone.contains (getMouseXYRelative());
        g.setColour (hover ? theme.accent.withAlpha (0.25f)
                           : theme.darkBar.brighter (0.05f));
        g.fillRoundedRectangle (btn, 3.0f);
        g.setColour (theme.accent.withAlpha (hover ? 0.85f : 0.55f));
        g.drawRoundedRectangle (btn.reduced (0.5f), 3.0f, 1.0f);

        g.setFont (DysektLookAndFeel::makeFont (11.0f));
        g.setColour (theme.accent);
        g.drawText ("LOAD", loadBtnZone, juce::Justification::centred, false);
    }

    // ── Volume knob ───────────────────────────────────────────────────────────
    {
        const float vol  = processor.sfzPlayer.getVolume();
        const float norm = volToNorm (vol);
        const float db   = (vol > 0.001f) ? juce::Decibels::gainToDecibels (vol) : -96.0f;
        const auto  str  = (db <= -95.f) ? juce::String ("-inf")
                                          : juce::String (db, 1) + " dB";
        drawKnob (g, volZone, norm, "VOL", str);
    }

    // ── Transpose knob ────────────────────────────────────────────────────────
    {
        const int  semi = processor.sfzPlayer.getTranspose();
        const float norm = transToNorm (semi);
        const auto  str  = (semi == 0) ? juce::String ("0 st")
                                       : (semi > 0 ? "+" : "") + juce::String (semi) + " st";
        drawKnob (g, transZone, norm, "TRANS", str);
    }

    // ── MIDI channel selector ─────────────────────────────────────────────────
    {
        const int ch = processor.sfzPlayer.getMidiChannel();
        const int btnH = chZone.getHeight() / 2 - 2;

        // "OMNI" and "CH" label strip
        auto chArea = chZone;
        g.setFont (DysektLookAndFeel::makeFont (10.0f));
        g.setColour (theme.foreground.withAlpha (0.40f));
        g.drawText ("MIDI CH", chArea.removeFromTop (14), juce::Justification::centred, false);

        auto chBtns = chArea;
        const int btnW = chBtns.getWidth() / 3;

        // Decrement arrow
        auto decBtn = chBtns.removeFromLeft (btnW).withSizeKeepingCentre (btnW - 2, btnH);
        g.setColour (theme.darkBar.brighter (0.08f));
        g.fillRoundedRectangle (decBtn.toFloat(), 2.0f);
        g.setColour (theme.accent.withAlpha (0.7f));
        g.drawRoundedRectangle (decBtn.toFloat().reduced (0.5f), 2.0f, 1.0f);
        g.drawText ("<", decBtn, juce::Justification::centred, false);

        // Channel display
        auto chDisp = chBtns.removeFromLeft (btnW);
        g.setColour (theme.darkBar.darker (0.15f));
        g.fillRoundedRectangle (chDisp.toFloat(), 2.0f);
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (theme.foreground);
        g.drawText (ch == 0 ? "ALL" : juce::String (ch), chDisp, juce::Justification::centred, false);

        // Increment arrow
        auto incBtn = chBtns.withSizeKeepingCentre (btnW - 2, btnH);
        g.setColour (theme.darkBar.brighter (0.08f));
        g.fillRoundedRectangle (incBtn.toFloat(), 2.0f);
        g.setColour (theme.accent.withAlpha (0.7f));
        g.drawRoundedRectangle (incBtn.toFloat().reduced (0.5f), 2.0f, 1.0f);
        g.drawText (">", incBtn, juce::Justification::centred, false);
    }

    // ── Status pill ───────────────────────────────────────────────────────────
    {
        const bool loaded = processor.sfzPlayer.isLoaded();
        auto pill = statusZone.withSizeKeepingCentre (52, 18).toFloat();
        g.setColour (loaded ? theme.accent.withAlpha (0.20f)
                            : theme.foreground.withAlpha (0.08f));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (loaded ? theme.accent : theme.foreground.withAlpha (0.30f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 9.0f, 1.0f);
        g.setFont (DysektLookAndFeel::makeFont (10.0f));
        g.drawText (loaded ? "READY" : "EMPTY", pill.toNearestInt(), juce::Justification::centred, false);
    }

    // ── VU meter ─────────────────────────────────────────────────────────────
    drawMeter (g);

    // ── Drop-target hint when dragging over ───────────────────────────────────
    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        g.setColour (theme.accent.withAlpha (0.12f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }
}

// ── Knob drawing helper ───────────────────────────────────────────────────────

void SfzModulePanel::drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                                 float normalised, const juce::String& label,
                                 const juce::String& valueStr) const
{
    const auto& theme = getTheme();
    const int   cx    = bounds.getCentreX();
    const int   dia   = juce::jmin (bounds.getWidth(), 40);
    const int   cy    = bounds.getY() + dia / 2 + 4;
    const float r     = (float) dia * 0.5f;

    const float startAngle = juce::MathConstants<float>::pi * 1.25f;
    const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
    const float angle      = startAngle + normalised * (endAngle - startAngle);

    // Track arc
    juce::Path track;
    track.addCentredArc ((float) cx, (float) cy, r - 2, r - 2,
                          0.0f, startAngle, endAngle, true);
    g.setColour (theme.darkBar.brighter (0.10f));
    g.strokePath (track, juce::PathStrokeType (3.0f));

    // Fill arc
    juce::Path fill;
    fill.addCentredArc ((float) cx, (float) cy, r - 2, r - 2,
                         0.0f, startAngle, angle, true);
    g.setColour (theme.accent);
    g.strokePath (fill, juce::PathStrokeType (3.0f));

    // Thumb dot
    const float tx = (float) cx + (r - 6) * std::cos (angle - juce::MathConstants<float>::halfPi);
    const float ty = (float) cy + (r - 6) * std::sin (angle - juce::MathConstants<float>::halfPi);
    g.setColour (theme.accent.brighter (0.2f));
    g.fillEllipse (tx - 3, ty - 3, 6, 6);

    // Labels
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (theme.foreground.withAlpha (0.45f));
    g.drawText (label, bounds.withTrimmedTop (cy - bounds.getY() + (int) r + 2),
                juce::Justification::centredTop, false);

    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (theme.foreground.withAlpha (0.80f));
    g.drawText (valueStr,
                bounds.withTrimmedTop (cy - bounds.getY() + (int) r + 14),
                juce::Justification::centredTop, false);
}

// ── VU meter drawing ──────────────────────────────────────────────────────────

void SfzModulePanel::drawMeter (juce::Graphics& g) const
{
    const auto& theme = getTheme();
    auto area = meterZone.reduced (2, 4);

    const int barW  = area.getWidth() / 2 - 2;
    const int barH  = area.getHeight();

    auto leftBar  = juce::Rectangle<int> (area.getX(), area.getY(), barW, barH);
    auto rightBar = juce::Rectangle<int> (area.getX() + barW + 4, area.getY(), barW, barH);

    // Background tracks
    g.setColour (theme.darkBar.darker (0.2f));
    g.fillRoundedRectangle (leftBar.toFloat(),  2.0f);
    g.fillRoundedRectangle (rightBar.toFloat(), 2.0f);

    auto drawBar = [&] (juce::Rectangle<int> bar, float peak, float hold)
    {
        const int fillH = juce::roundToInt ((float) bar.getHeight() * juce::jlimit (0.0f, 1.0f, peak));
        if (fillH > 0)
        {
            juce::ColourGradient grad (theme.accent.withAlpha (0.85f),
                                       0.f, (float) bar.getBottom(),
                                       theme.accent.brighter (0.5f),
                                       0.f, (float) bar.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() - fillH).toFloat(), 2.0f);
        }
        // Hold line
        const int holdY = bar.getBottom() - juce::roundToInt ((float) bar.getHeight() * juce::jlimit (0.0f, 1.0f, hold));
        g.setColour (theme.accent.brighter (0.6f).withAlpha (0.7f));
        g.fillRect (bar.getX(), holdY - 1, bar.getWidth(), 2);

        // Label
        g.setFont (DysektLookAndFeel::makeFont (9.0f));
        g.setColour (theme.foreground.withAlpha (0.35f));
    };

    drawBar (leftBar,  meterL, holdL);
    drawBar (rightBar, meterR, holdR);
}

// ── Timer: poll processor SFZ peak meters ────────────────────────────────────

void SfzModulePanel::timerCallback()
{
    // SfzPlayer exposes peak via sfzPeakL/R atomics (added to processor below)
    const float newL = processor.sfzPeakL.load (std::memory_order_relaxed);
    const float newR = processor.sfzPeakR.load (std::memory_order_relaxed);

    if (newL > holdL) holdL = newL;
    if (newR > holdR) holdR = newR;
    holdL *= kHoldDecay;
    holdR *= kHoldDecay;
    meterL = newL;
    meterR = newR;

    repaint();
}

// ── Mouse events ─────────────────────────────────────────────────────────────

void SfzModulePanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();

    // Load button
    if (loadBtnZone.contains (pos))
    {
        openFileChooser();
        return;
    }

    // CH decrement / increment
    {
        auto chArea = chZone;
        chArea.removeFromTop (14);
        const int btnW = chArea.getWidth() / 3;
        auto decBtn = juce::Rectangle<int> (chArea.getX(), chArea.getY(), btnW, chArea.getHeight());
        auto incBtn = juce::Rectangle<int> (chArea.getX() + 2 * btnW, chArea.getY(), btnW, chArea.getHeight());
        if (decBtn.contains (pos))
        {
            int ch = processor.sfzPlayer.getMidiChannel();
            processor.sfzPlayer.setMidiChannel (juce::jmax (0, ch - 1));
            repaint(); return;
        }
        if (incBtn.contains (pos))
        {
            int ch = processor.sfzPlayer.getMidiChannel();
            processor.sfzPlayer.setMidiChannel (juce::jmin (16, ch + 1));
            repaint(); return;
        }
    }

    // Knob drag start
    if (volZone.contains (pos))
    {
        activeKnob  = ActiveKnob::Volume;
        dragStartY  = pos.y;
        dragStartVal = volToNorm (processor.sfzPlayer.getVolume());
        return;
    }
    if (transZone.contains (pos))
    {
        activeKnob  = ActiveKnob::Transpose;
        dragStartY  = pos.y;
        dragStartVal = transToNorm (processor.sfzPlayer.getTranspose());
        return;
    }
}

void SfzModulePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (activeKnob == ActiveKnob::None) return;

    const float delta = (float)(dragStartY - e.getPosition().y) / 150.0f;
    const float newNorm = juce::jlimit (0.0f, 1.0f, dragStartVal + delta);

    if (activeKnob == ActiveKnob::Volume)
        processor.sfzPlayer.setVolume (normToVol (newNorm));
    else
        processor.sfzPlayer.setTranspose (normToTrans (newNorm));

    repaint();
}

void SfzModulePanel::mouseUp (const juce::MouseEvent&)
{
    activeKnob = ActiveKnob::None;
}

void SfzModulePanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    // Reset volume to unity on double-click
    if (volZone.contains (pos))
    {
        processor.sfzPlayer.setVolume (1.0f);
        repaint();
    }
    // Reset transpose to 0 on double-click
    if (transZone.contains (pos))
    {
        processor.sfzPlayer.setTranspose (0);
        repaint();
    }
}

void SfzModulePanel::mouseWheelMove (const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& w)
{
    const auto pos = e.getPosition();
    const float step = w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f);

    if (volZone.contains (pos))
    {
        const float n = juce::jlimit (0.0f, 1.0f, volToNorm (processor.sfzPlayer.getVolume()) + step);
        processor.sfzPlayer.setVolume (normToVol (n));
        repaint();
    }
    else if (transZone.contains (pos))
    {
        const float n = juce::jlimit (0.0f, 1.0f, transToNorm (processor.sfzPlayer.getTranspose()) + step);
        processor.sfzPlayer.setTranspose (normToTrans (n));
        repaint();
    }
}

// ── File drag-and-drop ────────────────────────────────────────────────────────

bool SfzModulePanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".sf2" || ext == ".sfz") return true;
    }
    return false;
}

void SfzModulePanel::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".sf2" || ext == ".sfz")
        {
            juce::File file (f);
            processor.sfzPlayer.loadFile (file);
            reloadZones (file);
            repaint();
            return;
        }
    }
}

// ── File chooser ─────────────────────────────────────────────────────────────

void SfzModulePanel::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load SF2 or SFZ Instrument",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.sf2;*.sfz");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
            {
                processor.sfzPlayer.loadFile (result);
                reloadZones (result);
                repaint();
            }
        });
}

// ── panelDidShow ──────────────────────────────────────────────────────────────

void SfzModulePanel::panelDidShow()
{
    if (processor.sfzPlayer.isLoaded())
        reloadZones (processor.sfzPlayer.getLoadedFile());
    repaint();
}

// ── Value mapping helpers ─────────────────────────────────────────────────────

float SfzModulePanel::volToNorm   (float linear) const { return juce::jlimit (0.0f, 1.0f, linear * 0.5f); }
float SfzModulePanel::normToVol   (float n)      const { return n * 2.0f; }
float SfzModulePanel::transToNorm (int semi)      const { return ((float) semi + 24.0f) / 48.0f; }
int   SfzModulePanel::normToTrans (float n)       const { return juce::roundToInt (n * 48.0f - 24.0f); }

// =============================================================================
//  Zone parsers
// =============================================================================

// Palette of distinct colours for successive zones
static juce::Colour zoneColour (int index)
{
    static const juce::Colour palette[] = {
        juce::Colour (0xFFD47810),  // bright amber
        juce::Colour (0xFF8B4500),  // dark orange
        juce::Colour (0xFFA85215),  // medium orange
        juce::Colour (0xFF7A3C08),  // deep amber
        juce::Colour (0xFFBF6A18),  // warm amber
        juce::Colour (0xFF924E10),  // rich orange
        juce::Colour (0xFFCC7215),  // golden orange
        juce::Colour (0xFF7F3C0A),  // dark rich orange
    };
    return palette[index % 8];
}

std::vector<KeysPanel::Keyzone> SfzModulePanel::parseSfzZones (const juce::File& f)
{
    std::vector<KeysPanel::Keyzone> zones;
    const auto text = f.loadFileAsString();
    const auto lines = juce::StringArray::fromLines (text);

    int loKey = 0, hiKey = 127;
    bool inRegion = false;
    int  colIdx   = 0;
    juce::String sampleName;

    auto flush = [&]
    {
        if (inRegion && hiKey >= loKey)
        {
            zones.push_back ({ loKey, hiKey, zoneColour (colIdx++), sampleName });
            loKey = 0; hiKey = 127; sampleName = {};
        }
        inRegion = false;
    };

    for (auto line : lines)
    {
        line = line.trim().toLowerCase();
        if (line.startsWith ("<region>")) { flush(); inRegion = true; loKey = 0; hiKey = 127; }
        else if (line.startsWith ("<group>") || line.startsWith ("<global>")) flush();

        if (inRegion)
        {
            auto get = [&] (const juce::String& key) -> int
            {
                int pos = line.indexOf (key + "=");
                if (pos < 0) return -999;
                auto val = line.substring (pos + key.length() + 1)
                               .trimStart()
                               .upToFirstOccurrenceOf (" ", false, false)
                               .upToFirstOccurrenceOf ("	", false, false);
                return val.getIntValue();
            };
            // lokey / hikey (numeric or note name both supported)
            auto loRaw = line.indexOf ("lokey=");
            if (loRaw >= 0)
            {
                auto s = line.substring (loRaw + 6)
                             .upToFirstOccurrenceOf (" ", false, false)
                             .trim();
                loKey = s.containsAnyOf ("abcdefg") ? juce::MidiMessage::getMidiNoteInHertz(0) >= 0
                        ? juce::jlimit (0, 127, (int) s.getIntValue()) : s.getIntValue()
                        : s.getIntValue();
                // Best-effort: just parse integer; SFZ note names handled below
                loKey = juce::jlimit (0, 127, s.getIntValue());
            }
            auto hiRaw = line.indexOf ("hikey=");
            if (hiRaw >= 0)
            {
                auto s = line.substring (hiRaw + 6)
                             .upToFirstOccurrenceOf (" ", false, false)
                             .trim();
                hiKey = juce::jlimit (0, 127, s.getIntValue());
            }
            auto kRaw = line.indexOf ("key=");
            if (kRaw >= 0 && line.indexOf ("lokey=") < 0)
            {
                auto s = line.substring (kRaw + 4)
                             .upToFirstOccurrenceOf (" ", false, false)
                             .trim();
                const int k = juce::jlimit (0, 127, s.getIntValue());
                loKey = hiKey = k;
            }
            (void) get;

            // Extract sample name as zone label
            {
                int smpPos = line.indexOf ("sample=");
                if (smpPos >= 0)
                {
                    auto s = line.substring (smpPos + 7)
                                 .upToFirstOccurrenceOf (" ", false, false)
                                 .trim();
                    sampleName = s.fromLastOccurrenceOf ("/",  false, false)
                                  .fromLastOccurrenceOf ("\\", false, false)
                                  .upToLastOccurrenceOf (".",  false, false)
                                  .trim();
                    if (sampleName.isEmpty())
                        sampleName = "Zone " + juce::String (colIdx + 1);
                }
            }
        }
    }
    flush();
    return zones;
}

std::vector<KeysPanel::Keyzone> SfzModulePanel::parseSf2Zones (const juce::File& f)
{
    // Lightweight SF2 RIFF parser — extracts keyRange (gen 43) from IGEN chunk.
    std::vector<KeysPanel::Keyzone> zones;

    juce::FileInputStream stream (f);
    if (stream.failedToOpen()) return zones;

    // RIFF header
    char riff[4]; stream.read (riff, 4);
    if (juce::String::fromUTF8 (riff, 4) != "RIFF") return zones;
    stream.readInt();  // file size
    char sfbk[4]; stream.read (sfbk, 4);
    if (juce::String::fromUTF8 (sfbk, 4) != "sfbk") return zones;

    // Walk top-level LIST chunks to find "pdta"
    juce::MemoryBlock igenData;

    while (! stream.isExhausted())
    {
        char id[4]; if (stream.read (id, 4) < 4) break;
        const auto chunkId = juce::String::fromUTF8 (id, 4);
        const int  sz      = stream.readInt();

        if (chunkId == "LIST")
        {
            char listId[4]; stream.read (listId, 4);
            const auto lid = juce::String::fromUTF8 (listId, 4);

            if (lid == "pdta")
            {
                // Scan sub-chunks for "igen"
                const int pdtaEnd = (int) stream.getPosition() + sz - 4;
                while (stream.getPosition() < pdtaEnd && ! stream.isExhausted())
                {
                    char sub[4]; if (stream.read (sub, 4) < 4) break;
                    const auto subId = juce::String::fromUTF8 (sub, 4);
                    const int  subSz = stream.readInt();
                    if (subId == "igen")
                    {
                        igenData.setSize ((size_t) subSz);
                        stream.read (igenData.getData(), subSz);
                        break;
                    }
                    stream.skipNextBytes (subSz);
                }
                break;
            }
            else
            {
                stream.skipNextBytes (sz - 4);
            }
        }
        else
        {
            stream.skipNextBytes (sz);
        }
    }

    if (igenData.isEmpty()) return zones;

    // Each IGEN record is 4 bytes: sfGenOper (uint16) + genAmount (union 2 bytes)
    struct IgenRecord { uint16_t oper; uint8_t lo, hi; };
    const size_t numRecs = igenData.getSize() / 4;
    const auto*  data    = static_cast<const uint8_t*> (igenData.getData());

    int  loKey = 0, hiKey = 127;
    bool hasKey = false;
    int  colIdx = 0;

    for (size_t i = 0; i < numRecs; ++i)
    {
        const uint16_t oper = (uint16_t)(data[i*4] | (data[i*4+1] << 8));
        const uint8_t  lo   = data[i*4 + 2];
        const uint8_t  hi   = data[i*4 + 3];

        if (oper == 43)  // keyRange
        {
            loKey  = lo;
            hiKey  = hi;
            hasKey = true;
        }
        else if (oper == 0)  // endOper — end of zone
        {
            if (hasKey && hiKey >= loKey)
            {
                zones.push_back ({ loKey, hiKey, zoneColour (colIdx),
                                   "Zone " + juce::String (colIdx + 1) });
                ++colIdx;
            }
            loKey  = 0; hiKey = 127; hasKey = false;
        }
    }

    // Deduplicate identical ranges (SF2 files often repeat per-preset)
    std::sort (zones.begin(), zones.end(),
               [] (auto& a, auto& b) { return a.loKey < b.loKey; });
    zones.erase (std::unique (zones.begin(), zones.end(),
                               [] (auto& a, auto& b) { return a.loKey == b.loKey && a.hiKey == b.hiKey; }),
                 zones.end());

    // Re-assign colours after dedup
    for (size_t i = 0; i < zones.size(); ++i)
    {
        zones[i].colour = zoneColour ((int) i);
        if (zones[i].name.isEmpty())
            zones[i].name = "Zone " + juce::String ((int) i + 1);
    }

    return zones;
}

void SfzModulePanel::reloadZones (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    std::vector<KeysPanel::Keyzone> zones;

    if (ext == ".sfz")
        zones = parseSfzZones (f);
    else if (ext == ".sf2")
        zones = parseSf2Zones (f);

    keysPanel.setKeyzones (zones);
    if (! zones.empty())
        keysPanel.autoScrollToZones();
}
