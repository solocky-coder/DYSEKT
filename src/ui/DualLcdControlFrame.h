#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  Centre control frame between the two LCD panels.
 *
 *   Top row:    [FIL icon] [WA icon] [CH icon]  [sliceCount chip]
 *   Bottom row: [ROOT knob]  [PITCH knob]  [VOL knob]
 *
 *   No child components — everything is drawn in paint() and hit-tested
 *   in mouseDown() so child buttons can't paint over the custom icons.
 */
class DualLcdControlFrame : public juce::Component
{
public:
    explicit DualLcdControlFrame (DysektProcessor& p);

    void paint        (juce::Graphics& g) override;
    void mouseDown    (const juce::MouseEvent& e) override;
    void mouseDrag    (const juce::MouseEvent& e) override;
    void mouseUp      (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    std::function<void()> onBrowserToggle;
    std::function<void()> onWaveToggle;
    std::function<void()> onChromaticToggle;

    void setBrowserActive   (bool v) { browserActive   = v; repaint(); }
    void setWaveActive      (bool v) { waveActive      = v; repaint(); }
    void setChromaticActive (bool v) { chromaticActive = v; repaint(); }

private:
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> b, int type, bool active);

    DysektProcessor& processor;

    // Icon toggle state
    bool browserActive   = false;
    bool waveActive      = false;
    bool chromaticActive = false;

    // Hit areas (set during paint, used in mouseDown)
    juce::Rectangle<int> filIconArea;
    juce::Rectangle<int> waIconArea;
    juce::Rectangle<int> chIconArea;
    juce::Rectangle<int> rootKnobArea;
    juce::Rectangle<int> pitchKnobArea;
    juce::Rectangle<int> volKnobArea;

    enum class DragTarget { None, Root, Pitch, Volume };
    DragTarget dragTarget    = DragTarget::None;
    float  dragStartValue    = 0.0f;
    int    dragStartY        = 0;

    std::unique_ptr<juce::TextEditor> textEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DualLcdControlFrame)
};
