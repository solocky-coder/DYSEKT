#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  Second LCD panel: renders only the selected slice's portion of the audio
 *   as a mini waveform so you can inspect the transient shape and tail without
 *   looking at the full waveform view.
 *
 *   Displayed alongside SliceLcdDisplay in a horizontal row, with the control
 *   frame (icons + knobs) sitting between the two LCDs.
 *
 *   Dimensions match SliceLcdDisplay::kPreferredHeight.
 *   Call repaintLcd() from the editor's timerCallback() at ~30 Hz.
 */
class SliceWaveformLcd : public juce::Component
{
public:
    explicit SliceWaveformLcd (DysektProcessor& p);

    void paint  (juce::Graphics& g) override;
    void resized() override;

    /** Call periodically from the UI timer to refresh the display. */
    void repaintLcd();

    /** Suggested height in pixels (un-scaled) — matches SliceLcdDisplay. */
    static constexpr int kPreferredHeight = 136;

private:
    // ── Snapshot used for one paint pass ─────────────────────────────────────
    struct DisplayData
    {
        bool         hasSlice       { false };
        bool         hasSample      { false };
        int          sliceIndex     { -1 };
        int          numSlices      { 0 };
        int          startSample    { 0 };
        int          endSample      { 0 };
        int          totalFrames    { 0 };
        int          midiNote       { 60 };
        float        volume         { 0.0f };
        float        pan            { 0.0f };
        float        pitchSemitones { 0.0f };
        int          algorithm      { 0 };
        double       sampleRate     { 44100.0 };
        juce::String sampleName;

        // Waveform thumbnail data (built from processor audio buffer)
        juce::Array<float> peaks;   // peak amplitudes, one per pixel column
    };

    void buildDisplayData();
    void drawBackground  (juce::Graphics& g);
    void drawWaveform    (juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawOverlay     (juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawNoData      (juce::Graphics& g);

    static juce::String midiNoteName (int note);
    static juce::String formatMs     (float secs);
    static juce::String formatAlgo   (int algo);
    static juce::String formatPan    (float pan);

    DysektProcessor& processor;
    DisplayData      data;

    // Teal palette — matches the main LCD colours
    static const juce::Colour kBg;
    static const juce::Colour kBezel;
    static const juce::Colour kPhosphor;
    static const juce::Colour kDim;
    static const juce::Colour kBright;
    static const juce::Colour kLabel;

    static constexpr int kScanlineAlpha = 18;
    static constexpr int kLeftPad       = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceWaveformLcd)
};
