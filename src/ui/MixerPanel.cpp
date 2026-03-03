// src/ui/MixerPanel.cpp

#include "MixerPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

MixerPanel::MixerPanel(DysektProcessor& p)
    : processor(p), masterStrip(p)
{
    addAndMakeVisible(masterStrip);
    addAndMakeVisible(hScrollBar);
    hScrollBar.addListener(this);
    hScrollBar.setRangeLimits(0.0, 1.0);
    hScrollBar.setCurrentRange(0.0, 1.0);
}

MixerPanel::~MixerPanel()
{
    hScrollBar.removeListener(this);
}

// Manages strip child components according to numSlices
void MixerPanel::rebuildStrips(int numSlices)
{
    while ((int)strips.size() > numSlices)
    {
        removeChildComponent(strips.back().get());
        strips.pop_back();
    }
    for (int i = (int)strips.size(); i < numSlices; ++i)
    {
        auto s = std::make_unique<MixerStrip>(processor, i);
        addAndMakeVisible(*s);
        strips.push_back(std::move(s));
    }
    lastNumSlices = numSlices;
}

void MixerPanel::layoutStrips()
{
    const int visibleW = getWidth() - kMasterWidth - 4;
    const int h = getHeight() - kScrollBarH;
    stripViewport = { 0, 0, visibleW, h };
    const int numStrips = (int)strips.size();

    // Update scrollbar
    if (numStrips <= kVisibleStrips)
    {
        hScrollBar.setVisible(false);
        scrollOffset = 0;
    }
    else
    {
        hScrollBar.setVisible(true);
        hScrollBar.setRangeLimits(0.0, (double)numStrips);
        hScrollBar.setCurrentRange((double)scrollOffset, (double)kVisibleStrips);
        hScrollBar.setBounds(0, h, visibleW, kScrollBarH);
    }
    // Position strips
    for (int i = 0; i < numStrips; ++i)
    {
        const int visIdx = i - scrollOffset;
        if (visIdx < 0 || visIdx >= kVisibleStrips)
        {
            strips[(size_t)i]->setVisible(false);
        }
        else
        {
            strips[(size_t)i]->setVisible(true);
            strips[(size_t)i]->setBounds(visIdx * kStripWidth, 0, kStripWidth - 2, h);
        }
    }
    masterStrip.setBounds(getWidth() - kMasterWidth, 0, kMasterWidth, h);
}

void MixerPanel::resized()
{
    layoutStrips();
}

void MixerPanel::paint(juce::Graphics& g)
{
    const auto& theme = getTheme();
    g.setColour(theme.background);
    g.fillRect(getLocalBounds());
    g.setColour(theme.gridLine);
    g.drawVerticalLine(getWidth() - kMasterWidth - 2, 0.0f, (float)getHeight());
}

void MixerPanel::updateFromSnapshot()
{
    const auto& ui = processor.getUiSliceSnapshot();
    const int numSlices = ui.numSlices;
    const int selectedSl = ui.selectedSlice;
    if (numSlices != lastNumSlices)
        rebuildStrips(numSlices);
    // Update selection/highlights per your logic
}