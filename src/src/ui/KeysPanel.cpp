#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

const int KeysPanel::whiteOffsets[14] = { 0,2,4,5,7,9,11, 12,14,16,17,19,21,23 };
const KeysPanel::BlackDef KeysPanel::blackDefs[10] = {
    {0,1},{1,3},{3,6},{4,8},{5,10},
    {7,13},{8,15},{10,18},{11,20},{12,22}
};

KeysPanel::KeysPanel (DysektProcessor& p) : processor (p)
{
    addAndMakeVisible (transposeDownBtn);
    addAndMakeVisible (transposeUpBtn);

    transposeDownBtn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
    transposeDownBtn.setColour (juce::TextButton::textColourOffId, getTheme().accent);
    transposeUpBtn.setColour   (juce::TextButton::buttonColourId,  getTheme().button);
    transposeUpBtn.setColour   (juce::TextButton::textColourOffId, getTheme().accent);

    transposeDownBtn.onClick = [this] { if (baseOctave > 0) { --baseOctave; resized(); repaint(); } };
    transposeUpBtn.onClick   = [this] { if (baseOctave < 7) { ++baseOctave; resized(); repaint(); } };

    startTimerHz (30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

void KeysPanel::resized()
{
    int cx = getWidth() / 2;
    transposeDownBtn.setBounds (cx - 90, 3, 26, 20);
    transposeUpBtn.setBounds   (cx + 64, 3, 26, 20);

    keyRects.clear();
    int kbW      = kWhiteKeyW * kNumWhite;
    int keyboardX = (getWidth() - kbW) / 2;
    int keyboardY = kTransposeRowH;

    for (int i = 0; i < kNumWhite; ++i)
    {
        KeyRect kr;
        kr.bounds  = { keyboardX + i * kWhiteKeyW, keyboardY, kWhiteKeyW - 1, kKeyH };
        kr.note    = baseOctave * 12 + whiteOffsets[i];
        kr.isBlack = false;
        keyRects.push_back (kr);
    }
    for (int i = 0; i < kNumBlack; ++i)
    {
        KeyRect kr;
        int bx     = keyboardX + (blackDefs[i].afterWhite + 1) * kWhiteKeyW - kBlackKeyW / 2;
        kr.bounds  = { bx, keyboardY, kBlackKeyW, kBlackKeyH };
        kr.note    = baseOctave * 12 + blackDefs[i].offset;
        kr.isBlack = true;
        keyRects.push_back (kr);
    }
}

void KeysPanel::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().darkBar);

    // Re-theme buttons
    transposeDownBtn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
    transposeDownBtn.setColour (juce::TextButton::textColourOffId, getTheme().accent);
    transposeUpBtn.setColour   (juce::TextButton::buttonColourId,  getTheme().button);
    transposeUpBtn.setColour   (juce::TextButton::textColourOffId, getTheme().accent);

    // Transpose label
    juce::String label = "TRANSPOSE     C" + juce::String (baseOctave)
                       + " - C" + juce::String (baseOctave + 2);
    g.setFont (DysektLookAndFeel::makeFont (11.0f));
    g.setColour (getTheme().accent);
    g.drawText (label, 0, 3, getWidth(), 20, juce::Justification::centred);

    // Get slice MIDI notes for highlight
    const auto& ui = processor.getUiSliceSnapshot();
    std::set<int> slicedNotes;
    for (int s = 0; s < ui.numSlices; ++s)
        slicedNotes.insert (ui.slices[(size_t)s].midiNote);

    // White keys
    for (const auto& kr : keyRects)
    {
        if (kr.isBlack) continue;
        bool hasSlice = slicedNotes.count (kr.note) > 0;

        g.setColour (juce::Colour (0xFFDDDDDD));
        g.fillRoundedRectangle (kr.bounds.toFloat(), 5.0f);

        if (hasSlice)
        {
            g.setColour (getTheme().accent);
            g.drawRoundedRectangle (kr.bounds.toFloat().reduced (1.5f), 5.0f, 2.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xFF888888));
            g.drawRoundedRectangle (kr.bounds.toFloat(), 5.0f, 1.0f);
        }

        g.setFont (DysektLookAndFeel::makeFont (7.5f));
        g.setColour (hasSlice ? getTheme().accent : juce::Colour (0xFF999999));
        g.drawText (juce::MidiMessage::getMidiNoteName (kr.note, true, true, 3),
                    kr.bounds.getX(), kr.bounds.getBottom() - 14,
                    kr.bounds.getWidth(), 12, juce::Justification::centred);
    }

    // Black keys
    for (const auto& kr : keyRects)
    {
        if (! kr.isBlack) continue;
        bool hasSlice = slicedNotes.count (kr.note) > 0;

        g.setColour (juce::Colour (0xFF1C1C1C));
        g.fillRoundedRectangle (kr.bounds.toFloat(), 4.0f);

        if (hasSlice)
        {
            g.setColour (getTheme().accent);
            g.drawRoundedRectangle (kr.bounds.toFloat().reduced (1.5f), 4.0f, 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xFF000000));
            g.drawRoundedRectangle (kr.bounds.toFloat(), 4.0f, 1.0f);
        }
    }
}

void KeysPanel::mouseDown (const juce::MouseEvent& e)
{
    // Black keys on top first
    for (auto it = keyRects.rbegin(); it != keyRects.rend(); ++it)
    {
        if (it->isBlack && it->bounds.contains (e.getPosition()))
            return; // click registered, no action needed without MIDI out hook
    }
}

void KeysPanel::timerCallback() { repaint(); }
