#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../MidiLearnManager.h"

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
    struct MappingRowComponent : public juce::Component
    {
        juce::Label         paramLabel, ccLabel;
        juce::ComboBox      modeCombo;
        int                 fieldId { -1 };
        MidiLearnManager*   midiLearn { nullptr };

        MappingRowComponent()
        {
            paramLabel.setFont (juce::Font (12.f));
            paramLabel.setColour (juce::Label::textColourId, juce::Colours::white);
            paramLabel.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (paramLabel);

            ccLabel.setFont (juce::Font (12.f));
            ccLabel.setColour (juce::Label::textColourId,
                               juce::Colours::lightcyan.withAlpha (0.85f));
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
                    midiLearn->setEncoderMode (fieldId,
                        (MidiLearnManager::EncoderMode)(modeCombo.getSelectedId() - 1));
            };
            addAndMakeVisible (modeCombo);
        }

        void update (int field, MidiLearnManager& ml, const juce::String& paramName)
        {
            fieldId   = field;
            midiLearn = &ml;
            paramLabel.setText (paramName, juce::dontSendNotification);
            ccLabel.setText    (ml.getLabelText (field), juce::dontSendNotification);
            modeCombo.setSelectedId ((int) ml.getEncoderMode (field) + 1,
                                     juce::dontSendNotification);
        }

        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            const int col1 = (int)(w * 0.32f);   // param name
            const int col2 = (int)(w * 0.22f);   // CC
            const int col3 = w - col1 - col2;    // combo
            paramLabel.setBounds (8,     0, col1 - 8, h);
            ccLabel   .setBounds (col1,  0, col2,     h);
            modeCombo .setBounds (col1 + col2, 2, col3 - 4, h - 4);
        }
    };

    void encoderModeChanged (int fieldId, MidiLearnManager::EncoderMode mode);
    void close();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnDialog)
};
