#include "TrimDialog.h"
#include "DysektLookAndFeel.h"

TrimDialog::TrimDialog (const juce::String& fileName,
                        double             durationSeconds,
                        std::function<void (Result)> callbackIn)
    : callback (std::move (callbackIn))
{
    setSize (kWidth, kHeight);

    // Title
    titleLabel.setText ("Trim Sample?", juce::dontSendNotification);
    titleLabel.setFont (DysektLookAndFeel::makeFont (13.0f, true));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // Info
    juce::String dur = juce::String (durationSeconds, 1) + "s";
    juce::String info = fileName + "\n" + dur;
    infoLabel.setText (info, juce::dontSendNotification);
    infoLabel.setFont (DysektLookAndFeel::makeFont (11.0f));
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    infoLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (infoLabel);

    // YES button
    auto accent = getTheme().accent;
    yesBtn.setColour (juce::TextButton::buttonColourId,  accent.withAlpha (0.8f));
    yesBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    yesBtn.onClick = [this] { dismiss (true); };
    addAndMakeVisible (yesBtn);

    // NO button
    noBtn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
    noBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    noBtn.onClick = [this] { dismiss (false); };
    addAndMakeVisible (noBtn);

    // Remember checkbox
    rememberChk.setButtonText ("Remember my choice");
    rememberChk.setColour (juce::ToggleButton::textColourId,    juce::Colours::white.withAlpha (0.7f));
    rememberChk.setColour (juce::ToggleButton::tickColourId,    accent);
    rememberChk.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible (rememberChk);
}

void TrimDialog::paint (juce::Graphics& g)
{
    // Dark background with slight border
    g.fillAll (getTheme().header);
    g.setColour (getTheme().accent.withAlpha (0.4f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
}

void TrimDialog::resized()
{
    auto bounds = getLocalBounds().reduced (12, 8);

    titleLabel.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (2);
    infoLabel.setBounds  (bounds.removeFromTop (28));
    bounds.removeFromTop (6);

    auto btnRow = bounds.removeFromTop (24);
    yesBtn.setBounds (btnRow.removeFromLeft (100));
    btnRow.removeFromLeft (8);
    noBtn.setBounds  (btnRow.removeFromLeft (100));

    bounds.removeFromTop (6);
    rememberChk.setBounds (bounds.removeFromTop (20));
}

void TrimDialog::dismiss (bool trim)
{
    Result r { trim, rememberChk.getToggleState() };
    if (auto* modal = juce::Component::getCurrentlyModalComponent())
        modal->exitModalState (0);
    if (callback)
        callback (r);
}

void TrimDialog::show (const juce::String& fileName,
                       double             durationSeconds,
                       juce::Component*   parent,
                       std::function<void (Result)> callback)
{
    auto* dlg = new TrimDialog (fileName, durationSeconds, std::move (callback));

    if (parent != nullptr)
    {
        // Centre over parent in screen coordinates
        auto parentBounds = parent->getScreenBounds();
        dlg->setTopLeftPosition (
            parentBounds.getCentreX() - dlg->getWidth() / 2,
            parentBounds.getCentreY() - dlg->getHeight() / 2);
    }

    dlg->addToDesktop (juce::ComponentPeer::windowHasDropShadow
                       | juce::ComponentPeer::windowIsTemporary);
    dlg->setVisible (true);
    dlg->enterModalState (true, nullptr, true /*deleteOnExit*/);
}
