/*
...
*/
// The tooltip initialization should be part of the constructor:
ActionPanel::ActionPanel(DysektProcessor& p, WaveformView& wv)
    : processor(p), waveformView(wv)
{
    addSliceBtn.setTooltip("Add Slice (A / hold Alt)");
    lazyChopBtn.setTooltip("MIDI SLICE");
    // ... Other initialization ...
}
/*
...
*/

// Remove the lines outside the constructor (original lines 4 and 5)
// Only keep the above inside constructor.