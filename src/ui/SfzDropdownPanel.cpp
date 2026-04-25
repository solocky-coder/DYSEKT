// =============================================================================
//  SfzDropdownPanel.cpp  —  SF2 instrument strip (FluidSynth backend)
// =============================================================================
#include "SfzDropdownPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── Layout constants (header strip) ──────────────────────────────────────────
static constexpr int kLoadW        = 64;
static constexpr int kLoadH        = 22;
static constexpr int kKnobW        = 52;
static constexpr int kChBtnW       = 36;
static constexpr int kMeterW       = 60;
static constexpr int kStatusW      = 52;
static constexpr int kPresetArrowW = 18;
static constexpr int kPad          = 6;

// =============================================================================
//  Constructor / destructor
// =============================================================================

SfzDropdownPanel::SfzDropdownPanel (DysektProcessor& p)
    : processor (p),
      keysPanel (p)
{
    addChildComponent (keysPanel);
    startTimerHz (30);
}

SfzDropdownPanel::~SfzDropdownPanel() = default;

// =============================================================================
//  Layout
// =============================================================================

void SfzDropdownPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Header strip always at the BOTTOM; keyboard fills everything above.
    const int stripY = h - kStripH;

    // ── Header strip zones ──────────────────────────────────────────────────
    auto strip = juce::Rectangle<int> (0, stripY, w, kStripH).reduced (kPad, 0);

    strip.removeFromLeft (4);   // left margin

    loadBtnZone = strip.removeFromLeft (kLoadW).withSizeKeepingCentre (kLoadW, kLoadH);
    strip.removeFromLeft (kPad);

    volZone   = strip.removeFromLeft (kKnobW);
    strip.removeFromLeft (4);
    transZone = strip.removeFromLeft (kKnobW);
    strip.removeFromLeft (kPad);

    chZone = strip.removeFromLeft (kChBtnW * 3 + 4);
    strip.removeFromLeft (kPad);

    meterZone  = strip.removeFromRight (kMeterW);
    strip.removeFromRight (kPad);

    statusZone = strip.removeFromRight (kStatusW);
    strip.removeFromRight (4);

    nameZone = strip;   // remainder → preset picker

    // Sub-divide nameZone into  [< arrow] [label] [> arrow]
    {
        auto z = nameZone;
        presetDecBtn  = z.removeFromLeft  (kPresetArrowW);
        presetIncBtn  = z.removeFromRight (kPresetArrowW);
        presetLabel   = z;
    }

    // ── Keyboard panel: fills the area above the strip ──────────────────────
    const int kbH = juce::jmax (60, stripY - kPad);
    keysPanel.setVisible (kbH > 0);
    if (kbH > 0)
        keysPanel.setBounds (kPad, kPad, w - kPad * 2, kbH);
    else
        keysPanel.setBounds ({});
}

// =============================================================================
//  Paint
// =============================================================================

void SfzDropdownPanel::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const int   w     = getWidth();
    const int   h     = getHeight();

    // ── Full background ───────────────────────────────────────────────────────
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient bg (theme.darkBar.darker (0.35f), 0.f, 0.f,
                                  theme.darkBar.darker (0.10f), 0.f, (float) h, false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 4.0f);

        // Separator between keyboard and strip
        const int sepY = h - kStripH;
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRect (kPad, sepY, w - kPad * 2, 1);
    }

    // ── Draw header strip ─────────────────────────────────────────────────────
    drawHeaderStrip (g);

    // ── Top accent line ───────────────────────────────────────────────────────
    g.setColour (theme.accent.withAlpha (0.45f));
    g.fillRect (0, 0, w, 1);
}

// =============================================================================
//  drawHeaderStrip
// =============================================================================

void SfzDropdownPanel::drawHeaderStrip (juce::Graphics& g) const
{
    const auto& theme = getTheme();

    // ── Preset picker ─────────────────────────────────────────────────────────
    drawPresetPicker (g);

    // ── Load button ───────────────────────────────────────────────────────────
    {
        const auto btn   = loadBtnZone.toFloat();
        const bool hover = loadBtnZone.contains (getMouseXYRelative());
        g.setColour (hover ? theme.accent.withAlpha (0.22f)
                           : theme.darkBar.brighter (0.06f));
        g.fillRoundedRectangle (btn, 3.0f);
        g.setColour (theme.accent.withAlpha (hover ? 0.85f : 0.50f));
        g.drawRoundedRectangle (btn.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (DysektLookAndFeel::makeFont (10.5f));
        g.setColour (theme.accent);
        g.drawText ("LOAD DYFONT", loadBtnZone, juce::Justification::centred, false);
    }

    // ── VOL knob ─────────────────────────────────────────────────────────────
    drawKnob (g, volZone, volToNorm (processor.sfzPlayer.getVolume()),
              "VOL",
              [&]() -> juce::String {
                  const float db = juce::Decibels::gainToDecibels (processor.sfzPlayer.getVolume());
                  return db <= -95.f ? "-inf" : juce::String (db, 1) + "dB";
              }());

    // ── TRANS knob ────────────────────────────────────────────────────────────
    drawKnob (g, transZone, transToNorm (processor.sfzPlayer.getTranspose()),
              "TRN",
              [&]() -> juce::String {
                  const int s = processor.sfzPlayer.getTranspose();
                  return s == 0 ? "0" : (s > 0 ? "+" : "") + juce::String (s);
              }());

    // ── MIDI channel selector ─────────────────────────────────────────────────
    {
        const int ch   = processor.sfzPlayer.getMidiChannel();
        auto      area = chZone;
        const int btnH = area.getHeight() - 4;
        const int btnW = area.getWidth() / 3;

        auto decBtn = juce::Rectangle<int> (area.getX(), area.getY() + 2, btnW - 1, btnH);
        auto chDisp = juce::Rectangle<int> (area.getX() + btnW, area.getY() + 2, btnW, btnH);
        auto incBtn = juce::Rectangle<int> (area.getX() + 2*btnW + 1, area.getY() + 2, btnW - 1, btnH);

        g.setColour (theme.darkBar.brighter (0.08f));
        g.fillRoundedRectangle (decBtn.toFloat(), 2.0f);
        g.fillRoundedRectangle (incBtn.toFloat(), 2.0f);
        g.setColour (theme.darkBar.darker (0.1f));
        g.fillRoundedRectangle (chDisp.toFloat(), 2.0f);

        g.setColour (theme.accent.withAlpha (0.60f));
        g.drawRoundedRectangle (decBtn.toFloat().reduced (0.5f), 2.0f, 1.0f);
        g.drawRoundedRectangle (incBtn.toFloat().reduced (0.5f), 2.0f, 1.0f);

        g.setFont (DysektLookAndFeel::makeFont (11.0f));
        g.setColour (theme.accent);
        g.drawText ("<", decBtn, juce::Justification::centred, false);
        g.drawText (">", incBtn, juce::Justification::centred, false);
        g.setColour (theme.foreground);
        g.drawText (ch == 0 ? "ALL" : juce::String (ch), chDisp,
                    juce::Justification::centred, false);
    }

    // ── Status pill ───────────────────────────────────────────────────────────
    {
        const bool isLoaded = processor.sfzPlayer.isLoaded();
        auto pill = statusZone.withSizeKeepingCentre (48, 16).toFloat();
        g.setColour (isLoaded ? theme.accent.withAlpha (0.18f)
                              : theme.foreground.withAlpha (0.07f));
        g.fillRoundedRectangle (pill, 8.0f);
        g.setColour (isLoaded ? theme.accent : theme.foreground.withAlpha (0.28f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 8.0f, 1.0f);
        g.setFont (DysektLookAndFeel::makeFont (9.5f));
        g.drawText (isLoaded ? "READY" : "EMPTY", pill.toNearestInt(),
                    juce::Justification::centred, false);
    }

    // ── VU meter ──────────────────────────────────────────────────────────────
    drawMeter (g);
}

// =============================================================================
//  drawPresetPicker
// =============================================================================

void SfzDropdownPanel::drawPresetPicker (juce::Graphics& g) const
{
    const auto& theme    = getTheme();
    const bool  isLoaded = processor.sfzPlayer.isLoaded();

    // ── Background panel for the whole picker area ────────────────────────────
    {
        auto bg = nameZone.toFloat();
        g.setColour (theme.darkBar.darker (0.12f));
        g.fillRoundedRectangle (bg, 3.0f);
        g.setColour (theme.accent.withAlpha (0.20f));
        g.drawRoundedRectangle (bg.reduced (0.5f), 3.0f, 1.0f);
    }

    // ── Arrow buttons ─────────────────────────────────────────────────────────
    auto drawArrow = [&] (juce::Rectangle<int> zone, const juce::String& sym)
    {
        const bool hover = zone.contains (getMouseXYRelative()) && isLoaded;
        g.setColour (hover ? theme.accent.withAlpha (0.30f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (zone.toFloat(), 2.0f);
        g.setFont (DysektLookAndFeel::makeFont (11.0f));
        g.setColour (isLoaded ? theme.accent.withAlpha (0.75f)
                              : theme.foreground.withAlpha (0.20f));
        g.drawText (sym, zone, juce::Justification::centred, false);
    };
    drawArrow (presetDecBtn, "<");
    drawArrow (presetIncBtn, ">");

    // ── Preset label (two rows: bank info + preset name) ──────────────────────
    {
        auto lbl = presetLabel;

        if (! isLoaded)
        {
            g.setFont (DysektLookAndFeel::makeFont (10.5f));
            g.setColour (theme.foreground.withAlpha (0.28f));
            g.drawText ("No DYFONT loaded", lbl, juce::Justification::centred, false);
        }
        else if (presetList.empty())
        {
            g.setFont (DysektLookAndFeel::makeFont (10.5f));
            g.setColour (theme.foreground.withAlpha (0.35f));
            g.drawText (processor.sfzPlayer.getLoadedFile().getFileNameWithoutExtension(),
                        lbl, juce::Justification::centred, true);
        }
        else
        {
            const int idx = juce::jlimit (0, (int) presetList.size() - 1,
                                          processor.sfzPlayer.getCurrentPresetIndex());
            const auto& info = presetList[(size_t) idx];

            // Top mini-label: filename + bank/preset index
            {
                auto topLine = lbl.removeFromTop (lbl.getHeight() / 2);
                g.setFont (DysektLookAndFeel::makeFont (8.5f));
                g.setColour (theme.foreground.withAlpha (0.38f));
                const auto caption =
                    processor.sfzPlayer.getLoadedFile().getFileNameWithoutExtension()
                    + "  B:" + juce::String (info.bank)
                    + " P:" + juce::String (info.preset);
                g.drawText (caption, topLine, juce::Justification::centred, true);
            }

            // Bottom: preset name
            g.setFont (DysektLookAndFeel::makeFont (11.0f));
            g.setColour (theme.foreground);
            g.drawText (info.name, lbl, juce::Justification::centred, true);
        }
    }
}

// =============================================================================
//  drawKnob — compact strip-height knob
// =============================================================================

void SfzDropdownPanel::drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                                   float normalised, const juce::String& label,
                                   const juce::String& valueStr) const
{
    const auto& theme = getTheme();
    const int   cx    = bounds.getCentreX();
    const int   dia   = juce::jmin (bounds.getWidth() - 4, bounds.getHeight() - 10, 24);
    const int   cy    = bounds.getY() + 4 + dia / 2;
    const float r     = (float) dia * 0.5f;

    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float angle  = startA + normalised * (endA - startA);

    juce::Path track;
    track.addCentredArc ((float) cx, (float) cy, r - 1, r - 1, 0.f, startA, endA, true);
    g.setColour (theme.darkBar.brighter (0.12f));
    g.strokePath (track, juce::PathStrokeType (2.2f));

    juce::Path fill;
    fill.addCentredArc ((float) cx, (float) cy, r - 1, r - 1, 0.f, startA, angle, true);
    g.setColour (theme.accent);
    g.strokePath (fill, juce::PathStrokeType (2.2f));

    const float tx = (float) cx + (r - 4) * std::cos (angle - juce::MathConstants<float>::halfPi);
    const float ty = (float) cy + (r - 4) * std::sin (angle - juce::MathConstants<float>::halfPi);
    g.setColour (theme.accent.brighter (0.25f));
    g.fillEllipse (tx - 2.5f, ty - 2.5f, 5.0f, 5.0f);

    const int labelY = cy + (int) r + 2;
    g.setFont (DysektLookAndFeel::makeFont (8.5f));
    g.setColour (theme.foreground.withAlpha (0.40f));
    g.drawText (label,    bounds.withY (labelY).withHeight (10), juce::Justification::centredTop, false);
    g.setColour (theme.foreground.withAlpha (0.75f));
    g.drawText (valueStr, bounds.withY (labelY + 10).withHeight (10), juce::Justification::centredTop, false);
}

// =============================================================================
//  drawMeter
// =============================================================================

void SfzDropdownPanel::drawMeter (juce::Graphics& g) const
{
    const auto& theme = getTheme();
    auto area = meterZone.reduced (2, 6);

    const int barW = area.getWidth() / 2 - 2;
    const int barH = area.getHeight();

    auto leftBar  = juce::Rectangle<int> (area.getX(),              area.getY(), barW, barH);
    auto rightBar = juce::Rectangle<int> (area.getX() + barW + 4,  area.getY(), barW, barH);

    g.setColour (theme.darkBar.darker (0.2f));
    g.fillRoundedRectangle (leftBar.toFloat(),  2.0f);
    g.fillRoundedRectangle (rightBar.toFloat(), 2.0f);

    auto drawBar = [&] (juce::Rectangle<int> bar, float peak, float hold)
    {
        const int fillH = juce::roundToInt ((float) bar.getHeight() * juce::jlimit (0.f, 1.f, peak));
        if (fillH > 0)
        {
            juce::ColourGradient grad (theme.accent.withAlpha (0.85f), 0.f, (float) bar.getBottom(),
                                        theme.accent.brighter (0.5f),  0.f, (float) bar.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() - fillH).toFloat(), 2.0f);
        }
        const int holdY = bar.getBottom() - juce::roundToInt ((float) bar.getHeight()
                           * juce::jlimit (0.f, 1.f, hold));
        g.setColour (theme.accent.brighter (0.6f).withAlpha (0.7f));
        g.fillRect (bar.getX(), holdY - 1, bar.getWidth(), 2);
    };

    drawBar (leftBar,  meterL, holdL);
    drawBar (rightBar, meterR, holdR);
}

// =============================================================================
//  Timer — meter + preset list refresh
// =============================================================================

void SfzDropdownPanel::timerCallback()
{
    // ── VU meters ─────────────────────────────────────────────────────────────
    const float newL = processor.sfzPeakL.load (std::memory_order_relaxed);
    const float newR = processor.sfzPeakR.load (std::memory_order_relaxed);
    if (newL > holdL) holdL = newL;
    if (newR > holdR) holdR = newR;
    holdL *= kHoldDecay;
    holdR *= kHoldDecay;
    meterL = newL;
    meterR = newR;

    // ── Refresh preset list from audio thread ─────────────────────────────────
    // getPresetList() swaps in any fresh data posted by the audio thread.
    presetList = processor.sfzPlayer.getPresetList();

    repaint();
}

// =============================================================================
//  Preset navigation
// =============================================================================

void SfzDropdownPanel::selectPreset (int delta)
{
    if (presetList.empty()) return;

    const int cur = processor.sfzPlayer.getCurrentPresetIndex();
    const int next = juce::jlimit (0, (int) presetList.size() - 1, cur + delta);

    if (next != cur)
    {
        processor.sfzPlayer.setPresetByIndex (next);

        // Reload key zones for the new preset if we have a loaded file.
        if (processor.sfzPlayer.isLoaded())
            reloadZones (processor.sfzPlayer.getLoadedFile());

        repaint();
    }
}

// =============================================================================
//  Mouse events
// =============================================================================

void SfzDropdownPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();

    // Load button
    if (loadBtnZone.contains (pos))
    {
        openFileChooser();
        return;
    }

    // Preset arrows
    if (presetDecBtn.contains (pos)) { selectPreset (-1); return; }
    if (presetIncBtn.contains (pos)) { selectPreset (+1); return; }

    // CH decrement / increment
    {
        auto area  = chZone;
        const int btnH = area.getHeight() - 4;
        const int btnW = area.getWidth() / 3;
        auto decBtn = juce::Rectangle<int> (area.getX(), area.getY() + 2, btnW - 1, btnH);
        auto incBtn = juce::Rectangle<int> (area.getX() + 2*btnW + 1, area.getY() + 2, btnW - 1, btnH);

        if (decBtn.contains (pos))
        {
            processor.sfzPlayer.setMidiChannel (juce::jmax (0, processor.sfzPlayer.getMidiChannel() - 1));
            repaint(); return;
        }
        if (incBtn.contains (pos))
        {
            processor.sfzPlayer.setMidiChannel (juce::jmin (16, processor.sfzPlayer.getMidiChannel() + 1));
            repaint(); return;
        }
    }

    // Knob drag start
    if (volZone.contains (pos))
    {
        activeKnob   = ActiveKnob::Volume;
        dragStartY   = pos.y;
        dragStartVal = volToNorm (processor.sfzPlayer.getVolume());
        return;
    }
    if (transZone.contains (pos))
    {
        activeKnob   = ActiveKnob::Transpose;
        dragStartY   = pos.y;
        dragStartVal = transToNorm (processor.sfzPlayer.getTranspose());
        return;
    }
}

void SfzDropdownPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (activeKnob == ActiveKnob::None) return;
    const float delta   = (float)(dragStartY - e.getPosition().y) / 120.0f;
    const float newNorm = juce::jlimit (0.f, 1.f, dragStartVal + delta);

    if (activeKnob == ActiveKnob::Volume)
        processor.sfzPlayer.setVolume (normToVol (newNorm));
    else
        processor.sfzPlayer.setTranspose (normToTrans (newNorm));
    repaint();
}

void SfzDropdownPanel::mouseUp (const juce::MouseEvent&)
{
    activeKnob = ActiveKnob::None;
}

void SfzDropdownPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    if (volZone.contains   (pos)) { processor.sfzPlayer.setVolume (1.0f); repaint(); }
    if (transZone.contains (pos)) { processor.sfzPlayer.setTranspose (0); repaint(); }
}

void SfzDropdownPanel::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& w)
{
    const auto  pos  = e.getPosition();
    const float step = w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f);

    if (nameZone.contains (pos) || presetLabel.contains (pos))
    {
        // Scroll wheel on preset picker → cycle presets
        if (w.deltaY > 0.05f)       selectPreset (+1);
        else if (w.deltaY < -0.05f) selectPreset (-1);
        return;
    }

    if (volZone.contains (pos))
    {
        const float n = juce::jlimit (0.f, 1.f, volToNorm (processor.sfzPlayer.getVolume()) + step);
        processor.sfzPlayer.setVolume (normToVol (n));
        repaint();
    }
    else if (transZone.contains (pos))
    {
        const float n = juce::jlimit (0.f, 1.f, transToNorm (processor.sfzPlayer.getTranspose()) + step);
        processor.sfzPlayer.setTranspose (normToTrans (n));
        repaint();
    }
}

// =============================================================================
//  File drag-and-drop — SF2 only
// =============================================================================

bool SfzDropdownPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).getFileExtension().toLowerCase() == ".sf2")
            return true;
    return false;
}

void SfzDropdownPanel::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        if (juce::File (f).getFileExtension().toLowerCase() == ".sf2")
        {
            juce::File file (f);
            processor.sfzPlayer.loadFile (file);
            reloadZones (file);
            repaint();
            return;
        }
    }
}

// =============================================================================
//  File chooser — SF2 only
// =============================================================================

void SfzDropdownPanel::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load SF2 SoundFont",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.sf2");

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

// =============================================================================
//  panelDidShow
// =============================================================================

void SfzDropdownPanel::panelDidShow()
{
    presetList = processor.sfzPlayer.getPresetList();

    if (processor.sfzPlayer.isLoaded())
        reloadZones (processor.sfzPlayer.getLoadedFile());
    resized();
    repaint();
}

// =============================================================================
//  Value mapping
// =============================================================================

float SfzDropdownPanel::volToNorm   (float linear) const { return juce::jlimit (0.f, 1.f, linear * 0.5f); }
float SfzDropdownPanel::normToVol   (float n)       const { return n * 2.0f; }
float SfzDropdownPanel::transToNorm (int semi)       const { return ((float) semi + 24.0f) / 48.0f; }
int   SfzDropdownPanel::normToTrans (float n)        const { return juce::roundToInt (n * 48.0f - 24.0f); }

// =============================================================================
//  Zone parsers (KeysPanel highlight visualisation)
// =============================================================================

static juce::Colour zoneColourDP (int index)
{
    static const juce::Colour palette[] = {
        juce::Colour (0xFF4FC3F7), juce::Colour (0xFF81C784), juce::Colour (0xFFFFB74D),
        juce::Colour (0xFFE57373), juce::Colour (0xFFBA68C8), juce::Colour (0xFF4DD0E1),
        juce::Colour (0xFFF06292), juce::Colour (0xFFA1887F),
    };
    return palette[index % 8];
}

std::vector<KeysPanel::Keyzone> SfzDropdownPanel::parseSfzZones (const juce::File& f)
{
    std::vector<KeysPanel::Keyzone> zones;
    const auto lines = juce::StringArray::fromLines (f.loadFileAsString());

    int  loKey = 0, hiKey = 127;
    bool inRegion = false;
    int  colIdx   = 0;

    auto flush = [&]
    {
        if (inRegion && hiKey >= loKey)
        {
            zones.push_back ({ loKey, hiKey, zoneColourDP (colIdx++) });
            loKey = 0; hiKey = 127;
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
            auto loRaw = line.indexOf ("lokey=");
            if (loRaw >= 0)
                loKey = juce::jlimit (0, 127,
                    line.substring (loRaw + 6).upToFirstOccurrenceOf (" ", false, false).trim().getIntValue());
            auto hiRaw = line.indexOf ("hikey=");
            if (hiRaw >= 0)
                hiKey = juce::jlimit (0, 127,
                    line.substring (hiRaw + 6).upToFirstOccurrenceOf (" ", false, false).trim().getIntValue());
            auto kRaw = line.indexOf ("key=");
            if (kRaw >= 0 && line.indexOf ("lokey=") < 0)
            {
                const int k = juce::jlimit (0, 127,
                    line.substring (kRaw + 4).upToFirstOccurrenceOf (" ", false, false).trim().getIntValue());
                loKey = hiKey = k;
            }
        }
    }
    flush();
    return zones;
}

std::vector<KeysPanel::Keyzone> SfzDropdownPanel::parseSf2Zones (const juce::File& f)
{
    std::vector<KeysPanel::Keyzone> zones;
    juce::FileInputStream stream (f);
    if (stream.failedToOpen()) return zones;

    char riff[4]; stream.read (riff, 4);
    if (juce::String::fromUTF8 (riff, 4) != "RIFF") return zones;
    stream.readInt();
    char sfbk[4]; stream.read (sfbk, 4);
    if (juce::String::fromUTF8 (sfbk, 4) != "sfbk") return zones;

    juce::MemoryBlock igenData;
    while (! stream.isExhausted())
    {
        char id[4]; if (stream.read (id, 4) < 4) break;
        const auto chunkId = juce::String::fromUTF8 (id, 4);
        const int  sz      = stream.readInt();
        if (chunkId == "LIST")
        {
            char listId[4]; stream.read (listId, 4);
            if (juce::String::fromUTF8 (listId, 4) == "pdta")
            {
                const int pdtaEnd = (int) stream.getPosition() + sz - 4;
                while (stream.getPosition() < pdtaEnd && ! stream.isExhausted())
                {
                    char sub[4]; if (stream.read (sub, 4) < 4) break;
                    const auto subId = juce::String::fromUTF8 (sub, 4);
                    const int  subSz = stream.readInt();
                    if (subId == "igen") { igenData.setSize ((size_t) subSz); stream.read (igenData.getData(), subSz); break; }
                    stream.skipNextBytes (subSz);
                }
                break;
            }
            stream.skipNextBytes (sz - 4);
        }
        else stream.skipNextBytes (sz);
    }

    if (igenData.isEmpty()) return zones;

    const size_t numRecs = igenData.getSize() / 4;
    const auto*  data    = static_cast<const uint8_t*> (igenData.getData());
    int loKey = 0, hiKey = 127;
    bool hasKey = false;
    int  colIdx = 0;

    for (size_t i = 0; i < numRecs; ++i)
    {
        const uint16_t oper = (uint16_t)(data[i*4] | (data[i*4+1] << 8));
        if (oper == 43) { loKey = data[i*4+2]; hiKey = data[i*4+3]; hasKey = true; }
        else if (oper == 0)
        {
            if (hasKey && hiKey >= loKey)
                zones.push_back ({ loKey, hiKey, zoneColourDP (colIdx++) });
            loKey = 0; hiKey = 127; hasKey = false;
        }
    }

    std::sort (zones.begin(), zones.end(), [] (auto& a, auto& b) { return a.loKey < b.loKey; });
    zones.erase (std::unique (zones.begin(), zones.end(),
                               [] (auto& a, auto& b) { return a.loKey == b.loKey && a.hiKey == b.hiKey; }),
                 zones.end());
    for (size_t i = 0; i < zones.size(); ++i)
        zones[i].colour = zoneColourDP ((int) i);

    return zones;
}

void SfzDropdownPanel::reloadZones (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    // Only SF2 for this panel; sfz kept for compatibility in case zones are needed.
    auto zones = (ext == ".sfz") ? parseSfzZones (f)
               : (ext == ".sf2") ? parseSf2Zones (f)
               : std::vector<KeysPanel::Keyzone>{};
    keysPanel.setKeyzones (zones);
    if (! zones.empty())
        keysPanel.autoScrollToZones();
}
