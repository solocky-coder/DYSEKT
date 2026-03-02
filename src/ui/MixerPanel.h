#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixerStrip.h"
#include "MasterMixerStrip.h"

class DysektProcessor;

/**
 * MixerPanel — main mixer container.
 *
 * Layout:
 *  - Up to kVisibleStrips slice channel strips (horizontally scrollable)
 *  - MasterMixerStrip docked on the right
 *  - Horizontal scroll bar when numSlices > kVisibleStrips
 *  - Auto-highlights the currently selected slice
 *
 * The panel is updated from the PluginEditor timer callback.
 * Call updateFromSnapshot() every timer tick to refresh levels & state.
 */
class MixerPanel : public juce::Component,
                   private juce::ScrollBar::Listener
{
public:
    static constexpr int kVisibleStrips = 8;
    static constexpr int kStripWidth    = 58;
    static constexpr int kMasterWidth   = 62;
    static constexpr int kScrollBarH    = 12;

    explicit MixerPanel (DysektProcessor& p);
    ~MixerPanel() override;

    /** Call from editor timer to refresh levels / selection. */
    void updateFromSnapshot();

    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    // ScrollBar::Listener
    void scrollBarMoved (juce::ScrollBar* scrollBar, double newRangeStart) override;

    void rebuildStrips (int numSlices);
    void layoutStrips();

    DysektProcessor& processor;

    // Channel strips
    std::vector<std::unique_ptr<MixerStrip>> strips;
    MasterMixerStrip masterStrip;

    juce::ScrollBar hScrollBar { false };   // false = horizontal

    int scrollOffset { 0 };    // first visible strip index
    int lastNumSlices { -1 };

    // Viewport area for channel strips (excludes master)
    juce::Rectangle<int> stripViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
