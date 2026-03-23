#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DysektLookAndFeel.h"

class DysektProcessor;

// Other includes or forward declarations as needed

class SliceControlBar : public juce::Component
{
public:
    SliceControlBar(DysektProcessor& p);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // ... other declarations ...

    // Our new marker slider cell drawer
    void drawMarkerSliderCell(juce::Graphics& g, int x, int y,
                              int markerSample, int sampleNumFrames, bool locked, int& outWidth);
protected:
    // ... other protected/private members ...
    struct ParamCell
    {
        int x = 0, y = 0, w = 0, h = 0;
        uint32_t lockBit = 0;
        int fieldId = 0;
        float minVal = 0.0f, maxVal = 1.0f, step = 0.01f;
        bool isKnob = false;
        bool isMidiLearnable = false;
        float knobNorm = 0.0f;
        bool isMidiLearnBtn = false, isBoolean = false, isChoice = false, isReadOnly = false, isSetBpm = false;
    };
    std::vector<ParamCell> cells;

    // ... theme/processor accessors, etc. ...
    DysektProcessor& processor;
    juce::Rectangle<int> rootNoteArea;
    int activeDragCell = -1;
    bool draggingRootNote = false;
    float dragStartValue = 0.f;
    int dragStartY = 0;
    std::unique_ptr<juce::TextEditor> textEditor;

    // ... your other methods and members ...
};