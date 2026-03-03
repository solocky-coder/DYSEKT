#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  MPC-style LCD readout for the currently selected slice.
 *
 *   Renders as a green-on-black segmented display showing:
 *   - Slice index / total  (e.g.  "SL 03 / 16")
 *   - MIDI note            (e.g.  "C#3  [ 49]")
 *   - Sample start / end   (e.g.  "ST:  44100")
 *   - Length in ms         (e.g.  "LEN: 250ms")
 *   - Volume / Pan         (e.g.  "VOL: +0dB  PAN: L12")
 *   - Pitch / Detune       (e.g.  "PIT: +3st  DET:-25ct")
 *   - Algorithm            (e.g.  "ALG: GRANULAR")
 *   - ADSR                 (e.g.  "A:0ms D:80ms S:100% R:200ms")
 *   - Flags strip          (e.g.  "[REV][LOOP][1SH][MUT:2]")
 *
 *   Call repaintLcd() from the editor's timerCallback() at ~30 Hz.
 */
class SliceLcdDisplay : public juce::Component
{
public:
    explicit SliceLcdDisplay (DysektProcessor& p);

    void paint (juce::Graphics& g) override;

    /** Call this periodically from the UI timer to refresh the display. */
    void repaintLcd();

    /** Suggested height in pixels (un-scaled). */
    static constexpr int kPreferredHeight = 136;

private:
    struct DisplayData
    {
        bool      hasSlice        { false };
        bool      hasSample       { false };
        int       sliceIndex      { -1 };
        int       numSlices       { 0 };
        int       midiNote        { 60 };
        int       startSample     { 0 };
        int       endSample       { 0 };
        int       sampleNumFrames { 0 };
        double    sampleRate      { 44100.0 };
        float     volume          { 0.0f };
        float     pan             { 0.0f };
        float     pitchSemitones  { 0.0f };
        float     centsDetune     { 0.0f };
        int       algorithm       { 0 };
        float     attackSec       { 0.0f };
        float     decaySec        { 0.0f };
        float     sustainLevel    { 1.0f };
        float     releaseSec      { 0.0f };
        bool      reverse         { false };
        int       loopMode        { 0 };
        bool      oneShot         { false };
        int       muteGroup       { 0 };
        float     filterCutoff    { 20000.0f };
        float     filterRes       { 0.0f };
        int       rootNote        { 36 };
        juce::String sampleName;
    };

    void buildDisplayData();
    void drawLcdBackground (juce::Graphics& g);
    void drawRow (juce::Graphics& g, int row, const juce::String& label,
                  const juce::String& value, bool highlight = false);
    void drawFlagsRow (juce::Graphics& g, int row);
    void drawNoSliceScreen (juce::Graphics& g);
    void drawNoSampleScreen (juce::Graphics& g);

    static juce::String midiNoteName (int note);
    static juce::String formatMs (float secs);
    static juce::String formatAlgo (int algo);
    static juce::String formatPan  (float pan);

    DysektProcessor& processor;
    DisplayData      data;

    // Pixel geometry — computed in paint()
    static constexpr int kRows       = 7;
    static constexpr int kRowPadV    = 3;
    static constexpr int kLeftPad    = 10;
    static constexpr int kLabelW     = 52;
    static constexpr int kScanlineAlpha = 18;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceLcdDisplay)
};
