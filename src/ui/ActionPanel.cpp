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
    for (auto* btn : { &addSliceBtn, &lazyChopBtn, &dupBtn, &splitBtn,
                       &deleteBtn, &trimBtn, &snapBtn, &midiSelectBtn })
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

    dupBtn.onClick = [this] {
        DysektProcessor::Command cmd; cmd.type = DysektProcessor::CmdDuplicateSlice; cmd.intParam1 = -1;
        processor.pushCommand (cmd); repaint();
    };

    splitBtn.onClick   = [this] { toggleAutoChop(); };

    deleteBtn.onClick = [this] {
        const auto& ui = processor.getUiSliceSnapshot();
        if (ui.selectedSlice >= 0)
        { DysektProcessor::Command cmd; cmd.type = DysektProcessor::CmdDeleteSlice; cmd.intParam1 = ui.selectedSlice; processor.pushCommand (cmd); }
    };

    trimBtn.onClick = [this] { toggleTrimMode(); };

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
    lazyChopBtn.setTooltip ("Lazy Chop (L)");
    dupBtn.setTooltip      ("Duplicate Slice (D)");
    splitBtn.setTooltip    ("Auto Chop (C)");
    deleteBtn.setTooltip   ("Delete Slice (Del)");
    trimBtn.setTooltip     ("Trim — crop sample to a selected region");
    snapBtn.setTooltip     ("Snap to Zero-Crossing (Z)");

    browserBtn.setTooltip  ("Toggle File Browser");
    waveBtn.setTooltip     ("Toggle Soft Waveform");
    chromaticBtn.setTooltip ("Chromatic Mode — play selected slice across full keyboard");

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
    if (autoChopPanel != nullptr)
    {
        if (auto* parent = autoChopPanel->getParentComponent())
            parent->removeChildComponent (autoChopPanel.get());
        autoChopPanel.reset(); return;
    }
    autoChopPanel = std::make_unique<AutoChopPanel> (processor, waveformView);
    if (auto* editor = waveformView.getParentComponent())
    {
        auto wfBounds = waveformView.getBoundsInParent();
        autoChopPanel->setBounds (wfBounds.getX(), wfBounds.getBottom() - 34, wfBounds.getWidth(), 34);
        editor->addAndMakeVisible (*autoChopPanel);
    }
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

void ActionPanel::activateTrimMode()
{
    // Only activate if not already in trim mode
    if (trimDialog != nullptr)
        return;
    toggleTrimMode();
}

void ActionPanel::resized()
{
    const int gap   = 5;
    const int h     = getHeight();
    const int thinW = 30;   // ZX, MIDI icons
    const int thinTotal = thinW * 2 + gap;
    const int trimW = 40;   // TRIM button
    const int availW = getWidth() - thinTotal - trimW - gap * 2;
    const int numMain = 5;
    const int btnW  = (availW - gap * (numMain - 1)) / numMain;
    int x = 0;

    addSliceBtn.setBounds (x, 0, btnW, h); x += btnW + gap;
    lazyChopBtn.setBounds (x, 0, btnW, h); x += btnW + gap;
    splitBtn.setBounds    (x, 0, btnW, h); x += btnW + gap;
    dupBtn.setBounds      (x, 0, btnW, h); x += btnW + gap;
    deleteBtn.setBounds   (x, 0, btnW, h); x += btnW + gap;

    trimBtn.setBounds       (x, 0, trimW, h); x += trimW + gap;
    snapBtn.setBounds       (x, 0, thinW, h); x += thinW + gap;
    midiSelectBtn.setBounds (x, 0, thinW, h);

    // browserBtn/waveBtn/chromaticBtn moved to HeaderBar — hide them
    browserBtn.setVisible (false);
    waveBtn.setVisible    (false);
    chromaticBtn.setVisible (false);
}

void ActionPanel::paint (juce::Graphics& g)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn, &dupBtn, &splitBtn, &deleteBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }
    updateMidiButtonAppearance (processor.midiSelectsSlice.load());
    updateSnapButtonAppearance (processor.snapToZeroCrossing.load());
    // FIL/WA/CH toggle state managed by HeaderBar

    if (waveformView.isSliceDrawModeActive())
    { g.setColour (getTheme().accent.withAlpha (0.25f)); g.fillRect (addSliceBtn.getBounds()); }

    if (processor.lazyChop.isActive())
    { lazyChopBtn.setButtonText ("STOP"); g.setColour (juce::Colours::red.withAlpha (0.25f)); g.fillRect (lazyChopBtn.getBounds()); }
    else
    { lazyChopBtn.setButtonText ("LAZY"); }

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
    // Draw MIDI (musical note) icon for midiSelectBtn
    {
        bool active = processor.midiSelectsSlice.load();
        auto col = active ? getTheme().accent : getTheme().foreground.withAlpha (0.75f);
        g.setColour (col);

        auto b   = midiSelectBtn.getBounds();
        float cx = (float) b.getCentreX();
        float cy = (float) b.getCentreY();

        // Note head (filled ellipse)
        juce::Path head;
        head.addEllipse (cx - 5.0f, cy + 1.5f, 8.0f, 5.5f);
        g.fillPath (head);

        // Stem (vertical line from right of head upward)
        g.drawLine (cx + 3.0f, cy + 4.0f, cx + 3.0f, cy - 7.0f, 1.5f);

        // Flag (two lines forming a curl to the right)
        g.drawLine (cx + 3.0f, cy - 7.0f, cx + 8.0f, cy - 3.0f, 1.5f);
        g.drawLine (cx + 8.0f, cy - 3.0f, cx + 3.5f, cy - 1.0f, 1.5f);
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
