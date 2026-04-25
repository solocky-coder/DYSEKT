#pragma once
// =============================================================================
//  SfzDropdownPanel.h  —  SF2 instrument strip (FluidSynth backend)
// =============================================================================
//  Occupies the full slot area assigned by the editor.  Always shows the
//  36-px header strip at the bottom with the KeysPanel filling the space
//  above it.
//
//  Header strip layout (left → right):
//    [LOAD] [< B:n  Preset Name >] [VOL] [TRN] [CH] … [STATUS] [METER]
//
//  The preset picker in the nameZone lets the user scroll through every preset
//  in the loaded SF2.  Bank and preset number are shown as a small label above
//  the preset name.  Left arrow = prev preset, right arrow = next preset.
// =============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "KeysPanel.h"
#include "../audio/SfzPlayer.h"

class DysektProcessor;

class SfzDropdownPanel : public juce::Component,
                         public juce::Timer,
                         public juce::FileDragAndDropTarget
{
public:
    explicit SfzDropdownPanel (DysektProcessor& p);
    ~SfzDropdownPanel() override;

    // ── Core overrides ────────────────────────────────────────────────────────
    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

    // ── FileDragAndDropTarget ─────────────────────────────────────────────────
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // ── Public API ────────────────────────────────────────────────────────────
    /// Call when the panel first becomes visible so zones / state are refreshed.
    void panelDidShow();

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kStripH = 36;   ///< Height of the header strip

    // ── Keyboard sub-component ────────────────────────────────────────────────
    KeysPanel keysPanel;

private:
    // ── Header-strip drawing ──────────────────────────────────────────────────
    void drawHeaderStrip (juce::Graphics& g) const;
    void drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                   float normalised, const juce::String& label,
                   const juce::String& valueStr) const;
    void drawMeter (juce::Graphics& g) const;
    void drawPresetPicker (juce::Graphics& g) const;

    // ── Layout zones (computed in resized) ────────────────────────────────────
    juce::Rectangle<int> nameZone, loadBtnZone,
                          volZone, transZone, chZone,
                          meterZone, statusZone;

    // Sub-zones inside nameZone for the preset picker
    juce::Rectangle<int> presetDecBtn, presetLabel, presetIncBtn;

    // ── Drag state for volume / transpose knobs ───────────────────────────────
    enum class ActiveKnob { None, Volume, Transpose };
    ActiveKnob activeKnob  { ActiveKnob::None };
    int        dragStartY  { 0 };
    float      dragStartVal{ 0.f };

    // ── VU meter ──────────────────────────────────────────────────────────────
    float meterL { 0.f }, meterR { 0.f };
    float holdL  { 0.f }, holdR  { 0.f };
    static constexpr float kHoldDecay = 0.93f;

    // ── Cached preset list (refreshed from SfzPlayer every timer tick) ────────
    std::vector<Sf2PresetInfo> presetList;

    // ── File chooser ──────────────────────────────────────────────────────────
    void openFileChooser();
    std::unique_ptr<juce::FileChooser> chooser;

    // ── Value mapping helpers ─────────────────────────────────────────────────
    float volToNorm   (float linear) const;
    float normToVol   (float n)      const;
    float transToNorm (int semi)     const;
    int   normToTrans (float n)      const;

    // ── Preset navigation ─────────────────────────────────────────────────────
    void selectPreset (int delta);   ///< +1 = next, -1 = prev

    // ── Zone parsers (for the KeysPanel highlight visualisation) ─────────────
    static std::vector<KeysPanel::Keyzone> parseSfzZones (const juce::File& f);
    static std::vector<KeysPanel::Keyzone> parseSf2Zones (const juce::File& f);
    void reloadZones (const juce::File& f);

    // ── Mouse events ──────────────────────────────────────────────────────────
    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&,
                           const juce::MouseWheelDetails&) override;

    DysektProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SfzDropdownPanel)
};
