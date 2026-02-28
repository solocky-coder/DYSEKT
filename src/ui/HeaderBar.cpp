#include "HeaderBar.h"
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../audio/GrainEngine.h"
#include "BinaryData.h"
#include <cmath>

HeaderBar::HeaderBar (DysektProcessor& p) : processor (p)
{
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (redoBtn);
    addAndMakeVisible (panicBtn);
    addAndMakeVisible (themeBtn);
    undoBtn.setAlwaysOnTop (true);
    redoBtn.setAlwaysOnTop (true);
    panicBtn.setAlwaysOnTop (true);
    themeBtn.setAlwaysOnTop (true);

    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    panicBtn.setTooltip ("Panic: kill all sound");
    panicBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdPanic;
        processor.pushCommand (cmd);
    };

    undoBtn.setTooltip ("Undo (Ctrl+Z)");
    redoBtn.setTooltip ("Redo (Ctrl+Shift+Z)");

    undoBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdUndo;
        processor.pushCommand (cmd);
    };

    redoBtn.onClick = [this] {
        DysektProcessor::Command cmd;
        cmd.type = DysektProcessor::CmdRedo;
        processor.pushCommand (cmd);
    };

    themeBtn.onClick = [this] { showThemePopup(); };
}

void HeaderBar::resized()
{
    int right  = getWidth() - 8;
    int cy     = (getHeight() - 28) / 2;  // vertically centre buttons
    int panicW = 56;
    int undoW  = 48;
    int gap    = 4;

    themeBtn.setBounds  (right - 26,                             cy, 26,    28);
    panicBtn.setBounds  (right - 26 - gap - panicW,             cy, panicW, 28);

    int undoX = right - 26 - gap - panicW - gap - undoW;
    undoBtn.setBounds   (undoX, cy,      undoW, 13);
    redoBtn.setBounds   (undoX, cy + 15, undoW, 13);
}

void HeaderBar::adjustScale (float delta)
{
    if (auto* p = processor.apvts.getParameter (ParamIds::uiScale))
    {
        float current = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
        float newScale = juce::jlimit (0.5f, 3.0f, current + delta);
        p->setValueNotifyingHost (p->convertTo0to1 (newScale));
    }
}

void HeaderBar::paint (juce::Graphics& g)
{
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    g.fillAll (getTheme().header);
    headerCells.clear();

    const int h   = getHeight();
    const int cy  = h / 2;

    // ── SAMPLE info + SLICES + ROOT — left side, under logo ──────────────
    {
        const auto& ui = processor.getUiSliceSnapshot();
        const int lx   = 10;   // align with logo left edge
        const int cellY = cy - 15;
        const int cellH = 30;
        const int lw   = 55;   // width of each info cell
        const int gap  = 6;

        // SAMPLE label + filename (left-aligned, expands rightward)
        int sampleX = lx;

        if (ui.sampleMissing)
        {
            g.setFont (DysektLookAndFeel::makeFont (12.0f));
            g.setColour (juce::Colours::orange);
            g.drawText ("MISSING", sampleX, cellY + 2, 65, 13, juce::Justification::left);
            g.setFont (DysektLookAndFeel::makeFont (14.0f));
            g.setColour (juce::Colours::orange.withAlpha (0.9f));
            // filename truncated to available width before SLICES cell
            int maxW = undoBtn.getX() - gap - lw - gap - lw - gap - sampleX - 8;
            g.drawText (ui.sampleFileName + " (click to relink)",
                        sampleX, cellY + 15, maxW, 14, juce::Justification::left);
            sampleInfoBounds = { sampleX, cellY, maxW, cellH };
        }
        else if (ui.sampleLoaded)
        {
            double srate = processor.getSampleRate();
            if (srate <= 0) srate = 44100.0;
            double lenSec = ui.sampleNumFrames / srate;
            g.setFont (DysektLookAndFeel::makeFont (12.0f));
            g.setColour (getTheme().foreground.withAlpha (0.35f));
            g.drawText ("SAMPLE", sampleX, cellY + 2, 65, 13, juce::Justification::left);
            g.setFont (DysektLookAndFeel::makeFont (14.0f));
            g.setColour (getTheme().foreground.withAlpha (0.7f));
            int maxW = undoBtn.getX() - gap - lw - gap - lw - gap - sampleX - 8;
            g.drawText (ui.sampleFileName + " (" + juce::String (lenSec, 2) + "s)",
                        sampleX, cellY + 15, maxW, 14, juce::Justification::left);
            sampleInfoBounds = { sampleX, cellY, maxW, cellH };
        }
        else
        {
            sampleInfoBounds = {};
        }

        // SLICES — right of the sample info, left of ROOT, left of UNDO
        int slcX = undoBtn.getX() - gap - lw - gap - lw;
        int rnX  = undoBtn.getX() - gap - lw;

        // SLICES (read-only count)
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (getTheme().foreground.withAlpha (0.35f));
        g.drawText ("SLICES", slcX, cellY + 2, lw, 13, juce::Justification::left);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (getTheme().foreground.withAlpha (0.55f));
        g.drawText (juce::String (ui.numSlices), slcX, cellY + 15, lw, 14, juce::Justification::left);

        // ROOT (draggable/editable when no slices exist)
        bool editable = (ui.numSlices == 0);
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (editable ? getTheme().accent.withAlpha (0.7f)
                              : getTheme().foreground.withAlpha (0.35f));
        g.drawText ("ROOT", rnX, cellY + 2, lw, 13, juce::Justification::left);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (editable ? getTheme().foreground.withAlpha (0.6f)
                              : getTheme().foreground.withAlpha (0.4f));
        g.drawText (juce::String (ui.rootNote), rnX, cellY + 15, lw, 14, juce::Justification::left);

        rootNoteArea   = { rnX,  cellY, lw, cellH };
        slicesInfoArea = { slcX, cellY, lw, cellH };
    }
}

void HeaderBar::mouseDown (const juce::MouseEvent& e)
{
    if (textEditor != nullptr)
        textEditor.reset();

    const auto& ui = processor.getUiSliceSnapshot();
    auto pos = e.getPosition();
    activeDragCell = -1;

    // Click on sample info area — open file browser or relink
    if (sampleInfoBounds.contains (pos))
    {
        if (ui.sampleMissing)
            openRelinkBrowser();
        else
            openFileBrowser();
        return;
    }

    // ROOT drag (only draggable when no slices)
    if (rootNoteArea.contains (pos) && ui.numSlices == 0)
    {
        draggingRoot = true;
        dragStartY   = pos.y;
        dragStartValue = (float) ui.rootNote;
        return;
    }
}

void HeaderBar::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingRoot)
    {
        float delta = (float) (dragStartY - e.y) * 0.3f;
        int newRoot = juce::jlimit (0, 127, (int) std::round (dragStartValue + delta));
        DysektProcessor::Command cmd;
        cmd.type      = DysektProcessor::CmdSetRootNote;
        cmd.intParam1 = newRoot;
        processor.pushCommand (cmd);
        repaint();
    }
}

void HeaderBar::mouseUp (const juce::MouseEvent& /*e*/)
{
    draggingRoot   = false;
    activeDragCell = -1;
}

void HeaderBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto& ui = processor.getUiSliceSnapshot();
    if (rootNoteArea.contains (e.getPosition()) && ui.numSlices == 0)
    {
        textEditor = std::make_unique<juce::TextEditor>();
        addAndMakeVisible (*textEditor);
        textEditor->setBounds (rootNoteArea.getX(), rootNoteArea.getY() + 15,
                               rootNoteArea.getWidth(), 16);
        textEditor->setFont (DysektLookAndFeel::makeFont (14.0f));
        textEditor->setColour (juce::TextEditor::backgroundColourId, getTheme().header.brighter (0.2f));
        textEditor->setColour (juce::TextEditor::textColourId, juce::Colours::white);
        textEditor->setColour (juce::TextEditor::outlineColourId, getTheme().accent);
        textEditor->setText (juce::String (ui.rootNote), false);
        textEditor->selectAll();
        textEditor->grabKeyboardFocus();

        textEditor->onReturnKey = [this] {
            if (textEditor == nullptr) return;
            int val = juce::jlimit (0, 127, textEditor->getText().getIntValue());
            DysektProcessor::Command cmd;
            cmd.type      = DysektProcessor::CmdSetRootNote;
            cmd.intParam1 = val;
            processor.pushCommand (cmd);
            textEditor.reset();
            repaint();
        };
        textEditor->onEscapeKey  = [this] { textEditor.reset(); repaint(); };
        textEditor->onFocusLost  = [this] { textEditor.reset(); repaint(); };
    }
}

void HeaderBar::showThemePopup()
{
    auto* editor = dynamic_cast<DysektEditor*> (getParentComponent());
    if (editor == nullptr)
        return;

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
            if (result == 100)
                adjustScale (-0.25f);
            else if (result == 101)
                adjustScale (0.25f);
            else if (result > 0 && result <= themes.size())
                editor->applyTheme (themes[result - 1]);
        });
}

void HeaderBar::openFileBrowser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Audio File",
        juce::File(),
        "*.wav;*.ogg;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
            {
                processor.loadFileAsync (result);
                processor.zoom.store (1.0f);
                processor.scroll.store (0.0f);
            }
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
            {
                processor.relinkFileAsync (result);
            }
        });
}
