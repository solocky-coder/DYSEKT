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
    void paintOverChildren (juce::Graphics& g) override;

    /// Invoked when the user clicks the "?" button to open the shortcuts panel.
    std::function<void()> onShortcutsToggle;

    // Callbacks wired up by DysektEditor
    void setBrowserActive    (bool v) { browserActive    = v; repaint(); }
    void setWaveActive       (bool v) { waveActive       = v; repaint(); }
    void setChromaticActive  (bool v) { chromaticActive  = v; repaint(); }

    // Callback wired by PluginEditor
    std::function<void()> onTrimToggle;

    /** Lock/unlock all buttons except TRIM while trim mode is active. */
    void setTrimLocked (bool locked);

private:
    DysektProcessor& processor;
    WaveformView&    waveformView;

    bool browserActive    = false;
    bool waveActive       = false;
    bool chromaticActive  = false;

    void updateToggleBtn (juce::TextButton& btn, bool active);
    void updateMidiButtonAppearance (bool active);
    void toggleTrimMode();

    juce::TextButton addSliceBtn    { "ADD"  };
    juce::TextButton lazyChopBtn    { "LAZY" };
    juce::TextButton trimBtn        { "TRIM" };
    juce::TextButton shortcutsBtn   { "?"    };

    // Kept as members for state sync with HeaderBar — not visible in action panel

    // snapBtn removed — snap-to-zero-crossing is hardcoded always-on
    // dupBtn / splitBtn / deleteBtn removed in earlier fixes

    std::unique_ptr<AutoChopPanel> autoChopPanel;
    // Trim is fully managed by PluginEditor — no trimDialog here
};
