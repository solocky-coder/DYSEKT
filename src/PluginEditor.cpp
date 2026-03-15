#include "PluginEditor.h"
#include <algorithm>

// ────────────── Add LAYOUT CONSTANTS at top ─────────────────────
namespace
{
    static constexpr int kBaseW      = 1130;
    static constexpr int kLogoH      = 52;
    static constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12;
    static constexpr int kSliceLaneH = 36;
    static constexpr int kScrollbarH = 28;
    static constexpr int kSliceCtrlH = 72;
    static constexpr int kActionH    = 22;
    static constexpr int kTrimBarH   = 34;
    static constexpr int kCtrlFrameW    = 180;
    static constexpr int kBrowserH    = 170;
    static constexpr int kMargin      = 8;
    static constexpr int kPanelSlotH  = 200;
    static constexpr int kBaseHCore   = kLogoH + kLcdRowH + kSliceLaneH
                                      + kScrollbarH + kSliceCtrlH + kActionH
                                      + 120;
    static constexpr int kTotalH      = kBaseHCore + kPanelSlotH + 16;
}

// ────────────── DysektEditor Implementation ──────────────────────

DysektEditor::DysektEditor (DysektProcessor& p)
    : juce::AudioProcessorEditor (p)
    , processor (p)
    , shortcutsPanel (processor)
{
    // Add and make visible all top-level GUI components
    addAndMakeVisible (logoBar);
    addAndMakeVisible (headerBar);
    addAndMakeVisible (sliceLcd);
    addAndMakeVisible (sliceWaveformLcd);
    addAndMakeVisible (sliceLane);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (sliceControlBar);
    addAndMakeVisible (actionPanel);
    addAndMakeVisible (waveformOverview);
    addAndMakeVisible (browserPanel);
    addAndMakeVisible (mixerPanel);
    addAndMakeVisible (shortcutsPanel);

    // Tooltip window is already created in member initializer
    // LookAndFeel setup if needed:
    setLookAndFeel (&lnf);

    // Size the editor
    setSize (kBaseW, kTotalH);

    // Start timer if you use it
    startTimerHz (timerHz);
}

DysektEditor::~DysektEditor()
{
    setLookAndFeel (nullptr);
}

void DysektEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);// Replace or theme as needed
}

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());

    // 1. Combined header bar (logo left + UNDO/REDO/PANIC/UI right)
    {
        auto headerStrip = area.removeFromTop (kLogoH);
        logoBar.setBounds (headerStrip.removeFromLeft (220));
        headerBar.setBounds (headerStrip);
    }

    // 2. Dual LCD row — LCD1 | CtrlFrame | LCD2
    {
        auto lcdRow = area.removeFromTop (kLcdRowH).reduced (kMargin, 6);
        const int lcdW = (lcdRow.getWidth() - kCtrlFrameW - kMargin * 2) / 2;
        sliceLcd.setBounds (lcdRow.removeFromLeft (lcdW));
        lcdRow.removeFromLeft (kMargin);

        if (auto* cf = headerBar.getControlFrame())
            cf->setBounds (lcdRow.removeFromLeft (kCtrlFrameW));
        lcdRow.removeFromLeft (kMargin);
        sliceWaveformLcd.setBounds (lcdRow);
    }

    auto actionArea = area.removeFromTop (kActionH);
    const int kFX = kMargin;
    const int kFW = getWidth() - kMargin * 2;

    // Panel slot
    {
        area.removeFromBottom (kMargin);
        auto slot = area.removeFromBottom (kPanelSlotH);
        area.removeFromBottom (kMargin);

        if (mixerOpen)
        {
            const int mh = juce::jmin (MixerPanel::kPanelH, kPanelSlotH);
            auto mb = juce::Rectangle<int> (kFX, slot.getY(), kFW, mh);
            mixerPanel.setBounds (mb);
            browserPanel.setBounds ({});
        }
        else if (browserOpen)
        {
            const int bh = juce::jmin (kBrowserH, kPanelSlotH);
            auto bb = juce::Rectangle<int> (kFX, slot.getY(), kFW, bh);
            browserPanel.setBounds (bb);
            mixerPanel.setBounds ({});
        }
        else
        {
            mixerPanel.setBounds ({});
            browserPanel.setBounds ({});
        }
    }

    // Slice control bar
    {
        auto scbArea = area.removeFromBottom (kSliceCtrlH);
        sliceControlBar.setBounds (juce::Rectangle<int> (kFX, scbArea.getY(), kFW, kSliceCtrlH));
    }

    area.removeFromBottom (kMargin);

    // Outer LCD frame rect
    const int kFrameInset = 4;
    const int kFrameX     = kFX;
    const int kFrameW     = kFW;
    const int frameTop    = actionArea.getY();
    const int frameBot    = area.getBottom();
    const int screenX     = kFrameX  + kFrameInset;
    const int screenW     = kFrameW  - kFrameInset * 2;
    const int screenTop   = frameTop + kFrameInset;
    const int screenBot   = frameBot - kFrameInset;
    const int screenH     = screenBot - screenTop;

    // Scrollbar (now just the waveformOverview)
    {
        auto r = juce::Rectangle<int> (screenX, screenBot - kScrollbarH, screenW, kScrollbarH);
        waveformOverview.setBounds (r);
    }

    // Action panel
    {
        auto r = juce::Rectangle<int> (screenX, screenTop, screenW, kActionH);
        actionPanel.setBounds (r);
    }

    // Slice lane
    {
        int y = screenTop + kActionH;
        auto r = juce::Rectangle<int> (screenX, y, screenW, kSliceLaneH);
        sliceLane.setBounds (r);
    }

    // Waveform
    {
        int y      = screenTop + kActionH + kSliceLaneH;
        int trimH  = (trimDialog != nullptr) ? kTrimBarH : 0;
        int h      = screenBot - kScrollbarH - trimH - y;
        waveformView.setBounds (juce::Rectangle<int> (screenX, y, screenW, h));

        if (trimDialog != nullptr)
            trimDialog->setBounds (screenX, y + h, screenW, kTrimBarH);
    }

    if (shortcutsPanel.isVisible())
        shortcutsPanel.setBounds (getLocalBounds());
}
