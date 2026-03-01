#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class DysektProcessor;
class WaveformView;

// TrimDialog is used in two ways:
//  1. As an inline trim-control toolbar added to the editor by ActionPanel
//     (constructor: TrimDialog(DysektProcessor&, WaveformView&))
//  2. Via the static show() method for a modal YES/NO file-load prompt

class TrimDialog : public juce::Component
{
public:
    struct Result
    {
        bool userClickedYes  = false;
        bool rememberChoice  = false;
    };

    // Inline toolbar constructor (used by ActionPanel)
    TrimDialog (DysektProcessor& proc, WaveformView& wv);
    ~TrimDialog() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent& e) override;

    // Modal file-load dialog — static helper
    // Shows an AlertWindow asking "Do you want to trim this file?".
    // Calls onComplete on the message thread before the function returns.
    static void show (juce::Component* parent,
                      const juce::File& file,
                      double durationSeconds,
                      std::function<void(const Result&)> onComplete);

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    juce::Rectangle<int> applyBtn, resetBtn, cancelBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrimDialog)
};
