#include "MidiLearnDialog.h"

// ── Parameter name table (must match SliceParamField order) ──────────────────
static const char* const gSlotParamNames[kMidiLearnNumSlots] = {
    "BPM",                  // 0
    "Pitch",
    "Algorithm",
    "Attack",
    "Decay",
    "Sustain",
    "Release",
    "Mute Group",
    "Stretch Enabled",
    "Tonality",
    "Formant",
    "Formant Compensation",
    "Grain Mode",
    "Volume",
    "Release Tail",
    "Reverse",
    "Output Bus",
    "Loop",
    "One Shot",
    "Cents Detune",
    "MIDI Note",
    "Marker",               // 21 - slice start / trim-in (same CC, context-aware)
    "",            // 22
    "Pan",
    "Filter Cutoff",
    "Filter Resonance",
    "Chromatic Channel",
    "Chromatic Legato",
    "Trim Out",             // 28 - trim-out marker (trim mode only)
    "Hold",                 // 29 - per-slice AHDSR hold time
};

static juce::String getSlotParameterName (int fieldId)
{
    if (fieldId >= 0 && fieldId < kMidiLearnNumSlots
        && juce::String (gSlotParamNames[fieldId]).isNotEmpty())
        return gSlotParamNames[fieldId];
    return juce::String ("Param ") + juce::String (fieldId);
}

// ── Constructor ───────────────────────────────────────────────────────────────

MidiLearnDialog::MidiLearnDialog (MidiLearnManager& ml, std::function<void()> onClose)
    : midiLearn (ml), onCloseCallback (onClose)
{
    addAndMakeVisible (mappingList);
    addAndMakeVisible (closeButton);

    closeButton.onClick = [this] { close(); };

    // Column header row height + item row height
    mappingList.setRowHeight (24);
    mappingList.setModel (this);
    mappingList.setColour (juce::ListBox::backgroundColourId,
                           juce::Colour (0xFF111418));
    mappingList.setColour (juce::ListBox::outlineColourId,
                           juce::Colours::transparentBlack);

    setSize (520, 420);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void MidiLearnDialog::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (juce::Colour (0xFF1A2024));

    // Title bar
    g.setColour (juce::Colour (0xFF263036));
    g.fillRect (0, 0, getWidth(), 36);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (15.f, juce::Font::bold));
    g.drawText ("MIDI Learn Assignments", 0, 0, getWidth(), 36,
                juce::Justification::centred);

    // Column headers
    const int hdrY  = 38;
    const int hdrH  = 20;
    const int w     = getWidth() - 20;   // list inset
    const int col1  = (int)(w * 0.32f);
    const int col2  = (int)(w * 0.22f);
    g.setColour (juce::Colour (0xFF263036));
    g.fillRect (10, hdrY, w, hdrH);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (10.f, juce::Font::bold));
    g.drawText ("PARAMETER",   18,         hdrY, (int)(w * 0.32f),       hdrH, juce::Justification::centredLeft);
    g.drawText ("CC",          10 + (int)(w * 0.32f), hdrY, (int)(w * 0.18f), hdrH, juce::Justification::centredLeft);
    g.drawText ("ENCODER MODE", 10 + (int)(w * 0.32f) + (int)(w * 0.18f) + 88, hdrY,
                w - (int)(w * 0.32f) - (int)(w * 0.18f) - 92, hdrH, juce::Justification::centredLeft);
}

void MidiLearnDialog::resized()
{
    mappingList.setBounds (10, 60, getWidth() - 20, getHeight() - 96);
    closeButton.setBounds ((getWidth() - 90) / 2, getHeight() - 30, 90, 24);
}

// ── ListBoxModel ──────────────────────────────────────────────────────────────

int MidiLearnDialog::getNumRows()
{
    // Only show named slots — skip empty entries at the end
    int count = 0;
    for (int i = 0; i < kMidiLearnNumSlots; ++i)
        if (juce::String (gSlotParamNames[i]).isNotEmpty()) ++count;
    return count;
}

// Map visible row index to field ID (skipping unnamed slots)
static int rowToFieldId (int row)
{
    int count = 0;
    for (int i = 0; i < kMidiLearnNumSlots; ++i)
    {
        if (juce::String (gSlotParamNames[i]).isNotEmpty())
        {
            if (count == row) return i;
            ++count;
        }
    }
    return -1;
}

void MidiLearnDialog::paintListBoxItem (int /*row*/, juce::Graphics& g,
                                        int width, int height, bool selected)
{
    // Row background only — text/controls drawn by refreshComponentForRow
    if (selected)
    {
        g.setColour (juce::Colours::deepskyblue.withAlpha (0.18f));
        g.fillAll();
    }
    // Bottom divider
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (height - 1, 0.f, (float) width);
}

juce::Component* MidiLearnDialog::refreshComponentForRow (int row, bool /*selected*/,
                                                           juce::Component* existing)
{
    auto* comp = dynamic_cast<MappingRowComponent*> (existing);
    if (! comp)
        comp = new MappingRowComponent();

    const int fieldId = rowToFieldId (row);
    if (fieldId >= 0)
        comp->update (fieldId, midiLearn, getSlotParameterName (fieldId));
    return comp;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MidiLearnDialog::close()
{
    if (onCloseCallback)
        onCloseCallback();
    else if (auto* parent = getParentComponent())
        parent->removeChildComponent (this);
}
