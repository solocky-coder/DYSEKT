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

class DysektProcessor;
class WaveformView;

/// A toolbar component shown at the bottom of the waveform when trim mode is active.
/// Provides Apply and Cancel buttons to confirm or discard a trim operation.
class TrimDialog : public juce::Component
{
public:
    TrimDialog (DysektProcessor& processor, WaveformView& waveformView);
    ~TrimDialog() override;

    void paint  (juce::Graphics& g) override;
    void resized() override;

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    juce::TextButton applyBtn  { "APPLY TRIM" };
    juce::TextButton cancelBtn { "CANCEL" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)

    juce::Label      infoLabel;
    juce::TextButton applyBtn  { "APPLY TRIM" };
    juce::TextButton cancelBtn { "CANCEL" };

    void onApply();
    void onCancel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)
    void dismiss (bool trim);

    juce::Label      titleLabel;
    juce::Label      infoLabel;
    juce::TextButton yesBtn { "YES" };
    juce::TextButton noBtn  { "NO" };
    juce::ToggleButton rememberChk;

    std::function<void (Result)> callback;

    static constexpr int kWidth  = 340;
    static constexpr int kHeight = 140;
};
