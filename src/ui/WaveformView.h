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
    bool isInteracting() const noexcept;

    void setSliceDrawMode (bool active);
    bool isSliceDrawModeActive() const noexcept { return sliceDrawMode; }

    void setTrimMode (bool active);
    bool isTrimModeActive() const noexcept { return trimMode; }
    void resetTrim();
    int  getTrimIn()  const noexcept { return trimInPoint; }
    int  getTrimOut() const noexcept { return trimOutPoint; }

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
                    DragTrimIn, DragTrimOut };

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
    void paintTrimMarkers (juce::Graphics& g);
    void paintAddModeMarkers (juce::Graphics& g);

    // ADD mode v2 helpers
    std::vector<int> collectBoundaries() const;
    int  findNearestBoundary (int x) const;   ///< pixel x; returns sample pos or -1
    void deleteSlicesAtBoundary (int samplePos);
    void flashNearestMarker (int x);          ///< trigger orange flash at pixel x

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
    bool softWaveform  = false;   // TAL-style gradient+outline rendering
    bool trimMode      = false;   // trim in/out marker editing mode
    int  trimInPoint   = 0;       // trim-in marker position in samples
    int  trimOutPoint  = 0;       // trim-out marker position in samples (0 = end of sample)
    mutable ViewState cachedPaintViewState;   // valid only between paint() start and end
    mutable bool paintViewStateActive = false; // true only during paint(); guards cachedPaintViewState

    DragMode dragMode = None;
    int dragSliceIdx = -1;
    int drawStart = 0;
    int drawEnd = 0;
    bool drawStartedFromAlt = false;
    int addClickStart = -1; // ADD click mode: -1 = waiting for first click, >= 0 = waiting for second click
    int dragOffset = 0;    // for MoveSlice: offset from mouse to slice start
    int dragSliceLen = 0;  // for MoveSlice: original slice length
    int dragPreviewStart = 0; // for edge/move drags: preview start sample
    int dragPreviewEnd = 0;   // for edge/move drags: preview end sample
    int dragOrigStart = 0;    // slice start at the moment drag began (for overlap clamping)
    int dragOrigEnd = 0;      // slice end at the moment drag began (for overlap clamping)
    int ghostStart = 0;    // for DuplicateSlice: ghost overlay start sample
    int ghostEnd   = 0;    // for DuplicateSlice: ghost overlay end sample

    // ADD mode v2 state
    int flashBoundaryPos   = -1;    ///< sample pos of boundary to flash orange (-1 = none)
    juce::Time flashStartTime;      ///< when the flash began (for 300 ms timer)
    int lastMouseX         = -1;    ///< most recent mouse x position for hover highlighting

    // Middle-mouse drag (scroll+zoom like ScrollZoomBar)
    bool midDragging = false;
    float midDragStartZoom = 1.0f;
    float midDragAnchorFrac = 0.0f;
    float midDragAnchorPixelFrac = 0.0f;
    int   midDragStartX = 0;
    int   midDragStartY = 0;
};
