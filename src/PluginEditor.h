#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/DysektLookAndFeel.h"
#include "ui/HeaderBar.h"
#include "ui/LogoBar.h"
#include "ui/SliceLane.h"
#include "ui/SliceControlBar.h"
#include "ui/WaveformView.h"
#include "ui/ScrollZoomBar.h"
#include "ui/ActionPanel.h"
#include "ui/TrimDialog.h"

#include "ui/FileBrowserPanel.h"
#include "ui/OscilloscopeView.h"

class DysektEditor : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    explicit DysektEditor (DysektProcessor&);
    ~DysektEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // FileDragAndDropTarget — catches drops anywhere on the editor
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    juce::StringArray getAvailableThemes();
    void applyTheme (const juce::String& themeName);


    void toggleBrowserPanel();
    void toggleSoftWave();
    void toggleChromatic();

    /** Show the trim-confirmation dialog for a newly dropped/opened audio file.
        Skips the dialog for soundfonts, very short samples, or when preference
        is already set to "always" or "never". */
    void showTrimDialog (const juce::File& file);

    /** Called by TrimConfirmDialog when the user clicks YES or NO. */
    void onTrimConfirmed (bool trimRequested, bool rememberChoice);

private:
    void timerCallback() override;
    void ensureDefaultThemes();
    void saveUserSettings (float scale, const juce::String& themeName);
    void loadUserSettings();
    int  computeTotalHeight() const;

    /** Actually load the file (called after trim dialog is resolved). */
    void loadFile (const juce::File& file);

    DysektProcessor& processor;
    float lastScale = 1.0f;
    bool scaleDirty = true;
    float lastZoom = -1.0f;
    float lastScroll = -1.0f;
    int timerHz = 30;
    bool lastWaveformAnimating = false;
    bool lastPreviewActive = false;
    float savedScale = -1.0f;
    uint32_t lastUiSnapshotVersion = 0;

    bool browserOpen = false;
    bool softWave    = false;

    /** File waiting for trim confirmation — cleared once decision is made. */
    juce::File pendingTrimFile;

    DysektLookAndFeel lnf;
    LogoBar         logoBar;
    HeaderBar       headerBar;
    SliceLane       sliceLane;
    WaveformView    waveformView;
    ScrollZoomBar   scrollZoomBar;
    SliceControlBar sliceControlBar;
    ActionPanel     actionPanel;

    FileBrowserPanel browserPanel;
    OscilloscopeView oscilloscopeView;

    /** Trim confirmation overlay — shown on top of the waveform view. */
    std::unique_ptr<TrimDialog> trimConfirmDialog;

    juce::TooltipWindow tooltipWindow { this, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektEditor)
};
