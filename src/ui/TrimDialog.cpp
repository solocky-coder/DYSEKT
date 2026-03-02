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
    // Commit the current trim marker positions to the processor
    processor.trimInSample.store  (waveformView.getTrimIn());
    processor.trimOutSample.store (waveformView.getTrimOut());

    waveformView.setTrimMode (false);

    // Remove this dialog from its parent (ActionPanel will reset the unique_ptr)
    if (auto* parent = getParentComponent())
        parent->removeChildComponent (this);
}

void TrimDialog::onCancel()
{
    waveformView.setTrimMode (false);

    if (auto* parent = getParentComponent())
        parent->removeChildComponent (this);
}

// Minimal stub: resolves the linker error; expand later to show an interactive dialog.
void TrimDialog::show (const juce::String& /*fileName*/, double /*durationSecs*/,
                       juce::Component* /*parent*/,
                       std::function<void (Result)> callback)
{
    if (callback)
        callback (Result{});
}
