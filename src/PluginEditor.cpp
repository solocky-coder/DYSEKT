#include "PluginEditor.h"
#include <algorithm>

static constexpr int kBaseW      = 900;
static constexpr int kLogoH      = 44;
static constexpr int kHeaderH    = 60;
static constexpr int kSliceLaneH = 30;
static constexpr int kScrollbarH = 28;
static constexpr int kSliceCtrlH = 72;
static constexpr int kActionH    = 34;
static constexpr int kOscilloscopeH = 120;

static constexpr int kBrowserH   = 170;
static constexpr int kMargin     = 8;
static constexpr int kBaseHCore  = kLogoH + kHeaderH + kSliceLaneH
                                 + kScrollbarH + kSliceCtrlH + kActionH
                                 + kOscilloscopeH
                                 + 120; // minimum waveform height

static juce::File getSettingsDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DYSEKT");
}
static juce::File getUserSettingsFile() { return getSettingsDir().getChildFile ("settings.yaml"); }
static juce::File getThemesDir()        { return getSettingsDir().getChildFile ("themes"); }

// ─────────────────────────────────────────────────────────────────────────────
DysektEditor::DysektEditor (DysektProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      logoBar        (p),
      headerBar      (p),
      sliceLane      (p),
      waveformView   (p),
      scrollZoomBar  (p),
      sliceControlBar(p),
      actionPanel    (p, waveformView),

      browserPanel   (p),
      oscilloscopeView (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);

    addAndMakeVisible (logoBar);
    addAndMakeVisible (headerBar);
    addAndMakeVisible (sliceLane);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (scrollZoomBar);
    addAndMakeVisible (sliceControlBar);
    addAndMakeVisible (actionPanel);

    addAndMakeVisible (oscilloscopeView);

    // Panels start hidden
    browserPanel.setVisible (false);
    addChildComponent (browserPanel);

    sliceLane.setWaveformView (&waveformView);

    // Auto-close browser when a file is loaded via double-click
    browserPanel.onFileLoaded = [this] { if (browserOpen) toggleBrowserPanel(); };

    // Route file double-clicks through trim dialog
    browserPanel.onFileSelected = [this] (const juce::File& f)
    {
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        double dur = 0.0;
        if (auto* reader = fmtMgr.createReaderFor (f))
        {
            const double sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            dur = (double) reader->lengthInSamples / sr;
            delete reader;
        }
        processor.zoom.store (1.0f);
        processor.scroll.store (0.0f);
        showTrimDialog (f, dur);
    };

    // FIL / WA / CH now live in headerBar — wire their callbacks there
    headerBar.onBrowserToggle   = [this] { toggleBrowserPanel(); };
    headerBar.onWaveToggle      = [this] { toggleSoftWave(); };
    headerBar.onChromaticToggle = [this] { toggleChromatic(); };
    headerBar.onFileOpen        = [this] (const juce::File& f)
    {
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        double dur = 0.0;
        if (auto* reader = fmtMgr.createReaderFor (f))
        {
            const double sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            dur = (double) reader->lengthInSamples / sr;
            delete reader;
        }
        showTrimDialog (f, dur);
    };

    // Route waveform file drops through trim dialog
    waveformView.onFileDropped = [this] (const juce::File& f)
    {
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        double dur = 0.0;
        if (auto* reader = fmtMgr.createReaderFor (f))
        {
            const double sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            dur = (double) reader->lengthInSamples / sr;
            delete reader;
        }
        showTrimDialog (f, dur);
    };

    // Keep actionPanel callbacks as no-ops (buttons removed from action bar)
    actionPanel.onBrowserToggle    = nullptr;
    actionPanel.onWaveToggle       = nullptr;
    actionPanel.onChromaticToggle  = nullptr;

    ensureDefaultThemes();
    loadUserSettings();

    float apvtsScale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    if (apvtsScale == 1.0f && savedScale > 0.0f && savedScale != apvtsScale)
    {
        if (auto* param = processor.apvts.getParameter (ParamIds::uiScale))
            param->setValueNotifyingHost (param->convertTo0to1 (savedScale));
    }

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (600, 500, 1920, 1200);
    setSize (kBaseW, computeTotalHeight());
    lastUiSnapshotVersion = processor.getUiSliceSnapshotVersion();
    startTimerHz (30);
}

DysektEditor::~DysektEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

int DysektEditor::computeTotalHeight() const
{
    int h = kBaseHCore;
    if (browserOpen) h += kBrowserH;

    return h;
}


void DysektEditor::toggleBrowserPanel()
{
    browserOpen = ! browserOpen;
    browserPanel.setVisible (browserOpen);
    headerBar.setBrowserActive (browserOpen);
    setSize (getWidth(), computeTotalHeight());
    resized();
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

void DysektEditor::toggleChromatic()
{
    const bool newVal = ! processor.chromaticMode.load();
    processor.chromaticMode.store (newVal);
    actionPanel.setChromaticActive (newVal);
    headerBar.setChromaticActive (newVal);
}

void DysektEditor::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().background);
}

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());

    // 1. Logo bar
    logoBar.setBounds (area.removeFromTop (kLogoH));

    // 2. Header controls
    headerBar.setBounds (area.removeFromTop (kHeaderH));

    // 3. Slice lane
    sliceLane.setBounds (area.removeFromTop (kSliceLaneH).reduced (kMargin, 0));

    // 4. Slice control bar — bottom
    sliceControlBar.setBounds (area.removeFromBottom (kSliceCtrlH));

    // 5. Browser panel — above slice ctrl (if open)
    if (browserOpen)
        browserPanel.setBounds (area.removeFromBottom (kBrowserH));

    // 7. Action panel
    actionPanel.setBounds (area.removeFromBottom (kActionH).reduced (kMargin, 0));

    area.removeFromBottom (4);  // gap

    // 8. Scrollbar
    scrollZoomBar.setBounds (area.removeFromBottom (kScrollbarH).reduced (kMargin, 0));

    // 9. Oscilloscope — below waveform
    oscilloscopeView.setBounds (area.removeFromBottom (kOscilloscopeH).reduced (kMargin, 0));

    // 10. Waveform — remaining
    waveformView.setBounds (area.reduced (kMargin, 0));
}

// ── Key shortcuts ─────────────────────────────────────────────────────────────
bool DysektEditor::keyPressed (const juce::KeyPress& key)
{
    auto mods = key.getModifiers();
    int code  = key.getKeyCode();

    if (code == 'Z' && mods.isCommandDown() && mods.isShiftDown())
    { DysektProcessor::Command c; c.type = DysektProcessor::CmdRedo; processor.pushCommand (c); return true; }
    if (code == 'Z' && mods.isCommandDown())
    { DysektProcessor::Command c; c.type = DysektProcessor::CmdUndo; processor.pushCommand (c); return true; }

    if (mods.isCommandDown() || mods.isAltDown()) return false;

    if (code == juce::KeyPress::escapeKey && actionPanel.isAutoChopOpen())
    { actionPanel.toggleAutoChop(); return true; }
    if (code == 'C') { actionPanel.toggleAutoChop(); return true; }
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
    if (code == 'Z') { processor.snapToZeroCrossing.store (! processor.snapToZeroCrossing.load()); repaint(); return true; }
    if (code == 'F') { processor.midiSelectsSlice.store  (! processor.midiSelectsSlice.load());   repaint(); return true; }

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

// ── Timer ─────────────────────────────────────────────────────────────────────
void DysektEditor::timerCallback()
{
    bool uiChanged = false, viewportChanged = false;
    const bool previewActive      = waveformView.hasActiveSlicePreview();
    const bool waveformInteracting = waveformView.isInteracting();
    const bool rulerDragging      = scrollZoomBar.isDraggingNow();

    const auto snapshotVersion = processor.getUiSliceSnapshotVersion();
    if (snapshotVersion != lastUiSnapshotVersion)
    {
        lastUiSnapshotVersion = snapshotVersion;
        uiChanged = true;

        // Activate trim mode once the new sample is available
        if (pendingTrimMode && processor.sampleData.getSnapshot() != nullptr)
        {
            pendingTrimMode = false;
            waveformView.setTrimMode (true);
        }
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

    const int targetHz = waveformAnimating ? 60 : 30;
    if (targetHz != timerHz) { startTimerHz (targetHz); timerHz = targetHz; }

    if (waveformNeedsRepaint) waveformView.repaint();
    if (laneNeedsRepaint)     sliceLane.repaint();
    if (rulerNeedsRepaint)    scrollZoomBar.repaint();

    oscilloscopeView.repaint();

    headerBar.repaint();
    sliceControlBar.repaint();
    if (uiChanged) actionPanel.repaint();
}

// ── Theme helpers (unchanged) ─────────────────────────────────────────────────
void DysektEditor::ensureDefaultThemes()
{
    auto dir = getThemesDir(); dir.createDirectory();
    auto write = [&] (const juce::String& name, const ThemeData& t)
    {
        auto f = dir.getChildFile (name + ".dysektstyle");
        if (! f.existsAsFile()) f.replaceWithText (t.toThemeFile());
    };
    write ("dark",  ThemeData::darkTheme());
    write ("light", ThemeData::lightTheme());
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
    if (names.isEmpty()) { names.add ("dark"); names.add ("light"); }
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
    if      (themeName == "light") setTheme (ThemeData::lightTheme());
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

    // Apply persisted wave style
    waveformView.setSoftWaveform (softWave);
    actionPanel.setWaveActive (softWave);

    // Sync chromatic button with processor state (restored from project)
    actionPanel.setChromaticActive (processor.chromaticMode.load());
    headerBar.setChromaticActive (processor.chromaticMode.load());
}

// =============================================================================
//  FileDragAndDropTarget  — catches file drops anywhere on the editor window
//  This is the critical top-level handler; WaveformView's handler only fires
//  when the cursor is precisely over that child component.
// =============================================================================
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
    auto ext = f.getFileExtension().toLowerCase();

    processor.zoom.store (1.0f);
    processor.scroll.store (0.0f);

    if (ext == ".sf2" || ext == ".sfz")
        processor.loadSoundFontAsync (f);
    else
    {
        // Quick metadata-only read to get duration
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        double dur = 0.0;
        if (auto* reader = fmtMgr.createReaderFor (f))
        {
            const double sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            dur = (double) reader->lengthInSamples / sr;
            delete reader;
        }
        showTrimDialog (f, dur);
    }
}

// ── Trim dialog helpers ────────────────────────────────────────────────────────
void DysektEditor::showTrimDialog (const juce::File& file, double durationSeconds)
{
    const auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".sf2" || ext == ".sfz")
    {
        processor.loadSoundFontAsync (file);
        return;
    }

    const int pref = processor.trimPreference.load();
    if (pref == 2 || durationSeconds < 5.0)
    {
        // "never" preference or short file — load immediately
        processor.loadFileAsync (file);
        return;
    }
    if (pref == 1)
    {
        // "always trim" preference — load then activate trim mode
        processor.loadFileAsync (file);
        showTrimMode();
        return;
    }

    // Ask the user
    TrimDialog::show (this, file, durationSeconds,
        [this, file] (const TrimDialog::Result& r)
        {
            if (r.rememberChoice)
                processor.trimPreference.store (r.userClickedYes ? 1 : 2);

            processor.loadFileAsync (file);
            if (r.userClickedYes)
                showTrimMode();
        });
}

void DysektEditor::showTrimMode()
{
    pendingTrimMode = true;
}

void DysektEditor::onTrimConfirmed (bool userClickedYes, bool rememberChoice)
{
    if (rememberChoice)
        processor.trimPreference.store (userClickedYes ? 1 : 2);

    if (userClickedYes)
    {
        processor.trimRegionStart.store (waveformView.getTrimIn());
        processor.trimRegionEnd.store   (waveformView.getTrimOut());
    }

    showTrimMode();
}
