#include "MidiLearnDialog.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>

namespace
{
    static juce::Font getParamFont(float size, juce::Font::FontStyleFlags style = juce::Font::plain)
    {
        // Modern JUCE: use the explicit constructor instead of deprecated Font(face, size, style)
        return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, style);
    }
}

//==============================================================================
MidiLearnDialog::MidiLearnDialog(DysektProcessor& p)
    : processor(p)
{
    setOpaque(true);

    infoFont   = getParamFont(14.0f, juce::Font::bold);
    slotFont   = getParamFont(15.0f, juce::Font::plain);
    ccFont     = getParamFont(14.0f, juce::Font::italic);

    setSize (600, 420);
    updateSlotDisplay();
}

void MidiLearnDialog::paint (juce::Graphics& g)
{
    g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));

    // Title
    g.setColour (juce::Colours::orange);
    g.setFont (infoFont);
    g.drawFittedText ("MIDI Learn", getLocalBounds().removeFromTop(36), juce::Justification::centred, 1);

    // Instructions
    g.setColour (juce::Colours::white.withAlpha(0.90f));
    g.setFont (ccFont);
    g.drawFittedText (
        "To assign a MIDI controller, click one of the slots below, "
        "then move a knob or slider on your MIDI device.",
        getLocalBounds().reduced (16, 48).removeFromTop(48),
        juce::Justification::centred, 2);

    // Slots
    auto slotsArea = getLocalBounds().reduced (24, 120);
    const int slotH = 40;
    for (size_t i = 0; i < slotRects.size(); ++i)
    {
        auto rect = slotsArea.removeFromTop (slotH).reduced (0, 3);
        slotRects[i] = rect;

        bool isSelected = (int) i == selectedSlot;
        g.setColour (isSelected ? juce::Colours::goldenrod : juce::Colours::darkgrey);
        g.fillRoundedRectangle (rect.toFloat(), 6.5f);

        // Param name
        g.setColour (juce::Colours::white);
        g.setFont (slotFont);
        g.drawText (slotDisplayNames[i], rect.reduced (10, 0).removeFromLeft (250), juce::Justification::centredLeft);

        // Midi CC number
        juce::String ccStr;
        if (slotDisplayCCs[i] > 0)
            ccStr = "CC " + juce::String (slotDisplayCCs[i]);
        else
            ccStr = "(unassigned)";

        g.setColour (juce::Colours::lightgreen);
        g.setFont (ccFont);
        g.drawText (ccStr, rect.reduced (10, 0).removeFromRight (100), juce::Justification::centredRight);
    }
}

void MidiLearnDialog::mouseDown (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < slotRects.size(); ++i)
    {
        if (slotRects[i].contains (e.getPosition()))
        {
            selectedSlot = (int) i;
            repaint();
            return;
        }
    }
}

void MidiLearnDialog::updateSlotDisplay()
{
    for (size_t i = 0; i < slotDisplayNames.size(); ++i)
    {
        slotDisplayNames[i] = MidiLearnManager::slotName((int)i);
        slotDisplayCCs[i]   = processor.midiLearn.getAssignedCC((int)i);
    }
    repaint();
}