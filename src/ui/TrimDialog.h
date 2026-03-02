#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

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
