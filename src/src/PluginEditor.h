#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/LogoBar.h"
#include "ui/HeaderBar.h"
#include "params/ParamIds.h"
#include "ui/SliceLane.h"
#include "ui/SliceControlBar.h"
#include "ui/WaveformView.h"
#include "ui/ActionPanel.h"
#include "ui/ShortcutsPanel.h"
#include "ui/FileBrowserPanel.h"
#include "ui/MixerPanel.h"
#include "ui/TrimDialog.h"
#include "ui/MidiLearnDialog.h"
#include "TrimSession.h"
#include "ui/SliceLcdDisplay.h"
#include "ui/SliceWaveformLcd.h"

// -------- Include constants header here:
#include "ui/PluginEditorConstants.h"
// ---------------------------------------

class DysektEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
// ... rest of your class unchanged ...
public:
    explicit DysektEditor (DysektProcessor&);
    ~DysektEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void setScaleFactor (float newScale) override;

    juce::StringArray getAvailableThemes();
    void applyTheme (const juce::String& themeName);

    void toggleBrowserPanel();
    void toggleSoftWave();
    void toggleMidiFollow();

    void showTrimDialog (const juce::File& file, bool isRelink = false);
    void showTrimMode   (const juce::File& file);

private:
    void timerCallback() override;
    void ensureDefaultThemes();
    void saveUserSettings (float scale, const juce::String& themeName);
    void loadUserSettings();
    int  computeTotalHeight() const;

    DysektProcessor& processor;
    float    lastScale             = 1.0f;
    bool     scaleDirty            = true;
    float    hostScale             = 1.0f;
    float    lastZoom              = -1.0f;
    float    lastScroll            = -1.0f;
    int      timerHz               = 30;
    bool     lastWaveformAnimating = false;
    bool     lastPreviewActive     = false;
    float    savedScale            = -1.0f;
    uint32_t lastUiSnapshotVersion = 0;
    int      lastNumSlices         = -1;
    bool     lastTrimActive        = false;

    bool browserOpen = false;
    bool mixerOpen   = false;
    int  waveformMode = 0;   // 0=Hard 1=Soft 2=Outline 3=Rectified 4=Mirrored 5=Bars 6=RMS 7=Stepped

    std::unique_ptr<TrimSession>     trimSession;
    std::unique_ptr<TrimDialog>      trimDialog;
    std::unique_ptr<MidiLearnDialog> midiLearnDialog;

    DysektLookAndFeel lnf;

    LogoBar         logoBar;
    HeaderBar       headerBar;

    SliceLcdDisplay  sliceLcd;
    SliceWaveformLcd sliceWaveformLcd;

    SliceLane       sliceLane;
    WaveformView    waveformView;
    SliceControlBar sliceControlBar;
    ActionPanel     actionPanel;

    FileBrowserPanel browserPanel;
    MixerPanel       mixerPanel;
    ShortcutsPanel   shortcutsPanel { processor };

    juce::TooltipWindow tooltipWindow { this, 500 };

    void toggleMixerPanel();
    void toggleShortcutsPanel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektEditor)
};