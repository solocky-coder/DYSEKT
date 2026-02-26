#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../MidiLearnManager.h"

class IntersectProcessor;

class SliceControlBar : public juce::Component
{
public:
    explicit SliceControlBar (IntersectProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    struct ParamCell
    {
        int x, y, w, h;
        uint32_t lockBit;
        int fieldId;       // SliceParamField enum value
        float minVal, maxVal, step;
        bool isBoolean;    // for ping-pong toggle
        bool isChoice;     // for algorithm popup
        bool isReadOnly = false;
        bool isSetBpm   = false;
        bool isMidiLearnBtn  = false;  // START / END boundary buttons
        bool isKnob          = false;  // numeric rotary
        bool isMidiLearnable = false;  // right-click → Learn menu
        float knobNorm       = 0.0f;   // 0-1 position for knob arc
    };

    std::vector<ParamCell> cells;

    void drawParamCell (juce::Graphics& g, int x, int y, const juce::String& label,
                        const juce::String& value, bool locked, uint32_t lockBit,
                        int fieldId, float minVal, float maxVal, float step,
                        bool isBoolean, bool isChoice, int& outWidth);

    // Rotary knob cell — used for all numeric parameters
    void drawKnobCell (juce::Graphics& g, int x, int y,
                       const juce::String& label, const juce::String& valueText,
                       float normVal, bool locked, uint32_t lockBit,
                       int fieldId, float minVal, float maxVal, float step,
                       int& outWidth);

    // START / END slice boundary MIDI Learn buttons
    void drawMidiLearnCell (juce::Graphics& g, int x, int y,
                            const juce::String& label, int fieldId, int& outWidth);

    void drawKnob (juce::Graphics& g, int cx, int cy, int r,
                   float normVal, bool locked, bool armed, bool mapped);

    void drawLockIcon (juce::Graphics& g, int x, int y, bool locked);
    void showTextEditor (const ParamCell& cell, float currentValue);
    void showSetBpmPopup();
    void showMidiLearnMenu (int fieldId);

    // Per-field helpers
    float getCurrentValue (int fieldId) const;
    float toNorm   (int fieldId, float nativeVal) const;
    float fromNorm (int fieldId, float norm)      const;

    static constexpr int kKnobR = 11;  // knob radius (px)

    IntersectProcessor& processor;

    // Drag state
    int activeDragCell = -1;
    float dragStartValue = 0.0f;
    int dragStartY = 0;

    // Root note cell (editable when no slices exist)
    juce::Rectangle<int> rootNoteArea;
    bool draggingRootNote = false;

    // Text editor overlay
    std::unique_ptr<juce::TextEditor> textEditor;
};
