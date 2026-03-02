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
    struct Result
    {
        bool trim     = false;
        bool remember = false;
    };

    static void show (const juce::String& fileName, double durationSecs,
                      juce::Component* parent,
                      std::function<void (Result)> callback);

    TrimDialog (DysektProcessor& processor, WaveformView& waveformView);
    ~TrimDialog() override;

    void paint  (juce::Graphics& g) override;
    void resized() override;

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    juce::Label      infoLabel;
    juce::TextButton applyBtn  { "APPLY TRIM" };
    juce::TextButton cancelBtn { "CANCEL" };

    void onApply();
    void onCancel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)
};
