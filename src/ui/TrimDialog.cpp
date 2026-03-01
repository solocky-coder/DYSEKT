#include "TrimDialog.h"  

TrimDialog::TrimDialog() : DialogWindow("Trim Dialog", juce::Colours::white, true)
{
    setResizable(true, true);
    setUsingNativeTitleBar(true);

    messageLabel.setText("Are you sure you want to trim?", juce::dontSendNotification);
    addAndMakeVisible(messageLabel);

    yesBtn.setButtonText("Yes");
    yesBtn.onClick = [this]() { onYesClicked(); };
    addAndMakeVisible(yesBtn);

    noBtn.setButtonText("No");
    noBtn.onClick = [this]() { onNoClicked(); };
    addAndMakeVisible(noBtn);

    rememberBtn.setButtonText("Remember this choice");
    addAndMakeVisible(rememberBtn);

    // Proper styling
    messageLabel.setColour(juce::Label::ColourIds::textColourId, juce::Colours::black);
    yesBtn.setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colours::lightblue);
    noBtn.setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colours::red);
    rememberBtn.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::green);
}

void TrimDialog::paint(Graphics& g)
{
    g.fillAll(juce::Colour(50, 50, 50)); // dark grey background
}

void TrimDialog::resized() 
{
    auto area = getLocalBounds();
    messageLabel.setBounds(area.removeFromTop(50));
    yesBtn.setBounds(area.removeFromTop(30));
    noBtn.setBounds(area.removeFromTop(30));
    rememberBtn.setBounds(area.removeFromTop(30));
}

void TrimDialog::onYesClicked() 
{
    // Call the callback and exit modal state
    callback();
    exitModalState(0);
}

void TrimDialog::onNoClicked() 
{
    // Call the callback and exit modal state
    callback();
    exitModalState(0);
}

void TrimDialog::show() 
{
    TrimDialog dialog;
    dialog.runModal();
}