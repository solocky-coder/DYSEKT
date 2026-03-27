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
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
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
    controlFrame.onBrowserToggle    = [this] { if (onBrowserToggle)    onBrowserToggle(); };
    controlFrame.onWaveToggle       = [this] { if (onWaveToggle)       onWaveToggle(); };
    controlFrame.onMidiFollowToggle = [this] { if (onMidiFollowToggle) onMidiFollowToggle(); };
    controlFrame.onBodeToggle       = [this] { if (onBodeToggle)       onBodeToggle(); };
    // Note: controlFrame is NOT added as a visible child here —
    // PluginEditor::resized() calls addAndMakeVisible(*headerBar.getControlFrame())
    // and positions it between the two LCD panels.
}

// ── State sync ────────────────────────────────────────────────────────────────

void HeaderBar::setBrowserActive   (bool v) { controlFrame.setBrowserActive (v); }
void HeaderBar::setWaveActive      (bool v) { controlFrame.setWaveActive (v); }
void HeaderBar::setMidiFollowActive(bool v) { controlFrame.setMidiFollowActive (v); }
void HeaderBar::setBodeActive      (bool v) { controlFrame.setBodeActive (v); }

// ── resized ───────────────────────────────────────────────────────────────────

void HeaderBar::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // 2×2 grid: Left col → UNDO (top) / REDO (bottom)
    //           Right col → PANIC (top) / ⚙ (bottom)
    const int btnW   = 36;  // CHANGED from 56
    const int btnH   = 20;  // height of each button
    const int colGap =  4;  // horizontal gap between the two columns
    const int rowGap =  4;  // vertical gap between top and bottom buttons

    const int groupW = btnW * 2 + colGap;
    const int groupH = btnH * 2 + rowGap;

    // Centre the group inside this bar
    const int gx = (w - groupW) / 2;
    const int gy = (h - groupH) / 2;

    // Left column
    undoBtn.setBounds (gx, gy,                btnW, btnH);
    redoBtn.setBounds (gx, gy + btnH + rowGap, btnW, btnH);

    // Right column
    const int rx = gx + btnW + colGap;
    panicBtn    .setBounds (rx, gy,                btnW, btnH);
    shortcutsBtn.setBounds (rx, gy + btnH + rowGap, btnW, btnH);
}

// ── paint ─────────────────────────────────────────────────────────────────────

void HeaderBar::paint (juce::Graphics& g)
{
    // Refresh standard button colours to follow theme
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &shortcutsBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    g.fillAll (getTheme().header);

    // ── Frame around the 2×2 button group ────────────────────────────────────
    const int frameX1 = undoBtn.getX()           - 3;
    const int frameY1 = undoBtn.getY()           - 3;
    const int frameX2 = shortcutsBtn.getRight()  + 3;
    const int frameY2 = shortcutsBtn.getBottom() + 3;
    const juce::Rectangle<int> btnFrame (frameX1, frameY1,
                                         frameX2 - frameX1, frameY2 - frameY1);

    g.setColour (getTheme().separator.withAlpha (0.75f));
    g.drawRoundedRectangle (btnFrame.toFloat().reduced (0.5f), 3.0f, 1.0f);

    // ── Subtle vertical divider between left and right columns ────────────────
    {
        const int vx = panicBtn.getX();
        const int vy = undoBtn.getY();
        const int vh = shortcutsBtn.getBottom() - vy;
        g.setColour (getTheme().foreground.withAlpha (0.12f));
        g.drawVerticalLine (vx, (float) vy + 3, (float) (vy + vh - 3));
    }

    // ── Subtle horizontal dividers within each column ─────────────────────────
    {
        const int rowY = redoBtn.getY();
        g.setColour (getTheme().foreground.withAlpha (0.12f));
        g.drawHorizontalLine (rowY, (float) undoBtn.getX()  + 3, (float) undoBtn.getRight()  - 3);
        g.drawHorizontalLine (rowY, (float) panicBtn.getX() + 3, (float) panicBtn.getRight() - 3);
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

void HeaderBar::mouseDrag        (const juce::MouseEvent&) {}
void HeaderBar::mouseUp          (const juce::MouseEvent&) {}
void HeaderBar::mouseDoubleClick (const juce::MouseEvent& /*e*/) {}

// ── Helpers ───────────────────────────────────────────────────────────────────

void HeaderBar::adjustScale (float delta)
{
    if (auto* p = processor.apvts.getParameter (ParamIds::uiScale))
    {
        float current  = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
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
