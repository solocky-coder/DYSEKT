#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class DysektProcessor;
class WaveformView;

class ActionPanel : public juce::Component
{
public:
    ActionPanel (DysektProcessor& p, WaveformView& wv);
    ~ActionPanel() override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;

    std::function<void()> onShortcutsToggle;

    void setBrowserActive    (bool v) { browserActive    = v; repaint(); }
    void setWaveActive       (bool v) { waveActive       = v; repaint(); }
    void setChromaticActive  (bool v) { chromaticActive  = v; repaint(); }
    bool isAddSliceModeActive() const { return addSliceBtn.getToggleState(); }

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    bool browserActive    = false;
    bool waveActive       = false;
    bool chromaticActive  = false;

    void updateToggleBtn (juce::ToggleButton& btn, bool active);

    juce::ToggleButton addSliceBtn  { "ADD"  };
    juce::ToggleButton midiSliceBtn { "MIDI" };
    juce::TextButton shortcutsBtn   { "?"    };
};