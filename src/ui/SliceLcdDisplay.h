#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class SliceLcdDisplay : public juce::Component,
                         private juce::ScrollBar::Listener
{
public:
    explicit SliceLcdDisplay (DysektProcessor& p);

    // Height the component requests — used by PluginEditor for layout
    static constexpr int kPreferredHeight = 114;  // 7 visible rows × 14px + padding

    void paint    (juce::Graphics& g) override;

    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& w) override;

    void repaintLcd();
    void resized() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kVisibleRows   = 7;   // rows shown without scrolling
    static constexpr int kTotalRows     = 10;  // total rows including scroll
    static constexpr int kRowH          = 14;  // fixed row height in pixels
    static constexpr int kScrollW       = 8;   // scrollbar strip width
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
        // Lock state
        bool         sliceLocked      = false;
        // Extended — scroll rows 7-9
        bool         stretchEnabled   = false;
        float        tonalityHz       = 0.0f;
        float        formantSemitones = 0.0f;
        bool         formantComp      = false;
        int          grainMode        = 0;
        bool         releaseTail      = false;
        int          outputBus        = 0;
        float        bpm              = 120.0f;
    };

    DisplayData data;

    // ── Scroll state ──────────────────────────────────────────────────────────
    juce::ScrollBar scrollBar { true };  // vertical scrollbar
    int scrollOffsetPx = 0;              // pixels scrolled from top

    void updateScrollBar();              // sync scrollBar range/position to content

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
