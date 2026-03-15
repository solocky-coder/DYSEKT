#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "WaveformCache.h"
#include "SampleData.h"

// Forward declaration
class DysektProcessor;

class WaveformView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit WaveformView(DysektProcessor& p);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void modifierKeysChanged(const juce::ModifierKeys& mods) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void rebuildCacheIfNeeded();

    // ---- Rendering & Utility ---
    void drawWaveform(juce::Graphics& g);
    void drawSlices(juce::Graphics& g);
    void drawPlaybackCursors(juce::Graphics& g);

    int pixelToSample(int px) const;
    int sampleToPixel(int sample) const;

    // --- Trim mode API ---
    void enterTrimMode(int start, int end);
    void setTrimPoints(int inPt, int outPt);
    void exitTrimMode();
    void getTrimBounds(int& outStart, int& outEnd) const;
    void setTrimMode(bool active);
    void resetTrim();

    // Core state for other logic
    void setSliceDrawMode(bool active);
    bool hasActiveSlicePreview() const noexcept;
    bool getActiveSlicePreview(int& sliceIdx, int& startSample, int& endSample) const;
    bool getLinkedSlicePreview(int& sliceIdx, int& startSample, int& endSample) const;
    bool isInteracting() const noexcept;

    // Callbacks
    std::function<void(const juce::File&)> onLoadRequest;
    std::function<void(int, int)> onTrimApplied;
    std::function<void()> onTrimCancelled;

private:
    // Copy of previously posted headers, but ViewState public for use
    struct ViewState
    {
        int numFrames = 0;
        int visibleStart = 0;
        int visibleLen = 0;
        int width = 0;
        float samplesPerPixel = 1.0f;
        bool valid = false;
    };

    ViewState buildViewState(const SampleData::SnapshotPtr& sampleSnap) const;

    // Internal state
    DysektProcessor& processor;
    WaveformCache cache;
    WaveformCache::CacheKey prevCacheKey;

    // Draw state and cache
    mutable ViewState cachedPaintViewState;
    mutable bool paintViewStateActive = false;

    // Drag, preview & editing state
    bool sliceDrawMode = false;
    bool trimMode = false;
    int trimInPoint = 0, trimOutPoint = 0, trimStart = 0, trimEnd = 0;
    bool trimDragging = false;

    enum DragMode { None, DragEdgeLeft, DragEdgeRight, MoveSlice };
    DragMode dragMode = None;
    int dragSliceIdx = -1;
    int dragPreviewStart = 0, dragPreviewEnd = 0;
    int linkedSliceIdx = -1;
    int linkedPreviewStart = 0, linkedPreviewEnd = 0;
    bool midDragging = false;
    bool shiftPreviewActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};