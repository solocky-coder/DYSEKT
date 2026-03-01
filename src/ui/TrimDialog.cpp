#include "TrimDialog.h"

TrimDialog::TrimDialog(const juce::File& file, double durationSeconds)
    : juce::DialogWindow("Trim Sample", juce::Colours::darkgrey, true, false),
      audioFile(file), duration(durationSeconds)
{
    auto content = std::make_unique<juce::Component>();

    messageLabel = std::make_unique<juce::Label>();
    messageLabel->setText("Sample: " + file.getFileName() + " (" +
                          juce::String(duration, 1) + "s)\nTrim before loading?",
                          juce::dontSendNotification);
    messageLabel->setFont(juce::FontOptions{}.withHeight(14.0f));
    messageLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    content->addAndMakeVisible(*messageLabel);

    yesBtn = std::make_unique<juce::TextButton>("YES");
    yesBtn->onClick = [this] { onYesClicked(); };
    content->addAndMakeVisible(*yesBtn);

    noBtn = std::make_unique<juce::TextButton>("NO");
    noBtn->onClick = [this] { onNoClicked(); };
    content->addAndMakeVisible(*noBtn);

    rememberBtn = std::make_unique<juce::ToggleButton>("Remember my choice");
    rememberBtn->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    content->addAndMakeVisible(*rememberBtn);

    setContentOwned(content.release(), false);
    setResizable(false, false);
    setSize(340, 160);
    centreAroundComponent(nullptr, 340, 160);
}

TrimDialog::~TrimDialog() = default;

void TrimDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1a1a1a));
}

void TrimDialog::resized()
{
    auto bounds = getLocalBounds().reduced(12);

    messageLabel->setBounds(bounds.removeFromTop(60));
    bounds.removeFromTop(10);

    auto buttonRow = bounds.removeFromTop(30);
    yesBtn->setBounds(buttonRow.removeFromLeft(100));
    buttonRow.removeFromLeft(20);
    noBtn->setBounds(buttonRow.removeFromLeft(100));

    bounds.removeFromTop(10);
    rememberBtn->setBounds(bounds);
}

void TrimDialog::onYesClicked()
{
    if (callback)
    {
        Result result;
        result.userClickedYes = true;
        result.rememberChoice = rememberBtn->getToggleState();
        callback(result);
    }
    exitModalState(1);
}

void TrimDialog::onNoClicked()
{
    if (callback)
    {
        Result result;
        result.userClickedYes = false;
        result.rememberChoice = rememberBtn->getToggleState();
        callback(result);
    }
    exitModalState(0);
}

void TrimDialog::show(juce::Component* parent, const juce::File& file, double durationSeconds,
                      std::function<void(const Result&)> onComplete)
{
    auto* dialog = new TrimDialog(file, durationSeconds);
    dialog->callback = onComplete;
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [dialog](int) { delete dialog; }), false);
}
