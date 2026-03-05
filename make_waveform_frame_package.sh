#!/bin/bash

set -e

mkdir -p src/ui

# --- ControlBar ---
cat > src/ui/ControlBar.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Top control bar for waveform section, with buttons/stubs for "ADD SLICE" etc.
class ControlBar : public juce::Component
{
public:
    ControlBar();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    juce::TextButton addSliceBtn {"ADD SLICE"};
    juce::TextButton midiSliceBtn {"MIDI SLICE"};
    juce::TextButton trimBtn {"TRIM"};
};
EOF

cat > src/ui/ControlBar.cpp <<'EOF'
#include "ControlBar.h"

ControlBar::ControlBar()
{
    addAndMakeVisible(addSliceBtn);
    addAndMakeVisible(midiSliceBtn);
    addAndMakeVisible(trimBtn);
}

void ControlBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey.withAlpha(0.85f));
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawRect(getLocalBounds(), 2);
}

void ControlBar::resized()
{
    auto area = getLocalBounds().reduced(4);
    auto buttonWidth = area.getWidth() / 3;
    addSliceBtn.setBounds(area.removeFromLeft(buttonWidth).reduced(2));
    midiSliceBtn.setBounds(area.removeFromLeft(buttonWidth).reduced(2));
    trimBtn.setBounds(area.reduced(2));
}
EOF

# --- SliceLane ---

cat > src/ui/SliceLane.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class WaveformView;
class SliceLane : public juce::Component
{
public:
    SliceLane();
    void paint(juce::Graphics&) override;
    void setWaveformView(WaveformView*);
private:
    WaveformView* waveformView = nullptr;
};
EOF

cat > src/ui/SliceLane.cpp <<'EOF'
#include "SliceLane.h"
#include "WaveformView.h"

SliceLane::SliceLane() {}
void SliceLane::setWaveformView(WaveformView* wv) { waveformView = wv; }
void SliceLane::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgreen.darker(0.2f));
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawText("Slice Lane [waveform + slice markers]", getLocalBounds(), juce::Justification::centred);
}
EOF

# --- WaveformView ---

cat > src/ui/WaveformView.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class WaveformView : public juce::Component
{
public:
    WaveformView();
    void paint(juce::Graphics&) override;
private:
};
EOF

cat > src/ui/WaveformView.cpp <<'EOF'
#include "WaveformView.h"
WaveformView::WaveformView() {}
void WaveformView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::deepskyblue.darker(0.3f));
    g.setColour(juce::Colours::white);
    g.drawText("Waveform Display Area", getLocalBounds(), juce::Justification::centred);
}
EOF

# --- SliceWaveformLcd ---

cat > src/ui/SliceWaveformLcd.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class SliceWaveformLcd : public juce::Component
{
public:
    SliceWaveformLcd();
    void paint(juce::Graphics&) override;
private:
};
EOF

cat > src/ui/SliceWaveformLcd.cpp <<'EOF'
#include "SliceWaveformLcd.h"
SliceWaveformLcd::SliceWaveformLcd() {}
void SliceWaveformLcd::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::teal.darker(0.2f));
    g.setColour(juce::Colours::white);
    g.drawText("SliceWaveformLcd (mini waveform/sample info)", getLocalBounds(), juce::Justification::centred);
}
EOF

# --- WaveformSectionFrame ---

cat > src/ui/WaveformSectionFrame.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ControlBar.h"
#include "WaveformView.h"
#include "SliceLane.h"

// Themed frame containing [control bar + waveform + slice lane]
class WaveformSectionFrame : public juce::Component
{
public:
    WaveformSectionFrame();
    void resized() override;
    void paint(juce::Graphics&) override;
    WaveformView& getWaveformView() { return waveformView; }
    SliceLane& getSliceLane() { return sliceLane; }
    ControlBar& getControlBar() { return controlBar; }
private:
    ControlBar controlBar;
    WaveformView waveformView;
    SliceLane sliceLane;
};
EOF

cat > src/ui/WaveformSectionFrame.cpp <<'EOF'
#include "WaveformSectionFrame.h"

WaveformSectionFrame::WaveformSectionFrame()
{
    addAndMakeVisible(controlBar);
    addAndMakeVisible(waveformView);
    addAndMakeVisible(sliceLane);

    sliceLane.setWaveformView(&waveformView);
}

void WaveformSectionFrame::resized()
{
    auto area = getLocalBounds().reduced(8);
    int cbHeight = 38;
    int wfHeight = (area.getHeight() - cbHeight) * 2 / 3;
    int slHeight = area.getHeight() - cbHeight - wfHeight;

    controlBar.setBounds(area.removeFromTop(cbHeight));
    waveformView.setBounds(area.removeFromTop(wfHeight));
    sliceLane.setBounds(area);
}

void WaveformSectionFrame::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0f);
    g.setColour(juce::Colours::cyan);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2), 10.0f, 2.5f);
}
EOF

# --- PluginEditor ---

cat > src/ui/PluginEditor.h <<'EOF'
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "WaveformSectionFrame.h"
#include "SliceWaveformLcd.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor();
    void resized() override;
private:
    WaveformSectionFrame waveformSectionFrame;
    SliceWaveformLcd sliceWaveformLcd;
};
EOF

cat > src/ui/PluginEditor.cpp <<'EOF'
#include "PluginEditor.h"

PluginEditor::PluginEditor()
    : juce::AudioProcessorEditor(0), // Pass dummy processor pointer for demo; replace with your DysektProcessor&
      waveformSectionFrame(),
      sliceWaveformLcd()
{
    addAndMakeVisible(waveformSectionFrame);
    addAndMakeVisible(sliceWaveformLcd);
    setSize(900, 600);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(14);

    waveformSectionFrame.setBounds(area.removeFromTop(340).reduced(0, 8));
    sliceWaveformLcd.setBounds(area.removeFromTop(130));
    // Add more UI elements as needed below
}
EOF

# ------- PACKAGE UP --------
cd src
zip -r ../waveform_section_dragdrop.zip ui/*
cd ..

echo
echo "DONE! Your archive is: waveform_section_dragdrop.zip"
echo "Unzip and copy to your src/ui folder in your repo for a working, drag+drop baseline!"