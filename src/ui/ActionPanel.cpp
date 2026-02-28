#include "ActionPanel.h"
#include "AutoChopPanel.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

ActionPanel::ActionPanel (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn, &dupBtn, &splitBtn,
                       &deleteBtn, &snapBtn, &midiSelectBtn })
    {
        addAndMakeVisible (btn);
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    // FILES, WAVE, CHRO get accent styling
    addAndMakeVisible (browserBtn);
    addAndMakeVisible (waveBtn);
    addAndMakeVisible (chromaticBtn);
    for (auto* btn : { &browserBtn, &waveBtn, &chromaticBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().accent);
    }

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

void ActionPanel::resized()
{
    const int gap     = 5;
    const int h       = getHeight();
    const int thinW   = 30;   // ZX, FM
    const int accentW = 38;   // FILES, WAVE, CHRO
    const int thinTotal   = thinW * 2 + gap;
    const int accentTotal = accentW * 3 + gap * 2;
    const int availW  = getWidth() - thinTotal - gap - accentTotal - gap;
    const int numMain = 5;
    const int btnW    = (availW - gap * (numMain - 1)) / numMain;
    int x = 0;

    addSliceBtn.setBounds (x, 0, btnW, h); x += btnW + gap;
    lazyChopBtn.setBounds (x, 0, btnW, h); x += btnW + gap;
    splitBtn.setBounds    (x, 0, btnW, h); x += btnW + gap;
    dupBtn.setBounds      (x, 0, btnW, h); x += btnW + gap;
    deleteBtn.setBounds   (x, 0, btnW, h); x += btnW + gap;

    snapBtn.setBounds       (x, 0, thinW, h); x += thinW + gap;
    midiSelectBtn.setBounds (x, 0, thinW, h); x += thinW + gap;

    browserBtn.setBounds   (x, 0, accentW, h); x += accentW + gap;
    waveBtn.setBounds      (x, 0, accentW, h); x += accentW + gap;
    chromaticBtn.setBounds (x, 0, accentW, h);
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
    updateToggleBtn (browserBtn,    browserActive);
    updateToggleBtn (waveBtn,       waveActive);
    updateToggleBtn (chromaticBtn,  chromaticActive);

    if (waveformView.isSliceDrawModeActive())
    { g.setColour (getTheme().accent.withAlpha (0.25f)); g.fillRect (addSliceBtn.getBounds()); }

    if (processor.lazyChop.isActive())
    { lazyChopBtn.setButtonText ("STOP"); g.setColour (juce::Colours::red.withAlpha (0.25f)); g.fillRect (lazyChopBtn.getBounds()); }
    else
    { lazyChopBtn.setButtonText ("LAZY"); }
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
