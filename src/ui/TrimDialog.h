#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;
class WaveformView;

/** Floating panel that appears over the waveform when trim mode is active.
    Shows APPLY TRIM / RESET TRIM / CANCEL controls and the current trim range. */
class TrimDialog : public juce::Component
{
public:
    TrimDialog (DysektProcessor& p, WaveformView& wv);
    ~TrimDialog() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    juce::TextButton applyBtn  { "APPLY TRIM"  };
    juce::TextButton resetBtn  { "RESET TRIM"  };
    juce::TextButton cancelBtn { "CANCEL"      };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)
};
