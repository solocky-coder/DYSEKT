#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixerMetering.h"

class DysektProcessor;

/**
 * MasterMixerStrip — master output channel strip.
 *
 * Controls:
 *  - Master Volume fader (APVTS masterVolume param)
 *  - Master Pan (APVTS defaultPan param)
 *  - Master VU Meter (L/R, fed from processor.masterPeakL/R)
 *  - Label "MASTER"
 */
class MasterMixerStrip : public juce::Component
{
public:
    explicit MasterMixerStrip (DysektProcessor& p);

    void setMeterLevels (float left, float right);

    void paint   (juce::Graphics& g) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;

private:
    void drawFader (juce::Graphics& g, const juce::Rectangle<int>& track, float norm) const;
    void drawKnob  (juce::Graphics& g, const juce::Rectangle<int>& bounds,
                    float norm, const juce::String& label) const;

    float getMasterVolNorm() const;
    float getMasterPanNorm() const;
    void  setMasterVol (float norm);
    void  setMasterPan (float norm);

    DysektProcessor& processor;

    juce::Rectangle<int> faderTrack;
    juce::Rectangle<int> panKnobBounds;
    juce::Rectangle<int> headerArea;
    MixerMetering        meter;

    // Drag state
    bool  draggingFader { false };
    bool  draggingPan   { false };
    int   dragStartY    { 0 };
    float dragStartVal  { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterMixerStrip)
};
