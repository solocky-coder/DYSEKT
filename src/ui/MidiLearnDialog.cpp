#include "MidiLearnDialog.h"

MidiLearnDialog::MidiLearnDialog(MidiLearnManager& ml, std::function<void()> onClose)
    : midiLearn(ml), onCloseCallback(onClose)
{
    addAndMakeVisible(mappingList);
    addAndMakeVisible(closeButton);

    closeButton.onClick = [this] { close(); };
    mappingList.setModel(this);

    setSize(420, 340);
}

void MidiLearnDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey);
    g.setColour(juce::Colours::white);
    g.setFont(17.0f);
    g.drawText("MIDI Learn Assignments", 0, 8, getWidth(), 24, juce::Justification::centred);
}

void MidiLearnDialog::resized()
{
    mappingList.setBounds(10, 38, getWidth() - 20, getHeight() - 68);
    closeButton.setBounds((getWidth() - 90) / 2, getHeight() - 26, 90, 22);
}

int MidiLearnDialog::getNumRows()
{
    return kMidiLearnNumSlots;
}

void MidiLearnDialog::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
        g.fillAll(juce::Colours::deepskyblue.withAlpha(0.15f));
    g.setColour(juce::Colours::white);

    // For better UI: Map fieldId to parameter (see next step)
    juce::String pname = "Param " + juce::String(row);
    g.drawText(pname, 8, 0, width / 3, height, juce::Justification::centredLeft);

    juce::String ccText = midiLearn.getLabelText(row);
    g.drawText(ccText, width / 3 + 8, 0, width / 3, height, juce::Justification::centredLeft);
}

juce::Component* MidiLearnDialog::refreshComponentForRow(int row, bool, juce::Component* existing)
{
    auto* selector = dynamic_cast<EncoderModeSelector*>(existing);
    if (!selector)
        selector = new EncoderModeSelector();

    selector->setField(row, midiLearn.getEncoderMode(row));
    selector->onChange = [this, selector] { encoderModeChanged(selector); };
    return selector;
}

void MidiLearnDialog::encoderModeChanged(EncoderModeSelector* s)
{
    if (s && s->fieldId >= 0)
        midiLearn.setEncoderMode(s->fieldId, (MidiLearnManager::EncoderMode)(s->getSelectedId() - 1));
}

void MidiLearnDialog::close()
{
    if (onCloseCallback)
        onCloseCallback();
    else if (auto* parent = getParentComponent())
        parent->removeChildComponent(this);
}

// =============== EncoderModeSelector ================
MidiLearnDialog::EncoderModeSelector::EncoderModeSelector()
{
    addItem("Absolute", MidiLearnManager::kAbsolute + 1);
    addItem("Relative 2'sComp", MidiLearnManager::kRelTwosComp + 1);
    addItem("Relative SignBit", MidiLearnManager::kRelSignBit + 1);
    addItem("Relative BinOffset", MidiLearnManager::kRelBinOffset + 1);
    setJustificationType(juce::Justification::centred);
}

void MidiLearnDialog::EncoderModeSelector::setField(int field, MidiLearnManager::EncoderMode mode)
{
    fieldId = field;
    setSelectedId((int)mode + 1, juce::dontSendNotification);
}