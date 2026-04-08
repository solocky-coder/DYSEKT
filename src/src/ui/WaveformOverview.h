#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

/**  Full-sample minimap that replaces ScrollZoomBar.
 *
 *   Renders a phosphor-style overview of the entire sample at 1:1 zoom.
 *   A translucent viewport box shows the currently visible region.
 *
 *   Interactions (same semantics as ScrollZoomBar):
 *     • Drag inside viewport box  → scroll
 *     • Click outside viewport    → jump-scroll to that position
 *     • Vertical drag anywhere    → zoom (drag up = zoom in)
 *     • Mouse wheel               → zoom anchored to cursor
 */
class WaveformOverview : public juce::Component
{
public:
    explicit WaveformOverview (DysektProcessor& p);

    void paint        (juce::Graphics& g) override;
    void mouseDown    (const juce::MouseEvent& e) override;
    void mouseDrag    (const juce::MouseEvent& e) override;
    void mouseUp      (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& w) override;

    bool isDraggingNow() const noexcept { return isDragging; }

    /** Call from editor timerCallback to refresh. */
    void repaintOverview();

private:
    DysektProcessor& processor;

    // Peak cache — rebuilt when sample changes or width changes
    std::vector<float> peaks;       // one max-abs peak per pixel column
    int                peakFrameCount { 0 };   // numFrames when peaks were built
    int                peakWidth      { 0 };   // width when peaks were built

    void rebuildPeaks();
    juce::Rectangle<float> viewportRect() const;   // viewport box in component coords

    // Drag state
    bool  isDragging       { false };
    bool  draggingViewport { false };   // true = drag scroll; false = zoom drag
    float dragStartScroll  { 0.0f };
    float dragStartZoom    { 0.0f };
    float dragAnchorFrac   { 0.0f };
    float dragAnchorPxFrac { 0.0f };
    int   dragStartX       { 0 };
    int   dragStartY       { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformOverview)
};
