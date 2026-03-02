#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DysektLookAndFeel.h"

class DysektProcessor;
class WaveformView;

/**
 *  TrimDialog — small inline overlay component shown at the bottom of the
 *  WaveformView when the user activates Trim mode.
 *
 *  Provides "Apply Trim" and "Cancel" buttons.  Apply commits the current
 *  trimIn/trimOut markers to the sample buffer via CmdApplyTrim; Cancel
 *  simply exits trim mode without modifying any data.
 */
class TrimDialog : public juce::Component
{
public:
    TrimDialog (DysektProcessor& processor, WaveformView& waveformView);
    ~TrimDialog() override = default;

    void paint  (juce::Graphics& g) override;
    void resized() override;

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    juce::TextButton applyBtn { "Apply Trim" };
    juce::TextButton cancelBtn { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)
};
