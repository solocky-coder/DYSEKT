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
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn })
    {
        btn->setAlwaysOnTop (true);
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
        addAndMakeVisible (*btn);
    }

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
    controlFrame.onChromaticToggle = [this] { if (onChromaticToggle) onChromaticToggle(); };
    // Note: controlFrame is NOT added as a visible child here —
    // PluginEditor::resized() calls addAndMakeVisible(*headerBar.getControlFrame())
    // and positions it between the two LCD panels.
}

// ── State sync ────────────────────────────────────────────────────────────────

void HeaderBar::setBrowserActive   (bool v) { controlFrame.setBrowserActive   (v); }
void HeaderBar::setWaveActive      (bool v) { controlFrame.setWaveActive      (v); }
void HeaderBar::setChromaticActive (bool v) { controlFrame.setChromaticActive (v); }

// ── resized ───────────────────────────────────────────────────────────────────

void HeaderBar::resized()
{
    const int h   = getHeight();
    const int btnH = 20;
    const int btnY = (h - btnH) / 2;
    const int gap  = 4;
    int right = getWidth() - 8;

    // Right to left: [UI] [PANIC] [REDO][UNDO]

    // UI / theme button
    themeBtn.setBounds (right - 26, btnY, 26, btnH);
    right -= 30;

    // PANIC
    const int panicW = 52;
    panicBtn.setBounds (right - panicW, btnY, panicW, btnH);
    right -= panicW + gap * 2;

    // REDO | UNDO — side-by-side as a joined pair
    const int urW = 38;
    redoBtn.setBounds (right - urW,         btnY, urW, btnH);
    undoBtn.setBounds (right - urW * 2 - 1, btnY, urW, btnH);
    right -= urW * 2 + 1 + gap;
}

// ── paint ─────────────────────────────────────────────────────────────────────

void HeaderBar::paint (juce::Graphics& g)
{
    // Refresh standard button colours
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    g.fillAll (getTheme().header);

    // Draw a subtle join line between UNDO and REDO to make them look like a pair
    {
        int rx = redoBtn.getX();
        int ry = redoBtn.getY();
        int rh = redoBtn.getHeight();
        g.setColour (getTheme().foreground.withAlpha (0.15f));
        g.drawVerticalLine (rx, (float) ry + 3, (float) (ry + rh - 3));
    }

    // ── SAMPLE info + SLICES — left side (same as before) ────────────────────
    {
        const auto& ui = processor.getUiSliceSnapshot();
        const int   lx = 10;
        const int   cy = getHeight() / 2;
        const int cellY = cy - 15;
        const int cellH = 30;
        const int   lw = 60;
        const int  gap = 8;
        const auto white  = juce::Colours::white;
        const auto fg     = getTheme().foreground;
        const auto accent = getTheme().accent;

        // Right boundary: left edge of undo button pair
        int rightBound = undoBtn.getX() - gap;
        int slcX = rightBound - lw;

        int sampleX = lx;
        int maxW    = slcX - gap - sampleX;

        if (ui.sampleMissing)
        {
            g.setFont (DysektLookAndFeel::makeFont (12.0f));
            g.setColour (white);
            g.drawText ("SAMPLE", sampleX, cellY + 2, 65, 13, juce::Justification::left);
            g.setFont (DysektLookAndFeel::makeFont (14.0f));
            g.setColour (juce::Colours::orange.withAlpha (0.9f));
            g.drawText (ui.sampleFileName + " (click to relink)",
                        sampleX, cellY + 15, maxW, 14, juce::Justification::left);
            sampleInfoBounds = { sampleX, cellY, maxW, cellH };
        }
        else if (ui.sampleLoaded)
        {
            juce::String nameOnly = juce::File (ui.sampleFileName).getFileNameWithoutExtension();
            g.setFont (DysektLookAndFeel::makeFont (12.0f));
            g.setColour (white);
            g.drawText ("SAMPLE", sampleX, cellY + 2, 65, 13, juce::Justification::left);
            g.setFont (DysektLookAndFeel::makeFont (14.0f));
            g.setColour (fg.withAlpha (0.75f));
            g.drawText (nameOnly, sampleX, cellY + 15, maxW, 14, juce::Justification::left);
            sampleInfoBounds = { sampleX, cellY, maxW, cellH };
        }
        else
        {
            sampleInfoBounds = {};
        }

        // SLICES count
        {
            const juce::Font lblFont = DysektLookAndFeel::makeFont (12.0f);
            const juce::Font valFont = DysektLookAndFeel::makeFont (14.0f);
            g.setFont (lblFont);
            g.setColour (white);
            g.drawText ("SLICES", slcX, cellY + 2, lw, 13, juce::Justification::left);
            int lastSCenterX = slcX + lblFont.getStringWidth ("SLICES")
                                     - lblFont.getStringWidth ("S") / 2;
            juce::String slcValStr = juce::String (ui.numSlices);
            int slcValW = valFont.getStringWidth (slcValStr) + 4;
            g.setFont (valFont);
            g.setColour (fg.withAlpha (0.65f));
            g.drawText (slcValStr, lastSCenterX - slcValW / 2, cellY + 15, slcValW, 14,
                        juce::Justification::centred);
            slicesInfoArea = { slcX, cellY, lw, cellH };
        }
    }
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
