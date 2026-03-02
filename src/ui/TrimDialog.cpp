#include "TrimDialog.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"

TrimDialog::TrimDialog (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    addAndMakeVisible (applyBtn);
    addAndMakeVisible (cancelBtn);

    for (auto* btn : { &applyBtn, &cancelBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    applyBtn.onClick = [this]
    {
        int tIn  = processor.trimInSample.load();
        int tOut = processor.trimOutSample.load();
        if (tOut <= tIn)
            tOut = processor.sampleData.getNumFrames();

        if (tOut > tIn && tOut > 0)
        {
            DysektProcessor::Command cmd;
            cmd.type      = DysektProcessor::CmdApplyTrim;
            cmd.intParam1 = tIn;
            cmd.intParam2 = tOut;
            processor.pushCommand (cmd);
        }

        waveformView.setTrimMode (false);
        if (auto* parent = getParentComponent())
            parent->removeChildComponent (this);
    };

    cancelBtn.onClick = [this]
    {
        waveformView.setTrimMode (false);
        waveformView.resetTrim();
        if (auto* parent = getParentComponent())
            parent->removeChildComponent (this);
    };
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
    int h   = getHeight();
    int pad = 4;
    int btnH = h - pad * 2;
    int gap  = 6;

    int cancelW = 60;
    int applyW  = 100;
    cancelBtn.setBounds (getWidth() - cancelW - pad, pad, cancelW, btnH);
    applyBtn.setBounds  (getWidth() - cancelW - applyW - gap - pad, pad, applyW, btnH);
}
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
