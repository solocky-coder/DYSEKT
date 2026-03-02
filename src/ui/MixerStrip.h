#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixerMetering.h"

class DysektProcessor;

/**
 * MixerStrip — a vertical channel strip for a single sample slice.
 *
 * Controls:
 *  - Volume fader (-100 to +24 dB)
 *  - Pan knob (-1..+1)
 *  - Filter Cutoff knob (20-20000 Hz, log)
 *  - Filter Resonance knob (0-1)
 *  - Output Bus selector (0-15)
 *  - Mute / Solo buttons
 *  - VU Meter (L/R)
 *  - Channel number label
 *  - Lock indicator
 */
class MixerStrip : public juce::Component
{
public:
    explicit MixerStrip (DysektProcessor& p, int sliceIndex);

    void setSliceIndex (int idx);
    int  getSliceIndex() const { return sliceIdx; }

    void setSelected (bool sel);
    void setMeterLevels (float left, float right);

    void paint    (juce::Graphics& g) override;
    void resized  () override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;

private:
    // ── per-strip controls (JUCE component slots) ─────────────────────────
    struct KnobArea
    {
        juce::Rectangle<int> bounds;
        float value   { 0.0f }; // normalised 0-1
        float minVal  { 0.0f };
        float maxVal  { 1.0f };
        bool  isDragging { false };
        int   dragStartY { 0 };
        float dragStartVal { 0.0f };
    };

    enum KnobId { KnobVolume=0, KnobPan, KnobCutoff, KnobRes, KnobCount };

    void buildLayout();
    void drawKnob (juce::Graphics& g, const KnobArea& ka, const juce::String& label,
                   bool locked) const;
    void drawFader (juce::Graphics& g, const juce::Rectangle<int>& track,
                    float norm, bool locked) const;

    float getSliceValue (KnobId id) const;
    void  pushSliceParam (KnobId id, float nativeVal);
    float toNorm   (KnobId id, float native) const;
    float fromNorm (KnobId id, float norm)   const;
    float logFreqToNorm   (float hz)   const;
    float normToLogFreq   (float norm) const;

    DysektProcessor& processor;
    int sliceIdx { 0 };
    bool selected { false };

    std::array<KnobArea, KnobCount> knobs;
    juce::Rectangle<int> faderTrack;
    juce::Rectangle<int> muteBtn;
    juce::Rectangle<int> soloBtn;
    juce::Rectangle<int> outputBusArea;
    juce::Rectangle<int> channelLabelArea;
    MixerMetering meter;

    int activeDrag { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStrip)
};
