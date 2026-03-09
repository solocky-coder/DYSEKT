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
                       &trimBtn, &shortcutsBtn })
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
    shortcutsBtn.setTooltip ("Keyboard Shortcuts (⌘?)");    addSliceBtn.setTooltip ("Add Slice (A / hold Alt)");
    lazyChopBtn.setTooltip ("MIDI Slice — chop by incoming MIDI notes (L)");

    addSliceBtn.setButtonText ("ADD SLICE");
    // lazyChopBtn label intentionally empty — PLAY/STOP icon drawn in paintOverChildren
    lazyChopBtn.setButtonText ("");
    trimBtn.setTooltip     ("Trim - crop sample to a selected region");
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

void ActionPanel::setTrimLocked (bool locked)
{
    // While trim is active: disable ADD SLICE and MIDI SLICE.
    // TRIM button itself stays enabled so user can cancel.
    // Everything else (browser, wave, chromatic, shortcuts) also locked.
    addSliceBtn.setEnabled  (! locked);
    lazyChopBtn.setEnabled  (! locked);
    // Visual: dim disabled buttons
    const float alpha = locked ? 0.25f : 1.0f;
    addSliceBtn.setAlpha  (alpha);
    lazyChopBtn.setAlpha  (alpha);
    repaint();
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
    int right = getWidth();    right -= thinW + gap;
    trimBtn.setBounds (right - trimW, 0, trimW, h);

    // ADD SLICE and MIDI SLICE left-aligned with natural widths
    addSliceBtn.setBounds (0,            0, addW, h);
    lazyChopBtn.setBounds (addW + gap,   0, midiW, h);

    shortcutsBtn.setVisible (false);
}

void ActionPanel::paint (juce::Graphics& g)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }
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

}
