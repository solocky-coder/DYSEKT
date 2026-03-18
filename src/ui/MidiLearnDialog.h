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

    juce::ListBox mappingList {"Midi Mapping List", this};
    juce::TextButton closeButton { "Close" };

    // Per-row encoder mode selectors
   struct EncoderModeSelector : public juce::ComboBox, public juce::ReferenceCountedObject
{
    EncoderModeSelector();
    void setField(int fieldId, MidiLearnManager::EncoderMode mode);
    int fieldId { -1 };
};
    juce::ReferenceCountedArray<EncoderModeSelector> rowSelectors;

    void encoderModeChanged(EncoderModeSelector*);
    void close();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnDialog)
};