#include "TrimDialog.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"

TrimDialog::TrimDialog (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    applyBtn.setTooltip  ("Crop the sample to the trim markers");
    cancelBtn.setTooltip ("Exit trim mode without changing the sample");

    applyBtn.onClick = [this]
    {
        const int tIn  = waveformView.getTrimIn();
        const int tOut = waveformView.getTrimOut();
        if (tOut > tIn)
        {
            DysektProcessor::Command cmd;
            cmd.type      = DysektProcessor::CmdApplyTrim;
            cmd.intParam1 = tIn;
            cmd.intParam2 = tOut;
            processor.pushCommand (cmd);
        }
        waveformView.setTrimMode (false);
        waveformView.resetTrim();
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

    addAndMakeVisible (applyBtn);
    addAndMakeVisible (cancelBtn);
}

void TrimDialog::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().darkBar.brighter (0.04f));

    // Separator line at top
    g.setColour (getTheme().separator);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void TrimDialog::resized()
{
    auto area = getLocalBounds().reduced (4, 3);
    const int btnW = 90;
    const int gap  = 6;

    cancelBtn.setBounds (area.removeFromRight (btnW));
    area.removeFromRight (gap);
    applyBtn.setBounds  (area.removeFromRight (btnW));
}
