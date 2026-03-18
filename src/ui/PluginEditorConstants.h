#pragma once

constexpr int kBaseW      = 1130;
constexpr int kLogoH      = 52;
constexpr int kLcdRowH    = SliceLcdDisplay::kPreferredHeight + 12; // Make sure SliceLcdDisplay is included before this!
constexpr int kSliceLaneH = 36;
constexpr int kScrollbarH = 28;
constexpr int kSliceCtrlH = 72;
constexpr int kActionH    = 22;
constexpr int kTrimBarH   = 34;
constexpr int kPanelSlotH = 200;
constexpr int kCtrlFrameW = 180;
constexpr int kBaseHCore  = kLogoH + kLcdRowH + kSliceLaneH + kScrollbarH + kSliceCtrlH + kActionH + 120;
constexpr int kTotalH     = kBaseHCore + kPanelSlotH + 16;
constexpr int kMargin     = 8;