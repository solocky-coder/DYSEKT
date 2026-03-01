#include "TrimDialog.h"
#include "WaveformView.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

TrimDialog::TrimDialog (DysektProcessor& proc, WaveformView& wv)
    : processor (proc), waveformView (wv)
{
    setInterceptsMouseClicks (true, true);
}

TrimDialog::~TrimDialog() = default;

void TrimDialog::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    g.fillAll (theme.darkBar.withAlpha (0.95f));

    const auto drawBtn = [&] (juce::Rectangle<int> r, const juce::String& label, bool highlight)
    {
        g.setColour (highlight ? theme.accent.withAlpha (0.25f) : theme.button);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (highlight ? theme.accent : theme.foreground);
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        g.drawText (label, r, juce::Justification::centred);
    };

    drawBtn (applyBtn,  "APPLY TRIM", true);
    drawBtn (resetBtn,  "RESET",      false);
    drawBtn (cancelBtn, "CANCEL",     false);
}

void TrimDialog::resized()
{
    const int h    = getHeight();
    const int btnH = juce::jmin (h - 6, 26);
    const int btnW = 90;
    const int gap  = 6;
    const int y    = (h - btnH) / 2;
    int x = getWidth() - (btnW * 3 + gap * 2) - 8;

    applyBtn  = { x,                     y, btnW, btnH };
    resetBtn  = { x + btnW + gap,         y, btnW, btnH };
    cancelBtn = { x + (btnW + gap) * 2,  y, btnW, btnH };
}

void TrimDialog::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();

    if (applyBtn.contains (pos))
    {
        DysektProcessor::Command cmd;
        cmd.type      = DysektProcessor::CmdApplyTrim;
        cmd.intParam1 = waveformView.getTrimIn();
        cmd.intParam2 = waveformView.getTrimOut();
        processor.pushCommand (cmd);
        waveformView.setTrimMode (false);
        if (auto* parent = getParentComponent())
            parent->removeChildComponent (this);
        return;
    }

    if (resetBtn.contains (pos))
    {
        waveformView.resetTrim();
        return;
    }

    if (cancelBtn.contains (pos))
    {
        waveformView.setTrimMode (false);
        if (auto* parent = getParentComponent())
            parent->removeChildComponent (this);
    }
}

// ── Static async helper ────────────────────────────────────────────────────────
// Buttons: 0 = "Trim Once", 1 = "Always Trim", 2 = "Skip"
// Callback result: 1=Trim Once, 2=Always Trim, 3=Skip, 0=dismissed
void TrimDialog::show (juce::Component* parent,
                       const juce::File& file,
                       double durationSeconds,
                       std::function<void(const Result&)> onComplete)
{
    if (! onComplete)
        return;

    const juce::String msg = "\"" + file.getFileNameWithoutExtension() + "\""
        + " is " + juce::String (durationSeconds, 1) + " seconds long.\n\n"
        + "Would you like to set trim IN/OUT points before loading?";

    auto options = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle ("Trim Sample?")
        .withMessage (msg)
        .withButton ("Trim Once")
        .withButton ("Always Trim")
        .withButton ("Skip")
        .withAssociatedComponent (parent);

    // showAsync calls the callback with 1, 2, or 3 (button pressed) or 0 (dismissed)
    juce::AlertWindow::showAsync (options, [onComplete] (int btn)
    {
        Result r;
        r.userClickedYes = (btn == 1 || btn == 2);
        r.rememberChoice = (btn == 2);  // "Always Trim" = remember=true
        onComplete (r);
    });
}
