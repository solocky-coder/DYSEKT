#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DysektLookAndFeel.h"

// ── TrimDialog ──────────────────────────────────────────────────────────────
// Modal dialog that appears when loading audio files longer than 5 seconds.
// The user can choose to enter trim mode (YES), load the full sample (NO),
// or optionally remember their choice for future loads.
class TrimDialog : public juce::Component
{
public:
    struct Result
    {
        bool trim;      // true  = enter trim mode
        bool remember;  // true  = remember this choice
    };

    // Show the dialog asynchronously.  |callback| is invoked on the message
    // thread after the user dismisses the dialog.
    static void show (const juce::String& fileName,
                      double             durationSeconds,
                      juce::Component*   parent,
                      std::function<void (Result)> callback);

private:
    TrimDialog (const juce::String& fileName,
                double             durationSeconds,
                std::function<void (Result)> callback);

    void paint  (juce::Graphics& g) override;
    void resized() override;

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
