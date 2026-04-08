#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DysektLookAndFeel.h"

class ConfirmOverlay : public juce::Component
{
public:
    std::function<void(bool)> onResult;

    ConfirmOverlay (const juce::String& title,
                    const juce::String& message,
                    const juce::String& yesText,
                    const juce::String& noText)
        : titleText (title), messageText (message)
    {
        yesBtn.setButtonText (yesText);
        noBtn .setButtonText (noText);

        const auto& T = getTheme();
        yesBtn.setColour (juce::TextButton::buttonColourId,  T.accent.withAlpha (0.85f));
        yesBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        noBtn .setColour (juce::TextButton::buttonColourId,  T.button);
        noBtn .setColour (juce::TextButton::textColourOffId, T.foreground);

        yesBtn.onClick = [this] { if (onResult) onResult (true);  };
        noBtn .onClick = [this] { if (onResult) onResult (false); };

        addAndMakeVisible (yesBtn);
        addAndMakeVisible (noBtn);

        setInterceptsMouseClicks (true, true);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& T = getTheme();

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (getLocalBounds());

        const auto box = dialogBox();
        g.setColour (T.header);
        g.fillRoundedRectangle (box.toFloat(), 5.0f);
        g.setColour (T.accent.withAlpha (0.7f));
        g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 5.0f, 1.5f);

        const int padX = 18;
        g.setFont (DysektLookAndFeel::makeFont (15.0f, true));
        g.setColour (T.accent);
        g.drawText (titleText,
                    box.getX() + padX, box.getY() + 14,
                    box.getWidth() - padX * 2, 20,
                    juce::Justification::centredLeft, false);

        g.setColour (T.accent.withAlpha (0.4f));
        g.drawHorizontalLine (box.getY() + 38, (float)(box.getX() + padX), (float)(box.getRight() - padX));

        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (T.foreground.withAlpha (0.85f));
        g.drawText (messageText,
                    box.getX() + padX, box.getY() + 46,
                    box.getWidth() - padX * 2, 36,
                    juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        const auto box  = dialogBox();
        const int btnW  = 90;
        const int btnH  = 28;
        const int gap   = 10;
        const int btnY  = box.getBottom() - btnH - 14;
        const int totalBtnW = btnW * 2 + gap;
        const int btnX  = box.getCentreX() - totalBtnW / 2;

        yesBtn.setBounds (btnX,           btnY, btnW, btnH);
        noBtn .setBounds (btnX + btnW + gap, btnY, btnW, btnH);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! dialogBox().contains (e.getPosition()))
            if (onResult) onResult (false);
    }

private:
    juce::String titleText, messageText;
    juce::TextButton yesBtn, noBtn;

    juce::Rectangle<int> dialogBox() const
    {
        const int w = juce::jmin (400, getWidth() - 40);
        const int h = 148;
        return juce::Rectangle<int> (
            (getWidth()  - w) / 2,
            (getHeight() - h) / 2,
            w, h);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfirmOverlay)
};
