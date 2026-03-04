#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  Centre control frame that sits between the two LCD panels.
 *
 *   Top row:    [FIL icon] [WA icon] [CH icon]  [sliceCount]
 *   Bottom row: [ROOT knob]  [PITCH knob]  [VOL knob]
 *
 *   Owned by HeaderBar; exposed via HeaderBar::getControlFrame().
 *   PluginEditor::resized() calls setBounds() on it directly.
 */
class DualLcdControlFrame : public juce::Component
{
public:
    explicit DualLcdControlFrame (DysektProcessor& p);

    void paint   (juce::Graphics& g) override;
    void resized () override;
    void mouseDown        (const juce::MouseEvent& e) override;
    void mouseDrag        (const juce::MouseEvent& e) override;
    void mouseUp          (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    // Callbacks wired by HeaderBar (which forwards them to PluginEditor)
    std::function<void()> onBrowserToggle;
    std::function<void()> onWaveToggle;
    std::function<void()> onChromaticToggle;

    // State sync — called by HeaderBar so icons stay in sync with editor state
    void setBrowserActive   (bool v) { browserActive   = v; repaint(); }
    void setWaveActive      (bool v) { waveActive      = v; repaint(); }
    void setChromaticActive (bool v) { chromaticActive = v; repaint(); }

private:
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> b,
                   int type, bool active);

    DysektProcessor& processor;

    // Icon buttons — top row
    juce::TextButton filBtn { "FIL" };
    juce::TextButton waBtn  { "WA"  };
    juce::TextButton chBtn  { "CH"  };
    bool browserActive   = false;
    bool waveActive      = false;
    bool chromaticActive = false;

    // Hit areas for knobs (bottom row) — set during paint()
    juce::Rectangle<int> rootKnobArea;
    juce::Rectangle<int> pitchKnobArea;
    juce::Rectangle<int> volKnobArea;

    // Mouse drag state
    enum class DragTarget { None, Root, Pitch, Volume };
    DragTarget dragTarget    = DragTarget::None;
    float  dragStartValue    = 0.0f;
    int    dragStartY        = 0;

    std::unique_ptr<juce::TextEditor> textEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DualLcdControlFrame)
};
