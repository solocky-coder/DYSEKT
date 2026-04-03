#include "SliceControlBar.h"
#include "../PluginProcessor.h"
#include <cmath>

// === Undo action for marker ratio changes ===
class MarkerRatioUndoAction : public juce::UndoableAction
{
public:
    MarkerRatioUndoAction (DysektProcessor& p, int sliceIdx, float beforeRatio, float afterRatio)
        : processor (p), sliceIndex (sliceIdx), beforeRatio (beforeRatio), afterRatio (afterRatio)
    {
    }

    bool perform() override
    {
        if (sliceIndex >= 0 && sliceIndex < processor.sliceManager.getNumSlices())
        {
            processor.sliceManager.getSlice (sliceIndex).markerRatio = afterRatio;
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (sliceIndex >= 0 && sliceIndex < processor.sliceManager.getNumSlices())
        {
            processor.sliceManager.getSlice (sliceIndex).markerRatio = beforeRatio;
            return true;
        }
        return false;
    }

private:
    DysektProcessor& processor;
    int sliceIndex;
    float beforeRatio, afterRatio;
};

SliceControlBar::SliceControlBar (DysektProcessor& p)
    : processor (p)
{
    setOpaque (false);
    startTimer (50);  // ~20 Hz pulse for MIDI learn indicators
}

void SliceControlBar::setSelectedSlice (int sliceIndex)
{
    currentSliceIndex = sliceIndex;
    repaint();
}

void SliceControlBar::paint (juce::Graphics& g)
{
    // Draw background
    g.fillAll (juce::Colours::black);

    // Get selected slice
    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel < 0 || sel >= processor.sliceManager.getNumSlices())
        return;

    const auto& slice = processor.sliceManager.getSlice (sel);
    if (! slice.active)
        return;

    const int totalFrames = processor.sampleData.getNumFrames();
    if (totalFrames <= 1)
        return;

    // Draw marker ratio slider
    int x = 10;
    int y = 10;
    int w = getWidth() - 20;
    int h = 30;

    // Background bar
    g.setColour (juce::Colours::darkgrey);
    g.fillRect (x, y, w, h);

    // Marker position
    int markerX = x + (int)(slice.markerRatio * w);
    g.setColour (juce::Colours::yellow);
    g.fillRect (markerX - 3, y, 6, h);

    // Label
    g.setColour (juce::Colours::white);
    g.setFont (12.0f);
    juce::String label = juce::String::formatted ("Marker: %.1f%%", slice.markerRatio * 100.0f);
    g.drawText (label, x, y, w, h, juce::Justification::centred);
}

void SliceControlBar::resized()
{
}

void SliceControlBar::mouseDown (const juce::MouseEvent& e)
{
    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel < 0 || sel >= processor.sliceManager.getNumSlices())
        return;

    auto& slice = processor.sliceManager.getSlice (sel);
    
    // Check if click is in marker slider area
    int sliderX = 10;
    int sliderW = getWidth() - 20;
    
    if (e.getMouseDownY() >= 10 && e.getMouseDownY() < 40 &&
        e.getMouseDownX() >= sliderX && e.getMouseDownX() < (sliderX + sliderW))
    {
        // Capture starting ratio for undo
        markerDragStartRatio = slice.markerRatio;
        currentSliceIndex = sel;
        activeDragCell = 0;  // Mark as dragging marker
    }
}

void SliceControlBar::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDragCell != 0)
        return;

    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel < 0 || sel >= processor.sliceManager.getNumSlices())
        return;

    auto& slice = processor.sliceManager.getSlice (sel);
    int sliderX = 10;
    int sliderW = getWidth() - 20;

    // Calculate new ratio from mouse position
    float newRatio = (float)(e.getMouseDownX() - sliderX) / (float)sliderW;
    newRatio = juce::jlimit (0.0f, 1.0f, newRatio);

    slice.markerRatio = newRatio;
    processor.markUiDirty();
    repaint();
}

void SliceControlBar::mouseUp (const juce::MouseEvent& e)
{
    if (activeDragCell != 0)
        return;

    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel < 0 || sel >= processor.sliceManager.getNumSlices())
        return;

    auto& slice = processor.sliceManager.getSlice (sel);

    // Post undo action if ratio changed
    if (std::abs (slice.markerRatio - markerDragStartRatio) > 0.0001f)
    {
        processor.undoMgr.beginNewTransaction();
        processor.undoMgr.perform (new MarkerRatioUndoAction (
            processor, sel, markerDragStartRatio, slice.markerRatio));
    }

    activeDragCell = -1;
    repaint();
}

void SliceControlBar::mouseDoubleClick (const juce::MouseEvent& e)
{
}

void SliceControlBar::updateMidiLearnPulse()
{
    // Pulse animation for MIDI learn indicators
}

void SliceControlBar::timerCallback()
{
    pulsePhase += 0.05f;
    if (pulsePhase > 1.0f)
        pulsePhase -= 1.0f;
}

void SliceControlBar::drawParamCell (juce::Graphics& g, int x, int y,
                                      const juce::String& label, const juce::String& value,
                                      bool locked, uint32_t lockBit, int fieldId,
                                      float minVal, float maxVal, float step,
                                      bool isBoolean, bool isChoice, int& outWidth)
{
}

void SliceControlBar::drawKnobCell (juce::Graphics& g, int x, int y,
                                     const juce::String& label, const juce::String& valueText,
                                     float normVal, bool locked, uint32_t lockBit,
                                     int fieldId, float minVal, float maxVal, float step,
                                     int& outWidth)
{
}

void SliceControlBar::drawMarkerSliderCell (juce::Graphics& g, int x, int y,
                                             int sampleVal, int totalFrames, int& outWidth)
{
}

void SliceControlBar::drawPanSliderCell (juce::Graphics& g, int x, int y,
                                          float panValue, bool locked, int& outWidth)
{
}

void SliceControlBar::drawMidiLearnCell (juce::Graphics& g, int x, int y,
                                          const juce::String& label, int fieldId, int& outWidth)
{
}

void SliceControlBar::drawKnob (juce::Graphics& g, int cx, int cy, int r,
                                 float normVal, bool locked, bool armed, bool mapped,
                                 juce::Colour tintOverride)
{
}

void SliceControlBar::drawLockIcon (juce::Graphics& g, int x, int y, bool locked)
{
}

void SliceControlBar::showTextEditor (const ParamCell& cell, float currentValue)
{
}

void SliceControlBar::showMidiLearnMenu (int fieldId, juce::Point<int> screenPos)
{
}

float SliceControlBar::getCurrentValue (int fieldId) const
{
    return 0.0f;
}

float SliceControlBar::toNorm (int fieldId, float nativeVal) const
{
    return 0.0f;
}

float SliceControlBar::fromNorm (int fieldId, float norm) const
{
    return 0.0f;
}