#pragma once

constexpr int kBaseW      = 1130;
constexpr int kLogoH      = 52;
// kLcdRowH — info LCD (SliceLcdDisplay) natural height
constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12;
// kWaveformLcdRowH — waveform/ADSR LCD height (taller for detailed ADSR editing)
constexpr int kWaveformLcdRowH = 400;
constexpr int kSliceLaneH = 36;
constexpr int kScrollbarH = 28;
constexpr int kSliceCtrlH = 72;
constexpr int kActionH    = 22;
constexpr int kTrimBarH   = 34;
constexpr int kPanelSlotH = 200;

// Centre column width and explicit frame height (3/4 of kLcdRowH)
constexpr int kCtrlFrameW = 200;
constexpr int kCtrlFrameH = (kLcdRowH * 3) / 4;
// Button bar (UNDO/REDO/PANIC/⚙) beneath the global frame
constexpr int kBtnBarH    = 38;

// Height of the 2×2 button grid (UNDO/REDO left col, PANIC/⚙ right col).
// Two 20px buttons + 4px row gap + 6px top/bottom padding = 50px.
constexpr int kBtnRowH    = 50;

constexpr int kBaseHCore  = kLogoH + kWaveformLcdRowH + kSliceLaneH + kScrollbarH
                           + kSliceCtrlH + kActionH + 120;
constexpr int kTotalH     = kBaseHCore + kPanelSlotH + 16;
constexpr int kMargin     = 8;
