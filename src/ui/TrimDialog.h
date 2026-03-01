#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class TrimDialog : public juce::DialogWindow
{
public:
    struct Result
    {
        bool userClickedYes = false;
        bool rememberChoice = false;
    };

    explicit TrimDialog(const juce::File& file, double durationSeconds);
    ~TrimDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    static void show(juce::Component* parent, const juce::File& file, double durationSeconds,
                     std::function<void(const Result&)> onComplete);

private:
    void onYesClicked();
    void onNoClicked();

    juce::File audioFile;
    double duration = 0.0;
    std::unique_ptr<juce::TextButton> yesBtn, noBtn;
    std::unique_ptr<juce::ToggleButton> rememberBtn;
    std::unique_ptr<juce::Label> messageLabel;
    std::function<void(const Result&)> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimDialog)
};
