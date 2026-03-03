#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  Thin scrolling-waveform strip showing real-time audio output.
 *
 *   Reads directly from DysektProcessor::oscRingBuffer / oscRingWriteHead.
 *   Designed to sit at kOscilloscopeH = 28 px.
 *   Call repaint() from the editor's timerCallback() at whatever rate you like
 *   (30–60 Hz is fine).
 */
class OscilloscopeView : public juce::Component
{
public:
    explicit OscilloscopeView (DysektProcessor& p);
    ~OscilloscopeView() override = default;

    void paint (juce::Graphics& g) override;

    /** Suggested component height in un-scaled pixels. */
    static constexpr int kPreferredHeight = 28;

private:
    DysektProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloscopeView)
};
