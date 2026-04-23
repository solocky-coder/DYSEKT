#pragma once
// =============================================================================
//  SfzDropdownPanel.h  —  Kontakt-style collapsible SFZ / SF2 strip
// =============================================================================
//  A self-animating component that lives at a fixed position in the editor.
//  Collapsed: shows only a 36-px header strip (chevron, load btn, vol/trans
//  knobs, MIDI channel selector, VU meter, status pill).
//  Expanded:  animates open to reveal a KeysPanel above the strip.
//
//  Height is animated each timer tick; the parent's resized() is called so
//  the editor can reposition siblings automatically.
// =============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "KeysPanel.h"

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

    /// Animated height — read by the parent's resized() to size this component.
    int  getAnimatedHeight() const { return juce::roundToInt (animH); }

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kStripH    = 36;   ///< Collapsed height (header strip only)
    static constexpr int kExpandedH = 200;  ///< Fully expanded height

    // ── Keyboard sub-component (public so editor can query if needed) ─────────
    KeysPanel keysPanel;

private:
    // ── Header-strip drawing ──────────────────────────────────────────────────
    void drawHeaderStrip (juce::Graphics& g) const;
    void drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                   float normalised, const juce::String& label,
                   const juce::String& valueStr) const;
    void drawMeter (juce::Graphics& g) const;

    // ── Layout zones (computed in resized) ────────────────────────────────────
    juce::Rectangle<int> chevronZone, nameZone, loadBtnZone,
                          volZone, transZone, chZone,
                          meterZone, statusZone;

    // ── Animation state ───────────────────────────────────────────────────────
    float animH          { (float) kStripH };   ///< Current animated height (px)
    bool  expandedTarget { false };              ///< True when panel should be open

    // ── Drag state for volume / transpose knobs ───────────────────────────────
    enum class ActiveKnob { None, Volume, Transpose };
    ActiveKnob activeKnob  { ActiveKnob::None };
    int        dragStartY  { 0 };
    float      dragStartVal{ 0.f };

    // ── VU meter ──────────────────────────────────────────────────────────────
    float meterL { 0.f }, meterR { 0.f };
    float holdL  { 0.f }, holdR  { 0.f };
    static constexpr float kHoldDecay = 0.93f;

    // ── File chooser ──────────────────────────────────────────────────────────
    void openFileChooser();
    std::unique_ptr<juce::FileChooser> chooser;

    // ── Value mapping helpers ─────────────────────────────────────────────────
    float volToNorm   (float linear) const;  ///< linear 0..2  → 0..1
    float normToVol   (float n)      const;  ///< 0..1          → linear 0..2
    float transToNorm (int semi)     const;  ///< semitones -24..24 → 0..1
    int   normToTrans (float n)      const;  ///< 0..1          → -24..24

    // ── Zone parsers ──────────────────────────────────────────────────────────
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
