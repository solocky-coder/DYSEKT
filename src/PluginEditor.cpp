// Layout Constants
const int kHeaderRowH = 0; // Removed concept of separate header row
const int kLcdW = 0; // Full width, previously 320
const int kLcdH = 240; // Added expanded LCD height

// Resized function
void resized() {
    // Logo bar
    auto logoBar = area.removeFromTop (logoBarHeight);

    // Set the full width for the LCD
    sliceLcdDisplay.setBounds (area.removeFromTop (kLcdH).reduced (kMargin, kMargin));

    // Configure Slice lane and the rest as needed
    sliceLane.setBounds (area.removeFromTop (sliceLaneHeight).reduced (kMargin, kMargin));
    rest.setBounds (area);
}