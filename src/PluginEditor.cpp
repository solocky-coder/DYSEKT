#include "PluginEditor.h"
#include <algorithm>

static constexpr int kBaseW      = 900;
static constexpr int kLogoH      = 44;
static constexpr int kHeaderH    = 66;
static constexpr int kSliceLaneH = 30;
static constexpr int kScrollbarH = 28;
static constexpr int kSliceCtrlH = 72;
static constexpr int kActionH    = 34;

static constexpr int kBrowserH   = 170;
static constexpr int kMargin     = 8;
static constexpr int kBaseHCore  = kLogoH + kHeaderH + kSliceLaneH
                                 + kScrollbarH + kSliceCtrlH + kActionH
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

      browserPanel   (p)
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

    // Panels start hidden
    browserPanel.setVisible (false);
    addChildComponent (browserPanel);

    sliceLane.setWaveformView (&waveformView);

    // Wire up BROWSER button in ActionPanel
    actionPanel.onBrowserToggle    = [this] { toggleBrowserPanel(); };
    actionPanel.onWaveToggle       = [this] { toggleSoftWave(); };
    actionPanel.onChromaticToggle  = [this] { toggleChromatic(); };

    ensureDefaultThemes();
    loadUserSettings();

    float apvtsScale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    if (apvtsScale == 1.0f && savedScale > 0.0f && savedScale != apvtsScale)
    {
        if (auto* param = processor.apvts.getParameter (ParamIds::uiScale))
            param->setValueNotifyingHost (param->convertTo0to1 (savedScale));
    }

    setWantsKeyboardFocus (true);
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
    setSize (kBaseW, computeTotalHeight());
    resized();
}

void DysektEditor::toggleSoftWave()
{
    softWave = ! softWave;
    waveformView.setSoftWaveform (softWave);
    actionPanel.setWaveActive (softWave);
    float scale = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    saveUserSettings (scale, getTheme().name);
}

void DysektEditor::toggleChromatic()
{
    const bool newVal = ! processor.chromaticMode.load();
    processor.chromaticMode.store (newVal);
    actionPanel.setChromaticActive (newVal);
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

    // 9. Waveform — remaining
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
    if (snapshotVersion != lastUiSnapshotVersion) { lastUiSnapshotVersion = snapshotVersion; uiChanged = true; }

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
                                 || playbackActive || processor.lazyChop.isActive();
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
}
