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
    addAndMakeVisible (filBtn);
    addAndMakeVisible (waBtn);
    addAndMakeVisible (chBtn);

    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn })
    {
        btn->setAlwaysOnTop (true);
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    for (auto* btn : { &filBtn, &waBtn, &chBtn })
    {
        btn->setAlwaysOnTop (true);
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().accent);
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

    filBtn.setTooltip ("Toggle File Browser");
    filBtn.onClick = [this] {
        browserActive = ! browserActive;
        updateAccentBtn (filBtn, browserActive);
        if (onBrowserToggle) onBrowserToggle();
    };

    waBtn.setTooltip ("Toggle Soft Waveform");
    waBtn.onClick = [this] {
        waveActive = ! waveActive;
        updateAccentBtn (waBtn, waveActive);
        if (onWaveToggle) onWaveToggle();
    };

    chBtn.setTooltip ("Chromatic Mode — play selected slice across full keyboard");
    chBtn.onClick = [this] {
        chromaticActive = ! chromaticActive;
        updateAccentBtn (chBtn, chromaticActive);
        if (onChromaticToggle) onChromaticToggle();
    };
}

void HeaderBar::resized()
{
    const int h    = getHeight();
    const int cy   = (h - 28) / 2;
    const int gap  = 4;
    int right = getWidth() - 8;

    // Rightmost: UI (theme) button
    themeBtn.setBounds (right - 26, cy, 26, 28);
    right -= 30;

    // PANIC button — full height
    const int panicW = 52;
    panicBtn.setBounds (right - panicW, cy, panicW, 28);
    right -= panicW + gap;

    // UNDO / REDO stacked
    const int undoW = 44;
    undoBtn.setBounds (right - undoW, cy,      undoW, 13);
    redoBtn.setBounds (right - undoW, cy + 15, undoW, 13);
    right -= undoW + gap;

    // GLOBAL box: pitch + volume knobs — 72px wide
    // Hidden as real buttons; drawn in paint(). Store bounds for hit-test.
    globalBoxBounds = { right - 72, 2, 72, h - 4 };
    right -= 76;

    // Icon buttons: CH | WA | FIL  (right-to-left)
    const int iconW = 28;
    const int iconCY = (h - iconW) / 2;
    chBtn.setBounds  (right - iconW, iconCY, iconW, iconW);
    right -= iconW + gap;
    waBtn.setBounds  (right - iconW, iconCY, iconW, iconW);
    right -= iconW + gap;
    filBtn.setBounds (right - iconW, iconCY, iconW, iconW);
    // right boundary for text content is now filBtn.getX()
}

void HeaderBar::updateAccentBtn (juce::TextButton& btn, bool active)
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

void HeaderBar::setBrowserActive   (bool v) { browserActive    = v; updateAccentBtn (filBtn, v); }
void HeaderBar::setWaveActive      (bool v) { waveActive       = v; updateAccentBtn (waBtn,  v); }
void HeaderBar::setChromaticActive (bool v) { chromaticActive  = v; updateAccentBtn (chBtn,  v); }

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
    // Refresh standard button colours
    for (auto* btn : { &undoBtn, &redoBtn, &panicBtn, &themeBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }
    // Refresh icon button states (drawn as custom icons, but still use button background)
    updateAccentBtn (filBtn, browserActive);
    updateAccentBtn (waBtn,  waveActive);
    updateAccentBtn (chBtn,  chromaticActive);
    // Hide text labels — icons drawn manually below
    for (auto* btn : { &filBtn, &waBtn, &chBtn })
        btn->setButtonText ("");

    g.fillAll (getTheme().header);
    headerCells.clear();

    const int h  = getHeight();
    const int cy = h / 2;
    const auto accent = getTheme().accent;
    const auto fg     = getTheme().foreground;

    // ── Draw icon buttons ─────────────────────────────────────────────────
    auto drawIcon = [&] (juce::Component& btn, bool active, int type)
    {
        auto b = btn.getBounds();
        float cx = b.getCentreX(), bcy = b.getCentreY();
        auto col = active ? accent : fg.withAlpha (0.45f);
        g.setColour (col);
        if (type == 0) // Folder icon (FIL)
        {
            // Tab on top-left
            g.fillRoundedRectangle (cx-8, bcy-3, 7, 3, 1.f);
            // Main folder body
            g.fillRoundedRectangle (cx-9, bcy-2, 18, 11, 1.5f);
        }
        else if (type == 1) // Waveform (WA)
        {
            float pts[] = {-8,-1, -6,-4, -4,0, -2,4, 0,-6, 2,6, 4,-2, 6,2, 8,0};
            juce::Path p;
            for (int i = 0; i < 9; i++)
            {
                float px = cx + pts[i*2], py = bcy + pts[i*2+1];
                i==0 ? p.startNewSubPath(px,py) : p.lineTo(px,py);
            }
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }
        else // Piano keyboard (CH)
        {
            // White keys
            for (int k = 0; k < 5; ++k)
                g.fillRect ((int)(cx-9+k*4), (int)(bcy-4), 3, 9);
            // Black keys (gaps between C-D, D-E, F-G, G-A, A-B)
            g.setColour (active ? accent.darker(0.6f) : fg.withAlpha(0.2f));
            for (int kb : {0,1,3,4})
                g.fillRect ((int)(cx-7+kb*4), (int)(bcy-4), 2, 5);
        }
    };
    drawIcon (filBtn, browserActive, 0);
    drawIcon (waBtn,  waveActive,    1);
    drawIcon (chBtn,  chromaticActive, 2);

    // ── GLOBAL box: pitch + volume knobs ─────────────────────────────────
    {
        auto gb = globalBoxBounds;
        // Box frame
        g.setColour (accent.withAlpha (0.5f));
        g.drawRoundedRectangle (gb.toFloat().reduced (0.5f), 2.f, 1.f);
        // Label
        g.setFont (DysektLookAndFeel::makeFont (8.0f));
        g.setColour (accent.withAlpha (0.7f));
        g.drawText ("GLOBAL", gb.getX(), gb.getY() + 1, gb.getWidth(), 9,
                    juce::Justification::centred);

        // Two mini knobs: PITCH (left) and VOLUME (right)
        float gPitch = processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();
        float gVol   = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
        float pitchN = (gPitch + 48.f) / 96.f;
        float volN   = (gVol + 100.f) / 124.f;

        const int kr = 8;
        const float kStart = juce::MathConstants<float>::pi * 1.25f;
        const float kEnd   = juce::MathConstants<float>::pi * 2.75f;

        auto drawGlobalKnob = [&] (int kcx, int kcy, float norm,
                                   const juce::String& lbl, const juce::String& val)
        {
            float angle = kStart + norm * (kEnd - kStart);
            juce::Path track, arc;
            track.addCentredArc ((float)kcx,(float)kcy,(float)kr,(float)kr,0.f,kStart,kEnd,true);
            g.setColour (getTheme().darkBar.brighter(0.3f));
            g.strokePath (track, juce::PathStrokeType (1.5f));
            arc.addCentredArc ((float)kcx,(float)kcy,(float)kr,(float)kr,0.f,kStart,angle,true);
            g.setColour (accent);
            g.strokePath (arc, juce::PathStrokeType (2.f));
            g.setColour (accent.brighter(0.2f));
            float lr = (float)kr - 2.f;
            g.drawLine ((float)kcx,(float)kcy,
                        (float)kcx+lr*std::cos(angle),(float)kcy+lr*std::sin(angle),1.2f);
            g.setFont (DysektLookAndFeel::makeFont (8.f));
            g.setColour (accent.withAlpha(0.75f));
            g.drawText (lbl, kcx-16, kcy+kr+1, 32, 9, juce::Justification::centred);
        };

        int k1cx = gb.getX() + gb.getWidth()/4;
        int k2cx = gb.getX() + 3*gb.getWidth()/4;
        int kcy  = gb.getY() + 11 + kr;

        int pvi = (int)std::round(gPitch);
        juce::String pitchStr = (pvi>=0?"+":"")+juce::String(pvi)+"st";
        juce::String volStr   = (gVol>=0.f?"+":"")+juce::String(gVol,1)+"dB";

        drawGlobalKnob (k1cx, kcy, pitchN, "PITCH", pitchStr);
        drawGlobalKnob (k2cx, kcy, volN,   "VOL",   volStr);

        // Store hit areas for mouse interaction
        globalPitchKnobArea  = { k1cx-kr-4, kcy-kr-2, (kr+4)*2, (kr+4)*2+10 };
        globalVolKnobArea    = { k2cx-kr-4, kcy-kr-2, (kr+4)*2, (kr+4)*2+10 };
    }

    // ── SAMPLE info + SLICES + SET ROOT — left side ───────────────────────
    {
        const auto& ui = processor.getUiSliceSnapshot();
        const int lx    = 10;
        const int cellY = cy - 15;
        const int cellH = 30;
        const int lw    = 60;
        const int gap   = 8;
        const auto white  = juce::Colours::white;
        const auto white2 = juce::Colours::white.withAlpha (0.5f);

        // Right boundary: left edge of FIL button
        int rightBound = filBtn.getX() - gap;
        int rnX  = rightBound - lw;
        int slcX = rnX - gap - lw;

        // SAMPLE label (white) + name only (no extension, no length)
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
            // Name only: strip extension
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

        // SLICES — white label, right-aligned count
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (white);
        g.drawText ("SLICES", slcX, cellY + 2, lw, 13, juce::Justification::left);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (fg.withAlpha (0.65f));
        g.drawText (juce::String (ui.numSlices), slcX, cellY + 15, lw, 14,
                    juce::Justification::right);  // right-aligned

        // SET ROOT — white label, accent value
        bool editable = (ui.numSlices == 0);
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (white);
        g.drawText ("SET ROOT", rnX, cellY + 2, lw, 13, juce::Justification::left);
        g.setFont (DysektLookAndFeel::makeFont (14.0f));
        g.setColour (editable ? accent : fg.withAlpha (0.5f));
        g.drawText (juce::String (ui.rootNote), rnX, cellY + 15, lw, 14,
                    juce::Justification::left);

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
    globalDragTarget = GlobalDragTarget::None;

    // Global PITCH knob
    if (globalPitchKnobArea.contains (pos))
    {
        globalDragTarget    = GlobalDragTarget::Pitch;
        globalDragStartY    = pos.y;
        globalDragStartValue = processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();
        if (auto* p = processor.apvts.getParameter (ParamIds::defaultPitch))
            p->beginChangeGesture();
        return;
    }

    // Global VOLUME knob
    if (globalVolKnobArea.contains (pos))
    {
        globalDragTarget    = GlobalDragTarget::Volume;
        globalDragStartY    = pos.y;
        globalDragStartValue = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
        if (auto* p = processor.apvts.getParameter (ParamIds::masterVolume))
            p->beginChangeGesture();
        return;
    }

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
    float delta = (float) (globalDragStartY - e.y);

    if (globalDragTarget == GlobalDragTarget::Pitch)
    {
        float sens  = e.mods.isShiftDown() ? 0.05f : 0.5f;
        float newV  = juce::jlimit (-48.f, 48.f, globalDragStartValue + delta * sens);
        if (auto* p = processor.apvts.getParameter (ParamIds::defaultPitch))
            p->setValueNotifyingHost (p->convertTo0to1 (newV));
        repaint(); return;
    }

    if (globalDragTarget == GlobalDragTarget::Volume)
    {
        float sens  = e.mods.isShiftDown() ? 0.1f : 1.0f;
        float newV  = juce::jlimit (-100.f, 24.f, globalDragStartValue + delta * sens);
        if (auto* p = processor.apvts.getParameter (ParamIds::masterVolume))
            p->setValueNotifyingHost (p->convertTo0to1 (newV));
        repaint(); return;
    }

    if (draggingRoot)
    {
        float d = (float) (dragStartY - e.y) * 0.3f;
        int newRoot = juce::jlimit (0, 127, (int) std::round (dragStartValue + d));
        DysektProcessor::Command cmd;
        cmd.type      = DysektProcessor::CmdSetRootNote;
        cmd.intParam1 = newRoot;
        processor.pushCommand (cmd);
        repaint();
    }
}

void HeaderBar::mouseUp (const juce::MouseEvent& /*e*/)
{
    if (globalDragTarget == GlobalDragTarget::Pitch)
        if (auto* p = processor.apvts.getParameter (ParamIds::defaultPitch))
            p->endChangeGesture();
    if (globalDragTarget == GlobalDragTarget::Volume)
        if (auto* p = processor.apvts.getParameter (ParamIds::masterVolume))
            p->endChangeGesture();

    globalDragTarget = GlobalDragTarget::None;
    draggingRoot     = false;
    activeDragCell   = -1;
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
