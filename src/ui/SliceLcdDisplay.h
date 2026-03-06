#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class SliceLcdDisplay : public juce::Component
{
public:
    explicit SliceLcdDisplay (DysektProcessor& p);

    void paint    (juce::Graphics& g) override;
    void resized  ()                  override {}

    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& w) override;

    void repaintLcd();

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kRows         = 7;   // number of content rows
    static constexpr int kLeftPad      = 6;
    static constexpr int kLabelW       = 46;
    static constexpr int kScanlineAlpha = 28;

    // ── Data snapshot ─────────────────────────────────────────────────────────
    struct DisplayData
    {
        bool         hasSample    = false;
        bool         hasSlice     = false;
        int          sliceIndex   = 0;
        int          numSlices    = 0;
        int          rootNote     = 36;
        juce::String sampleName;
        int          sampleNumFrames = 0;
        double       sampleRate   = 44100.0;

        int          midiNote     = 36;
        int          startSample  = 0;
        int          endSample    = 0;
        float        volume       = 0.0f;
        float        pan          = 0.0f;
        float        pitchSemitones = 0.0f;
        float        centsDetune  = 0.0f;
        int          algorithm    = 0;
        float        attackSec    = 0.005f;
        float        decaySec     = 0.1f;
        float        sustainLevel = 1.0f;
        float        releaseSec   = 0.02f;
        bool         reverse      = false;
        int          loopMode     = 0;
        bool         oneShot      = false;
        int          muteGroup    = 0;
        float        filterCutoff = 20000.0f;
        float        filterRes    = 0.0f;
    };

    DisplayData data;

    // ── Scroll state ──────────────────────────────────────────────────────────
    int scrollOffsetPx = 0;   // pixels scrolled from top

    // ── Draw helpers ──────────────────────────────────────────────────────────
    void buildDisplayData();
    void drawLcdBackground  (juce::Graphics& g);
    void drawRow            (juce::Graphics& g, int row,
                             const juce::String& label,
                             const juce::String& value,
                             bool highlight = false);
    void drawRowPair        (juce::Graphics& g, int row,
                             const juce::String& leftStr,
                             const juce::String& rightStr,
                             bool highlight = false);
    void drawFlagsRow       (juce::Graphics& g, int row);
    void drawNoSliceScreen  (juce::Graphics& g);
    void drawNoSampleScreen (juce::Graphics& g);

    // ── String formatters ─────────────────────────────────────────────────────
    static juce::String midiNoteName (int note);
    static juce::String formatMs     (float secs);
    static juce::String formatAlgo   (int algo);
    static juce::String formatPan    (float pan);

    DysektProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceLcdDisplay)
};
