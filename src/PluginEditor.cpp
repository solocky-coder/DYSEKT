#include "PluginEditor.h"
#include <filesystem>
#include "ui/PluginEditorConstants.h"

// ========================== FILEPATH HELPERS ==========================
static juce::File getSettingsDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
           .getChildFile ("DYSEKT");
}
static juce::File getUserSettingsFile() { return getSettingsDir().getChildFile ("settings.yaml"); }
static juce::File getThemesDir() { return getSettingsDir().getChildFile ("themes"); }

// ========================= CLASS CONSTRUCTOR ==========================
DysektEditor::DysektEditor (DysektProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      logoBar (p),
      headerBar (p),
      sliceLcd (p),
      sliceWaveformLcd (p),
      sliceLane (p),
      waveformView (p),
      waveformOverview (p),
      sliceControlBar (p),
      actionPanel (p, waveformView),
      padGridView (p),
      browserPanel (p),
      mixerPanel (p),
      shortcutsPanel (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);

    addAndMakeVisible (logoBar);
    addAndMakeVisible (headerBar);

    addAndMakeVisible (sliceLcd);
    addAndMakeVisible (sliceWaveformLcd);
    if (auto* cf = headerBar.getControlFrame())
        addAndMakeVisible (*cf);

    addAndMakeVisible (sliceLane);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (waveformOverview);
    addAndMakeVisible (sliceControlBar);
    addAndMakeVisible (actionPanel);

    // Pad grid — starts hidden; shown when uiMode == 1
    padGridView.setVisible (false);
    addChildComponent (padGridView);

    browserPanel.setVisible (false);
    addChildComponent (browserPanel);
    mixerPanel.setVisible (false);
    addChildComponent (mixerPanel);
    shortcutsPanel.setVisible (false);
    addChildComponent (shortcutsPanel);
    shortcutsPanel.onDismiss = [this] { toggleShortcutsPanel(); };
    shortcutsPanel.onThemeRequest = [this]
    {
        shortcutsPanel.setVisible (false);
        toggleThemeEditor();
    };
    // Interface mode toggle — switching routes through setUiMode() so the
    // original waveform UI is never destroyed, just hidden.
    headerBar.dualFrame().onUiModeChanged = [this] (int mode) { setUiMode (mode); };

    sliceLane.setWaveformView (&waveformView);

    browserPanel.onFileLoaded = [this] { if (browserOpen) toggleBrowserPanel(); };
    browserPanel.onLoadRequest = [this] (const juce::File& f) { showTrimDialog (f); };
    waveformView.onLoadRequest = [this] (const juce::File& f) { showTrimDialog (f); };
    waveformView.onShortcutsToggle = [this] { toggleShortcutsPanel(); };
    waveformView.onRenameRequest   = [this] (int sliceIdx, const juce::String& currentName)
    {
        renameOverlay = std::make_unique<RenameOverlay> (sliceIdx + 1, currentName);
        addAndMakeVisible (*renameOverlay);
        renameOverlay->setBounds (getLocalBounds());
        renameOverlay->toFront (true);
        renameOverlay->onResult = [this, sliceIdx] (const juce::String& newName, bool cancelled)
        {
            renameOverlay.reset();
            if (! cancelled)
            {
                DysektProcessor::Command cmd;
                cmd.type        = DysektProcessor::CmdSetSliceName;
                cmd.intParam1   = sliceIdx;
                cmd.stringParam = newName;
                processor.pushCommand (cmd);
            }
        };
    };
    waveformView.onTrimApplied = [this] (int s, int e)
    {
        processor.applyTrimToCurrentSample (s, e);
        processor.trimModeActive.store (false, std::memory_order_relaxed);
        waveformView.setTrimMode (false);
        trimSession.reset();
        trimDialog.reset();
        resized();
    };
    waveformView.onTrimCancelled = [this]
    {
        processor.trimModeActive.store (false, std::memory_order_relaxed);
        waveformView.setTrimMode (false);
        trimSession.reset();
        trimDialog.reset();
        resized();
    };

    headerBar.onBodeToggle = [this] { toggleMixerPanel(); };
    headerBar.onBrowserToggle = [this] { toggleBrowserPanel(); };
    headerBar.onWaveToggle = [this] { toggleSoftWave(); };
    headerBar.onMidiFollowToggle = [this] { toggleMidiFollow(); };
    headerBar.onShortcutsToggle = [this] { toggleShortcutsPanel(); };

    ensureDefaultThemes();
    loadUserSettings();

    if (processor.sampleData.getSnapshot() == nullptr)
        processor.loadDefaultSampleIfNeeded();

    float apvtsScale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    if (apvtsScale == 1.0f && savedScale > 0.0f && savedScale != apvtsScale)
    {
        if (auto* param = processor.apvts.getParameter (ParamIds::uiScale))
            param->setValueNotifyingHost (param->convertTo0to1 (savedScale));
    }

    setWantsKeyboardFocus (true);
    setResizable (true, false);
    setResizeLimits (kBaseW, 650, 3840, 2160);
    setSize (kBaseW, kTotalH);
    lastUiSnapshotVersion = processor.getUiSliceSnapshotVersion();
    startTimerHz (30);
}

DysektEditor::~DysektEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

int DysektEditor::computeTotalHeight() const { return kTotalH; }

// ── Interface mode switch ─────────────────────────────────────────────────────
void DysektEditor::setUiMode (int mode)
{
    if (uiMode == mode) return;
    uiMode = mode;

    // Keep the EDIT|PAD tab in sync
    headerBar.dualFrame().setPadGridActive (uiMode == 1);

    // Hide waveform overview immediately when switching to PAD mode
    waveformOverview.setVisible (uiMode == 0);

    // Persist the new mode
    float scale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    saveUserSettings (scale, getTheme().name);

    resized();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
void DysektEditor::toggleBrowserPanel()
{
    if (browserOpen) {
        browserOpen = false;
        browserPanel.setVisible (false);
        headerBar.setBrowserActive (false);
    } else {
        if (mixerOpen) {
            mixerOpen = false;
            mixerPanel.setVisible (false);
            headerBar.setBodeActive (false);
        }
        browserOpen = true;
        browserPanel.setVisible (true);
        headerBar.setBrowserActive (true);
    }
    resized(); repaint(); resized(); repaint();
}

void DysektEditor::showTrimDialog (const juce::File& file, bool isRelink)
{
    if (isRelink) {
        processor.loadFileAsync (file);
        return;
    }
    auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".sf2" || ext == ".sfz") {
        processor.loadSoundFontAsync (file);
        return;
    }
    const int pref = processor.trimPreference.load (std::memory_order_relaxed);
    if (pref == DysektProcessor::TrimPrefNever) {
        processor.loadFileAsync (file);
        processor.zoom.store (1.0f);
        processor.scroll.store (0.0f);
        return;
    }
    if (pref == DysektProcessor::TrimPrefAsk) {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        double duration = 0.0;
        if (reader != nullptr && reader->sampleRate > 0.0)
            duration = (double) reader->lengthInSamples / reader->sampleRate;

        if (duration < 5.0)
        {
            processor.loadFileAsync (file);
            processor.zoom.store (1.0f);
            processor.scroll.store (0.0f);
            return;
        }

        confirmOverlay = std::make_unique<ConfirmOverlay> (
            "Trim Sample?",
            "This sample is long.  Would you like to trim it before slicing?",
            "Trim",
            "No Thanks");
        addAndMakeVisible (*confirmOverlay);
        confirmOverlay->setBounds (getLocalBounds());
        confirmOverlay->toFront (true);
        confirmOverlay->onResult = [this, file] (bool trim)
        {
            confirmOverlay.reset();
            if (trim)
                showTrimMode (file);
            else
            {
                processor.loadFileAsync (file);
                processor.zoom.store (1.0f);
                processor.scroll.store (0.0f);
            }
        };
        return;
    }
    showTrimMode (file);
}

void DysektEditor::showTrimMode (const juce::File& file)
{
    trimSession = std::make_unique<TrimSession>();
    trimSession->file = file;
    trimSession->active = false;

    processor.loadFileAsync (file);
    processor.zoom.store (1.0f);
    processor.scroll.store (0.0f);
}

void DysektEditor::toggleSoftWave()
{
    waveformMode = (waveformMode + 1) % 8;
    waveformView.setWaveformMode (waveformMode);
    waveformOverview.setWaveformMode (waveformMode);
    actionPanel.setWaveActive (waveformMode != 0);
    headerBar.setBrowserActive (browserOpen);
    headerBar.setWaveMode (waveformMode);
    float scale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    saveUserSettings (scale, getTheme().name);
}

void DysektEditor::toggleMidiFollow()
{
    const bool newVal = ! processor.midiSelectsSlice.load();
    processor.midiSelectsSlice.store (newVal);
    headerBar.setMidiFollowActive (newVal);
}

void DysektEditor::toggleShortcutsPanel()
{
    const bool show = ! shortcutsPanel.isVisible();
    // Sync the current mode into the panel before showing it
    shortcutsPanel.setVisible (show);
    if (show)
    {
        shortcutsPanel.setBounds (getLocalBounds());
        shortcutsPanel.toFront (true);
        shortcutsPanel.grabKeyboardFocus();
    }
}

void DysektEditor::toggleThemeEditor()
{
    if (themeEditorPanel != nullptr)
    {
        themeEditorPanel.reset();
        repaint();
        return;
    }

    themeEditorPanel = std::make_unique<ThemeEditorPanel> (getThemesDir());

    themeEditorPanel->onThemeChanged = [this] (const ThemeData& t)
    {
        setTheme (t);
        processor.sliceManager.setSlicePalette (t.slicePalette);
        repaint();
    };

    themeEditorPanel->onThemeSaved = [this] (const juce::String& name)
    {
        processor.sliceManager.setSlicePalette (getTheme().slicePalette);
        saveUserSettings (processor.apvts.getRawParameterValue (ParamIds::uiScale)->load(), name);
        repaint();
    };

    themeEditorPanel->onDismiss = [this] { toggleThemeEditor(); };

    addAndMakeVisible (*themeEditorPanel);
    themeEditorPanel->setBounds (getLocalBounds());
    themeEditorPanel->toFront (true);
    themeEditorPanel->grabKeyboardFocus();
    repaint();
}

// ── Waveform frame rect helper ────────────────────────────────────────────────
static juce::Rectangle<float> waveformFrameRect (const DysektEditor& ed,
                                                   const juce::Rectangle<int>& wvBounds,
                                                   bool hasTrimDialog)
{
    const float sf        = (float) ed.getWidth() / (float) kBaseW;
    const int kFrameInset = juce::roundToInt (4.0f * sf);
    const int kFX         = juce::roundToInt (kMargin * sf);
    const int kFW         = ed.getWidth() - kFX * 2;
    const int trimExtra   = hasTrimDialog ? juce::roundToInt (kTrimBarH * sf) : 0;
    return { (float) kFX,
             (float) wvBounds.getY() - kFrameInset,
             (float) kFW,
             (float) (wvBounds.getHeight() + trimExtra + kFrameInset * 2) };
}

void DysektEditor::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().background);

    // Draw the CRT frame in waveform mode, or whenever trim mode forces the waveform visible
    const bool wvVisible = waveformView.isVisible() && waveformView.getHeight() > 0;

    if (wvVisible)
    {
        const auto outerF = waveformFrameRect (*this, waveformView.getBounds(), trimDialog != nullptr);

        juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0.f, outerF.getY(),
                                        juce::Colour (0xFF0E0E0E), 0.f, outerF.getBottom(), false);
        g.setGradientFill (outerGrad);
        g.fillRoundedRectangle (outerF, 4.0f);

        const float sf = (float) getWidth() / (float) kBaseW;
        const auto screenF = outerF.reduced (4.0f * sf);
        g.setColour (getTheme().darkBar.darker (0.55f));
        g.fillRoundedRectangle (screenF, 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (int y = juce::roundToInt (screenF.getY()); y < juce::roundToInt (screenF.getBottom()); y += 2)
            g.drawHorizontalLine (y, screenF.getX(), screenF.getRight());

        juce::ColourGradient glow (getTheme().accent.withAlpha (0.06f), 0.f, screenF.getY(),
                                   juce::Colours::transparentBlack, 0.f, screenF.getY() + 20.f, false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (screenF, 2.0f);
    }
}

void DysektEditor::paintOverChildren (juce::Graphics& g)
{
    const bool modalActive = (midiLearnBackdrop != nullptr)
                          || shortcutsPanel.isVisible()
                          || (confirmOverlay   != nullptr)
                          || (renameOverlay    != nullptr)
                          || (themeEditorPanel != nullptr);
    if (modalActive) return;

    const bool wvVisible = waveformView.isVisible() && waveformView.getHeight() > 0;

    // Scale all border pixel amounts proportionally to avoid sub-pixel overlap
    // at non-integer UI scales (1.5×, 1.75× etc.)
    const float sf = (float) getWidth() / (float) kBaseW;

    if (wvVisible)
    {
        const auto outerF = waveformFrameRect (*this, waveformView.getBounds(), trimDialog != nullptr);
        const auto ac     = getTheme().accent;

        g.setColour (ac.withAlpha (0.18f));
        g.drawRoundedRectangle (outerF.expanded (1.0f * sf), 5.0f * sf, 1.0f * sf);

        g.setColour (ac.withAlpha (0.60f));
        g.drawRoundedRectangle (outerF.reduced (0.5f * sf), 4.0f * sf, 1.5f * sf);

        const auto screenF = outerF.reduced (4.0f * sf);
        g.setColour (ac.withAlpha (0.30f));
        g.drawRoundedRectangle (screenF.expanded (0.5f * sf), 2.0f * sf, 1.0f * sf);
    }

    // Pad grid frame border — identical recipe and width as the waveform frame
    const bool padVisible = padGridView.isVisible() && padGridView.getHeight() > 0;
    if (padVisible)
    {
        const auto outerF = waveformFrameRect (*this, padGridView.getBounds(), false);
        const auto ac     = getTheme().accent;

        g.setColour (ac.withAlpha (0.18f));
        g.drawRoundedRectangle (outerF.expanded (1.0f * sf), 5.0f * sf, 1.0f * sf);

        g.setColour (ac.withAlpha (0.60f));
        g.drawRoundedRectangle (outerF.reduced (0.5f * sf), 4.0f * sf, 1.5f * sf);

        g.setColour (ac.withAlpha (0.30f));
        g.drawRoundedRectangle (outerF.reduced (4.0f * sf), 2.0f * sf, 1.0f * sf);
    }

    // Logo frame border
    if (logoBar.isVisible() && logoBar.getHeight() > 0)
    {
        const auto ac = getTheme().accent;
        const juce::Rectangle<float> logoF (logoBar.getBounds().toFloat()
                                                .withTrimmedTop (4.0f * sf));
        g.setColour (ac.withAlpha (0.18f));
        g.drawRoundedRectangle (logoF.expanded (1.0f * sf), 5.0f * sf, 1.0f * sf);
        g.setColour (ac.withAlpha (0.72f));
        g.drawRoundedRectangle (logoF.reduced (0.5f * sf), 4.0f * sf, 1.5f * sf);
        g.setColour (ac.withAlpha (0.18f));
        g.drawRoundedRectangle (logoF.reduced (2.0f * sf), 3.5f * sf, 1.0f * sf);
    }

    // Full-window accent frame
    {
        const auto ac = getTheme().accent;
        const juce::Rectangle<float> win (getLocalBounds().toFloat());
        g.setColour (ac.withAlpha (0.60f));
        g.drawRoundedRectangle (win.reduced (2.0f * sf), 2.5f * sf, 1.5f * sf);
        g.setColour (ac.withAlpha (0.14f));
        g.drawRoundedRectangle (win.reduced (4.0f * sf), 2.0f * sf, 1.0f * sf);
    }
}

void DysektEditor::resized()
{
    // ── Scale factor: all fixed-pixel constants scale with component width ────
    // This prevents layout overlap when the host scales component bounds in
    // response to setTransform (e.g. at 1.5× and above on high-DPI displays).
    const float sf = (float) getWidth() / (float) kBaseW;
    auto si = [sf](int v) -> int { return juce::roundToInt ((float) v * sf); };

    auto area = juce::Rectangle (0, 0, getWidth(), getHeight());

    // ── Top strip ─────────────────────────────────────────────────────────────
    // In PAD mode shrink LCD rows to 65% — frees ~116px for the pad grid
    const int lcdRowH  = (uiMode == 1) ? juce::roundToInt (kLcdRowH   * sf * 0.65f) : si (kLcdRowH);
    const int ctrlFrmH = (uiMode == 1) ? juce::roundToInt (kCtrlFrameH * sf * 0.65f) : si (kCtrlFrameH);
    const int kTopStripH = si (kLogoH) + lcdRowH;
    auto topArea = area.removeFromTop (kTopStripH);
    auto topRow  = topArea.reduced (si (kMargin), si (4));

    const int sideW = (topRow.getWidth() - si (kCtrlFrameW) - si (kMargin) * 2) / 2;
    sliceLcd.setBounds (topRow.removeFromLeft (sideW));
    topRow.removeFromLeft (si (kMargin));

    auto centreCol = topRow.removeFromLeft (si (kCtrlFrameW));
    auto logoRow   = centreCol.removeFromTop (si (kLogoH));
    // logoBar placed after cfY is known — centred vertically between plugin top and CF top
    {
        const int btnBarY = centreCol.getBottom() - si (kBtnBarH) - si (4);
        headerBar.setBounds (centreCol.getX(), btnBarY, centreCol.getWidth(), si (kBtnBarH));
        if (auto* cf = headerBar.getControlFrame())
        {
            const int cfY = centreCol.getY() + (btnBarY - centreCol.getY() - ctrlFrmH) / 2;
            cf->setBounds (centreCol.getX(), cfY, centreCol.getWidth(), ctrlFrmH);
            // Centre logo: equal margin above and below between plugin top (y=0) and CF top
            const int logoY = (cfY - si (kLogoH)) / 2;
            logoBar.setBounds (logoRow.getX(), logoY, logoRow.getWidth(), si (kLogoH));
        }
        else
        {
            logoBar.setBounds (logoRow);   // fallback: top-aligned
        }
    }

    topRow.removeFromLeft (si (kMargin));
    sliceWaveformLcd.setBounds (topRow);

    auto actionArea = area.removeFromTop (si (kActionH));
    const int kFX = si (kMargin);
    const int kFW = getWidth() - si (kMargin) * 2;

    area.removeFromBottom (si (kMargin));
    const int slotH = (mixerOpen || browserOpen) ? si (kPanelSlotH) : 0;
    auto slot = area.removeFromBottom (slotH);
    area.removeFromBottom (si (kMargin));

    if (mixerOpen) {
        // Expand mixer to fill ALL available area (waveformView space + slot)
        const int mixTop = actionArea.getY();  // close gap: start at LCD bottom, not content area top
        const int mixBot = slot.getBottom();
        mixerPanel.setBounds (kFX, mixTop, kFW, mixBot - mixTop);
        browserPanel.setBounds ({});
    }
    else if (browserOpen) {
        browserPanel.setBounds (kFX, slot.getY(), kFW, slot.getHeight());
        mixerPanel.setBounds ({});
    } else {
        mixerPanel.setBounds ({});
        browserPanel.setBounds ({});
    }

    const int kFrameInset   = si (4);
    const int kOverviewH    = si (28);
    const int kInterGap     = si (kMargin) + kFrameInset;
    const int kOverviewRowH = kInterGap + kOverviewH + si (kMargin);

    int overviewTopGuard = area.getBottom();

    if (trimDialog != nullptr) {
        sliceControlBar.setBounds ({});
        waveformOverview.setBounds ({});
    } else {
        if ((trimDialog != nullptr || (uiMode == 0 && hasSampleLoaded)) && !mixerOpen)
        {
            auto scbArea = area.removeFromBottom (si (kSliceCtrlH));
            sliceControlBar.setBounds (juce::Rectangle (kFX, scbArea.getY(), kFW, si (kSliceCtrlH)));
        }
        else
        {
            sliceControlBar.setBounds ({});
            // SCB space NOT removed from area — pad grid uses it
        }

        if (uiMode == 0 && !mixerOpen && hasSampleLoaded)
        {
            auto overviewRow = area.removeFromBottom (kOverviewRowH);
            const int overviewY = overviewRow.getY() + kInterGap;
            waveformOverview.setBounds (kFX, overviewY, kFW, kOverviewH);
            overviewTopGuard = overviewRow.getY();
        }
        else
        {
            waveformOverview.setBounds ({});
        }
    }

    const int kFrameX  = kFX;
    const int kFrameW  = kFW;
    const int frameTop = actionArea.getY();
    const int frameBot = juce::jmin (area.getBottom(), overviewTopGuard);
    const int screenX  = kFrameX + kFrameInset;
    const int screenW  = kFrameW - kFrameInset * 2;
    const int screenTop = frameTop + kFrameInset;
    const int screenBot = frameBot - kFrameInset;

    actionPanel.setBounds ({});
    int y = screenTop;
    sliceLane.setBounds ({});

    int trimH = (trimDialog != nullptr) ? si (kTrimBarH) : 0;
    int h     = juce::jmax (si (80), screenBot - trimH - y);

    // ── Route the main content area to the active view ────────────────────────
    // Trim mode always requires the waveform view, regardless of uiMode.
    const bool trimActive = (trimDialog != nullptr || (trimSession != nullptr && trimSession->active));

    if (mixerOpen)
    {
        // Mixer is open — hide both main views so nothing overlaps the mixer panel
        waveformView.setVisible (false);
        waveformView.setBounds ({});
        padGridView.setVisible (false);
        padGridView.setBounds ({});
    }
    else if (uiMode == 0 || trimActive)
    {
        // Original waveform layout — unchanged
        waveformView.setVisible (true);
        waveformView.setBounds (juce::Rectangle (screenX, y, screenW, h));

        padGridView.setVisible (false);
        padGridView.setBounds ({});
    }
    else
    {
        // Pad grid layout
        padGridView.setVisible (true);
        padGridView.setBounds (juce::Rectangle (screenX, y, screenW, h));

        waveformView.setVisible (false);
        waveformView.setBounds ({});
    }

    if (trimDialog != nullptr)
        trimDialog->setBounds (screenX, y + h, screenW, kTrimBarH);

    if (shortcutsPanel.isVisible())
        shortcutsPanel.setBounds (getLocalBounds());

    if (midiLearnBackdrop != nullptr)
        midiLearnBackdrop->setBounds (getLocalBounds());
    if (midiLearnDialog != nullptr)
        midiLearnDialog->setBounds (getLocalBounds().reduced (40));
    if (confirmOverlay != nullptr)
        confirmOverlay->setBounds (getLocalBounds());
    if (themeEditorPanel != nullptr)
        themeEditorPanel->setBounds (getLocalBounds());
}

void DysektEditor::toggleMixerPanel()
{
    if (mixerOpen) {
        mixerOpen = false;
        mixerPanel.setVisible (false);
        headerBar.setBodeActive (false);
    } else {
        if (browserOpen) {
            browserOpen = false;
            browserPanel.setVisible (false);
            headerBar.setBrowserActive (false);
        }
        mixerOpen = true;
        mixerPanel.setVisible (true);
        headerBar.setBodeActive (true);
    }
    resized(); repaint(); resized(); repaint();
}

// ── Keyboard shortcuts ────────────────────────────────────────────────────────
bool DysektEditor::keyPressed (const juce::KeyPress& key)
{
    auto mods = key.getModifiers();
    int code  = key.getKeyCode();

    if (code == 'Z' && mods.isCommandDown() && mods.isShiftDown())
    { DysektProcessor::Command c; c.type = DysektProcessor::CmdRedo; processor.pushCommand (c); return true; }
    if (code == 'Z' && mods.isCommandDown())
    { DysektProcessor::Command c; c.type = DysektProcessor::CmdUndo; processor.pushCommand (c); return true; }

    if (mods.isCommandDown()) return false;

    if (code == juce::KeyPress::escapeKey && shortcutsPanel.isVisible())
    { toggleShortcutsPanel(); return true; }

    if (code == '?') { toggleShortcutsPanel(); return true; }

    if (code == 'M')
    {
        if (midiLearnDialog != nullptr)
        {
            midiLearnDialog.reset();
            midiLearnBackdrop.reset();
            resized();
        }
        else
        {
            struct Backdrop : public juce::Component {
                void paint (juce::Graphics& g) override {
                    g.fillAll (juce::Colours::black.withAlpha (0.55f));
                }
            };
            midiLearnBackdrop = std::make_unique<Backdrop>();
            addAndMakeVisible (*midiLearnBackdrop);
            midiLearnBackdrop->toFront (false);

            midiLearnDialog = std::make_unique<MidiLearnDialog> (
                processor.midiLearn,
                processor,
                [this] { midiLearnDialog.reset(); midiLearnBackdrop.reset(); resized(); }
            );
            addAndMakeVisible (*midiLearnDialog);
            midiLearnDialog->toFront (true);
            resized();
        }
        return true;
    }

    if (code == 'L')
    {
        DysektProcessor::Command c;
        c.type = processor.lazyChop.isActive() ? DysektProcessor::CmdLazyChopStop
                                                : DysektProcessor::CmdLazyChopStart;
        processor.pushCommand (c); repaint(); return true;
    }
    if (code == juce::KeyPress::deleteKey)
    {
        const auto& ui = processor.getUiSliceSnapshot();
        if (ui.selectedSlice >= 0)
        { DysektProcessor::Command c; c.type = DysektProcessor::CmdDeleteSlice; c.intParam1 = ui.selectedSlice; processor.pushCommand (c); }
        return true;
    }
    if (code == 'F') { toggleMidiFollow(); return true; }

    if (code == juce::KeyPress::rightKey)
    {
        const auto& ui = processor.getUiSliceSnapshot();
        if (ui.numSlices > 0)
        { DysektProcessor::Command c; c.type = DysektProcessor::CmdSelectSlice; c.intParam1 = juce::jlimit (0, ui.numSlices-1, ui.selectedSlice+1); processor.pushCommand (c); repaint(); }
        return true;
    }
    if (code == juce::KeyPress::leftKey)
    {
        const auto& ui = processor.getUiSliceSnapshot();
        if (ui.numSlices > 0)
        { DysektProcessor::Command c; c.type = DysektProcessor::CmdSelectSlice; c.intParam1 = juce::jlimit (0, ui.numSlices-1, ui.selectedSlice-1); processor.pushCommand (c); repaint(); }
        return true;
    }

    return false;
}

void DysektEditor::timerCallback()
{
    bool uiChanged = false, viewportChanged = false;
    const bool previewActive     = waveformView.hasActiveSlicePreview();
    const bool waveformInteracting = waveformView.isInteracting();

    const auto snapshotVersion = (uint32_t) processor.getUiSliceSnapshotVersion();
    if (snapshotVersion != lastUiSnapshotVersion) { lastUiSnapshotVersion = snapshotVersion; uiChanged = true; }

    {
        const bool procState = processor.midiSelectsSlice.load (std::memory_order_relaxed);
        headerBar.setMidiFollowActive (procState);
    }

    {
        const int curSlices = processor.sliceManager.getNumSlices();
        if (lastNumSlices == 0 && curSlices > 0)
        {
            processor.midiSelectsSlice.store (true, std::memory_order_relaxed);
            headerBar.setMidiFollowActive (true);
        }
        lastNumSlices = curSlices;
    }

    const float zoom = processor.zoom.load(), scroll = processor.scroll.load();
    if (zoom != lastZoom || scroll != lastScroll) { lastZoom = zoom; lastScroll = scroll; viewportChanged = true; }

    // MIDI follow: scroll waveform viewport
    if (processor.midiSelectsSlice.load (std::memory_order_relaxed))
    {
        const int followSlice = processor.midiFollowTriggeredSlice.load (std::memory_order_relaxed);
        if (followSlice >= 0 && followSlice != lastMidiFollowSlice)
        {
            lastMidiFollowSlice = followSlice;
            const float z = processor.zoom.load();
            if (z > 1.0f)
            {
                auto snap = processor.sampleData.getSnapshot();
                if (snap != nullptr && snap->buffer.getNumSamples() > 0)
                {
                    const int numFrames  = snap->buffer.getNumSamples();
                    const int visibleLen = (int) ((float) numFrames / z);
                    const int maxStart   = numFrames - visibleLen;
                    const auto& uiSnap   = processor.getUiSliceSnapshot();
                    if (maxStart > 0 && followSlice < uiSnap.numSlices)
                    {
                        const int sliceStart  = uiSnap.slices[(size_t) followSlice].startSample;
                        const int sliceEnd    = (followSlice + 1 < uiSnap.numSlices)
                                                  ? uiSnap.slices[(size_t)(followSlice + 1)].startSample
                                                  : numFrames;
                        const int sliceCenter = (sliceStart + sliceEnd) / 2;
                        const int newStart    = juce::jlimit (0, maxStart, sliceCenter - visibleLen / 2);
                        processor.scroll.store ((float) newStart / (float) maxStart,
                                                std::memory_order_relaxed);
                        viewportChanged = true;
                    }
                }
            }
        }
    }

    {
        const bool trimNow = processor.trimModeActive.load (std::memory_order_relaxed);
        if (trimNow != lastTrimActive)
        {
            lastTrimActive = trimNow;
            actionPanel.setTrimActive (trimNow);
        }
    }

    float userScale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    float scale     = userScale * hostScale;
    if (scaleDirty || scale != lastScale)
    {
        scaleDirty = false; lastScale = scale;
        // Reset to base size first so the host doesn't compound the scale on
        // top of a previously-enlarged component (avoids double-scaling at 1.5×+)
        setTransform (juce::AffineTransform{});
        setSize (kBaseW, kTotalH);
        setTransform (juce::AffineTransform::scale (scale));
        DysektLookAndFeel::setMenuScale (scale);
        saveUserSettings (scale, getTheme().name);
        uiChanged = true;
    }

    const bool playbackActive = std::any_of (processor.voicePool.voicePositions.begin(),
                                              processor.voicePool.voicePositions.end(),
                                              [] (const std::atomic<float>& pos) { return pos.load (std::memory_order_relaxed) > 0.0f; });

    const bool waveformAnimating = waveformInteracting || previewActive
                                || playbackActive || processor.lazyChop.isActive()
                                || (processor.liveDragSliceIdx.load (std::memory_order_relaxed) >= 0);
    const bool waveformShowing      = (uiMode == 0) || processor.trimModeActive.load (std::memory_order_relaxed);
    const bool waveformNeedsRepaint = waveformShowing && (uiChanged || viewportChanged || waveformAnimating || lastWaveformAnimating);
    const bool laneNeedsRepaint     = waveformShowing && (uiChanged || viewportChanged || previewActive || lastPreviewActive);

    lastWaveformAnimating = waveformAnimating;
    lastPreviewActive     = previewActive;

    if (trimSession != nullptr && ! trimSession->active)
    {
        auto snap = processor.sampleData.getSnapshot();
        if (snap != nullptr && snap->filePath == trimSession->file.getFullPathName())
        {
            // Trim mode requires the waveform view — auto-switch if in Pad Grid mode.
            if (uiMode != 0)
                setUiMode (0);

            trimSession->active = true;
            const int totalFrames = snap->buffer.getNumSamples();
            waveformView.enterTrimMode (0, totalFrames);

            processor.trimModeActive.store (true, std::memory_order_relaxed);
            processor.trimRegionStart.store (0, std::memory_order_relaxed);
            processor.trimRegionEnd  .store (totalFrames, std::memory_order_relaxed);

            if (trimDialog == nullptr)
            {
                trimDialog = std::make_unique<TrimDialog> (processor, waveformView);
                addAndMakeVisible (*trimDialog);
                trimDialog->toFront (false);
                resized();
            }
        }
    }

    if (processor.trimModeActive.load (std::memory_order_relaxed)
        && ! waveformView.isTrimDragging())
    {
        const int procStart = processor.trimRegionStart.load (std::memory_order_relaxed);
        const int procEnd   = processor.trimRegionEnd  .load (std::memory_order_relaxed);
        if (procStart != waveformView.getTrimIn() || procEnd != waveformView.getTrimOut())
            waveformView.setTrimPoints (procStart, procEnd);
    }

    const int targetHz = waveformAnimating ? 60 : 30;
    if (targetHz != timerHz) { startTimerHz (targetHz); timerHz = targetHz; }

    if (waveformNeedsRepaint) waveformView.repaint();
    if (laneNeedsRepaint)     sliceLane.repaint();

    // Pad grid refresh
    if (uiMode == 1 && (uiChanged || playbackActive))
        padGridView.repaintGrid();

    sliceLcd.repaintLcd();
    sliceWaveformLcd.repaintLcd();

    {
        const bool hasSample = (processor.sampleData.getSnapshot() != nullptr
                                 && processor.sampleData.getSnapshot()->buffer.getNumSamples() > 0);
        if (hasSample != hasSampleLoaded)
        {
            hasSampleLoaded = hasSample;
            resized();   // expand/collapse SCB + zoom bar
        }
        const bool overviewShouldShow = hasSample && (uiMode == 0);
        if (overviewShouldShow != waveformOverview.isVisible())
        {
            waveformOverview.setVisible (overviewShouldShow);
            resized();
        }
    }
    if (waveformOverview.isVisible())
        waveformOverview.repaintOverview();
    if (mixerOpen) mixerPanel.repaint();

    headerBar.repaint();
    sliceControlBar.updateMidiLearnPulse();
    sliceControlBar.repaint();
    if (uiChanged) actionPanel.repaint();
    if (mixerOpen) mixerPanel.updateFromSnapshot();
}

void DysektEditor::ensureDefaultThemes()
{
    auto dir = getThemesDir(); dir.createDirectory();
    auto write = [&] (const juce::String& name, const ThemeData& t)
    {
        auto f = dir.getChildFile (name + ".dsk");
        if (! f.existsAsFile()) f.replaceWithText (t.toThemeFile());
    };
    write ("dark",      ThemeData::darkTheme());
    write ("shell",     ThemeData::shellTheme());
    write ("lazy",      ThemeData::lazyTheme());
    write ("snow",      ThemeData::snowTheme());
    write ("ghost",     ThemeData::ghostTheme());
    write ("hack",      ThemeData::hackTheme());
    write ("midnight",  ThemeData::midnightTheme());
    write ("pigments",  ThemeData::pigmentsTheme());
    write ("dysekt",    ThemeData::dysektTheme());
}

juce::StringArray DysektEditor::getAvailableThemes()
{
    juce::StringArray names;
    for (auto& f : getThemesDir().findChildFiles (juce::File::findFiles, false, "*.dsk"))
    {
        auto t = ThemeData::fromThemeFile (f.loadFileAsString());
        if (t.name.isNotEmpty()) names.add (t.name);
    }
    if (names.isEmpty()) { names.add ("dark"); names.add ("shell"); }
    return names;
}

void DysektEditor::applyTheme (const juce::String& themeName)
{
    for (auto& f : getThemesDir().findChildFiles (juce::File::findFiles, false, "*.dsk"))
    {
        auto t = ThemeData::fromThemeFile (f.loadFileAsString());
        if (t.name == themeName)
        {
            setTheme (t);
            processor.sliceManager.setSlicePalette (getTheme().slicePalette);
            saveUserSettings (processor.apvts.getRawParameterValue (ParamIds::uiScale)->load(), themeName);
            repaint(); return;
        }
    }
    if      (themeName == "shell")    setTheme (ThemeData::shellTheme());
    else if (themeName == "lazy")     setTheme (ThemeData::lazyTheme());
    else if (themeName == "snow")     setTheme (ThemeData::snowTheme());
    else if (themeName == "ghost")    setTheme (ThemeData::ghostTheme());
    else if (themeName == "hack")     setTheme (ThemeData::hackTheme());
    else if (themeName == "midnight") setTheme (ThemeData::midnightTheme());
    else if (themeName == "pigments") setTheme (ThemeData::pigmentsTheme());
    else if (themeName == "dysekt")   setTheme (ThemeData::dysektTheme());
    else                              setTheme (ThemeData::darkTheme());
    processor.sliceManager.setSlicePalette (getTheme().slicePalette);
    saveUserSettings (processor.apvts.getRawParameterValue (ParamIds::uiScale)->load(), themeName);
    repaint();
}

void DysektEditor::setScaleFactor (float newScale)
{
    hostScale = newScale;
    scaleDirty = true;
}

void DysektEditor::saveUserSettings (float scale, const juce::String& themeName)
{
    auto file = getUserSettingsFile();
    file.getParentDirectory().createDirectory();

    file.replaceWithText ("uiScale: "   + juce::String (scale, 2)
                        + "\ntheme: "   + themeName
                        + "\nwaveStyle: " + juce::String (waveformMode)
                        + "\nuiMode: "  + juce::String (uiMode) + "\n");
}

void DysektEditor::loadUserSettings()
{
    savedScale = -1.0f;
    juce::String themeName = "dark";
    auto file = getUserSettingsFile();
    if (file.existsAsFile())
    {
        for (auto line : juce::StringArray::fromLines (file.loadFileAsString()))
        {
            line = line.trim();
            if (line.startsWith ("uiScale:"))
            {
                float v = line.fromFirstOccurrenceOf (":", false, false).trim().getFloatValue();
                if (v >= 0.5f && v <= 3.0f) savedScale = v;
            }
            else if (line.startsWith ("theme:"))
            {
                themeName = line.fromFirstOccurrenceOf (":", false, false).trim();
            }
            else if (line.startsWith ("waveStyle:"))
            {
                auto val = line.fromFirstOccurrenceOf (":", false, false).trim();
                if      (val == "soft") waveformMode = 1;
                else if (val == "hard") waveformMode = 0;
                else                   waveformMode = juce::jlimit (0, 7, val.getIntValue());
            }
            else if (line.startsWith ("uiMode:"))
            {
                auto val = line.fromFirstOccurrenceOf (":", false, false).trim().getIntValue();
                uiMode = juce::jlimit (0, 1, val);
            }
        }
    }
    applyTheme (themeName);

    waveformView.setWaveformMode (waveformMode);
    waveformOverview.setWaveformMode (waveformMode);
    padGridView.setWaveformMode (waveformMode);
    headerBar.dualFrame().setPadGridActive (uiMode == 1);
    headerBar.setWaveMode (waveformMode);
    actionPanel.setWaveActive (waveformMode != 0);

    headerBar.setMidiFollowActive (processor.midiSelectsSlice.load());
}

bool DysektEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
            ext == ".ogg" || ext == ".flac" || ext == ".mp3" ||
            ext == ".sf2" || ext == ".sfz")
            return true;
    }
    return false;
}

void DysektEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    juce::File f (files[0]);
    processor.zoom.store (1.0f);
    processor.scroll.store (0.0f);
    showTrimDialog (f);
}
