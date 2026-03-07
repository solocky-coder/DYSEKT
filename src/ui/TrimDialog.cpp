#include "TrimDialog.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"

TrimDialog::TrimDialog (DysektProcessor& proc, WaveformView& wv)
    : processor (proc), waveformView (wv)
{
    infoLabel.setText ("Trim mode: drag markers to set in/out points", juce::dontSendNotification);
    infoLabel.setFont (DysektLookAndFeel::makeFont (11.0f));
    infoLabel.setColour (juce::Label::textColourId, getTheme().foreground.withAlpha (0.8f));
    infoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (infoLabel);

    applyBtn.setColour (juce::TextButton::buttonColourId,  getTheme().accent.withAlpha (0.8f));
    applyBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    applyBtn.onClick = [this] { onApply(); };
    addAndMakeVisible (applyBtn);

    cancelBtn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
    cancelBtn.setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    cancelBtn.onClick = [this] { onCancel(); };
    addAndMakeVisible (cancelBtn);
}

TrimDialog::~TrimDialog() = default;

void TrimDialog::paint (juce::Graphics& g)
{
    g.setColour (getTheme().darkBar.withAlpha (0.95f));
    g.fillRect (getLocalBounds());
    g.setColour (getTheme().separator);
    g.drawRect (getLocalBounds(), 1);
    g.fillAll (getTheme().header);
    g.setColour (getTheme().accent.withAlpha (0.3f));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);
}

void TrimDialog::resized()
{
    auto bounds = getLocalBounds().reduced (6, 4);
    const int btnW = 90;
    const int gap  = 6;

    cancelBtn.setBounds (bounds.removeFromRight (btnW));
    bounds.removeFromRight (gap);
    applyBtn.setBounds (bounds.removeFromRight (btnW));
    bounds.removeFromRight (gap);
    infoLabel.setBounds (bounds);
}

void TrimDialog::onApply()
{
    const int trimIn  = waveformView.getTrimIn();
    const int trimOut = waveformView.getTrimOut();

    waveformView.setTrimMode (false);

    // Fire the callback wired in PluginEditor → processor.applyTrimToCurrentSample
    if (waveformView.onTrimApplied)
        waveformView.onTrimApplied (trimIn, trimOut);

    if (auto* parent = getParentComponent())
        parent->removeChildComponent (this);
}

void TrimDialog::onCancel()
{
    waveformView.setTrimMode (false);

    if (waveformView.onTrimCancelled)
        waveformView.onTrimCancelled();

    if (auto* parent = getParentComponent())
        parent->removeChildComponent (this);
}

void TrimDialog::show (const juce::String& fileName, double durationSecs,
                       juce::Component* parent,
                       std::function<void (Result)> callback)
{
    // Build message: file name + duration
    const int    totalSec = (int) durationSecs;
    const int    mins     = totalSec / 60;
    const int    secs     = totalSec % 60;
    juce::String dur      = juce::String::formatted ("%d:%02d", mins, secs);
    juce::String msg      = fileName + "  (" + dur + ")\n\nDo you want to trim this sample before loading?";

    auto* box       = new juce::AlertWindow ("Trim Sample", msg, juce::MessageBoxIconType::QuestionIcon, parent);
    auto* rememberBtn = new juce::ToggleButton ("Remember my choice");
    rememberBtn->setToggleState (false, juce::dontSendNotification);
    box->addButton ("Trim",      1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Load Full", 2);
    box->addCustomComponent (rememberBtn);   // AlertWindow owns lifetime

    box->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [box, rememberBtn, cb = std::move (callback)] (int result) mutable
            {
                Result r;
                r.trim     = (result == 1);
                r.remember = rememberBtn->getToggleState();
                if (cb) cb (r);
            }),
        true);  // deleteWhenDismissed = true
}
