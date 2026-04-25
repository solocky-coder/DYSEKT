#pragma once
// =============================================================================
//  SfzModulePanel.h  —  SFZ / SF2 instrument strip
// =============================================================================
//  Stacks below the waveform / pad-grid frame when sfzModuleOpen == true.
//  Shows: instrument name | file load button | volume knob | transpose knob
//         | MIDI channel selector | VU meters | status pill
// =============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include "KeysPanel.h"

class DysektProcessor;

class SfzModulePanel : public juce::Component,
                       public juce::Timer,
                       public juce::FileDragAndDropTarget
{
public:
    explicit SfzModulePanel (DysektProcessor& p);
    ~SfzModulePanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Called by editor when the panel becomes visible
    void panelDidShow();

    // Callback wired by editor — triggers file chooser
    std::function<void()> onLoadRequest;

    // Keyboard sub-component
    KeysPanel keysPanel;

private:
    // ── Layout zones (computed in resized) ────────────────────────────────────
    juce::Rectangle<int> nameZone, loadBtnZone, volZone, transZone,
                          chZone, meterZone, statusZone;

    // ── Drag state for volume / transpose knobs ───────────────────────────────
    enum class ActiveKnob { None, Volume, Transpose };
    ActiveKnob activeKnob { ActiveKnob::None };
    int        dragStartY  { 0 };
    float      dragStartVal { 0.f };

    // ── VU meter decay ────────────────────────────────────────────────────────
    float meterL { 0.f }, meterR { 0.f };
    float holdL  { 0.f }, holdR  { 0.f };
    static constexpr float kHoldDecay = 0.93f;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void openFileChooser();
    void drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                   float normalised, const juce::String& label,
                   const juce::String& valueStr) const;
    void drawMeter (juce::Graphics& g) const;

    float volToNorm    (float linear) const;  ///< linear 0..2 → 0..1
    float normToVol    (float n)      const;  ///< 0..1 → linear 0..2
    float transToNorm  (int semi)     const;  ///< -24..24 → 0..1
    int   normToTrans  (float n)      const;  ///< 0..1 → -24..24

    juce::Rectangle<int> loadBtnHitbox() const;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&,
                           const juce::MouseWheelDetails&) override;

    std::unique_ptr<juce::FileChooser> chooser;

    // Zone parsers
    static std::vector<KeysPanel::Keyzone> parseSf2Zones  (const juce::File& f);
    void reloadZones (const juce::File& f);

    DysektProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SfzModulePanel)
};
