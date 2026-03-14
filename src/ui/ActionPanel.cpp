#include "ActionPanel.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include <algorithm>

ActionPanel::ActionPanel(DysektProcessor& p, WaveformView& wv)
    : processor(p), waveformView(wv)
{
    addSliceBtn.setClickingTogglesState(true);
    addSliceBtn.setTooltip("Add Slice (A / hold Alt)");
    midiSliceBtn.setClickingTogglesState(true);
    midiSliceBtn.setTooltip("MIDI Slice — chop by incoming MIDI notes (L)");
    shortcutsBtn.setTooltip("Keyboard Shortcuts (⌘?)");

    addAndMakeVisible(addSliceBtn);
    addAndMakeVisible(midiSliceBtn);
    addAndMakeVisible(shortcutsBtn);

    // Both off at startup
    addSliceBtn.setToggleState(false, juce::dontSendNotification);
    midiSliceBtn.setToggleState(false, juce::dontSendNotification);
    updateToggleBtn(addSliceBtn, false);
    updateToggleBtn(midiSliceBtn, false);

    // Add Slice button: Exclusive logic
    addSliceBtn.onClick = [this]() {
        bool on = addSliceBtn.getToggleState();
        if (on) {
            midiSliceBtn.setToggleState(false, juce::sendNotification);
        }
        updateToggleBtn(addSliceBtn, on);
        updateToggleBtn(midiSliceBtn, midiSliceBtn.getToggleState());
    };

    // MIDI Slice button: Exclusive logic
    midiSliceBtn.onClick = [this]() {
        bool on = midiSliceBtn.getToggleState();
        if (on) {
            addSliceBtn.setToggleState(false, juce::sendNotification);
        }
        updateToggleBtn(midiSliceBtn, on);
        updateToggleBtn(addSliceBtn, addSliceBtn.getToggleState());
        // Insert your MIDI slice enable logic here if needed
    };

    waveformView.setAddSliceActiveGetter([this]() { return this->addSliceBtn.getToggleState(); });

    shortcutsBtn.onClick = [this] { if (onShortcutsToggle) onShortcutsToggle(); };
}

ActionPanel::~ActionPanel() = default;

void ActionPanel::updateToggleBtn(juce::ToggleButton& btn, bool active)
{
    if (active)
    {
        btn.setColour(juce::ToggleButton::tickColourId, getTheme().accent);
        btn.setColour(juce::ToggleButton::textColourId, getTheme().accent);
        btn.setAlpha(1.0f);
    }
    else
    {
        btn.setColour(juce::ToggleButton::tickColourId, getTheme().button.withAlpha(0.5f));
        btn.setColour(juce::ToggleButton::textColourId, getTheme().foreground.withAlpha(0.5f));
        btn.setAlpha(0.6f);
    }
}

void ActionPanel::resized()
{
    const int gap    = 4;
    const int h      = getHeight();
    const int btnW   = 70;

    int x = 0;
    addSliceBtn.setBounds(x, 0, btnW, h);
    x += btnW + gap;
    midiSliceBtn.setBounds(x, 0, btnW, h);
    x += btnW + gap;
    shortcutsBtn.setBounds(x, 0, 40, h);

    shortcutsBtn.setVisible(true);
}

void ActionPanel::paint(juce::Graphics& g)
{
    for (auto* btn : { &addSliceBtn, &midiSliceBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour(juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour(juce::TextButton::textColourOffId, getTheme().foreground);
    }
}

void ActionPanel::paintOverChildren(juce::Graphics&) {}