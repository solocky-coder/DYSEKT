#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class SliceLcdDisplay : public juce::Component
{
public:
    explicit SliceLcdDisplay (DysektProcessor& p);

    // Height the component requests — used by PluginEditor for layout
    static constexpr int kPreferredHeight = 292;  // 10 rows × 28px + bezel

    void paint    (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    /** Fired when the user clicks the NAME field; wire up to show RenameOverlay. */
    std::function<void (int sliceIdx, const juce::String& currentName)> onRenameRequest;

    void repaintLcd();
    void resized() override;

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kTotalRows     = 10;  // total rows
    static constexpr int kRowH          = 28;  // fixed row height in pixels
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
        float        attackSec    = 0.005f;
        float        holdSec      = 0.0f;
        float        decaySec     = 0.1f;
        float        sustainLevel = 1.0f;
        float        releaseSec   = 0.02f;
        bool         reverse      = false;
        int          loopMode     = 0;
        bool         oneShot      = false;
        juce::Colour sliceColour;         // vibrant colour of the selected slice
        int          muteGroup    = 0;
        float        filterCutoff = 20000.0f;
        float        filterRes    = 0.0f;
        juce::String sliceName;           // user-defined name; empty = not set
        // Lock state
        bool         sliceLocked      = false;
        // Extended — scroll rows 7-9
        bool         stretchEnabled   = false;
        float        tonalityHz       = 0.0f;
        float        formantSemitones = 0.0f;
        bool         formantComp      = false;

        bool         releaseTail      = false;
        int          outputBus        = 0;
        float        bpm              = 120.0f;
    };

    DisplayData data;

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

    static juce::String formatPan    (float pan);

    DysektProcessor& processor;

    // ── Flag hit rects (updated each paint, used by mouseDown) ───────────────
    struct FlagHitRect
    {
        juce::Rectangle<int> bounds;
        int fieldId;   // DysektProcessor::FieldXxx
        bool isCycle;  // true = cycle through values (loopMode, muteGroup)
    };
    std::vector<FlagHitRect> flagHitRects;

    // ── NAME field hit rect (Row 1 right side) ────────────────────────────
    juce::Rectangle<int> nameHitRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceLcdDisplay)
};
