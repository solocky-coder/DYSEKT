#include "PluginEditor.h"
#include <algorithm>

// ==== [ RESTORE LAYOUT CONSTANTS ] ====
static constexpr int kBaseW      = 1130;
static constexpr int kLogoH      = 52;
static constexpr int kLcdRowH    = 56;
static constexpr int kSliceLaneH = 36;
static constexpr int kScrollbarH = 28;
static constexpr int kSliceCtrlH = 72;
static constexpr int kActionH    = 22;
static constexpr int kTrimBarH   = 34;
static constexpr int kCtrlFrameW = 180;
static constexpr int kBrowserH   = 170;
static constexpr int kMargin     = 8;
static constexpr int kPanelSlotH = 200;

// [ ... other code unchanged ... ]

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());

    // 1. Combined header bar (logo left + UNDO/REDO/PANIC/UI right)
    //    LogoBar and HeaderBar share the same horizontal strip.
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

// [ ... everything else unmodified ... ]