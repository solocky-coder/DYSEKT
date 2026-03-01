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
}

TrimDialog::~TrimDialog() = default;

void TrimDialog::paint (juce::Graphics& g)
{
    g.setColour (getTheme().darkBar.withAlpha (0.95f));
    g.fillRect (getLocalBounds());
    g.setColour (getTheme().separator);
    g.drawRect (getLocalBounds(), 1);
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
