#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  HALion-style full-sample minimap overview strip.
 *
 *   A thin horizontal bar showing the entire sample at reduced scale.
 *   A highlighted viewport rectangle shows the currently visible region.
 *
 *   Interactions:
 *     - Drag left handle         -> resize left edge (zoom), right edge fixed
 *     - Drag right handle        -> resize right edge (zoom), left edge fixed
 *     - Drag inside viewport box -> scroll
 *     - Click outside viewport   -> jump-scroll to that position
 *     - Mouse wheel              -> zoom anchored to cursor
 */
class WaveformOverview : public juce::Component
{
public:
    explicit WaveformOverview (DysektProcessor& p);

    void paint           (juce::Graphics& g) override;
    void mouseDown       (const juce::MouseEvent& e) override;
    void mouseDrag       (const juce::MouseEvent& e) override;
    void mouseUp         (const juce::MouseEvent& e) override;
    void mouseWheelMove  (const juce::MouseEvent& e,
                          const juce::MouseWheelDetails& w) override;

    bool isDraggingNow() const noexcept { return isDragging; }

    /** Call from the editor's timerCallback to keep the display current. */
    void repaintOverview();

private:
    DysektProcessor& processor;

    // Peak cache — one max-abs value per pixel column, rebuilt on sample/size change
    std::vector<float> peaks;
    int peakNumFrames { 0 };
    int peakWidth     { 0 };

    void rebuildPeaks();
    juce::Rectangle<float> viewportRect() const;

    enum class DragMode { None, Scroll, ResizeLeft, ResizeRight };

    bool     isDragging     { false };
    DragMode dragMode       { DragMode::None };

    // State captured at mouseDown
    float dragStartScroll   { 0.0f };   // processor.scroll at drag start
    float dragStartZoom     { 1.0f };   // processor.zoom   at drag start
    float dragFixedFrac     { 0.0f };   // fixed edge as fraction of numFrames (resize modes)
                                        // or click offset within viewport (scroll mode)
    float dragMovingFrac    { 0.0f };   // moving edge as fraction of numFrames (resize modes)
    int   dragStartX        { 0 };

    static constexpr int   kHandleW       = 7;
    static constexpr float kMinViewFrac   = 0.01f;
    static constexpr float kZoomWheelSens = 0.18f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformOverview)
};
