#pragma once

constexpr int kBaseW      = 1130;
constexpr int kLogoH      = 52;
// kLcdRowH depends on SliceLcdDisplay::kPreferredHeight — must include SliceLcdDisplay.h first
constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12;
constexpr int kSliceLaneH = 36;
constexpr int kScrollbarH = 28;
constexpr int kSliceCtrlH = 72;
constexpr int kActionH    = 22;
constexpr int kTrimBarH   = 34;
constexpr int kPanelSlotH = 200;

// Centre column width
constexpr int kCtrlFrameW = 200;

// Height of the 2×2 button grid (UNDO/REDO left col, PANIC/⚙ right col).
// Two 20px buttons + 4px row gap + 6px top/bottom padding = 50px.
constexpr int kBtnRowH    = 50;

constexpr int kBaseHCore  = kLogoH + kLcdRowH + kSliceLaneH + kScrollbarH
                           + kSliceCtrlH + kActionH + 120;
constexpr int kTotalH     = kBaseHCore + kPanelSlotH + 16;
constexpr int kMargin     = 8;
