#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**
 * MixerPanel — horizontal row mixer.
 *
 * Each row = one slice.  Columns: GAIN · PAN · FCUT · PRES · MUTE GRP · OUT
 * Clicking a row fires CmdSelectSlice.
 * Knobs write per-slice values directly (no global fallback from this panel).
 * Scrollable — all slices visible.
 */
class MixerPanel : public juce::Component,
                   public juce::Timer
{
public:
    /** Fixed panel height used by PluginEditor layout. */
    static constexpr int kPanelH      = 230;
    /** Height of the column-header row. */
    static constexpr int kHeaderH     = 24;
    /** Height of each slice row. */
    static constexpr int kRowH        = 30;
    /** Height of the master row at the bottom. */
    static constexpr int kMasterH     = 34;
    /** Width of the slice name column. */
    static constexpr int kNameColW    = 82;
    /** Width of each knob column. */
    static constexpr int kKnobColW    = 72;
    /** Number of knob columns. */
    static constexpr int kNumCols     = 7;

    explicit MixerPanel (DysektProcessor& p);
    ~MixerPanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&,
                           const juce::MouseWheelDetails&) override;

    /** Called from editor timer — snapshot version check, repaint if stale. */
    void updateFromSnapshot();

    // Timer (drives scroll animation if ever needed)
    void timerCallback() override {}

private:
    // ── Column layout ─────────────────────────────────────────────────────
    enum Col { ColGain=0, ColPan, ColFcut, ColPres, ColMute, ColChro, ColOut };

    // ── Hit-test cell ─────────────────────────────────────────────────────
    struct Cell {
        int row  { -1 };   // -1 = master row
        Col col  { ColGain };
        juce::Rectangle<int> bounds;
        bool isMaster { false };
    };

    // ── Drawing helpers ───────────────────────────────────────────────────
    void drawHeader    (juce::Graphics&) const;
    void drawSliceRow  (juce::Graphics&, int rowY, int sliceIdx, bool selected) const;
    void drawMasterRow (juce::Graphics&, int rowY) const;
    void drawKnobInRow (juce::Graphics&, int cx, int cy, float norm,
                        bool locked, bool isMaster = false) const;
    void drawMuteBadge (juce::Graphics&, int cx, int cy,
                        int muteGroup, bool locked) const;
    void drawChroBadge (juce::Graphics&, int cx, int cy, int channel) const;

    juce::String fmtGain (float db)      const;
    juce::String fmtPan  (float pan)     const;
    juce::String fmtFcut (float hz)      const;
    juce::String fmtPres (float res)     const;
    juce::String fmtOut  (int bus)       const;
    juce::String fmtMute (int mg)        const;

    float toNormGain (float db)   const;
    float toNormPan  (float pan)  const;
    float toNormFcut (float hz)   const;
    float toNormPres (float res)  const;
    float toNormOut  (int bus)    const;

    float fromNormFcut (float n) const;

    int   colX        (Col col) const;
    int   rowY        (int sliceIdx) const;   // top Y of a slice row in the scroll area
    int   masterRowY  () const;
    Cell  hitTest     (juce::Point<int> pos) const;

    // ── Drag state ────────────────────────────────────────────────────────
    struct DragState {
        bool   active    { false };
        bool   isMaster  { false };
        int    sliceIdx  { -1 };
        Col    col       { ColGain };
        int    startY    { 0 };
        float  startVal  { 0.f };
    } drag;

    // ── Scroll ────────────────────────────────────────────────────────────
    int   scrollPixels { 0 };   // vertical scroll offset in pixels

    // ── State cache ───────────────────────────────────────────────────────
    uint32_t cachedVersion { 0xFFFFFFFF };
    int      cachedNumSlices { -1 };

    // ── Text editor for double-click ──────────────────────────────────────
    std::unique_ptr<juce::TextEditor> textEditor;

    DysektProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
