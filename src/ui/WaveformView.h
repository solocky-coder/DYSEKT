#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "WaveformCache.h"

class DysektProcessor;

class WaveformView : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    explicit WaveformView (DysektProcessor& p);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void modifierKeysChanged (const juce::ModifierKeys& mods) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void rebuildCacheIfNeeded();
    bool hasActiveSlicePreview() const noexcept;
    bool getActiveSlicePreview (int& sliceIdx, int& startSample, int& endSample) const;
    bool getLinkedSlicePreview (int& sliceIdx, int& startSample, int& endSample) const;
    bool isInteracting() const noexcept;

    void setSliceDrawMode (bool active);
    bool isSliceDrawModeActive() const noexcept { return sliceDrawMode; }

    // Trim mode — entered when the user asks to trim before loading
    void enterTrimMode (int start, int end);
    void setTrimPoints (int inPt, int outPt);  // MIDI feedback path
    void exitTrimMode();
    void getTrimBounds (int& outStart, int& outEnd) const;
    bool isTrimModeActive() const noexcept { return trimMode; }

    // Trim mode API used by TrimDialog and ActionPanel
    void setTrimMode (bool active);
    void resetTrim();
    int  getTrimIn()  const noexcept { return trimInPoint; }
    int  getTrimOut() const noexcept { return trimOutPoint; }
    bool isTrimDragging() const noexcept { return trimDragging; }

    // Callback invoked when user applies trim; parameters are sample-accurate bounds
    std::function<void (int trimStart, int trimEnd)> onTrimApplied;
    // Callback invoked when user cancels trim (CANCEL button)
    std::function<void()> onTrimCancelled;
    // Callback for file load requests (routed through trim dialog if set)
    std::function<void (const juce::File&)> onLoadRequest;

    void setSoftWaveform (bool soft) { softWaveform = soft; repaint(); }
    bool isSoftWaveform() const noexcept { return softWaveform; }

    bool altModeActive = false;
    bool shiftPreviewActive = false;
    std::vector<int> transientPreviewPositions;

private:
    struct ViewState
    {
        int numFrames = 0;
        int visibleStart = 0;
        int visibleLen = 0;
        int width = 0;
        float samplesPerPixel = 1.0f;
        bool valid = false;
    };

    enum DragMode { None, DragEdgeLeft, DragEdgeRight, DrawSlice, MoveSlice, DuplicateSlice,
                    TrimMarkerLeft, TrimMarkerRight, DragTrimIn, DragTrimOut };

    // Internal state
    DysektProcessor& processor;

    bool sliceDrawMode = false;
    bool trimMode = false;
    int trimInPoint = 0;
    int trimOutPoint = 0;
    int trimStart = 0;
    int trimEnd = 0;
    bool trimDragging = false;

    DragMode dragMode = None;
    int dragSliceIdx = -1;
    int dragPreviewStart = 0;
    int dragPreviewEnd = 0;
    int dragOrigStart = 0;
    int dragOrigEnd = 0;
    int linkedSliceIdx = -1;
    int linkedPreviewStart = 0;
    int linkedPreviewEnd = 0;
    bool midDragging = false;
    bool drawStartedFromAlt = false;

    // For waveform cache, rendering, and preview overlays
    WaveformCache cache;
    WaveformCache::CacheKey prevCacheKey;
    ViewState cachedPaintViewState;
    bool paintViewStateActive = false;

    // Painting and transient overlays
    bool softWaveform = false;

    enum class HoveredEdge { None, Left, Right };
    HoveredEdge hoveredEdge = HoveredEdge::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};