#include "HeaderBar.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../audio/GrainEngine.h"
#include "BinaryData.h"
#include <cmath>

HeaderBar::HeaderBar (DysektProcessor& p)
    : processor (p),
      controlFrame (p)
{
    // Standard buttons
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn, &shortcutsBtn })
    {
        btn->setAlwaysOnTop (true);
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
        addAndMakeVisible (*btn);
    }

    shortcutsBtn.setTooltip ("Keyboard Shortcuts");
    shortcutsBtn.onClick = [this] { if (onShortcutsToggle) onShortcutsToggle(); };

    panicBtn.setTooltip ("Panic: kill all sound");
    panicBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdPanic;
        processor.pushCommand (cmd);
    };

    undoBtn.setTooltip ("Undo (Ctrl+Z)");
    undoBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdUndo;
        processor.pushCommand (cmd);
    };

    redoBtn.setTooltip ("Redo (Ctrl+Shift+Z)");
    redoBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdRedo;
        processor.pushCommand (cmd);
    };

    themeBtn.onClick = [this] { showThemePopup(); };

    // Wire control frame callbacks → forward to PluginEditor via HeaderBar's lambdas
    controlFrame.onBrowserToggle   = [this] { if (onBrowserToggle)   onBrowserToggle(); };
    controlFrame.onWaveToggle      = [this] { if (onWaveToggle)      onWaveToggle(); };
    controlFrame.onMidiFollowToggle = [this] { if (onMidiFollowToggle) onMidiFollowToggle(); };
    controlFrame.onBodeToggle      = [this] { if (onBodeToggle)      onBodeToggle(); };
    // Note: controlFrame is NOT added as a visible child here —
    // PluginEditor::resized() calls addAndMakeVisible(*headerBar.getControlFrame())
    // and positions it between the two LCD panels.
}

// ── State sync ────────────────────────────────────────────────────────────────

void HeaderBar::setBrowserActive   (bool v) { controlFrame.setBrowserActive   (v); }
void HeaderBar::setWaveActive      (bool v) { controlFrame.setWaveActive      (v); }
void HeaderBar::setMidiFollowActive (bool v) { controlFrame.setMidiFollowActive (v); }
void HeaderBar::setBodeActive      (bool v) { controlFrame.setBodeActive      (v); }

// ── resized ───────────────────────────────────────────────────────────────────

void HeaderBar::resized()
{
    const int h   = getHeight();
    const int btnH = 20;
    const int btnY = (h - btnH) / 2;
    const int gap  = 4;
    int right = getWidth() - 8;

    // Right to left: [?] [UI] [PANIC] [REDO][UNDO]

    // ? shortcuts button — rightmost
    shortcutsBtn.setBounds (right - 22, btnY, 22, btnH);
    right -= 26;

    // UI / theme button
    themeBtn.setBounds (right - 26, btnY, 26, btnH);
    right -= 30;

    // PANIC
    const int panicW = 52;
    panicBtn.setBounds (right - panicW, btnY, panicW, btnH);
    right -= panicW + gap * 2;

    // REDO | UNDO — side-by-side as a joined pair
    const int urW = 50;
    redoBtn.setBounds (right - urW,         btnY, urW, btnH);
    undoBtn.setBounds (right - urW * 2 - 1, btnY, urW, btnH);
    right -= urW * 2 + 1 + gap;
}

// ── paint ─────────────────────────────────────────────────────────────────────

void HeaderBar::paint (juce::Graphics& g)
{
    // Refresh standard button colours to follow theme
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn, &shortcutsBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().accent);      // on = accent
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    g.fillAll (getTheme().header);

    // ── Frame around the button group ─────────────────────────────────────────
    // Compute a tight bounding rect around all 5 buttons with 2px padding
    const int frameX1 = undoBtn.getX() - 2;
    const int frameY1 = undoBtn.getY() - 2;
    const int frameX2 = shortcutsBtn.getRight() + 2;
    const int frameY2 = shortcutsBtn.getBottom() + 2;
    const juce::Rectangle<int> btnFrame (frameX1, frameY1,
                                         frameX2 - frameX1, frameY2 - frameY1);

    g.setColour (getTheme().separator);
    g.drawRect (btnFrame, 1);

    // ── Subtle divider between UNDO and REDO ──────────────────────────────────
    {
        const int rx = redoBtn.getX();
        const int ry = redoBtn.getY();
        const int rh = redoBtn.getHeight();
        g.setColour (getTheme().foreground.withAlpha (0.15f));
        g.drawVerticalLine (rx, (float) ry + 3, (float) (ry + rh - 3));
    }

    // Left side intentionally empty — sample name lives in LCD 1
    sampleInfoBounds = {};
    slicesInfoArea   = {};
    (void) processor;
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void HeaderBar::mouseDown (const juce::MouseEvent& e)
{
    if (textEditor) textEditor.reset();
    const auto& ui = processor.getUiSliceSnapshot();
    auto pos = e.getPosition();

    if (sampleInfoBounds.contains (pos) && ui.sampleMissing)
        openRelinkBrowser();
}

void HeaderBar::mouseDrag  (const juce::MouseEvent&) {}
void HeaderBar::mouseUp    (const juce::MouseEvent&) {}

void HeaderBar::mouseDoubleClick (const juce::MouseEvent& /*e*/) {}

// ── Helpers ───────────────────────────────────────────────────────────────────

void HeaderBar::adjustScale (float delta)
{
    if (auto* p = processor.apvts.getParameter (ParamIds::uiScale))
    {
        float current = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
        float newScale = juce::jlimit (0.5f, 3.0f, current + delta);
        p->setValueNotifyingHost (p->convertTo0to1 (newScale));
    }
}

void HeaderBar::showThemePopup()
{
    auto* editor = dynamic_cast<DysektEditor*> (getParentComponent());
    if (editor == nullptr) return;

    auto themes = editor->getAvailableThemes();
    auto currentName = getTheme().name;

    juce::PopupMenu menu;

    float currentScale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    menu.addSectionHeader ("Scale  " + juce::String (currentScale, 2) + "x");
    menu.addItem (100, "- 0.25");
    menu.addItem (101, "+ 0.25");
    menu.addSeparator();

    menu.addSectionHeader ("Theme");
    for (int i = 0; i < themes.size(); ++i)
        menu.addItem (i + 1, themes[i], true, themes[i] == currentName);

    auto* topLevel = getTopLevelComponent();
    float ms2 = DysektLookAndFeel::getMenuScale();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&themeBtn)
                            .withParentComponent (topLevel)
                            .withStandardItemHeight ((int) (24 * ms2)),
        [this, editor, themes] (int result) {
            if (result == 100)        adjustScale (-0.25f);
            else if (result == 101)   adjustScale ( 0.25f);
            else if (result > 0 && result <= themes.size())
                editor->applyTheme (themes[result - 1]);
        });
}

void HeaderBar::openRelinkBrowser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Relink Audio File",
        juce::File(),
        "*.wav;*.ogg;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
                processor.relinkFileAsync (result);
        });
}
