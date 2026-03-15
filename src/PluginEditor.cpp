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

// ────────────── DysektEditor Implementation ──────────────────────

// The following assumes that all the *Panel and *Bar and *Display, etc classes
// require a DysektProcessor& in their constructor (see their explicit constructors).

DysektEditor::DysektEditor(DysektProcessor& p)
    : AudioProcessorEditor (p)
    , processor(p)
    , waveformOverview(processor)
    , logoBar(processor)
    , headerBar(processor)
    , sliceLcd(processor)
    , sliceWaveformLcd(processor)
    , sliceLane(processor)
    , waveformView(processor)
    , sliceControlBar(processor)
    , actionPanel(processor, waveformView)
    , browserPanel(processor)
    , mixerPanel(processor)
    , shortcutsPanel(processor)
{
    // You should addAndMakeVisible() for UI components you want visible by default
    addAndMakeVisible(logoBar);
    addAndMakeVisible(headerBar);
    addAndMakeVisible(sliceLcd);
    addAndMakeVisible(sliceWaveformLcd);
    addAndMakeVisible(sliceLane);
    addAndMakeVisible(waveformView);
    addAndMakeVisible(sliceControlBar);
    addAndMakeVisible(actionPanel);
    addAndMakeVisible(browserPanel);
    addAndMakeVisible(mixerPanel);
    addAndMakeVisible(shortcutsPanel);
    addAndMakeVisible(waveformOverview);

    // TODO: Add rest of initialization (callbacks, timer, etc)
    setSize(kBaseW, kTotalH);
}

// ────────────── Your Resized And Other Methods As Before ──────────────

void DysektEditor::resized()
{
    auto area = juce::Rectangle<int> (0, 0, getWidth(), getHeight());

    // [ ... rest of your layout code exactly as in your working version ... ]
    // This method does not need to change from your previous message.
    // Copy your existing resized() body in here.
}

// ... etc for the rest of the methods in your DysektEditor implementation ...