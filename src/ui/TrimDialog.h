#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DysektLookAndFeel.h"

class DysektProcessor;
class WaveformView;

/** Inline trim panel rendered at the bottom of the waveform view.
    Shows APPLY TRIM and CANCEL buttons; on apply it pushes CmdApplyTrim
    to the processor using the current trimInSample / trimOutSample values. */
class TrimDialog : public juce::Component
{
public:
    TrimDialog (DysektProcessor& p, WaveformView& wv);
    ~TrimDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    juce::TextButton applyBtn  { "APPLY TRIM" };
    juce::TextButton cancelBtn { "CANCEL" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)
};
