#include "TrimDialog.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"

TrimDialog::TrimDialog (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    for (auto* btn : { &applyBtn, &resetBtn, &cancelBtn })
    {
        addAndMakeVisible (btn);
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    applyBtn.setTooltip ("Apply trim — this permanently crops the audio to the selected region");
    resetBtn.setTooltip ("Reset trim markers to the full sample range");
    cancelBtn.setTooltip ("Cancel trim mode");

    applyBtn.onClick = [this]
    {
        const int tIn  = waveformView.getTrimIn();
        const int tOut = waveformView.getTrimOut();
        if (tOut > tIn + 64)
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

    resetBtn.onClick = [this]
    {
        waveformView.resetTrim();
    };

    cancelBtn.onClick = [this]
    {
        waveformView.setTrimMode (false);
        if (auto* parent = getParentComponent())
            parent->removeChildComponent (this);
    };
}

TrimDialog::~TrimDialog()
{
    waveformView.setTrimMode (false);
}

void TrimDialog::paint (juce::Graphics& g)
{
    g.setColour (getTheme().darkBar.withAlpha (0.95f));
    g.fillRect (getLocalBounds());

    g.setColour (getTheme().separator);
    g.drawRect (getLocalBounds(), 1);

    // Show current trim range as sample counts
    const int tIn  = waveformView.getTrimIn();
    const int tOut = waveformView.getTrimOut();
    const int len  = tOut - tIn;

    g.setFont (DysektLookAndFeel::makeFont (10.5f));
    g.setColour (getTheme().foreground.withAlpha (0.65f));
    juce::String info = "TRIM: " + juce::String (tIn) + " – " + juce::String (tOut)
                        + "  (" + juce::String (len) + " samples)";
    g.drawText (info, 6, 0, getWidth() - 6, getHeight(), juce::Justification::centredLeft);
}

void TrimDialog::resized()
{
    const int pad  = 4;
    const int h    = getHeight() - pad * 2;
    const int gap  = 6;
    const int appW = 100;
    const int canW = 92;
    const int resW = 92;

    int x = getWidth() - pad - canW;
    cancelBtn.setBounds (x, pad, canW, h);
    x -= gap + resW;
    resetBtn.setBounds (x, pad, resW, h);
    x -= gap + appW;
    applyBtn.setBounds (x, pad, appW, h);
}
