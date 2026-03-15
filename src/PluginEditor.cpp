#include "PluginEditor.h"
#include <algorithm>

// Layout constants
static constexpr int kBaseW      = 1130;
static constexpr int kLogoH      = 52;
static constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12;
static constexpr int kSliceLaneH = 36;
static constexpr int kScrollbarH = 28;
static constexpr int kSliceCtrlH = 72;
static constexpr int kActionH    = 22;
static constexpr int kTrimBarH   = 34;
static constexpr int kCtrlFrameW = 180;

static constexpr int kBrowserH    = 170;
static constexpr int kMargin      = 8;
static constexpr int kPanelSlotH  = 200;
static constexpr int kBaseHCore   = kLogoH + kLcdRowH + kSliceLaneH
                                  + kScrollbarH + kSliceCtrlH + kActionH
                                  + 120;
static constexpr int kTotalH      = kBaseHCore + kPanelSlotH + 16;

static juce::File getSettingsDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DYSEKT");
}
static juce::File getUserSettingsFile() { return getSettingsDir().getChildFile ("settings.yaml"); }
static juce::File getThemesDir()        { return getSettingsDir().getChildFile ("themes"); }

// Main Editor constructor
DysektEditor::DysektEditor (DysektProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      logoBar        (p),
      headerBar      (p),
      sliceLcd       (p),
      sliceWaveformLcd (p),
      sliceLane      (p),
      waveformView   (p),
      waveformOverview (p),
      sliceControlBar(p),
      actionPanel    (p, waveformView),
      browserPanel   (p),
      mixerPanel     (p),
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

    // Panels start hidden
    browserPanel.setVisible (false);
    addChildComponent (browserPanel);
    mixerPanel.setVisible (false);
    addChildComponent (mixerPanel);
    shortcutsPanel.setVisible (false);
    addChildComponent (shortcutsPanel);
    shortcutsPanel.onDismiss = [this] { toggleShortcutsPanel(); };

    sliceLane.setWaveformView (&waveformView);

    browserPanel.onFileLoaded = [this] { if (browserOpen) toggleBrowserPanel(); };
    browserPanel.onLoadRequest = [this] (const juce::File& f) { showTrimDialog (f); };
    waveformView.onLoadRequest = [this] (const juce::File& f) { showTrimDialog (f); };

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

    headerBar.onBodeToggle       = [this] { toggleMixerPanel(); };
    headerBar.onBrowserToggle    = [this] { toggleBrowserPanel(); };
    headerBar.onWaveToggle       = [this] { toggleSoftWave(); };
    headerBar.onMidiFollowToggle = [this] { toggleMidiFollow(); };
    headerBar.onShortcutsToggle  = [this] { toggleShortcutsPanel(); };

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
    setResizable (true, true);
    setResizeLimits (600, 500, 1920, 1200);
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

void DysektEditor::toggleBrowserPanel()
{
    if (browserOpen)
    {
        browserOpen = false;
        browserPanel.setVisible (false);
        headerBar.setBrowserActive (false);
    }
    else
    {
        if (mixerOpen)
        {
            mixerOpen = false;
            mixerPanel.setVisible (false);
            headerBar.setBodeActive (false);
        }
        browserOpen = true;
        browserPanel.setVisible (true);
        headerBar.setBrowserActive (true);
    }
    resized();
    repaint();
    resized();
    repaint();
}

void DysektEditor::showTrimDialog (const juce::File& file, bool isRelink)
{
    if (isRelink)
    {
        processor.loadFileAsync (file);
        return;
    }

    auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".sf2" || ext == ".sfz")
    {
        processor.loadSoundFontAsync (file);
        return;
    }

    const int pref = processor.trimPreference.load (std::memory_order_relaxed);
    if (pref == DysektProcessor::TrimPrefNever)
    {
        processor.loadFileAsync (file);
        processor.zoom.store (1.0f);
        processor.scroll.store (0.0f);
        return;
    }
    if (pref == DysektProcessor::TrimPrefAsk)
    {
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
    }
    showTrimMode (file);
}

void DysektEditor::showTrimMode (const juce::File& file)
{
    trimSession = std::make_unique<TrimSession>();
    trimSession->file   = file;
    trimSession->active = false;

    processor.loadFileAsync (file);
    processor.zoom.store (1.0f);
    processor.scroll.store (0.0f);
}

void DysektEditor::toggleSoftWave()
{
    softWave = ! softWave;
    waveformView.setSoftWaveform (softWave);
    actionPanel.setWaveActive (softWave);
    headerBar.setBrowserActive (browserOpen);
    headerBar.setWaveActive (softWave);
    headerBar.setWaveActive (softWave);
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
    shortcutsPanel.setVisible (show);
    if (show)
    {
        shortcutsPanel.setBounds (getLocalBounds());
        shortcutsPanel.toFront (true);
        shortcutsPanel.grabKeyboardFocus();
    }
}

void DysektEditor::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().background);

    if (actionPanel.isVisible() && waveformView.isVisible() && waveformOverview.isVisible())
    {
        const auto& abnd = actionPanel.getBounds();
        const auto& sbnd = waveformOverview.getBounds();
        const auto  ac   = getTheme().accent;

        const int kFrameInset = 4;
        const int kFrameX     = kMargin;
        const int kFrameW     = getWidth() - kMargin * 2;
        const juce::Rectangle<float> outerF (
            (float) kFrameX,   (float) abnd.getY() - kFrameInset,
            (float) kFrameW,   (float) (sbnd.getBottom() - abnd.getY() + kFrameInset * 2));

        juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0.f, outerF.getY(),
                                        juce::Colour (0xFF0E0E0E), 0.f, outerF.getBottom(), false);
        g.setGradientFill (outerGrad);
        g.fillRoundedRectangle (outerF, 4.0f);

        g.setColour (ac.withAlpha (0.20f));
        g.drawRoundedRectangle (outerF.reduced (0.5f), 4.0f, 1.0f);

        const auto screenF = outerF.reduced (4.0f);
        g.setColour (getTheme().darkBar.darker (0.55f));
        g.fillRoundedRectangle (screenF, 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (int y = juce::roundToInt (screenF.getY()); y < juce::roundToInt (screenF.getBottom()); y += 2)
            g.drawHorizontalLine (y, screenF.getX(), screenF.getRight());

        juce::ColourGradient glow (ac.withAlpha (0.06f), 0.f, screenF.getY(),
                                   juce::Colours::transparentBlack, 0.f, screenF.getY() + 20.f, false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (screenF, 2.0f);

        g.setColour (ac.withAlpha (0.12f));
        g.drawRoundedRectangle (screenF.expanded (0.5f), 2.0f, 1.0f);

        const auto& lbnd = sliceLane.getBounds();
        g.setColour (ac.withAlpha (0.08f));
        g.drawHorizontalLine (lbnd.getY(),
                              screenF.getX() + 4.f, screenF.getRight() - 4.f);
    }
}

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());

    {
        auto headerStrip = area.removeFromTop (kLogoH);
        logoBar.setBounds (headerStrip.removeFromLeft (220));
        headerBar.setBounds (headerStrip);
    }
    {
        auto lcdRow = area.removeFromTop (kLcdRowH).reduced (kMargin, 6);
        const int lcdW = (lcdRow.getWidth() - kCtrlFrameW - kMargin * 2) / 2;
        sliceLcd.setBounds (lcdRow.removeFromLeft (lcdW));
        lcdRow.removeFromLeft (kMargin);
        if (auto* cf = headerBar.getControlFrame())
            cf->setBounds (lcdRow.removeFromLeft (kCtrlFrameW));
        lcdRow.removeFromLeft (kMargin);
        sliceWaveformLcd.setBounds (lcdRow);
    }
    auto actionArea = area.removeFromTop (kActionH);
    const int kFX = kMargin;
    const int kFW = getWidth() - kMargin * 2;

    {
        area.removeFromBottom (kMargin);
        auto slot = area.removeFromBottom (kPanelSlotH);
        area.removeFromBottom (kMargin);

        if (mixerOpen)
        {
            const int mh = juce::jmin (MixerPanel::kPanelH, kPanelSlotH);
            auto mb = juce::Rectangle<int> (kFX, slot.getY(), kFW, mh);
            mixerPanel.setBounds (mb);
            browserPanel.setBounds ({});
        }
        else if (browserOpen)
        {
            const int bh = juce::jmin (kBrowserH, kPanelSlotH);
            auto bb = juce::Rectangle<int> (kFX, slot.getY(), kFW, bh);
            browserPanel.setBounds (bb);
            mixerPanel.setBounds ({});
        }
        else
        {
            mixerPanel.setBounds ({});
            browserPanel.setBounds ({});
        }
    }
    {
        auto scbArea = area.removeFromBottom (kSliceCtrlH);
        sliceControlBar.setBounds (juce::Rectangle<int> (kFX, scbArea.getY(), kFW, kSliceCtrlH));
    }
    area.removeFromBottom (kMargin);

    const int kFrameInset = 4;
    const int kFrameX     = kFX;
    const int kFrameW     = kFW;
    const int frameTop    = actionArea.getY();
    const int frameBot    = area.getBottom();
    const int screenX     = kFrameX  + kFrameInset;
    const int screenW     = kFrameW  - kFrameInset * 2;
    const int screenTop   = frameTop + kFrameInset;
    const int screenBot   = frameBot - kFrameInset;
    const int screenH     = screenBot - screenTop;

    {
        auto r = juce::Rectangle<int> (screenX, screenBot - kScrollbarH, screenW, kScrollbarH);
        waveformOverview.setBounds (r);
    }
    {
        auto r = juce::Rectangle<int> (screenX, screenTop, screenW, kActionH);
        actionPanel.setBounds (r);
    }
    {
        int y = screenTop + kActionH;
        auto r = juce::Rectangle<int> (screenX, y, screenW, kSliceLaneH);
        sliceLane.setBounds (r);
    }
    {
        int y      = screenTop + kActionH + kSliceLaneH;
        int trimH  = (trimDialog != nullptr) ? kTrimBarH : 0;
        int h      = screenBot - kScrollbarH - trimH - y;
        waveformView.setBounds (juce::Rectangle<int> (screenX, y, screenW, h));
        if (trimDialog != nullptr)
            trimDialog->setBounds (screenX, y + h, screenW, kTrimBarH);
    }
    if (shortcutsPanel.isVisible())
        shortcutsPanel.setBounds (getLocalBounds());
}

void DysektEditor::toggleMixerPanel()
{
    if (mixerOpen)
    {
        mixerOpen = false;
        mixerPanel.setVisible (false);
        headerBar.setBodeActive (false);
    }
    else
    {
        if (browserOpen)
        {
            browserOpen = false;
            browserPanel.setVisible (false);
            headerBar.setBrowserActive (false);
        }
        mixerOpen = true;
        mixerPanel.setVisible (true);
        headerBar.setBodeActive (true);
    }
    resized();
    repaint();
    resized();
    repaint();
}

bool DysektEditor::keyPressed (const juce::KeyPress& key)
{
    auto mods = key.getModifiers();
    int code  = key.getKeyCode();

    if (code == 'Z' && mods.isCommandDown() && mods.isShiftDown())
    { DysektProcessor::Command c; c.type = DysektProcessor::CmdRedo; processor.pushCommand (c); return true; }
    if (code == 'Z' && mods.isCommandDown())
    { DysektProcessor::Command c; c.type = DysektProcessor::CmdUndo; processor.pushCommand (c); return true; }
    if ((code == '?' || code == '/') && mods.isCommandDown())
    { toggleShortcutsPanel(); return true; }
    if (mods.isCommandDown() || mods.isAltDown()) return false;
    if (code == juce::KeyPress::escapeKey && shortcutsPanel.isVisible())
    { toggleShortcutsPanel(); return true; }
    if (code == 'A') { waveformView.setSliceDrawMode (! waveformView.isSliceDrawModeActive()); repaint(); return true; }
    if (code == 'L')
    {
        DysektProcessor::Command c;
        c.type = processor.lazyChop.isActive() ? DysektProcessor::CmdLazyChopStop : DysektProcessor::CmdLazyChopStart;
        processor.pushCommand (c); repaint(); return true;
    }
    if (code == 'D') { DysektProcessor::Command c; c.type = DysektProcessor::CmdDuplicateSlice; processor.pushCommand (c); return true; }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        const auto& ui = processor.getUiSliceSnapshot();
        if (ui.selectedSlice >= 0)
        { DysektProcessor::Command c; c.type = DysektProcessor::CmdDeleteSlice; c.intParam1 = ui.selectedSlice; processor.pushCommand (c); }
        return true;
    }
    if (code == 'F') { toggleMidiFollow(); return true; }
    if (code == juce::KeyPress::rightKey || (code == juce::KeyPress::tabKey && ! mods.isShiftDown()))
    {
        const auto& ui = processor.getUiSliceSnapshot();
        if (ui.numSlices > 0)
        { DysektProcessor::Command c; c.type = DysektProcessor::CmdSelectSlice; c.intParam1 = juce::jlimit (0, ui.numSlices-1, ui.selectedSlice+1); processor.pushCommand (c); repaint(); }
        return true;
    }
    if (code == juce::KeyPress::leftKey || (code == juce::KeyPress::tabKey && mods.isShiftDown()))
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
    const bool previewActive      = waveformView.hasActiveSlicePreview();
    const bool waveformInteracting = waveformView.isInteracting();
    const bool rulerDragging      = waveformOverview.isDraggingNow();

    const auto snapshotVersion = processor.getUiSliceSnapshotVersion();
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
    float scale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    if (scaleDirty || scale != lastScale)
    {
        scaleDirty = false; lastScale = scale;
        setTransform (juce::AffineTransform::scale (scale));
        DysektLookAndFeel::setMenuScale (scale);
        saveUserSettings (scale, getTheme().name);
        uiChanged = true;
    }

    const bool playbackActive = std::any_of (processor.voicePool.voicePositions.begin(),
                                             processor.voicePool.voicePositions.end(),
                                             [] (const std::atomic<float>& pos) { return pos.load (std::memory_order_relaxed) > 0.0f; });

    const bool waveformAnimating = waveformInteracting || rulerDragging || previewActive
                                 || playbackActive || processor.lazyChop.isActive()
                                 || (processor.liveDragSliceIdx.load (std::memory_order_relaxed) >= 0);
    const bool waveformNeedsRepaint = uiChanged || viewportChanged || waveformAnimating || lastWaveformAnimating;
    const bool laneNeedsRepaint     = uiChanged || viewportChanged || previewActive || lastPreviewActive;
    const bool rulerNeedsRepaint    = uiChanged || viewportChanged || rulerDragging;

    lastWaveformAnimating = waveformAnimating;
    lastPreviewActive     = previewActive;

    if (trimSession != nullptr && ! trimSession->active)
    {
        auto snap = processor.sampleData.getSnapshot();
        if (snap != nullptr && snap->filePath == trimSession->file.getFullPathName())
        {
            trimSession->active = true;
            const int totalFrames = snap->buffer.getNumSamples();
            waveformView.enterTrimMode (0, totalFrames);

            processor.trimModeActive.store (true, std::memory_order_relaxed);
            processor.trimRegionStart.store (0,           std::memory_order_relaxed);
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
    if (rulerNeedsRepaint)    waveformOverview.repaint();
    sliceLcd.repaintLcd();
    sliceWaveformLcd.repaintLcd();
    if (mixerOpen) mixerPanel.repaint();

    headerBar.repaint();
    sliceControlBar.repaint();
    if (uiChanged) actionPanel.repaint();
    if (mixerOpen) mixerPanel.updateFromSnapshot();
}

// Theme helpers
void DysektEditor::ensureDefaultThemes()
{
    auto dir = getThemesDir(); dir.createDirectory();
    auto write = [&] (const juce::String& name, const ThemeData& t)
    {
        auto f = dir.getChildFile (name + ".dysektstyle");
        if (! f.existsAsFile()) f.replaceWithText (t.toThemeFile());
    };
    write ("dark",  ThemeData::darkTheme());
    write ("shell", ThemeData::shellTheme());
    write ("lazy",  ThemeData::lazyTheme());
    write ("snow",  ThemeData::snowTheme());
    write ("ghost", ThemeData::ghostTheme());
    write ("hack",  ThemeData::hackTheme());
}

juce::StringArray DysektEditor::getAvailableThemes()
{
    juce::StringArray names;
    for (auto& f : getThemesDir().findChildFiles (juce::File::findFiles, false, "*.dysektstyle"))
    {
        auto t = ThemeData::fromThemeFile (f.loadFileAsString());
        if (t.name.isNotEmpty()) names.add (t.name);
    }
    if (names.isEmpty()) { names.add ("dark"); names.add ("shell"); }
    return names;
}

void DysektEditor::applyTheme (const juce::String& themeName)
{
    for (auto& f : getThemesDir().findChildFiles (juce::File::findFiles, false, "*.dysektstyle"))
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
    if      (themeName == "shell") setTheme (ThemeData::shellTheme());
    else if (themeName == "lazy")  setTheme (ThemeData::lazyTheme());
    else if (themeName == "snow")  setTheme (ThemeData::snowTheme());
    else if (themeName == "ghost") setTheme (ThemeData::ghostTheme());
    else if (themeName == "hack")  setTheme (ThemeData::hackTheme());
    else                           setTheme (ThemeData::darkTheme());
    processor.sliceManager.setSlicePalette (getTheme().slicePalette);
    saveUserSettings (processor.apvts.getRawParameterValue (ParamIds::uiScale)->load(), themeName);
    repaint();
}

void DysektEditor::saveUserSettings (float scale, const juce::String& themeName)
{
    auto file = getUserSettingsFile();
    file.getParentDirectory().createDirectory();

    file.replaceWithText ("uiScale: " + juce::String (scale, 2)
                        + "\ntheme: " + themeName
                        + "\nwaveStyle: " + (softWave ? "soft" : "hard") + "\n");
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
            { float v = line.fromFirstOccurrenceOf (":", false, false).trim().getFloatValue(); if (v >= 0.5f && v <= 3.0f) savedScale = v; }
            else if (line.startsWith ("theme:"))
            { themeName = line.fromFirstOccurrenceOf (":", false, false).trim(); }
            else if (line.startsWith ("waveStyle:"))
            {
                auto val = line.fromFirstOccurrenceOf (":", false, false).trim();
                softWave = (val == "soft");
            }
        }
    }
    applyTheme (themeName);

    waveformView.setSoftWaveform (softWave);
    actionPanel.setWaveActive (softWave);

    headerBar.setMidiFollowActive (processor.midiSelectsSlice.load());
}

bool DysektEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav"  || ext == ".aif"  || ext == ".aiff" ||
            ext == ".ogg"  || ext == ".flac"  || ext == ".mp3"  ||
            ext == ".sf2"  || ext == ".sfz")
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
