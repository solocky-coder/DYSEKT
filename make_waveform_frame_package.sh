#!/bin/bash

set -e

mkdir -p src/ui

cat > src/ui/WaveformSectionFrame.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "WaveformView.h"
#include "SliceLane.h"

class DysektProcessor;

// Frame that contains the control bar, waveform, and slice lane for the “Waveform Section”.
class WaveformSectionFrame : public juce::Component
{
public:
    explicit WaveformSectionFrame(DysektProcessor& processor);

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Set child pointers in case these are needed for external setup
    SliceLane& getSliceLane()        { return sliceLane; }
    WaveformView& getWaveformView()  { return waveformView; }

private:
    DysektProcessor& processor;
    juce::Component controlBar;
    SliceLane sliceLane;
    WaveformView waveformView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformSectionFrame)
};
EOF

cat > src/ui/WaveformSectionFrame.cpp <<'EOF'
#include "WaveformSectionFrame.h"
#include "DysektLookAndFeel.h"

WaveformSectionFrame::WaveformSectionFrame(DysektProcessor& p)
    : processor(p), sliceLane(p), waveformView(p)
{
    // -- Dummy Control Bar (Replace with your actual implementation) --
    controlBar.setName("Waveform Control Bar");
    addAndMakeVisible(controlBar);

    addAndMakeVisible(waveformView);
    addAndMakeVisible(sliceLane);

    // You might want to wire things up:
    sliceLane.setWaveformView(&waveformView);
}

void WaveformSectionFrame::resized()
{
    auto r = getLocalBounds();

    // Height allocations, adjust as needed!
    int barHeight = 32;
    int waveformHeight = r.getHeight() * 0.65;
    int sliceLaneHeight = r.getHeight() - barHeight - waveformHeight;

    controlBar.setBounds(r.removeFromTop(barHeight));
    waveformView.setBounds(r.removeFromTop(waveformHeight));
    sliceLane.setBounds(r);
}

void WaveformSectionFrame::paint(juce::Graphics& g)
{
    // Frame background
    g.setColour(findColour(juce::ResizableWindow::backgroundColourId).darker(0.2f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    // Themed border (change as needed)
    g.setColour(juce::Colour(0xFF28b2c7));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 2.5f);
}
EOF

# Dummy control bar for demonstration (replace with your version)
cat > src/ui/ControlBar.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// A placeholder for the “ADD SLICE”, “MIDI SLICE”, “TRIM” etc. control bar.
class ControlBar : public juce::Component
{
public:
    ControlBar() {}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::darkgrey);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText("ADD SLICE   MIDI SLICE   TRIM", getLocalBounds(), juce::Justification::centred);
    }
};
EOF

# --- Patch PluginEditor.cpp to use the new frame!
cat > src/ui/PluginEditor.cpp <<'EOF'
// This is a representative snippet for how to use the new frame in your editor.

#include "PluginEditor.h"
#include "WaveformSectionFrame.h"
#include "SliceWaveformLcd.h"
#include "ControlBar.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor(DysektProcessor& p)
    : juce::AudioProcessorEditor(p),
      waveformSectionFrame(p),
      sliceWaveformLcd(p)
    {
        addAndMakeVisible(waveformSectionFrame);
        addAndMakeVisible(sliceWaveformLcd);
        setSize(1080, 600);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        // Place main waveform section in a dedicated frame
        waveformSectionFrame.setBounds(area.removeFromTop(340).reduced(16, 8));
        sliceWaveformLcd.setBounds(area.removeFromTop(140).reduced(16, 8));
        // ... layout the rest of your UI as needed
    }

private:
    WaveformSectionFrame waveformSectionFrame;
    SliceWaveformLcd sliceWaveformLcd;
};
EOF

# Assuming all your other files are present, exclude duplicating them here.

# Package!
cd src
zip -r ../waveform_section_dragdrop.zip ui/*
cd ..

echo
echo "Ready! Your package is:"
echo "   waveform_section_dragdrop.zip"
echo "Unzip this into your src/ui/ directory (drag into your repo),"
echo "and adapt the .cpp/.h includes and integration as shown in PluginEditor.cpp!"