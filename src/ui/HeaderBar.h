#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DualLcdControlFrame.h"

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

    // Callbacks set by PluginEditor
    std::function<void()> onBrowserToggle;
    std::function<void()> onWaveToggle;
    std::function<void()> onMidiFollowToggle;
    std::function<void()> onBodeToggle;

    // State sync
    void setBrowserActive   (bool v);
    void setWaveActive      (bool v);
    void setMidiFollowActive (bool v);
    void setBodeActive      (bool v);

    /** Returns the control frame component (FIL/WA/CH icons + ROOT/PITCH/VOL knobs).
     *  PluginEditor adds this as a visible child and positions it between the LCDs. */
    juce::Component* getControlFrame() { return &controlFrame; }

    // Header buttons — public so PluginEditor can access if needed
    juce::TextButton undoBtn      { "UNDO"  };
    juce::TextButton redoBtn      { "REDO"  };
    juce::TextButton panicBtn     { "PANIC" };
    juce::TextButton themeBtn     { "UI"    };
    juce::TextButton shortcutsBtn { "?"     };

    std::function<void()> onShortcutsToggle;

private:
    void showThemePopup();
    void adjustScale (float delta);
    void openRelinkBrowser();

    DysektProcessor& processor;

    // v8: icon buttons and global knobs live in DualLcdControlFrame
    DualLcdControlFrame controlFrame;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::TextEditor>  textEditor;

    // Hit areas for sample info / root (left side, same as before)
    juce::Rectangle<int> sampleInfoBounds;
    juce::Rectangle<int> rootNoteArea;
    juce::Rectangle<int> slicesInfoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
