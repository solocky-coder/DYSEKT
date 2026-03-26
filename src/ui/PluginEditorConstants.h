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

// Centre column width — widened from 180 to 200 to comfortably fit the four
// action buttons (UNDO / REDO / PANIC / ⚙) in a single row.
constexpr int kCtrlFrameW = 200;

// Height of the Undo/Redo/Panic/Settings button row inside the centre column.
constexpr int kBtnRowH    = 28;

constexpr int kBaseHCore  = kLogoH + kLcdRowH + kSliceLaneH + kScrollbarH
                           + kSliceCtrlH + kActionH + 120;
constexpr int kTotalH     = kBaseHCore + kPanelSlotH + 16;
constexpr int kMargin     = 8;
