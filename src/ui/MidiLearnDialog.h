#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../MidiLearnManager.h"
#include "DysektLookAndFeel.h"

class MidiLearnDialog : public juce::Component, private juce::ListBoxModel
{
public:
    explicit MidiLearnDialog(MidiLearnManager& ml, std::function<void()> onClose = nullptr);

    void paint(juce::Graphics&) override;
    void resized() override;

    // ListBox Model
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    juce::Component* refreshComponentForRow(int, bool, juce::Component*) override;

private:
    MidiLearnManager& midiLearn;
    std::function<void()> onCloseCallback;

    juce::ListBox   mappingList { "Midi Mapping List", this };
    juce::TextButton closeButton { "Close" };

    // ── Per-row component: param name | CC | encoder mode combo ──────────
struct MappingRowComponent : public juce::Component, private juce::Timer
{
    juce::Label         paramLabel, ccLabel;
    juce::ComboBox      modeCombo;
    juce::TextButton    armButton  { "LEARN" };
    juce::TextButton    clearButton { "X" };
    juce::TextButton    flipButton { "FLIP" };
    int                 fieldId { -1 };
    MidiLearnManager*   midiLearn { nullptr };

    MappingRowComponent()
    {
        const auto& th = getTheme();

        paramLabel.setFont (juce::Font (12.f));
        paramLabel.setColour (juce::Label::textColourId, th.foreground);
        paramLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (paramLabel);

        ccLabel.setFont (juce::Font (12.f));
        ccLabel.setColour (juce::Label::textColourId, th.accent.withAlpha (0.85f));
        ccLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (ccLabel);

        modeCombo.addItem ("Absolute",           MidiLearnManager::kAbsolute      + 1);
        modeCombo.addItem ("Relative 2's Comp",  MidiLearnManager::kRelTwosComp   + 1);
        modeCombo.addItem ("Relative Sign Bit",  MidiLearnManager::kRelSignBit    + 1);
        modeCombo.addItem ("Relative Bin Offset",MidiLearnManager::kRelBinOffset  + 1);
        modeCombo.setJustificationType (juce::Justification::centred);
        modeCombo.onChange = [this]
        {
            if (midiLearn && fieldId >= 0)
            {
                const auto mode = (MidiLearnManager::EncoderMode)(modeCombo.getSelectedId() - 1);
                midiLearn->setEncoderMode (fieldId, mode);
                const bool isRel = (mode != MidiLearnManager::kAbsolute);
                flipButton.setEnabled (isRel);
                flipButton.setAlpha (isRel ? 1.0f : 0.3f);
            }
        };
        addAndMakeVisible (modeCombo);

        // ARM button — blinks while waiting for CC
        armButton.setColour (juce::TextButton::buttonColourId,  th.button);
        armButton.setColour (juce::TextButton::textColourOffId, th.accent);
        armButton.onClick = [this]
        {
            if (! midiLearn || fieldId < 0) return;
            if (midiLearn->getArmedSlot() == fieldId)
            {
                midiLearn->armLearn (-1);  // cancel
                stopTimer();
                armButton.setButtonText ("LEARN");
                armButton.setColour (juce::TextButton::buttonColourId, getTheme().button);
            }
            else
            {
                midiLearn->armLearn (fieldId);
                startTimerHz (4);  // blink at 4Hz while armed
            }
        };
        addAndMakeVisible (armButton);

        // Clear button
        clearButton.setColour (juce::TextButton::buttonColourId,  th.button.withMultipliedSaturation (1.4f).withMultipliedBrightness (0.85f));
        clearButton.setColour (juce::TextButton::textColourOffId, juce::Colours::tomato.withAlpha (0.85f));
        clearButton.onClick = [this]
        {
            if (midiLearn && fieldId >= 0)
            {
                midiLearn->clearMapping (fieldId);
                ccLabel.setText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")), juce::dontSendNotification);
            }
        };
        addAndMakeVisible (clearButton);

        // Flip button — inverts relative encoder direction for this slot
        flipButton.setClickingTogglesState (true);
        flipButton.setColour (juce::TextButton::buttonColourId,   th.button);
        flipButton.setColour (juce::TextButton::buttonOnColourId, th.buttonHover.brighter (0.1f));
        flipButton.setColour (juce::TextButton::textColourOffId,  th.foreground.withAlpha (0.45f));
        flipButton.setColour (juce::TextButton::textColourOnId,   th.accent);
        flipButton.onClick = [this]
        {
            if (midiLearn && fieldId >= 0)
                midiLearn->setDirectionFlip (fieldId, flipButton.getToggleState());
        };
        addAndMakeVisible (flipButton);
    }

    void timerCallback() override
    {
        if (! midiLearn) { stopTimer(); return; }

        // Check if armed slot was captured (audio thread cleared it)
        if (midiLearn->getArmedSlot() != fieldId)
        {
            stopTimer();
            armButton.setButtonText ("LEARN");
            armButton.setColour (juce::TextButton::buttonColourId,
                                 getTheme().button);
            // Refresh CC label
            if (fieldId >= 0)
                ccLabel.setText (midiLearn->getLabelText (fieldId),
                                 juce::dontSendNotification);
            return;
        }

        // Blink while waiting
        const bool lit = ((juce::Time::getMillisecondCounter() / 250) % 2 == 0);
        armButton.setButtonText (lit ? "WAITING..." : "");
        armButton.setColour (juce::TextButton::buttonColourId,
                             lit ? getTheme().buttonHover.brighter (0.2f)
                                 : getTheme().button);
    }

    void update (int field, MidiLearnManager& ml, const juce::String& paramName)
    {
        fieldId   = field;
        midiLearn = &ml;
        paramLabel.setText (paramName, juce::dontSendNotification);
        ccLabel.setText    (ml.getLabelText (field), juce::dontSendNotification);

        const auto mode = ml.getEncoderMode (field);
        modeCombo.setSelectedId ((int) mode + 1, juce::dontSendNotification);

        // Flip only meaningful for relative modes
        const bool isRel = (mode != MidiLearnManager::kAbsolute);
        flipButton.setToggleState (ml.getDirectionFlip (field), juce::dontSendNotification);
        flipButton.setEnabled (isRel);
        flipButton.setAlpha (isRel ? 1.0f : 0.3f);

        // Disarm if re-used for different field
        if (midiLearn->getArmedSlot() != fieldId)
        {
            stopTimer();
            armButton.setButtonText ("LEARN");
            armButton.setColour (juce::TextButton::buttonColourId,
                                 getTheme().button);
        }
    }

    void resized() override
    {
        const int w      = getWidth(), h = getHeight();
        const int armW   = 64;
        const int clearW = 20;
        const int flipW  = 36;
        const int col1   = (int)(w * 0.32f);
        const int col2   = (int)(w * 0.18f);
        const int col3   = w - col1 - col2 - armW - clearW - flipW - 10;
        paramLabel .setBounds (8,                                       0, col1 - 8, h);
        ccLabel    .setBounds (col1,                                    0, col2,     h);
        armButton  .setBounds (col1 + col2,                             2, armW,     h - 4);
        clearButton.setBounds (col1 + col2 + armW + 2,                  2, clearW,   h - 4);
        flipButton .setBounds (col1 + col2 + armW + clearW + 4,         2, flipW,    h - 4);
        modeCombo  .setBounds (col1 + col2 + armW + clearW + flipW + 6, 2, col3,     h - 4);
    }
};

    void encoderModeChanged (int fieldId, MidiLearnManager::EncoderMode mode);
    void close();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnDialog)
};
