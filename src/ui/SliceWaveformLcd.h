#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  Second LCD panel: renders the selected slice waveform with an interactive
 *   ADSR envelope overlay.  Each envelope node is a draggable handle; moving
 *   it updates the corresponding parameter via apvts in real time.
 *
 *   Node layout:
 *     P0  (fixed)  — silence at slice start
 *     P1  Attack   — drag X (time) + Y (peak level)   colour: Toxic Lime
 *     P2  Decay    — drag X (time) + Y (sustain level) colour: Radioactive Yellow
 *     P3  Release  — drag X (release-start time)       colour: Molten Orange
 *     P4  (fixed)  — silence at slice end
 *     SH  Sustain  — mid-plateau Y handle              colour: Ice Blue
 *
 *   Dimensions match SliceLcdDisplay::kPreferredHeight.
 *   Call repaintLcd() from the editor's timerCallback() at ~30 Hz.
 */
class SliceWaveformLcd : public juce::Component
{
public:
    explicit SliceWaveformLcd (DysektProcessor& p);

    void paint    (juce::Graphics& g) override;
    void resized  () override;
    void mouseMove    (const juce::MouseEvent& e) override;
    void mouseDown    (const juce::MouseEvent& e) override;
    void mouseDrag    (const juce::MouseEvent& e) override;
    void mouseUp      (const juce::MouseEvent& e) override;

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
        bool         isDefault   { false };
        juce::Array<float> peaks;
    };

    // ── ADSR envelope node ────────────────────────────────────────────────────
    enum class NodeRole { None, Attack, Decay, Sustain, Release };

    struct EnvNode
    {
        float      xn { 0.0f };   // normalised 0-1 across component width
        float      yn { 0.0f };   // normalised 0-1 (0=top/loud 1=bottom/silent)
        NodeRole   role { NodeRole::None };
        juce::Colour colour;
        const char*  label { nullptr };
    };

    void  buildDisplayData();
    void  buildEnvelopeNodes();            // read params → nodes[]
    void  commitNodes();                   // nodes[] → write params
    float envAt (float xn) const;         // interpolated Y at position xn

    void drawBackground  (juce::Graphics& g);
    void drawWaveform    (juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawEnvelope    (juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawNodes       (juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawNoData      (juce::Graphics& g);
    void drawSegmentLabel(juce::Graphics& g, float x0, float y0,
                          float x1, float y1, const char* text,
                          juce::Colour col, const juce::Rectangle<float>& area);

    NodeRole hitTest (juce::Point<float> pos) const;

    static juce::String midiNoteName (int note);
    static juce::String formatMs     (float ms);

    DysektProcessor& processor;
    DisplayData      data;

    // ── Envelope state ────────────────────────────────────────────────────────
    // Five control points (normalised).
    // P0 = (0,1) fixed  P1=attack  P2=decay  SH=sustain  P3=release  P4=(1,1) fixed
    struct {
        float ax  { 0.07f };   // attack peak X
        float ay  { 0.10f };   // attack peak Y  (near 0 = loud peak)
        float dx  { 0.25f };   // decay end X
        float sy  { 0.30f };   // sustain Y level
        float rx  { 0.82f };   // release start X
    } env;

    // Cache of computed nodes (rebuilt each paint + mouse event)
    juce::Array<EnvNode> envNodes;    // P1..P3 + sustain handle

    NodeRole dragRole        { NodeRole::None };
    NodeRole hovRole         { NodeRole::None };
    float    dragStartX     { 0.0f };
    int      postCommitGuard { 0 };   // frames to skip rebuild after commitNodes()
    int      lastEnvSnapVer  { -1 };  // snapshot version when env was last built

    // Content area cached in resized() / used for hit testing
    juce::Rectangle<float> screenArea;

    static const juce::Colour kBg;
    static const juce::Colour kBezel;
    static const juce::Colour kPhosphor;
    static const juce::Colour kDim;
    static const juce::Colour kBright;
    static const juce::Colour kLabel;

    static constexpr int   kScanlineAlpha = 18;
    static constexpr int   kLeftPad       = 8;
    static constexpr float kNodeR         = 4.5f;
    static constexpr float kHitR          = 10.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceWaveformLcd)
};
