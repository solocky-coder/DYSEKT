#include "HeaderBar.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../audio/GrainEngine.h"
#include "BinaryData.h"

HeaderBar::HeaderBar (DysektProcessor& p)
 : processor (p),
   controlFrame (p)
{
    // Standard buttons
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &shortcutsBtn })
    {
        btn->setAlwaysOnTop (true);
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
        addAndMakeVisible (*btn);
    }

    // Cogwheel icon — U+2699 GEAR, UTF-8: E2 9A 99
    shortcutsBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x9a\x99"));
    shortcutsBtn.setTooltip ("Settings");
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

    // Wire control frame callbacks → forward to PluginEditor via HeaderBar's lambdas
    controlFrame.onBrowserToggle = [this] { if (onBrowserToggle) onBrowserToggle(); };
    controlFrame.onWaveToggle    = [this] { if (onWaveToggle)    onWaveToggle(); };
    controlFrame.onMidiFollowToggle = [this] { if (onMidiFollowToggle) onMidiFollowToggle(); };
    controlFrame.onBodeToggle    = [this] { if (onBodeToggle)    onBodeToggle(); };
    // Note: controlFrame is NOT added as a visible child here —
    // PluginEditor::resized() calls addAndMakeVisible(*headerBar.getControlFrame())
    // and positions it between the two LCD panels.
}

// ── State sync ────────────────────────────────────────────────────────────────

void HeaderBar::setBrowserActive  (bool v) { controlFrame.setBrowserActive (v); }
void HeaderBar::setWaveActive     (bool v) { controlFrame.setWaveActive (v); }
void HeaderBar::setMidiFollowActive (bool v) { controlFrame.setMidiFollowActive (v); }
void HeaderBar::setBodeActive     (bool v) { controlFrame.setBodeActive (v); }

// ── resized ───────────────────────────────────────────────────────────────────

void HeaderBar::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Two separate 1×2 groups flanking the logo:
    //   Left  → UNDO (top) / REDO (bottom), flush to left edge
    //   Right → PANIC (top) / ⚙  (bottom), flush to right edge
    const int btnW   = 36; // width of each button
    const int btnH   = 20; // height of each button
    const int rowGap =  4; // vertical gap between top and bottom buttons

    const int groupH = btnH * 2 + rowGap;
    const int gy     = (h - groupH) / 2;

    // Left pair — flush left
    undoBtn.setBounds (0,         gy,                 btnW, btnH);
    redoBtn.setBounds (0,         gy + btnH + rowGap, btnW, btnH);

    // Right pair — flush right
    const int rx = w - btnW;
    panicBtn    .setBounds (rx, gy,                 btnW, btnH);
    shortcutsBtn.setBounds (rx, gy + btnH + rowGap, btnW, btnH);
}

// ── paint ─────────────────────────────────────────────────────────────────────

void HeaderBar::paint (juce::Graphics& g)
{
    // Refresh standard button colours to follow theme
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &shortcutsBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().accent);    // on = accent
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    // Background is transparent — LogoBar paints behind us
    g.fillAll (juce::Colours::transparentBlack);

    // ── Frame around the left pair (UNDO / REDO) ─────────────────────────
    {
        const juce::Rectangle<int> lf (
            undoBtn.getX() - 3,  undoBtn.getY() - 3,
            undoBtn.getWidth() + 6,
            redoBtn.getBottom() - undoBtn.getY() + 6);

        g.setColour (getTheme().separator.withAlpha (0.75f));
        g.drawRoundedRectangle (lf.toFloat().reduced (0.5f), 3.0f, 1.0f);

        // Subtle horizontal divider between UNDO and REDO
        g.setColour (getTheme().foreground.withAlpha (0.12f));
        g.drawHorizontalLine (redoBtn.getY(),
                              (float) undoBtn.getX() + 3,
                              (float) undoBtn.getRight() - 3);
    }

    // ── Frame around the right pair (PANIC / ⚙) ──────────────────────────
    {
        const juce::Rectangle<int> rf (
            panicBtn.getX() - 3, panicBtn.getY() - 3,
            panicBtn.getWidth() + 6,
            shortcutsBtn.getBottom() - panicBtn.getY() + 6);

        g.setColour (getTheme().separator.withAlpha (0.75f));
        g.drawRoundedRectangle (rf.toFloat().reduced (0.5f), 3.0f, 1.0f);

        // Subtle horizontal divider between PANIC and ⚙
        g.setColour (getTheme().foreground.withAlpha (0.12f));
        g.drawHorizontalLine (shortcutsBtn.getY(),
                              (float) panicBtn.getX() + 3,
                              (float) panicBtn.getRight() - 3);
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

void HeaderBar::mouseDrag       (const juce::MouseEvent&) {}
void HeaderBar::mouseUp         (const juce::MouseEvent&) {}

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

    auto themes      = editor->getAvailableThemes();
    auto currentName = getTheme().name;

    juce::PopupMenu menu;

    float currentScale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    menu.addSectionHeader ("Scale " + juce::String (currentScale, 2) + "x");
    menu.addItem (100, "- 0.25");
    menu.addItem (101, "+ 0.25");
    menu.addSeparator();

    menu.addSectionHeader ("Theme");
    for (int i = 0; i < themes.size(); ++i)
        menu.addItem (i + 1, themes[i], true, themes[i] == currentName);

    auto* topLevel = getTopLevelComponent();
    float ms2 = DysektLookAndFeel::getMenuScale();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shortcutsBtn)
                                                  .withParentComponent (topLevel)
                                                  .withStandardItemHeight ((int) (24 * ms2)),
        [this, editor, themes] (int result) {
            if      (result == 100) adjustScale (-0.25f);
            else if (result == 101) adjustScale ( 0.25f);
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
