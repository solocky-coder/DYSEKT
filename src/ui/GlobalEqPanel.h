#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class DysektProcessor;

/**
 * GlobalEqPanel — interactive 3-band EQ with a Bode-style magnitude curve.
 *
 * Bands:
 *   Low  shelf  @ 200 Hz   (±18 dB)
 *   Mid  peak   @ 200–8000 Hz, Q 0.5–4  (±18 dB) — node is draggable in X+Y
 *   High shelf  @ 8000 Hz  (±18 dB)
 *
 * Dragging a node up/down adjusts gain; dragging the Mid node left/right
 * also adjusts frequency.  All writes go through APVTS so undo/automation
 * work out of the box.
 *
 * Layout follows the same addChildComponent / setVisible / setBounds pattern
 * used by MixerPanel and SfzDropdownPanel in PluginEditor.
 */
class GlobalEqPanel : public juce::Component
{
public:
    explicit GlobalEqPanel (DysektProcessor& p);
    ~GlobalEqPanel() override = default;

    void paint       (juce::Graphics&) override;
    void resized     () override;
    void mouseDown   (const juce::MouseEvent&) override;
    void mouseDrag   (const juce::MouseEvent&) override;
    void mouseUp     (const juce::MouseEvent&) override;
    void mouseMove   (const juce::MouseEvent&) override;

private:
    // ── Internal node representation ─────────────────────────────────────────
    enum Band { Low = 0, Mid = 1, High = 2, NoBand = -1 };

    struct Node
    {
        Band  band;
        float gainDb;    // −18 … +18
        float freqHz;    // Mid only: 200 … 8000
        float q;         // Mid only: 0.5 … 4.0
        juce::Point<float> pos;  // screen position (centre of handle), updated in layout()
    };

    // ── Helpers ───────────────────────────────────────────────────────────────
    /** Recompute node screen positions from current APVTS values. */
    void  layout();

    /** Return the plot area (excludes labels and border padding). */
    juce::Rectangle<float> plotArea() const;

    /** Map gain dB → Y within plotArea. */
    float gainToY  (float dB) const;
    /** Map Y → gain dB. */
    float yToGain  (float y)  const;

    /** Map frequency (log scale) → X within plotArea (Mid only). */
    float freqToX  (float hz) const;
    /** Map X → frequency (log scale). */
    float xToFreq  (float x)  const;

    /** Build the magnitude response curve as a Path. */
    juce::Path buildCurve() const;

    /**
     * Return the IIR magnitude at freqHz for a shelf / peak filter,
     * given gain dB, frequency, and Q.
     *
     * type: 0 = lowShelf, 1 = peakFilter, 2 = highShelf
     */
    static float filterMagnitudeAt (int type, float evalHz, float filterHz,
                                    float gainDb, float q, float sampleRate);

    /** Hit-test: return the band whose node is within radius of p, else NoBand. */
    Band hitTest (juce::Point<float> p) const;

    // ── State ─────────────────────────────────────────────────────────────────
    DysektProcessor& processor;

    std::array<Node, 3> nodes;
    Band  dragBand     = NoBand;
    float dragStartGain = 0.f;
    float dragStartFreq = 0.f;
    juce::Point<float> dragStartPos;

    static constexpr float kNodeRadius  = 7.f;
    static constexpr float kGainMax     = 18.f;
    static constexpr float kFreqLo      = 200.f;
    static constexpr float kFreqHi      = 8000.f;
    static constexpr float kSampleRate  = 44100.f;  // for curve drawing only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalEqPanel)
};
