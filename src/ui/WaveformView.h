#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class WaveformView : public juce::Component
{
public:
    WaveformView();
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void setAddSliceActiveGetter(std::function<bool()> fn) { isAddSliceActive = std::move(fn); }

private:
    std::function<bool()> isAddSliceActive;

    // Update these for your system:
    int positionToSample(float x) const;
    void addSliceAtPosition(int sample);
};