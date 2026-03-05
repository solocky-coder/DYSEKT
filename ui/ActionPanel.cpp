#include "ActionPanel.h"
#include "AutoChopPanel.h"
#include "TrimDialog.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

ActionPanel::ActionPanel (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn,
                       &trimBtn, &snapBtn, &midiSelectBtn, &shortcutsBtn })
    {
        addAndMakeVisible (btn);
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    // FIL/WA/CH buttons moved to HeaderBar — keep members for state sync only

    addSliceBtn.onClick = [this] { waveformView.setSliceDrawMode (! waveformView.isSliceDrawModeActive()); repaint(); };

    lazyChopBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = processor.lazyChop.isActive() ? DysektProcessor::CmdLazyChopStop : DysektProcessor::CmdLazyChopStart;
        processor.pushCommand (cmd); repaint();
    };

    // dupBtn (Copy) removed per Fix #3 — function entirely removed
    // splitBtn (Auto Chop) removed per Fix #3 — function entirely removed
    // deleteBtn (Del) removed per Fix #4 — function moved to right-click on slice lane

    trimBtn.onClick = [this] { toggleTrimMode(); };

    shortcutsBtn.onClick = [this] { if (onShortcutsToggle) onShortcutsToggle(); };
    shortcutsBtn.setTooltip ("Keyboard Shortcuts (⌘?)");

    snapBtn.onClick = [this] {
        bool ns = ! processor.snapToZeroCrossing.load();
        processor.snapToZeroCrossing.store (ns);
        updateSnapButtonAppearance (ns); repaint();
    };

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
    lazyChopBtn.setButtonText ("MIDI SLICE");
    trimBtn.setTooltip     ("Trim - crop sample to a selected region");
    snapBtn.setTooltip     ("Snap to Zero-Crossing (Z)");

    browserBtn.setTooltip  ("Toggle File Browser");
    waveBtn.setTooltip     ("Toggle Soft Waveform");
    chromaticBtn.setTooltip ("Chromatic Mode - play selected slice across full keyboard");

    updateMidiButtonAppearance (false);
    updateSnapButtonAppearance (false);
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
    // Auto Chop removed per Fix #3 — function entirely removed
    // This method is kept to avoid breaking call sites in PluginEditor key handler;
    // callers should be updated to remove 'C' key binding as well.
}

void ActionPanel::toggleTrimMode()
{
    // Close trim dialog if already open
    if (trimDialog != nullptr)
    {
        if (auto* parent = trimDialog->getParentComponent())
            parent->removeChildComponent (trimDialog.get());
        trimDialog.reset();
        waveformView.setTrimMode (false);
        repaint();
        return;
    }

    // Check that a sample is loaded
    if (processor.sampleData.getSnapshot() == nullptr)
        return;

    auto* editor = waveformView.getParentComponent();
    if (editor == nullptr)
        return;

    waveformView.setTrimMode (true);
    trimDialog = std::make_unique<TrimDialog> (processor, waveformView);
    auto wfBounds = waveformView.getBoundsInParent();
    trimDialog->setBounds (wfBounds.getX(), wfBounds.getBottom() - 34, wfBounds.getWidth(), 34);
    editor->addAndMakeVisible (*trimDialog);
    repaint();
}

void ActionPanel::resized()
{
    const int gap   = 5;
    const int h     = getHeight();
    const int thinW = 30;   // snap, MIDI select icon buttons
    const int trimW = 42;

    // Lay out right-to-left so icon buttons always sit flush at the right edge,
    // preventing overflow regardless of panel width.
    int right = getWidth();

    midiSelectBtn.setBounds (right - thinW, 0, thinW, h); right -= thinW + gap;
    snapBtn.setBounds       (right - thinW, 0, thinW, h); right -= thinW + gap;
    trimBtn.setBounds       (right - trimW, 0, trimW, h); right -= trimW + gap;

    // ADD SLICE and MIDI SLICE split the remaining width equally
    const int remaining = right;
    const int btnW      = (remaining - gap) / 2;
    addSliceBtn.setBounds (0,          0, btnW, h);
    lazyChopBtn.setBounds (btnW + gap, 0, btnW, h);

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
    updateSnapButtonAppearance (processor.snapToZeroCrossing.load());

    if (waveformView.isSliceDrawModeActive())
    { g.setColour (getTheme().accent.withAlpha (0.25f)); g.fillRect (addSliceBtn.getBounds()); }

    if (processor.lazyChop.isActive())
    { lazyChopBtn.setButtonText ("STOP"); g.setColour (juce::Colours::red.withAlpha (0.25f)); g.fillRect (lazyChopBtn.getBounds()); }
    else
    { lazyChopBtn.setButtonText ("MIDI SLICE"); }

    // TRIM button — highlight when trim mode is active
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

void ActionPanel::updateSnapButtonAppearance (bool active)
{
    auto col = active ? getTheme().accent : getTheme().foreground;
    auto bg  = active ? getTheme().accent.withAlpha (0.2f) : getTheme().button;
    snapBtn.setColour (juce::TextButton::textColourOnId,  col);
    snapBtn.setColour (juce::TextButton::textColourOffId, col);
    snapBtn.setColour (juce::TextButton::buttonColourId,  bg);
}

void ActionPanel::paintOverChildren (juce::Graphics& g)
{
    // Draw 5-pin DIN MIDI connector icon for midiSelectBtn (Halion-style)
    {
        bool active = processor.midiSelectsSlice.load();
        auto col = active ? getTheme().accent : getTheme().foreground.withAlpha (0.75f);
        g.setColour (col);

        auto b   = midiSelectBtn.getBounds().toFloat();
        float cx = b.getCentreX();
        float cy = b.getCentreY() + 1.0f;

        // Outer circle (connector body)
        const float outerR = 8.5f;
        g.drawEllipse (cx - outerR, cy - outerR, outerR * 2, outerR * 2, 1.2f);

        // Flat edge on bottom (D-shell cutoff)
        g.setColour (active ? getTheme().accent.withAlpha(0.0f) : getTheme().button);
        g.fillRect  (cx - outerR - 1, cy + 4.5f, outerR * 2 + 2, outerR);
        g.setColour (col);
        g.drawLine  (cx - outerR, cy + 4.5f, cx + outerR, cy + 4.5f, 1.2f);

        // 5 pins arranged in a semicircle (top arc)
        // Pin layout: 2 top row, 3 bottom row (standard DIN-5 arrangement)
        struct Pin { float x, y; };
        const float pinR = 1.4f;
        const float arcR = 5.0f;
        // Standard MIDI DIN-5: 3 pins on top arc, 2 on lower arc
        Pin pins[] = {
            { cx - arcR * 0.95f, cy - arcR * 0.31f },   // pin 1 (left)
            { cx + arcR * 0.95f, cy - arcR * 0.31f },   // pin 2 (right)
            { cx,                cy - arcR          },   // pin 3 (top centre)
            { cx - arcR * 0.59f, cy + arcR * 0.81f },   // pin 4 (bottom left)
            { cx + arcR * 0.59f, cy + arcR * 0.81f },   // pin 5 (bottom right)
        };
        for (auto& pin : pins)
            g.fillEllipse (pin.x - pinR, pin.y - pinR, pinR * 2, pinR * 2);
    }

    // Draw zero-crossing icon for snapBtn
    {
        bool active = processor.snapToZeroCrossing.load();
        auto col = active ? getTheme().accent : getTheme().foreground.withAlpha (0.75f);
        g.setColour (col);

        auto b   = snapBtn.getBounds();
        float cx = (float) b.getCentreX();
        float cy = (float) b.getCentreY();

        // Horizontal zero line
        g.drawLine (cx - 9.0f, cy, cx + 9.0f, cy, 1.0f);

        // Sine wave crossing through the zero line
        juce::Path wave;
        wave.startNewSubPath (cx - 9.0f, cy);
        wave.cubicTo (cx - 5.0f, cy,        cx - 5.0f, cy - 6.5f, cx,        cy);
        wave.cubicTo (cx + 5.0f, cy,        cx + 5.0f, cy + 6.5f, cx + 9.0f, cy);
        g.strokePath (wave, juce::PathStrokeType (1.5f));
    }
}
