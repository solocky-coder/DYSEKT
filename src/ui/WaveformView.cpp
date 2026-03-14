#include "WaveformView.h"
#include "DysektLookAndFeel.h"

// ... constructor and other code ...

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    if (isAddSliceActive && isAddSliceActive())
    {
        int markerSample = positionToSample((float)e.x);
        addSliceAtPosition(markerSample);
        // Optionally: turn Add mode OFF after placing if you want
        // (do this by exposing a callback to ActionPanel)
        return;
    }
    // ...other mouse behavior...
}

// Update these for your system:
int WaveformView::positionToSample(float x) const
{
    // Example translation: scale x pos to sample count
    int w = getWidth();
    int totalSamples = 100000; // Replace this with your sample count
    if (w > 0)
        return (int)(x * totalSamples / w);
    return 0;
}

void WaveformView::addSliceAtPosition(int sample)
{
    // Call your slice manager, or emit as appropriate for your engine
    // Example:
    // processor.sliceManager.addSlice(sample);
    // (Replace with your real slice-adding code.)
}