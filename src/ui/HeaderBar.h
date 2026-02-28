#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (DysektProcessor& p);
    void paint            (juce::Graphics& g) override;
    void resized          () override;
    void mouseDown        (const juce::MouseEvent& e) override;
    void mouseDrag        (const juce::MouseEvent& e) override;
    void mouseUp          (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    // Callbacks set by PluginEditor (same targets as ActionPanel had)
    std::function<void()> onBrowserToggle;
    std::function<void()> onWaveToggle;
    std::function<void()> onChromaticToggle;

    // State sync — called by PluginEditor so buttons stay in sync
    void setBrowserActive   (bool v);
    void setWaveActive      (bool v);
    void setChromaticActive (bool v);

private:
    void showThemePopup();
    void adjustScale (float delta);
    void openFileBrowser();
    void openRelinkBrowser();
    void updateAccentBtn (juce::TextButton& btn, bool active);

    DysektProcessor& processor;

    juce::TextButton undoBtn  { "UNDO" };
    juce::TextButton redoBtn  { "REDO" };
    juce::TextButton panicBtn { "PANIC" };
    juce::TextButton themeBtn { "UI" };

    // FIL / WA / CH toggle buttons (moved from ActionPanel)
    juce::TextButton filBtn  { "FIL" };
    juce::TextButton waBtn   { "WA"  };
    juce::TextButton chBtn   { "CH"  };
    bool browserActive    = false;
    bool waveActive       = false;
    bool chromaticActive  = false;

    std::unique_ptr<juce::FileChooser>  fileChooser;
    std::unique_ptr<juce::TextEditor>   textEditor;

    // Drag state for ROOT knob
    bool  draggingRoot     = false;
    int   activeDragCell   = -1;
    float dragStartValue   = 0.0f;
    int   dragStartY       = 0;

    // Hit areas
    juce::Rectangle<int> sampleInfoBounds;
    juce::Rectangle<int> rootNoteArea;
    juce::Rectangle<int> slicesInfoArea;
    juce::Rectangle<int> globalBoxBounds;
    juce::Rectangle<int> globalPitchKnobArea;
    juce::Rectangle<int> globalVolKnobArea;

    // Global knob drag state
    enum class GlobalDragTarget { None, Pitch, Volume };
    GlobalDragTarget globalDragTarget = GlobalDragTarget::None;
    float  globalDragStartValue = 0.f;
    int    globalDragStartY     = 0;

    struct HeaderCell { int x = 0, y = 0, w = 0, h = 0; juce::String paramId;
                        float minVal = 0, maxVal = 1, step = 1;
                        bool isChoice = false, isBoolean = false,
                             isReadOnly = false, isSetBpm = false; };
    std::vector<HeaderCell> headerCells;
};
