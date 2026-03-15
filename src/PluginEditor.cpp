#include "PluginEditor.h"
#include <algorithm>

// ────────────── Add LAYOUT CONSTANTS at top ─────────────────────
namespace
{
    static constexpr int kBaseW      = 1130;
    static constexpr int kLogoH      = 52;    // single combined header bar
    static constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12; // LCD row + padding
    static constexpr int kSliceLaneH = 36;   // 30 body + 6 ADSR dot strip
    static constexpr int kScrollbarH = 28;
    static constexpr int kSliceCtrlH = 72;
    static constexpr int kActionH    = 22;
    static constexpr int kTrimBarH   = 34;   // height of inline trim bar
    static constexpr int kCtrlFrameW    = 180; // width of the centre control frame

    static constexpr int kBrowserH    = 170;
    static constexpr int kMargin      = 8;
    // Fixed panel slot — same height used for both mixer and browser.
    // Mixer is taller so we cap it; browser gets the full slot.
    static constexpr int kPanelSlotH  = 200;
    static constexpr int kBaseHCore   = kLogoH + kLcdRowH + kSliceLaneH
                                      + kScrollbarH + kSliceCtrlH + kActionH
                                      + 120; // minimum waveform height
    // Total height is FIXED — panel slot always reserved, window never resizes.
    static constexpr int kTotalH      = kBaseHCore + kPanelSlotH + 16; // 16 = frame padding
}
// ────────────── End layout constants block ───────────────────────

// Correct constructor initialization for members with custom constructors!
DysektEditor::DysektEditor(DysektProcessor& p)
    : AudioProcessorEditor(p), 
      processor(p),
      waveformOverview(p),
      logoBar(p),
      headerBar(p),
      sliceLcd(p),
      sliceWaveformLcd(p),
      sliceLane(p),
      waveformView(p),
      sliceControlBar(p),
      actionPanel(p, waveformView),
      browserPanel(p),
      mixerPanel(p),
      shortcutsPanel(p) // <--- note: takes processor, not ref in {}
{
    // Usual addAndMakeVisible, customizers for children, etc...
    // ... addAndMakeVisible(logoBar); etc as before
}

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());
    // ... rest of your code unchanged ...
}

// ... everything else unmodified ...