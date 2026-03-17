#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParamIds.h"
#include "ui/LogoBar.h"
#include "ui/HeaderBar.h"
#include <algorithm>

// (all your static constants remain unchanged)

static juce::File getSettingsDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DYSEKT");
}
static juce::File getUserSettingsFile() { return getSettingsDir().getChildFile ("settings.yaml"); }
static juce::File getThemesDir()        { return getSettingsDir().getChildFile ("themes"); }

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

    headerBar.onBodeToggle      = [this] { toggleMixerPanel(); };
    headerBar.onBrowserToggle   = [this] { toggleBrowserPanel(); };
    headerBar.onWaveToggle      = [this] { toggleSoftWave(); };
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
        if (duration < 5.0) {
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
    auto headerStrip = area.removeFromTop (kLogoH);
    logoBar.setBounds (headerStrip.removeFromLeft (220));
    headerBar.setBounds (headerStrip);
    auto lcdRow = area.removeFromTop (kLcdRowH).reduced (kMargin, 6);
    const int lcdW = (lcdRow.getWidth() - kCtrlFrameW - kMargin * 2) / 2;
    sliceLcd.setBounds (lcdRow.removeFromLeft (lcdW));
    lcdRow.removeFromLeft (kMargin);
    if (auto* cf = headerBar.getControlFrame())
        cf->setBounds (lcdRow.removeFromLeft (kCtrlFrameW));
    lcdRow.removeFromLeft (kMargin);
    sliceWaveformLcd.setBounds (lcdRow);

    auto actionArea = area.removeFromTop (kActionH);
    const int kFX = kMargin;
    const int kFW = getWidth() - kMargin * 2;
    area.removeFromBottom (kMargin);
    auto slot = area.removeFromBottom (kPanelSlotH);
    area.removeFromBottom (kMargin);

    if (mixerOpen) {
        const int mh = juce::jmin (MixerPanel::kPanelH, kPanelSlotH);
        auto mb = juce::Rectangle<int> (kFX, slot.getY(), kFW, mh);
        mixerPanel.setBounds (mb);
        browserPanel.setBounds ({});
    }
    else if (browserOpen) {
        browserPanel.setBounds (kFX, slot.getY(), kFW, slot.getHeight());
        mixerPanel.setBounds ({});
    } else {
        mixerPanel.setBounds ({});
        browserPanel.setBounds ({});
    }

    auto scbArea = area.removeFromBottom (kSliceCtrlH);
    sliceControlBar.setBounds (juce::Rectangle<int> (kFX, scbArea.getY(), kFW, kSliceCtrlH));
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

    waveformOverview.setBounds (juce::Rectangle<int> (screenX, screenBot - kScrollbarH, screenW, kScrollbarH));
    actionPanel.setBounds (juce::Rectangle<int> (screenX, screenTop, screenW, kActionH));
    int y = screenTop + kActionH;
    sliceLane.setBounds (juce::Rectangle<int> (screenX, y, screenW, kSliceLaneH));
    y      = screenTop + kActionH + kSliceLaneH;
    int trimH  = (trimDialog != nullptr) ? kTrimBarH : 0;
    int h      = screenBot - kScrollbarH - trimH - y;
    waveformView.setBounds (juce::Rectangle<int> (screenX, y, screenW, h));
    if (trimDialog != nullptr)
        trimDialog->setBounds (screenX, y + h, screenW, kTrimBarH);
    if (shortcutsPanel.isVisible())
        shortcutsPanel.setBounds (getLocalBounds());
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

// -- KEYBOARD SHORTCUTS --
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

    // ======= NEW MIDI LEARN DIALOG SHORTCUT =======
    if (code == 'M' && mods.isCommandDown())
    {
        if (!midiLearnDialog)
        {
            midiLearnDialog = std::make_unique<MidiLearnDialog>(processor.midiLearn, [this]{ midiLearnDialog.reset(); resized(); });
            addAndMakeVisible(*midiLearnDialog);
            midiLearnDialog->toFront(true);
            resized();
        }
        return true;
    }
    // ======= END MIDI LEARN SHORTCUT =======

    return false;
}

// ===== ALL OTHER METHODS UNCHANGED BELOW! (trimmed for readability) =====

void DysektEditor::timerCallback() { /* unchanged, from your source */ }
// ...
void DysektEditor::ensureDefaultThemes() {/* unchanged */}
juce::StringArray DysektEditor::getAvailableThemes() {/* unchanged */}
void DysektEditor::applyTheme (const juce::String& themeName) {/* unchanged */}
void DysektEditor::saveUserSettings (float scale, const juce::String& themeName) {/* unchanged */}
void DysektEditor::loadUserSettings(){/* unchanged */}
bool DysektEditor::isInterestedInFileDrag (const juce::StringArray& files){/* unchanged */}
void DysektEditor::filesDropped (const juce::StringArray& files, int, int){/* unchanged */}