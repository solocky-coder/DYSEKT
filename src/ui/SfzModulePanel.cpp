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
    : processor (p)
{
    startTimerHz (30);
}

SfzModulePanel::~SfzModulePanel() = default;

// ── Layout ────────────────────────────────────────────────────────────────────

void SfzModulePanel::resized()
{
    auto area = getLocalBounds().reduced (kPad);

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
    auto bounds = getLocalBounds().toFloat();
    g.setColour (theme.darkBar.darker (0.25f));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (theme.accent.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

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
            processor.sfzPlayer.loadFile (juce::File (f));
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
                repaint();
            }
        });
}

// ── panelDidShow ──────────────────────────────────────────────────────────────

void SfzModulePanel::panelDidShow()
{
    repaint();
}

// ── Value mapping helpers ─────────────────────────────────────────────────────

float SfzModulePanel::volToNorm   (float linear) const { return juce::jlimit (0.0f, 1.0f, linear * 0.5f); }
float SfzModulePanel::normToVol   (float n)      const { return n * 2.0f; }
float SfzModulePanel::transToNorm (int semi)      const { return ((float) semi + 24.0f) / 48.0f; }
int   SfzModulePanel::normToTrans (float n)       const { return juce::roundToInt (n * 48.0f - 24.0f); }
