#pragma once
#include "../audio/SampleData.h"
#include "../audio/SliceManager.h"
#include "../PluginProcessor.h"
#include "WaveformCache.h"
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

class WaveformView : public juce::Component
{
public:
    WaveformView(DysektProcessor& p);
    ~WaveformView() override;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    void resized() override {}

    void setSliceDrawMode(bool v)       { sliceDrawMode = v; }
    bool isSliceDrawModeActive() const  { return sliceDrawMode; }

    void setTrimMode(bool t)            { trimMode = t; }
    bool isTrimDragging() const         { return trimDragging; }

    int getTrimIn() const               { return trimStart; }
    int getTrimOut() const              { return trimEnd; }
    void setTrimPoints(int in, int out) { trimStart = in; trimEnd = out; repaint(); }
    void enterTrimMode(int in, int out) { trimStart = in; trimEnd = out; trimMode = true; trimDragging = false; repaint(); }

    std::function<void(int,int)> onTrimApplied;
    std::function<void()> onTrimCancelled;
    std::function<void(const juce::File&)> onLoadRequest;

    void setSoftWaveform(bool soft) { softWaveform = soft; repaint(); }
    bool isSoftWaveform() const noexcept { return softWaveform; }

    bool altModeActive = false;
    bool shiftPreviewActive = false;
    std::vector<int> transientPreviewPositions;

    // These are needed for draggable slice markers
    enum DragMode { None, DragEdgeLeft, DragEdgeRight, DrawSlice, MoveSlice, DuplicateSlice,
                    TrimMarkerLeft, TrimMarkerRight, DragTrimIn, DragTrimOut };
    DragMode dragMode = None;

    enum class HoveredEdge { None, Left, Right };
    HoveredEdge hoveredEdge = HoveredEdge::None;

    int dragSliceIdx = -1;           // index of slice being dragged
    int hoveredSliceIdx = -1;        // index of slice currently hovered (for UI feedback)
    int dragInitialSample = 0;       // the sample position when the drag started
    int dragInitialValue = 0;        // the start/end value when the drag started

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

    int pixelToSample(int px) const;
    int sampleToPixel(int sample) const;
    ViewState buildViewState(const SampleData::SnapshotPtr& sampleSnap) const;
    void syncAltStateFromMods(const juce::ModifierKeys& mods);

    void drawWaveform(juce::Graphics& g);
    void drawSlices(juce::Graphics& g);
    void drawPlaybackCursors(juce::Graphics& g);
    void paintDrawSlicePreview(juce::Graphics& g);
    void paintLazyChopOverlay(juce::Graphics& g);
    void paintTransientMarkers(juce::Graphics& g);
    void paintTrimOverlay(juce::Graphics& g);

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
    int  trimInPoint   = 0;       // trim-in marker position in samples (DragTrimIn path)
    int  trimOutPoint  = 0;       // trim-out marker position in samples (DragTrimOut path)
    bool trimDragging  = false;   // true while user is actively dragging a trim handle
    int  trimStart     = 0;       // trim-in marker position in samples (enterTrimMode path)
    int  trimEnd       = 0;       // trim-out marker position in samples (enterTrimMode path)
    mutable ViewState cachedPaintViewState;   // valid only between paint() start and end
    mutable bool paintViewStateActive = false; // true only during paint(); guards cachedPaintViewState

    // Hit areas for trim mode buttons (updated each paint)
    juce::Rectangle<int> trimApplyBtnBounds;
    juce::Rectangle<int> trimResetBtnBounds;
    juce::Rectangle<int> trimCancelBtnBounds;

    static constexpr int kTrimMarkerHitTolerance = 6;   // px within which clicks hit a trim marker
    static constexpr int kMinTrimRegionSamples   = 64;  // minimum trim region in samples

    // ---- Slice-drawing mode state ----
    int drawStart = 0;
    int drawEnd = 0;
    bool drawStartedFromAlt = false;
    int addClickStart = -1; // ADD click mode: -1 = waiting for first click, >= 0 = waiting for second click

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
