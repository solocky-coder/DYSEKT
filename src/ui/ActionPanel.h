#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class DysektProcessor;
class WaveformView;
class AutoChopPanel;

class ActionPanel : public juce::Component
{
public:
    ActionPanel (DysektProcessor& p, WaveformView& wv);
    ~ActionPanel() override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void toggleAutoChop();
    bool isAutoChopOpen() const { return autoChopPanel != nullptr; }

    // Callbacks wired up by DysektEditor
    std::function<void()> onKeysToggle;
    std::function<void()> onBrowserToggle;
    std::function<void()> onWaveToggle;
    std::function<void()> onChromaticToggle;

    void setKeysActive       (bool v) { keysActive       = v; repaint(); }
    void setBrowserActive    (bool v) { browserActive    = v; repaint(); }
    void setWaveActive       (bool v) { waveActive       = v; repaint(); }
    void setChromaticActive  (bool v) { chromaticActive  = v; repaint(); }

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    bool keysActive       = false;
    bool browserActive    = false;
    bool waveActive       = false;
    bool chromaticActive  = false;

    void updateToggleBtn (juce::TextButton& btn, bool active);
    void updateMidiButtonAppearance (bool active);
    void updateSnapButtonAppearance (bool active);

    juce::TextButton addSliceBtn   { "ADD" };
    juce::TextButton lazyChopBtn   { "LAZY" };
    juce::TextButton dupBtn        { "COPY" };
    juce::TextButton splitBtn      { "AUTO" };
    juce::TextButton deleteBtn     { "DEL" };
    juce::TextButton snapBtn       { "ZX" };
    juce::TextButton midiSelectBtn { "FM" };
    juce::TextButton keysBtn       { "KEYS" };
    juce::TextButton browserBtn    { "FILES" };
    juce::TextButton waveBtn       { "WAVE" };
    juce::TextButton chromaticBtn  { "CHRO" };

    std::unique_ptr<AutoChopPanel> autoChopPanel;
};
