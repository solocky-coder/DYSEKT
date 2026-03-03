// PluginEditor.cpp

#include "PluginEditor.h"

// Layout constants
constexpr int kMargin = 10;
constexpr int logoBarHeight = 50;
constexpr int sliceLaneHeight = 40;
constexpr int kLcdH = 240;

DysektEditor::DysektEditor(DysektProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    // Add components to visible hierarchy
    addAndMakeVisible(logoBar);
    addAndMakeVisible(sliceLcdDisplay);
    addAndMakeVisible(sliceLane);

    setSize(800, 600); // or whatever default size you prefer
}

DysektEditor::~DysektEditor() {}

void DysektEditor::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(juce::Colours::darkgrey);

    // Custom paint code (optional)
}

void DysektEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // Logo bar
    logoBar.setBounds(area.removeFromTop(logoBarHeight));

    // LCD display
    sliceLcdDisplay.setBounds(area.removeFromTop(kLcdH).reduced(kMargin, kMargin));

    // Slice lane
    sliceLane.setBounds(area.removeFromTop(sliceLaneHeight).reduced(kMargin, kMargin));

    // Add layout for other UI elements below, if needed
}

// If you have other methods (keyboard, timers, etc) you can add them here.
