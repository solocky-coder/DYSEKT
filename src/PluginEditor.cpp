#include "PluginEditor.h"
#include <algorithm>

static constexpr int kBaseW      = 1130;
static constexpr int kLogoH      = 52;    // single combined header bar
static constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12; // LCD row + padding
static constexpr int kSliceLaneH = 36;   // 30 body + 6 ADSR dot strip
static constexpr int kScrollbarH = 28;
static constexpr int kSliceCtrlH = 72;
static constexpr int kActionH    = 22;
static constexpr int kTrimBarH   = 34;   // height of inline trim bar
static constexpr int kCtrlFrameW    = 180; // width of the centre control frame

static constexpr int kBrowserH   = 170;
static constexpr int kMargin     = 8;
static constexpr int kBaseHCore  = kLogoH + kLcdRowH + kSliceLaneH
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
      sliceLcd       (p),
      sliceWaveformLcd (p),
      sliceLane      (p),
      waveformView   (p),
      scrollZoomBar  (p),
      sliceControlBar(p),
      actionPanel    (p, waveformView),

      browserPanel   (p),
      mixerPanel     (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);

    addAndMakeVisible (logoBar);
    addAndMakeVisible (headerBar);

    // v8: dual LCD row
    addAndMakeVisible (sliceLcd);
    addAndMakeVisible (sliceWaveformLcd);
    // Control frame (icons + knobs) is a child of headerBar — exposed via
    // headerBar.getControlFrame() and laid out in resized().
    if (auto* cf = headerBar.getControlFrame())
        addAndMakeVisible (*cf);

    addAndMakeVisible (sliceLane);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (scrollZoomBar);
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

    // Auto-close browser when a file is loaded via double-click
    browserPanel.onFileLoaded = [this] { if (browserOpen) toggleBrowserPanel(); };

    // Route file loads from the browser panel through the trim dialog
    browserPanel.onLoadRequest = [this] (const juce::File& f) { showTrimDialog (f); };

    // Route file drops on the waveform through the trim dialog
    waveformView.onLoadRequest = [this] (const juce::File& f) { showTrimDialog (f); };

    // Handle trim apply / cancel callbacks from WaveformView
    waveformView.onTrimApplied = [this] (int s, int e)
    {
        processor.applyTrimToCurrentSample (s, e);
        trimSession.reset();
        trimDialog.reset();
        actionPanel.setTrimLocked (false);
        resized();
    };
    waveformView.onTrimCancelled = [this]
    {
        trimSession.reset();
        trimDialog.reset();
        actionPanel.setTrimLocked (false);
        resized();
    };

    // TRIM button path: ActionPanel delegates here so PluginEditor owns
    // trim lifecycle, enterTrimMode, and TrimDialog placement.
    actionPanel.onTrimToggle = [this]
    {
        if (trimDialog != nullptr)
        {
            // Second press = cancel
            if (auto* p = trimDialog->getParentComponent())
                p->removeChildComponent (trimDialog.get());
            trimDialog.reset();
            waveformView.setTrimMode (false);
            actionPanel.setTrimLocked (false);
            resized();
            repaint();
            return;
        }

        if (processor.sampleData.getSnapshot() == nullptr)
            return;

        auto snap = processor.sampleData.getSnapshot();
        const int totalFrames = snap->buffer.getNumSamples();

        // Initialise markers at full extent so user drags inward
        waveformView.enterTrimMode (0, totalFrames);

        trimDialog = std::make_unique<TrimDialog> (processor, waveformView);
        addAndMakeVisible (*trimDialog);
        trimDialog->toFront (false);
        actionPanel.setTrimLocked (true);  // block ADD SLICE / MIDI SLICE
        resized();   // re-layout: waveform shrinks, trim bar placed below
        repaint();
    };

    // FIL / WA / CH now live in headerBar — wire their callbacks there
    headerBar.onBodeToggle      = [this] { toggleMixerPanel(); };
    headerBar.onBrowserToggle   = [this] { toggleBrowserPanel(); };
    headerBar.onWaveToggle      = [this] { toggleSoftWave(); };
    headerBar.onMidiFollowToggle = [this] { toggleMidiFollow(); };
    headerBar.onShortcutsToggle = [this] { toggleShortcutsPanel(); };

    // Keep actionPanel callbacks as no-ops (buttons removed from action bar)
    ensureDefaultThemes();
    loadUserSettings();

    // Load default sample (Empty.wav) if no DAW preset has already provided one
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
    if (mixerOpen)     h += MixerPanel::kPanelH;
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

// ── Trim workflow ─────────────────────────────────────────────────────────────

void DysektEditor::showTrimDialog (const juce::File& file, bool isRelink)
{
    // Skip trim for relinks and soundfonts
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

    // Check user's stored preference
    int pref = processor.trimPreference.load (std::memory_order_relaxed);
    if (pref == DysektProcessor::TrimPrefNever)
    {
        processor.loadFileAsync (file);
        processor.zoom.store (1.0f);
        processor.scroll.store (0.0f);
        return;
    }
    if (pref == DysektProcessor::TrimPrefAlways)
    {
        showTrimMode (file);
        return;
    }

    // Estimate duration from file metadata (without full decode)
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    double duration = 0.0;
    if (reader != nullptr && reader->sampleRate > 0.0)
        duration = (double) reader->lengthInSamples / reader->sampleRate;

    // Short files skip the dialog
    if (duration < 5.0 || duration <= 0.0)
    {
        processor.loadFileAsync (file);
        processor.zoom.store (1.0f);
        processor.scroll.store (0.0f);
        return;
    }

    // Show trim dialog
    TrimDialog::show (file.getFileName(), duration, this,
        [this, file] (TrimDialog::Result result)
        {
            if (result.remember)
                processor.trimPreference.store (result.trim ? DysektProcessor::TrimPrefAlways
                                                            : DysektProcessor::TrimPrefNever,
                                                std::memory_order_relaxed);
            if (result.trim)
                showTrimMode (file);
            else
            {
                processor.loadFileAsync (file);
                processor.zoom.store (1.0f);
                processor.scroll.store (0.0f);
            }
        });
}

void DysektEditor::showTrimMode (const juce::File& file)
{
    // Store session so the timer callback knows to enter trim mode on load
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

    // ── Waveform frame — exact same style as SliceLcdDisplay / SliceWaveformLcd ──
    if (actionPanel.isVisible() && waveformView.isVisible() && scrollZoomBar.isVisible())
    {
        const auto& abnd = actionPanel.getBounds();
        const auto& sbnd = scrollZoomBar.getBounds();
        const auto  ac   = getTheme().accent;

        // Outer frame rect — kMargin inset on left/right, spanning actionPanel
        // top to scrollZoomBar bottom.  Must match resized() exactly.
        const int kFrameInset = 4;
        const int kFrameX     = kMargin;
        const int kFrameW     = getWidth() - kMargin * 2;
        const juce::Rectangle<float> outerF (
            (float) kFrameX,   (float) abnd.getY() - kFrameInset,
            (float) kFrameW,   (float) (sbnd.getBottom() - abnd.getY() + kFrameInset * 2));

        // 1. Outer gradient fill
        juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0.f, outerF.getY(),
                                         juce::Colour (0xFF0E0E0E), 0.f, outerF.getBottom(), false);
        g.setGradientFill (outerGrad);
        g.fillRoundedRectangle (outerF, 4.0f);

        // 2. Outer accent border
        g.setColour (ac.withAlpha (0.20f));
        g.drawRoundedRectangle (outerF.reduced (0.5f), 4.0f, 1.0f);

        // 3. Inner screen
        const auto screenF = outerF.reduced (4.0f);
        g.setColour (getTheme().darkBar.darker (0.55f));
        g.fillRoundedRectangle (screenF, 2.0f);

        // 4. Scanlines every 2px
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (int y = juce::roundToInt (screenF.getY()); y < juce::roundToInt (screenF.getBottom()); y += 2)
            g.drawHorizontalLine (y, screenF.getX(), screenF.getRight());

        // 5. Phosphor glow at top
        juce::ColourGradient glow (ac.withAlpha (0.06f), 0.f, screenF.getY(),
                                    juce::Colours::transparentBlack, 0.f, screenF.getY() + 20.f, false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (screenF, 2.0f);

        // 6. Inner accent border
        g.setColour (ac.withAlpha (0.12f));
        g.drawRoundedRectangle (screenF.expanded (0.5f), 2.0f, 1.0f);

        // 7. Subtle divider between action bar and slice lane
        const auto& lbnd = sliceLane.getBounds();
        g.setColour (ac.withAlpha (0.08f));
        g.drawHorizontalLine (lbnd.getY(),
                              screenF.getX() + 4.f, screenF.getRight() - 4.f);
    }
}

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());

    // 1. Combined header bar (logo left + UNDO/REDO/PANIC/UI right)
    //    LogoBar and HeaderBar share the same horizontal strip.
    {
        auto headerStrip = area.removeFromTop (kLogoH);
        // LogoBar takes a fixed width on the left; HeaderBar gets the rest.
        logoBar.setBounds (headerStrip.removeFromLeft (220));
        headerBar.setBounds (headerStrip);
    }

    // 2. Dual LCD row — LCD1 | CtrlFrame | LCD2
    {
        auto lcdRow = area.removeFromTop (kLcdRowH).reduced (kMargin, 6);

        // Left LCD — slice data
        const int lcdW = (lcdRow.getWidth() - kCtrlFrameW - kMargin * 2) / 2;
        sliceLcd.setBounds (lcdRow.removeFromLeft (lcdW));

        lcdRow.removeFromLeft (kMargin);  // gap

        // Centre control frame (icons top row + knobs bottom row)
        if (auto* cf = headerBar.getControlFrame())
            cf->setBounds (lcdRow.removeFromLeft (kCtrlFrameW));

        lcdRow.removeFromLeft (kMargin);  // gap

        // Right LCD — slice mini waveform
        sliceWaveformLcd.setBounds (lcdRow);
    }

    // 2b. Reserve the action panel height — it becomes the TOP of the LCD frame.
    auto actionArea = area.removeFromTop (kActionH);

    // 4. Slice control bar — bottom (outside frame)
    sliceControlBar.setBounds (area.removeFromBottom (kSliceCtrlH));

    // 4a. Mixer panel — sits above slice ctrl bar (if open)
    if (mixerOpen)
        mixerPanel.setBounds (area.removeFromBottom (MixerPanel::kPanelH));

    // 5. Browser panel — above slice ctrl (if open)
    if (browserOpen)
        browserPanel.setBounds (area.removeFromBottom (kBrowserH));

    area.removeFromBottom (4);  // bottom gap

    // ── LCD frame outer rect ─────────────────────────────────────────────────
    // The full outer rect spans actionArea.top → area.bottom, kMargin inset
    // on left/right.  paint() draws the frame border over this rect.
    // Components sit inside the "screen" — reduced by 4px on all sides —
    // just like the LCD panels do with b.reduced(4).
    const int kFrameInset = 4;  // matches LCD b.reduced(4)
    const int kFrameX     = kMargin;
    const int kFrameW     = getWidth() - kMargin * 2;

    // Full outer frame rect (used by paint() — computed identically there)
    const int frameTop    = actionArea.getY();
    const int frameBot    = area.getBottom();

    // Inner screen rect — components live here
    const int screenX     = kFrameX  + kFrameInset;
    const int screenW     = kFrameW  - kFrameInset * 2;
    const int screenTop   = frameTop + kFrameInset;
    const int screenBot   = frameBot - kFrameInset;
    const int screenH     = screenBot - screenTop;

    // Scrollbar — bottom of screen
    {
        auto r = juce::Rectangle<int> (screenX, screenBot - kScrollbarH, screenW, kScrollbarH);
        scrollZoomBar.setBounds (r);
    }

    // Action panel — top of screen
    {
        auto r = juce::Rectangle<int> (screenX, screenTop, screenW, kActionH);
        actionPanel.setBounds (r);
    }

    // Slice lane — just below action panel
    {
        int y = screenTop + kActionH;
        auto r = juce::Rectangle<int> (screenX, y, screenW, kSliceLaneH);
        sliceLane.setBounds (r);
    }

    // Waveform — fills remaining space between slice lane and scrollbar
    // Shrinks when trim bar is active so the bar sits below without overlapping.
    {
        int y      = screenTop + kActionH + kSliceLaneH;
        int trimH  = (trimDialog != nullptr) ? kTrimBarH : 0;
        int h      = screenBot - kScrollbarH - trimH - y;
        waveformView.setBounds (juce::Rectangle<int> (screenX, y, screenW, h));

        if (trimDialog != nullptr)
            trimDialog->setBounds (screenX, y + h, screenW, kTrimBarH);
    }

    // ShortcutsPanel covers the whole editor as an overlay
    if (shortcutsPanel.isVisible())
        shortcutsPanel.setBounds (getLocalBounds());
}

void DysektEditor::toggleMixerPanel()
{
    mixerOpen = ! mixerOpen;
    mixerPanel.setVisible (mixerOpen);
    headerBar.setBodeActive (mixerOpen);
    setSize (getWidth(), computeTotalHeight());
    resized();
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

    // ⌘? — toggle keyboard shortcuts panel
    if ((code == '?' || code == '/') && mods.isCommandDown())
    { toggleShortcutsPanel(); return true; }

    if (mods.isCommandDown() || mods.isAltDown()) return false;

    // Esc closes shortcuts panel if open
    if (code == juce::KeyPress::escapeKey && shortcutsPanel.isVisible())
    { toggleShortcutsPanel(); return true; }

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
    // 'Z' snap shortcut removed — snap-to-zero always on
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
                                 || playbackActive || processor.lazyChop.isActive()
                                 || (processor.liveDragSliceIdx.load (std::memory_order_relaxed) >= 0);
    const bool waveformNeedsRepaint = uiChanged || viewportChanged || waveformAnimating || lastWaveformAnimating;
    const bool laneNeedsRepaint     = uiChanged || viewportChanged || previewActive || lastPreviewActive;
    const bool rulerNeedsRepaint    = uiChanged || viewportChanged || rulerDragging;

    lastWaveformAnimating = waveformAnimating;
    lastPreviewActive     = previewActive;

    // ── Trim session: enter trim mode once the async load completes ───────────
    if (trimSession != nullptr && ! trimSession->active)
    {
        auto snap = processor.sampleData.getSnapshot();
        if (snap != nullptr && snap->filePath == trimSession->file.getFullPathName())
        {
            trimSession->active = true;
            const int totalFrames = snap->buffer.getNumSamples();
            waveformView.enterTrimMode (0, totalFrames);

            // Show the trim bar overlay
            if (trimDialog == nullptr)
            {
                trimDialog = std::make_unique<TrimDialog> (processor, waveformView);
                addAndMakeVisible (*trimDialog);
                trimDialog->toFront (false);
                actionPanel.setTrimLocked (true);
                resized();   // re-layout: waveform shrinks, trim bar placed below
            }
        }
    }

    const int targetHz = waveformAnimating ? 60 : 30;
    if (targetHz != timerHz) { startTimerHz (targetHz); timerHz = targetHz; }

    if (waveformNeedsRepaint) waveformView.repaint();
    if (laneNeedsRepaint)     sliceLane.repaint();
    if (rulerNeedsRepaint)    scrollZoomBar.repaint();
    // v8: refresh both LCD panels
    sliceLcd.repaintLcd();
    sliceWaveformLcd.repaintLcd();

    headerBar.repaint();
    sliceControlBar.repaint();
    if (uiChanged) actionPanel.repaint();
    if (mixerOpen) mixerPanel.updateFromSnapshot();
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

    // Apply persisted wave style
    waveformView.setSoftWaveform (softWave);
    actionPanel.setWaveActive (softWave);

    // Sync MIDI follow button with processor state (restored from project)
    headerBar.setMidiFollowActive (processor.midiSelectsSlice.load());

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
    processor.zoom.store (1.0f);
    processor.scroll.store (0.0f);
    showTrimDialog (f);
}
