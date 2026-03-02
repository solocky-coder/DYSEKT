#include "MixerPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

MixerPanel::MixerPanel (DysektProcessor& p)
    : processor (p), masterStrip (p)
{
    addAndMakeVisible (masterStrip);
    addAndMakeVisible (hScrollBar);
    hScrollBar.addListener (this);
    hScrollBar.setRangeLimits (0.0, 1.0);
    hScrollBar.setCurrentRange (0.0, 1.0);
}

MixerPanel::~MixerPanel()
{
    hScrollBar.removeListener (this);
}

// =============================================================================
void MixerPanel::rebuildStrips (int numSlices)
{
    // Remove old strips that are now out of range
    while ((int)strips.size() > numSlices)
    {
        removeChildComponent (strips.back().get());
        strips.pop_back();
    }

    // Add new strips if needed
    for (int i = (int)strips.size(); i < numSlices; ++i)
    {
        auto s = std::make_unique<MixerStrip> (processor, i);
        addAndMakeVisible (*s);
        strips.push_back (std::move (s));
    }

    lastNumSlices = numSlices;
}

// =============================================================================
void MixerPanel::layoutStrips()
{
    const int visibleW = getWidth() - kMasterWidth - 4;
    const int h        = getHeight() - kScrollBarH;
    stripViewport = { 0, 0, visibleW, h };

    const int numStrips = (int)strips.size();

    // Update scrollbar
    if (numStrips <= kVisibleStrips)
    {
        hScrollBar.setVisible (false);
        scrollOffset = 0;
    }
    else
    {
        hScrollBar.setVisible (true);
        hScrollBar.setRangeLimits (0.0, (double)numStrips);
        hScrollBar.setCurrentRange ((double)scrollOffset, (double)kVisibleStrips);
        hScrollBar.setBounds (0, h, visibleW, kScrollBarH);
    }

    // Position visible strips
    for (int i = 0; i < numStrips; ++i)
    {
        const int visIdx = i - scrollOffset;
        if (visIdx < 0 || visIdx >= kVisibleStrips)
        {
            strips[(size_t)i]->setVisible (false);
        }
        else
        {
            strips[(size_t)i]->setVisible (true);
            strips[(size_t)i]->setBounds (visIdx * kStripWidth, 0, kStripWidth - 2, h);
        }
    }

    // Master strip on the right
    masterStrip.setBounds (getWidth() - kMasterWidth, 0, kMasterWidth, h);
}

// =============================================================================
void MixerPanel::resized()
{
    layoutStrips();
}

// =============================================================================
void MixerPanel::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    g.setColour (theme.darkBar);
    g.fillRect (getLocalBounds());

    // Separator line between strips and master
    g.setColour (theme.separator);
    g.drawVerticalLine (getWidth() - kMasterWidth - 2, 0.0f, (float)getHeight());
}

// =============================================================================
void MixerPanel::updateFromSnapshot()
{
    const auto& ui       = processor.getUiSliceSnapshot();
    const int numSlices  = ui.numSlices;
    const int selectedSl = ui.selectedSlice;

    if (numSlices != lastNumSlices)
    {
        rebuildStrips (numSlices);
        layoutStrips();
    }

    // Update meter levels and selection state for each visible strip
    for (int i = 0; i < (int)strips.size(); ++i)
    {
        strips[(size_t)i]->setSelected (i == selectedSl);
        // Per-slice metering: zeroed for now (no per-slice peak atomics yet)
        strips[(size_t)i]->setMeterLevels (0.0f, 0.0f);
        strips[(size_t)i]->repaint();
    }

    // Auto-scroll to show the selected strip
    if (selectedSl >= 0 && (int)strips.size() > kVisibleStrips)
    {
        if (selectedSl < scrollOffset)
            scrollOffset = selectedSl;
        else if (selectedSl >= scrollOffset + kVisibleStrips)
            scrollOffset = selectedSl - kVisibleStrips + 1;
        layoutStrips();
    }

    // Master strip metering from processor peak atomics
    // These are updated once per processBlock; the atomic load is cheap enough
    // to read every timer tick without caching.
    const float peakL = processor.masterPeakL.load (std::memory_order_relaxed);
    const float peakR = processor.masterPeakR.load (std::memory_order_relaxed);
    masterStrip.setMeterLevels (peakL, peakR);
}

// =============================================================================
void MixerPanel::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    scrollOffset = juce::jlimit (0, juce::jmax (0, (int)strips.size() - kVisibleStrips),
                                 (int)newRangeStart);
    layoutStrips();
    repaint();
}
