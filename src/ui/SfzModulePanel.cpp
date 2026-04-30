// =============================================================================
//  SfzModulePanel.cpp
// =============================================================================
#include "SfzModulePanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include <set>
#include <cmath>

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

    // Bottom portion — keyboard (zone table + piano)
    const int kbH = juce::jmax (90, (area.getHeight() * 3) / 5);
    auto kbArea = area.removeFromBottom (kbH);
    keysPanel.setBounds (kbArea);
    area.removeFromBottom (4); // gap

    // ADSR row — sits between top strip and keyboard
    constexpr int kAdsrRowH  = 68;
    constexpr int kAdsrKnobW = 52;
    auto adsrArea = area.removeFromBottom (kAdsrRowH);
    area.removeFromBottom (4); // gap above ADSR row

    // Centre the four knobs in the ADSR row
    const int totalKnobW = kAdsrKnobW * 4 + kPad * 3;
    const int adsrX = adsrArea.getX() + (adsrArea.getWidth() - totalKnobW) / 2;
    atkZone = juce::Rectangle<int> (adsrX,                               adsrArea.getY(), kAdsrKnobW, kAdsrRowH);
    decZone = juce::Rectangle<int> (adsrX + (kAdsrKnobW + kPad),        adsrArea.getY(), kAdsrKnobW, kAdsrRowH);
    susZone = juce::Rectangle<int> (adsrX + (kAdsrKnobW + kPad) * 2,   adsrArea.getY(), kAdsrKnobW, kAdsrRowH);
    relZone = juce::Rectangle<int> (adsrX + (kAdsrKnobW + kPad) * 3,   adsrArea.getY(), kAdsrKnobW, kAdsrRowH);

    // Top control strip — left to right:
    //   [LOAD] [VOL knob] [TRANS knob] [name label ... ] [meter] [status]

    // LOAD button — leftmost
    loadBtnZone = area.removeFromLeft (kLoadBtnW).withSizeKeepingCentre (kLoadBtnW, kLoadBtnH);
    area.removeFromLeft (kPad);

    // VOL knob
    constexpr int kKnobWNarrow = 48;
    volZone   = area.removeFromLeft (kKnobWNarrow);
    area.removeFromLeft (4);
    transZone = area.removeFromLeft (kKnobWNarrow);
    area.removeFromLeft (kPad);

    // Status pill — rightmost
    statusZone = area.removeFromRight (60);
    area.removeFromRight (4);

    // VU meter — second from right
    meterZone = area.removeFromRight (kMeterW);
    area.removeFromRight (kPad);

    chZone   = {};
    nameZone = area;
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

    // ── ADSR row background ───────────────────────────────────────────────────
    {
        // Span all four knob zones
        auto rowBounds = atkZone.getUnion (relZone).expanded (kPad / 2, 2).toFloat();
        g.setColour (theme.darkBar.brighter (0.04f));
        g.fillRoundedRectangle (rowBounds, 3.0f);
        g.setColour (theme.foreground.withAlpha (0.07f));
        g.drawRoundedRectangle (rowBounds.reduced (0.5f), 3.0f, 1.0f);
    }

    // ── ADSR knobs ───────────────────────────────────────────────────────────
    {
        const bool sfzLoaded = processor.sfzPlayer.isLoaded();
        const float alpha    = sfzLoaded ? 1.0f : 0.35f;

        auto fmtTime = [] (float sec) -> juce::String
        {
            if (sec < 1.0f) return juce::String (juce::roundToInt (sec * 1000.0f)) + " ms";
            return juce::String (sec, 2) + " s";
        };

        // Normalise to 0-1 for drawKnob arc
        const float atkNorm = processor.sfzPlayer.getSfzAttack()  / 30.0f;
        const float decNorm = processor.sfzPlayer.getSfzDecay()   / 30.0f;
        const float susNorm = processor.sfzPlayer.getSfzSustain() / 100.0f;
        const float relNorm = processor.sfzPlayer.getSfzRelease() / 60.0f;

        // Draw each knob — highlight if MIDI-learned
        auto drawAdsrKnob = [&] (juce::Rectangle<int> zone, float norm,
                                  const juce::String& label, const juce::String& valStr,
                                  int fieldId)
        {
            const bool learned = processor.midiLearn.isMapped (fieldId);
            const bool armed   = processor.midiLearn.getArmedSlot() == fieldId;
            g.saveState();
            g.setOpacity (alpha);
            drawKnob (g, zone, norm, label, valStr);
            g.restoreState();
            // Accent ring if learned or armed
            if (learned || armed)
            {
                const auto& th = getTheme();
                const auto  rc = zone.toFloat().reduced (2.0f);
                g.setColour ((armed ? th.accent.brighter (0.4f) : th.accent).withAlpha (0.55f));
                g.drawRoundedRectangle (rc, 3.0f, 1.5f);
            }
        };

        drawAdsrKnob (atkZone, atkNorm, "ATK", fmtTime (processor.sfzPlayer.getSfzAttack()),  DysektProcessor::FieldSfzAttack);
        drawAdsrKnob (decZone, decNorm, "DEC", fmtTime (processor.sfzPlayer.getSfzDecay()),   DysektProcessor::FieldSfzDecay);
        drawAdsrKnob (susZone, susNorm, "SUS", juce::String (juce::roundToInt (processor.sfzPlayer.getSfzSustain())) + "%", DysektProcessor::FieldSfzSustain);
        drawAdsrKnob (relZone, relNorm, "REL", fmtTime (processor.sfzPlayer.getSfzRelease()), DysektProcessor::FieldSfzRelease);
    }

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

// ── MIDI Learn context menu ───────────────────────────────────────────────────

void SfzModulePanel::showMidiLearnMenu (int fieldId, juce::Point<int> screenPos)
{
    const bool mapped = processor.midiLearn.isMapped (fieldId);
    juce::PopupMenu menu;
    menu.addItem (1, "Learn MIDI CC");
    if (mapped)
        menu.addItem (2, "Clear (" + processor.midiLearn.getLabelText (fieldId) + ")");
    menu.addSeparator();
    menu.addItem (1000, "Open MIDI Learn Dialog...");

    auto* topLvl = getTopLevelComponent();
    float ms = DysektLookAndFeel::getMenuScale();
    menu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetScreenArea (juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1))
            .withParentComponent (topLvl)
            .withStandardItemHeight ((int)(24 * ms)),
        [this, fieldId] (int result)
        {
            if (result == 1)       { processor.midiLearn.armLearn (fieldId);     repaint(); }
            else if (result == 2)  { processor.midiLearn.clearMapping (fieldId); repaint(); }
            else if (result == 1000)
            {
                if (auto* editor = findParentComponentOfClass<DysektEditor>())
                    editor->keyPressed (juce::KeyPress ('M', juce::ModifierKeys(), 0));
            }
        });
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

    // Right-click on ADSR knobs → MIDI Learn menu
    if (e.mods.isRightButtonDown())
    {
        if (atkZone.contains (pos)) { showMidiLearnMenu (DysektProcessor::FieldSfzAttack,  e.getScreenPosition()); return; }
        if (decZone.contains (pos)) { showMidiLearnMenu (DysektProcessor::FieldSfzDecay,   e.getScreenPosition()); return; }
        if (susZone.contains (pos)) { showMidiLearnMenu (DysektProcessor::FieldSfzSustain, e.getScreenPosition()); return; }
        if (relZone.contains (pos)) { showMidiLearnMenu (DysektProcessor::FieldSfzRelease, e.getScreenPosition()); return; }
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
    if (atkZone.contains (pos))
    {
        activeKnob   = ActiveKnob::Attack;
        dragStartY   = pos.y;
        dragStartVal = processor.sfzPlayer.getSfzAttack() / 30.0f;
        return;
    }
    if (decZone.contains (pos))
    {
        activeKnob   = ActiveKnob::Decay;
        dragStartY   = pos.y;
        dragStartVal = processor.sfzPlayer.getSfzDecay() / 30.0f;
        return;
    }
    if (susZone.contains (pos))
    {
        activeKnob   = ActiveKnob::Sustain;
        dragStartY   = pos.y;
        dragStartVal = processor.sfzPlayer.getSfzSustain() / 100.0f;
        return;
    }
    if (relZone.contains (pos))
    {
        activeKnob   = ActiveKnob::Release;
        dragStartY   = pos.y;
        dragStartVal = processor.sfzPlayer.getSfzRelease() / 60.0f;
        return;
    }
}

void SfzModulePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (activeKnob == ActiveKnob::None) return;

    const float delta = (float)(dragStartY - e.getPosition().y) / 150.0f;
    const float newNorm = juce::jlimit (0.0f, 1.0f, dragStartVal + delta);

    switch (activeKnob)
    {
        case ActiveKnob::Volume:
            processor.sfzPlayer.setVolume (normToVol (newNorm));    break;
        case ActiveKnob::Transpose:
            processor.sfzPlayer.setTranspose (normToTrans (newNorm)); break;
        case ActiveKnob::Attack:
            processor.sfzPlayer.setSfzAttack  (newNorm * 30.0f);   break;
        case ActiveKnob::Decay:
            processor.sfzPlayer.setSfzDecay   (newNorm * 30.0f);   break;
        case ActiveKnob::Sustain:
            processor.sfzPlayer.setSfzSustain (newNorm * 100.0f);  break;
        case ActiveKnob::Release:
            processor.sfzPlayer.setSfzRelease (newNorm * 60.0f);   break;
        default: break;
    }
    repaint();
}

void SfzModulePanel::mouseUp (const juce::MouseEvent&)
{
    activeKnob = ActiveKnob::None;
}

void SfzModulePanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    if (volZone.contains (pos))   { processor.sfzPlayer.setVolume (1.0f);         repaint(); }
    if (transZone.contains (pos)) { processor.sfzPlayer.setTranspose (0);          repaint(); }
    if (atkZone.contains (pos))   { processor.sfzPlayer.setSfzAttack  (0.005f);   repaint(); }
    if (decZone.contains (pos))   { processor.sfzPlayer.setSfzDecay   (0.1f);     repaint(); }
    if (susZone.contains (pos))   { processor.sfzPlayer.setSfzSustain (100.0f);   repaint(); }
    if (relZone.contains (pos))   { processor.sfzPlayer.setSfzRelease (0.05f);    repaint(); }
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
    else if (atkZone.contains (pos))
    {
        const float n = juce::jlimit (0.0f, 1.0f, processor.sfzPlayer.getSfzAttack() / 30.0f + step);
        processor.sfzPlayer.setSfzAttack (n * 30.0f);
        repaint();
    }
    else if (decZone.contains (pos))
    {
        const float n = juce::jlimit (0.0f, 1.0f, processor.sfzPlayer.getSfzDecay() / 30.0f + step);
        processor.sfzPlayer.setSfzDecay (n * 30.0f);
        repaint();
    }
    else if (susZone.contains (pos))
    {
        const float n = juce::jlimit (0.0f, 1.0f, processor.sfzPlayer.getSfzSustain() / 100.0f + step);
        processor.sfzPlayer.setSfzSustain (n * 100.0f);
        repaint();
    }
    else if (relZone.contains (pos))
    {
        const float n = juce::jlimit (0.0f, 1.0f, processor.sfzPlayer.getSfzRelease() / 60.0f + step);
        processor.sfzPlayer.setSfzRelease (n * 60.0f);
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

    int   loKey      = 0;
    int   hiKey      = 127;
    int   rootPitch  = -1;
    float volDb      = -7.0f;
    float panVal     = 0.0f;
    float tuneVal    = 0.0f;
    float releaseSec = 0.664f;
    bool  inRegion   = false;
    int   colIdx     = 0;
    juce::String sampleName;

    auto flush = [&]
    {
        if (inRegion && hiKey >= loKey)
        {
            KeysPanel::Keyzone z;
            z.loKey      = loKey;
            z.hiKey      = hiKey;
            z.rootPitch  = rootPitch;
            z.volDb      = volDb;
            z.pan        = panVal;
            z.tuneCents  = tuneVal;
            z.releaseSec = releaseSec;
            z.colour     = zoneColour (colIdx++);
            z.name       = sampleName;
            z.isSfz      = true;   // SFZ zones are editable
            zones.push_back (z);
        }
        loKey = 0; hiKey = 127; rootPitch = -1;
        volDb = -7.0f; panVal = 0.0f; tuneVal = 0.0f; releaseSec = 0.664f;
        sampleName = {};
        inRegion = false;
    };

    for (auto line : lines)
    {
        line = line.trim().toLowerCase();
        if (line.startsWith ("<region>")) { flush(); inRegion = true; loKey = 0; hiKey = 127; }
        else if (line.startsWith ("<group>") || line.startsWith ("<global>")) flush();

        if (inRegion)
        {
            // Generic float extractor
            auto getFloat = [&] (const juce::String& key) -> float
            {
                int pos = line.indexOf (key + "=");
                if (pos < 0) return -999999.f;
                return line.substring (pos + key.length() + 1)
                           .trimStart()
                           .upToFirstOccurrenceOf (" ",  false, false)
                           .upToFirstOccurrenceOf ("	", false, false)
                           .getFloatValue();
            };

            // lokey / hikey
            auto loRaw = line.indexOf ("lokey=");
            if (loRaw >= 0)
            {
                auto s = line.substring (loRaw + 6)
                             .upToFirstOccurrenceOf (" ", false, false).trim();
                loKey = juce::jlimit (0, 127, s.getIntValue());
            }
            auto hiRaw = line.indexOf ("hikey=");
            if (hiRaw >= 0)
            {
                auto s = line.substring (hiRaw + 6)
                             .upToFirstOccurrenceOf (" ", false, false).trim();
                hiKey = juce::jlimit (0, 127, s.getIntValue());
            }
            // key= shorthand
            auto kRaw = line.indexOf ("key=");
            if (kRaw >= 0 && line.indexOf ("lokey=") < 0)
            {
                auto s = line.substring (kRaw + 4)
                             .upToFirstOccurrenceOf (" ", false, false).trim();
                loKey = hiKey = juce::jlimit (0, 127, s.getIntValue());
            }

            // pitch_keycenter -> rootPitch
            auto pkRaw = line.indexOf ("pitch_keycenter=");
            if (pkRaw >= 0)
            {
                auto s = line.substring (pkRaw + 16)
                             .upToFirstOccurrenceOf (" ", false, false).trim();
                rootPitch = juce::jlimit (0, 127, s.getIntValue());
            }

            // volume (dB in SFZ)
            float v = getFloat ("volume");
            if (v > -999998.f) volDb = v;

            // pan (-100..+100 in SFZ percent, stored as -1..+1)
            float p = getFloat ("pan");
            if (p > -999998.f) panVal = juce::jlimit (-1.0f, 1.0f, p / 100.0f);

            // tune (cents, -100..+100 in SFZ)
            float t = getFloat ("tune");
            if (t > -999998.f) tuneVal = juce::jlimit (-100.0f, 100.0f, t);

            // ampeg_release (seconds)
            float rel = getFloat ("ampeg_release");
            if (rel > -999998.f) releaseSec = juce::jlimit (0.0f, 60.0f, rel);

            // sample name — value runs to next opcode (word=) or end of line.
            // Cannot split on the first space because filenames commonly contain spaces
            // (e.g. "Blip Bass.wav", "Sine E0.wav").
            {
                int smpPos = line.indexOf ("sample=");
                if (smpPos >= 0)
                {
                    auto rest = line.substring (smpPos + 7);   // everything after "sample="

                    // Find where the next opcode starts: <space>keyword=
                    int endPos = rest.length();
                    for (int ci = 0; ci < rest.length() - 1; ++ci)
                    {
                        if (rest[ci] == ' ')
                        {
                            // Look ahead for a bare opcode keyword (letters/digits/underscores then '=')
                            int j = ci + 1;
                            while (j < rest.length() &&
                                   (juce::CharacterFunctions::isLetterOrDigit (rest[j]) || rest[j] == '_'))
                                ++j;
                            if (j < rest.length() && rest[j] == '=')
                            {
                                endPos = ci;
                                break;
                            }
                        }
                    }

                    auto s = rest.substring (0, endPos).trim();

                    // Strip directory path separators, remove extension
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
    juce::MemoryBlock shdrData;

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
                // Scan sub-chunks for "igen" and "shdr"
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
                    }
                    else if (subId == "shdr")
                    {
                        // Each SHDR record is 46 bytes; first 20 bytes = sample name
                        shdrData.setSize ((size_t) subSz);
                        stream.read (shdrData.getData(), subSz);
                    }
                    else
                    {
                        stream.skipNextBytes (subSz);
                    }
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

    // Helper: extract sample name from SHDR by sample index (record 46 bytes, name at offset 0, 20 chars)
    auto shdrName = [&] (int sampleIdx) -> juce::String
    {
        if (shdrData.isEmpty()) return {};
        const int offset = sampleIdx * 46;
        if (offset + 20 > (int) shdrData.getSize()) return {};
        const char* raw = static_cast<const char*> (shdrData.getData()) + offset;
        auto name = juce::String::fromUTF8 (raw, 20).trimCharactersAtEnd ("\0 ");
        // Strip stereo suffixes like " L" / " R" / "_L" / "_R" — keep base name
        if (name.endsWithIgnoreCase (" L") || name.endsWithIgnoreCase ("_L"))
            name = name.dropLastCharacters (2).trim();
        else if (name.endsWithIgnoreCase (" R") || name.endsWithIgnoreCase ("_R"))
            name = name.dropLastCharacters (2).trim();
        return name;
    };

    // isUsefulName: rejects auto-generated names like "alphabet.sf2 001 L"
    auto isUsefulName = [&] (const juce::String& n) -> bool
    {
        if (n.isEmpty()) return false;
        // Reject if name is just the filename + a number pattern
        if (n.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._-()"))
        {
            // If it contains the sf2 filename stem it's auto-generated
            const auto stem = f.getFileNameWithoutExtension().toLowerCase();
            if (n.toLowerCase().startsWith (stem)) return false;
        }
        return true;
    };

    // Each IGEN record is 4 bytes: sfGenOper (uint16) + genAmount (lo, hi bytes)
    // SF2 generator opcodes used:
    //   43 = keyRange          (lo=loKey, hi=hiKey)
    //   58 = overridingRootKey (lo=rootMidi, hi=unused)
    //   48 = initialAttenuation (lo|hi as int16 centibels; dB = -val/10.0)
    //   38 = releaseVolEnv      (lo|hi as int16 timecents; s = 2^(val/1200))
    //
    // Strategy: accumulate gens per zone; flush when a new keyRange (43) appears.
    // Each instrument zone in SF2 ends implicitly when the next one begins — the
    // ibag pointers encode boundaries, but we approximate with keyRange as delimiter.
    const size_t numRecs = igenData.getSize() / 4;
    const auto*  data    = static_cast<const uint8_t*> (igenData.getData());

    // Current zone accumulator
    int   zLoKey     = -1;
    int   zHiKey     = -1;
    int   zRootPitch = -1;
    float zVolDb     = -7.0f;
    float zPan       = 0.0f;
    float zTune      = 0.0f;
    float zRelSec    = 0.664f;
    int   zSampleIdx = -1;   // SHDR sample index from gen 53 (sampleID)

    std::set<std::pair<int,int>> seen;
    int colIdx = 0;

    auto flushZone = [&]
    {
        if (zLoKey < 0 || zHiKey < zLoKey) return;
        auto key = std::make_pair (zLoKey, zHiKey);
        if (seen.find (key) == seen.end())
        {
            seen.insert (key);
            KeysPanel::Keyzone z;
            z.loKey      = zLoKey;
            z.hiKey      = zHiKey;
            z.rootPitch  = zRootPitch;
            z.volDb      = zVolDb;
            z.pan        = zPan;
            z.tuneCents  = zTune;
            z.releaseSec = zRelSec;
            z.colour     = zoneColour (colIdx);
            z.isSfz      = false;

            // Use SHDR name if we have a valid sample index and the name is meaningful
            juce::String name;
            if (zSampleIdx >= 0)
                name = shdrName (zSampleIdx);
            if (! isUsefulName (name))
                name = "Zone " + juce::String (colIdx + 1);
            z.name = name;

            zones.push_back (z);
            ++colIdx;
        }
        zLoKey = -1; zHiKey = -1; zRootPitch = -1;
        zVolDb = -7.0f; zPan = 0.0f; zTune = 0.0f; zRelSec = 0.664f;
        zSampleIdx = -1;
    };

    for (size_t i = 0; i < numRecs; ++i)
    {
        const uint16_t oper = (uint16_t)(data[i*4] | (data[i*4+1] << 8));
        const uint8_t  lo   = data[i*4 + 2];
        const uint8_t  hi   = data[i*4 + 3];

        if (oper == 43)  // keyRange — start new zone
        {
            flushZone();
            zLoKey = (int) lo;
            zHiKey = (int) hi;
        }
        else if (oper == 58 && zLoKey >= 0)  // overridingRootKey
        {
            zRootPitch = juce::jlimit (0, 127, (int) lo);
        }
        else if (oper == 48 && zLoKey >= 0)  // initialAttenuation (centibels)
        {
            const int16_t cb = (int16_t)((uint16_t)(lo | (hi << 8)));
            zVolDb = -(float) cb / 10.0f;   // centibels → dB (attenuation → negative gain)
        }
        else if (oper == 17 && zLoKey >= 0)  // pan (0.1% units: -500=L, 0=C, +500=R)
        {
            const int16_t raw = (int16_t)((uint16_t)(lo | (hi << 8)));
            zPan = juce::jlimit (-1.0f, 1.0f, (float) raw / 500.0f);
        }
        else if (oper == 52 && zLoKey >= 0)  // fineTune (cents, -99..+99)
        {
            const int16_t raw = (int16_t)((uint16_t)(lo | (hi << 8)));
            zTune = juce::jlimit (-100.0f, 100.0f, (float) raw);
        }
        else if (oper == 53 && zLoKey >= 0)  // sampleID — index into SHDR table
        {
            zSampleIdx = (int)((uint16_t)(lo | (hi << 8)));
        }
        else if (oper == 38 && zLoKey >= 0)  // releaseVolEnv (timecents)
        {
            const int16_t tc = (int16_t)((uint16_t)(lo | (hi << 8)));
            zRelSec = juce::jlimit (0.0f, 60.0f,
                          (float) std::pow (2.0, (double) tc / 1200.0));
        }
    }
    flushZone();

    // Sort by loKey for display and re-assign colours
    std::sort (zones.begin(), zones.end(),
               [] (auto& a, auto& b) { return a.loKey < b.loKey; });

    for (size_t i = 0; i < zones.size(); ++i)
        zones[i].colour = zoneColour ((int) i);

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

    // Wire zone-edit callback → SfzPlayer real-time OSC (SFZ only; SF2 is no-op)
    keysPanel.onZoneChanged = [this] (int zoneIndex, float volDb, float pan, float tuneCents)
    {
        processor.sfzPlayer.setZoneVolume (zoneIndex, volDb);
        processor.sfzPlayer.setZonePan    (zoneIndex, pan);
        processor.sfzPlayer.setZoneTune   (zoneIndex, tuneCents);
    };
}
