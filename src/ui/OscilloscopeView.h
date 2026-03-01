#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class OscilloscopeView : public juce::Component
{
public:
    explicit OscilloscopeView (DysektProcessor& p);

    void paint (juce::Graphics& g) override;

private:
    DysektProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloscopeView)
};
