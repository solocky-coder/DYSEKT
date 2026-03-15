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

    enum class HoveredEdge { None, Left, Right };
    HoveredEdge hoveredEdge = HoveredEdge::None;

    int pixelToSample (int px) const;
    int sampleToPixel (int sample) const;
    ViewState buildViewState (const SampleData::SnapshotPtr& sampleSnap) const;
    void syncAltStateFromMods (const juce::ModifierKeys& mods);

    void drawWaveform (juce::Graphics& g);
    void drawSlices (juce::Graphics& g);
    void drawPlaybackCursors (juce::Graphics& g);
    void paintDrawSlicePreview (juce::Graphics& g);
    void paintLazyChopOverlay (juce::Graphics& g);
    void paintTransientMarkers (juce::Graphics& g);
    void paintTrimOverlay (juce::Graphics& g);

    /** --- ADDED FOR MARKER DRAG --- **/
    int findMarkerUnderMouse(int mouseX, int tolerance, int& edgeType);

    // Aggregates all cache-invalidation inputs; rebuild is skipped when unchanged.
    struct CacheKey
    {
        int visibleStart = 0, visibleLen = 0, width = 0, numFrames = 0;
        const void* samplePtr = nullptr;
        bool operator== (const CacheKey&) const = default;
    };

    DysektProcessor& processor;
    WaveformCache cache;
    CacheKey prevCacheKey;
    bool sliceDrawMode = false;
    bool softWaveform  = false;
    bool trimMode      = false;
    int  trimInPoint   = 0;
    int  trimOutPoint  = 0;
    bool trimDragging  = false;
    int  trimStart     = 0;
    int  trimEnd       = 0;
    mutable ViewState cachedPaintViewState;
    mutable bool paintViewStateActive = false;

    // Hit areas for trim mode buttons (updated each paint)
    juce::Rectangle<int> trimApplyBtnBounds;
    juce::Rectangle<int> trimResetBtnBounds;
    juce::Rectangle<int> trimCancelBtnBounds;

    static constexpr int kTrimMarkerHitTolerance = 6;
    static constexpr int kMinTrimRegionSamples   = 64;

    DragMode dragMode = None;
    int dragSliceIdx = -1;
    int drawStart = 0;
    int drawEnd = 0;
    bool drawStartedFromAlt = false;
    int addClickStart = -1;
    int dragOffset = 0;
    int dragSliceLen = 0;
    int dragPreviewStart = 0;
    int dragPreviewEnd = 0;
    int dragOrigStart = 0;
    int dragOrigEnd = 0;
    int ghostStart = 0;
    int ghostEnd   = 0;

    int linkedSliceIdx     = -1;
    int linkedPreviewStart = 0;
    int linkedPreviewEnd   = 0;

    bool midDragging = false;
    float midDragStartZoom = 1.0f;
    float midDragAnchorFrac = 0.0f;
    float midDragAnchorPixelFrac = 0.0f;
    int   midDragStartX = 0;
    int   midDragStartY = 0;
};