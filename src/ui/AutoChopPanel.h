#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;
class WaveformView;

class AutoChopPanel : public juce::Component
{
public:
    AutoChopPanel (DysektProcessor& p, WaveformView& wv);
    ~AutoChopPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void updatePreview();

    DysektProcessor& processor;
    WaveformView& waveformView;

    juce::Slider sensitivitySlider;
    juce::ComboBox modeCombo;
    juce::TextEditor divisionsEditor;
    juce::TextButton splitEqualBtn { "SPLIT EQUAL" };
    juce::TextButton detectBtn     { "SPLIT TRANSIENTS" };
    juce::TextButton cancelBtn     { "CANCEL" };
};
