#include "ActionPanel.h"
#include "AutoChopPanel.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

ActionPanel::ActionPanel (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn,
                       &trimBtn, &midiSelectBtn, &shortcutsBtn })
    {
        addAndMakeVisible (btn);
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    // snapBtn removed — snap-to-zero-crossing is now always active (hardcoded)
    // FIL/WA/CH buttons moved to HeaderBar — keep members for state sync only

    addSliceBtn.onClick = [this] { waveformView.setSliceDrawMode (! waveformView.isSliceDrawModeActive()); repaint(); };

    lazyChopBtn.onClick = [this] {
        const bool wasActive = processor.lazyChop.isActive();
        DysektProcessor::Command cmd;
        cmd.type = wasActive ? DysektProcessor::CmdLazyChopStop : DysektProcessor::CmdLazyChopStart;
        processor.pushCommand (cmd);

        // MIDI FOLLOW auto-on: enabling MIDI Slice automatically turns on MIDI Follow
        if (! wasActive)
            processor.midiSelectsSlice.store (true);

        repaint();
    };

    trimBtn.onClick = [this] { toggleTrimMode(); };

    shortcutsBtn.onClick = [this] { if (onShortcutsToggle) onShortcutsToggle(); };
    shortcutsBtn.setTooltip ("Keyboard Shortcuts (⌘?)");

    midiSelectBtn.onClick = [this] {
        bool ns = ! processor.midiSelectsSlice.load();
        processor.midiSelectsSlice.store (ns);
        updateMidiButtonAppearance (ns); repaint();
    };

    browserBtn.onClick    = [this] { browserActive    = ! browserActive;    if (onBrowserToggle)    onBrowserToggle();    updateToggleBtn (browserBtn,    browserActive); };
    waveBtn.onClick       = [this] { waveActive       = ! waveActive;       if (onWaveToggle)       onWaveToggle();       updateToggleBtn (waveBtn,       waveActive); };
    chromaticBtn.onClick  = [this] { chromaticActive  = ! chromaticActive;  if (onChromaticToggle)  onChromaticToggle();  updateToggleBtn (chromaticBtn,  chromaticActive); };

    addSliceBtn.setTooltip ("Add Slice (A / hold Alt)");
    lazyChopBtn.setTooltip ("MIDI Slice — chop by incoming MIDI notes (L)");

    addSliceBtn.setButtonText ("ADD SLICE");
    // lazyChopBtn label intentionally empty — PLAY/STOP icon drawn in paintOverChildren
    lazyChopBtn.setButtonText ("");
    trimBtn.setTooltip     ("Trim - crop sample to a selected region");

    browserBtn.setTooltip  ("Toggle File Browser");
    waveBtn.setTooltip     ("Toggle Soft Waveform");
    chromaticBtn.setTooltip ("Chromatic Mode - play selected slice across full keyboard");

    updateMidiButtonAppearance (false);
}

ActionPanel::~ActionPanel() = default;

void ActionPanel::updateToggleBtn (juce::TextButton& btn, bool active)
{
    if (active)
    {
        btn.setColour (juce::TextButton::buttonColourId,  getTheme().accent.withAlpha (0.2f));
        btn.setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn.setColour (juce::TextButton::textColourOffId, getTheme().accent);
    }
    else
    {
        btn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn.setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn.setColour (juce::TextButton::textColourOffId, getTheme().accent);
    }
}

void ActionPanel::toggleAutoChop()
{
    // Auto Chop removed per Fix #3 — kept to avoid breaking PluginEditor key handler
}

void ActionPanel::toggleTrimMode()
{
    // Trim mode lifecycle is owned entirely by PluginEditor.
    // ActionPanel just fires the callback so the editor can handle
    // enterTrimMode(), TrimDialog placement, and onTrimApplied wiring.
    if (onTrimToggle)
        onTrimToggle();
}

void ActionPanel::resized()
{
    const int gap    = 4;
    const int h      = getHeight();
    const int thinW  = 30;   // MIDI select icon button (hard right)
    const int trimW  = 42;   // TRIM fixed width
    const int addW   = 80;   // ADD SLICE natural width
    const int midiW  = 34;   // MIDI SLICE natural width (icon only)

    // MIDI icon hard right, then TRIM, then ADD SLICE and MIDI SLICE left-aligned
    int right = getWidth();
    midiSelectBtn.setBounds (right - thinW, 0, thinW, h);
    right -= thinW + gap;
    trimBtn.setBounds (right - trimW, 0, trimW, h);

    // ADD SLICE and MIDI SLICE left-aligned with natural widths
    addSliceBtn.setBounds (0,            0, addW, h);
    lazyChopBtn.setBounds (addW + gap,   0, midiW, h);

    shortcutsBtn.setVisible (false);
    browserBtn.setVisible   (false);
    waveBtn.setVisible      (false);
    chromaticBtn.setVisible (false);
}

void ActionPanel::paint (juce::Graphics& g)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }
    updateMidiButtonAppearance (processor.midiSelectsSlice.load());

    if (waveformView.isSliceDrawModeActive())
    { g.setColour (getTheme().accent.withAlpha (0.25f)); g.fillRect (addSliceBtn.getBounds()); }

    if (processor.lazyChop.isActive())
    {
        g.setColour (juce::Colours::red.withAlpha (0.25f));
        g.fillRect (lazyChopBtn.getBounds());
    }

    const bool trimActive = waveformView.isTrimModeActive();
    trimBtn.setColour (juce::TextButton::buttonColourId,
                       trimActive ? getTheme().accent.withAlpha (0.2f) : getTheme().button);
    trimBtn.setColour (juce::TextButton::textColourOnId,
                       trimActive ? getTheme().accent : getTheme().foreground);
    trimBtn.setColour (juce::TextButton::textColourOffId,
                       trimActive ? getTheme().accent : getTheme().foreground);
    if (trimActive)
    { g.setColour (getTheme().accent.withAlpha (0.15f)); g.fillRect (trimBtn.getBounds()); }
}

void ActionPanel::updateMidiButtonAppearance (bool active)
{
    auto col = active ? getTheme().accent : getTheme().foreground;
    auto bg  = active ? getTheme().accent.withAlpha (0.2f) : getTheme().button;
    midiSelectBtn.setColour (juce::TextButton::textColourOnId,  col);
    midiSelectBtn.setColour (juce::TextButton::textColourOffId, col);
    midiSelectBtn.setColour (juce::TextButton::buttonColourId,  bg);
}

// snapBtn removed — updateSnapButtonAppearance no longer needed

void ActionPanel::paintOverChildren (juce::Graphics& g)
{
    // ── PLAY / STOP icon for lazyChopBtn ─────────────────────────────────
    {
        const bool lazyActive = processor.lazyChop.isActive();
        auto b   = lazyChopBtn.getBounds().toFloat();
        float cx = b.getCentreX();
        float cy = b.getCentreY();

        if (lazyActive)
        {
            // STOP: solid red square
            const float sq = 7.0f;
            g.setColour (juce::Colours::red.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (cx - sq * 0.5f, cy - sq * 0.5f, sq, sq));
        }
        else
        {
            // PLAY: solid green right-pointing triangle
            const float tw = 9.0f;
            const float th = 10.0f;
            juce::Path tri;
            tri.addTriangle (cx - tw * 0.4f, cy - th * 0.5f,
                             cx - tw * 0.4f, cy + th * 0.5f,
                             cx + tw * 0.6f, cy);
            g.setColour (juce::Colours::limegreen.withAlpha (0.9f));
            g.fillPath (tri);
        }
    }

    // ── 5-pin DIN MIDI connector icon for midiSelectBtn ──────────────────
    // Drawn entirely as filled/stroked paths — no background erase tricks,
    // so the full icon is always visible regardless of active state.
    {
        bool  active = processor.midiSelectsSlice.load();
        auto  col    = active ? getTheme().accent : getTheme().foreground.withAlpha (0.85f);

        auto  bRect  = midiSelectBtn.getBounds();
        g.saveState();
        g.reduceClipRegion (bRect);

        float cx = bRect.getCentreX();
        float cy = bRect.getCentreY();

        // Scale so the full icon fits within the button with 3px margin each side
        const float maxR   = juce::jmin (bRect.getWidth(), bRect.getHeight()) * 0.5f - 3.0f;
        const float scale  = maxR / 8.5f;   // 8.5f is the natural outerR

        const float outerR = 8.5f * scale;
        const float arcR   = 5.0f * scale;
        const float pinR   = 1.4f * scale;

        // Full outer circle — drawn as a complete ellipse (no erase needed)
        g.setColour (col);
        g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 1.2f);

        // D-shell flat bottom: draw the chord line, then fill the lower segment
        // using the button background colour via a filled path (no raw fillRect erase)
        const float flatY = cy + outerR * 0.53f;   // chord cuts at ~53% radius down

        // Filled pie/segment below chord — masks the lower arc cleanly
        {
            juce::Path mask;
            mask.startNewSubPath (cx - outerR - 1.0f, flatY);
            mask.lineTo          (cx + outerR + 1.0f, flatY);
            mask.lineTo          (cx + outerR + 1.0f, cy + outerR + 1.0f);
            mask.lineTo          (cx - outerR - 1.0f, cy + outerR + 1.0f);
            mask.closeSubPath();
            // Fill with button bg to erase the lower arc of the circle
            g.setColour (active ? getTheme().accent.withAlpha (0.2f) : getTheme().button);
            g.fillPath  (mask);
        }

        // Flat chord line
        g.setColour (col);
        g.drawLine (cx - outerR, flatY, cx + outerR, flatY, 1.2f);

        // 5 pins in standard DIN-5 arrangement (all in upper half)
        struct Pin { float x, y; };
        const float pinCY = cy - outerR * 0.12f;  // centre the pin cluster slightly above mid
        Pin pins[] = {
            { cx - arcR * 0.95f, pinCY - arcR * 0.31f },
            { cx + arcR * 0.95f, pinCY - arcR * 0.31f },
            { cx,                pinCY - arcR           },
            { cx - arcR * 0.59f, pinCY + arcR * 0.81f },
            { cx + arcR * 0.59f, pinCY + arcR * 0.81f },
        };
        g.setColour (col);
        for (auto& pin : pins)
        {
            // Only draw pins that are above the flat line
            if (pin.y - pinR < flatY)
                g.fillEllipse (pin.x - pinR, pin.y - pinR, pinR * 2.0f, pinR * 2.0f);
        }

        g.restoreState();
    }
}
