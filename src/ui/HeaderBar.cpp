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
    const int h      = getHeight();   // kLogoH = 52
    const int btnW   = 54;
    const int btnH   = 20;
    const int rowGap = 4;
    const int groupH = btnH * 2 + rowGap;
    const int gy     = (h - groupH) / 2;
    const int pad    = 3;

    // LEFT side of logo: PANIC (top) + ⚙ (bottom)
    panicBtn    .setBounds (pad, gy,               btnW, btnH);
    shortcutsBtn.setBounds (pad, gy + btnH + rowGap, btnW, btnH);

    // RIGHT side of logo: UNDO (top) + REDO (bottom)
    const int rx = getWidth() - btnW - pad;
    undoBtn.setBounds (rx, gy,               btnW, btnH);
    redoBtn.setBounds (rx, gy + btnH + rowGap, btnW, btnH);
}

// ── paint ─────────────────────────────────────────────────────────────────────

void HeaderBar::paint (juce::Graphics& g)
{
    // Refresh standard button colours to follow theme
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &shortcutsBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    // No background fill — LogoBar paints the header background beneath us.

    // ── Helper: draw rounded frame + inner divider around a stacked button pair ──
    auto drawPairFrame = [&] (juce::TextButton& top, juce::TextButton& bot)
    {
        const int fx = top.getX() - 3;
        const int fy = top.getY() - 3;
        const int fw = top.getWidth()  + 6;
        const int fh = bot.getBottom() - top.getY() + 6;
        g.setColour (getTheme().separator.withAlpha (0.75f));
        g.drawRoundedRectangle ((float) fx, (float) fy, (float) fw, (float) fh, 3.0f, 1.0f);

        // Horizontal divider between the two buttons
        g.setColour (getTheme().foreground.withAlpha (0.12f));
        g.drawHorizontalLine (bot.getY(),
                              (float) top.getX() + 3,
                              (float) top.getRight() - 3);
    };

    drawPairFrame (panicBtn,  shortcutsBtn); // left group
    drawPairFrame (undoBtn,   redoBtn);      // right group

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
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shortcutsBtn)
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
