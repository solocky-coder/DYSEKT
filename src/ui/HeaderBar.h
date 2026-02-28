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

private:
    void showThemePopup();
    void adjustScale (float delta);
    void openFileBrowser();
    void openRelinkBrowser();

    DysektProcessor& processor;

    juce::TextButton undoBtn  { "UNDO" };
    juce::TextButton redoBtn  { "REDO" };
    juce::TextButton panicBtn { "PANIC" };
    juce::TextButton themeBtn { "UI" };

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

    // Unused but kept so headerCells vector type still compiles if referenced elsewhere
    struct HeaderCell { int x = 0, y = 0, w = 0, h = 0; juce::String paramId;
                        float minVal = 0, maxVal = 1, step = 1;
                        bool isChoice = false, isBoolean = false,
                             isReadOnly = false, isSetBpm = false; };
    std::vector<HeaderCell> headerCells;
};
