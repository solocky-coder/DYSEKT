#pragma once
// =============================================================================
//  SfzDropdownPanel.h  —  SF2 / SFZ instrument strip with inline file browser
// =============================================================================
//  Header strip layout (left → right):
//    [< Preset Name  📁 >] [TRN] [FINE] [REV] [CHO] [PAN] [VOL] [METER]
//
//  The preset picker doubles as the file browser entry-point:
//    • When a file IS loaded   — scrolls through SF2 presets as before.
//                                Small 📁 icon on right edge opens browser.
//    • When NO file is loaded  — clicking anywhere on the picker opens the
//                                inline browser.
//    • Mouse-wheel on the picker scrolls presets (when loaded).
//
//  The inline browser is a full-panel overlay (below the header strip) with:
//    • Breadcrumb path bar + ↑ up-button
//    • Scrollable list: directories first, then .sfz / .sf2 files
//    • Single-click selects; double-click enters directory or loads file
//    • Pressing Escape / clicking the 📁 icon again closes the browser
// =============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "KeysPanel.h"
#include "../audio/SfzPlayer.h"

class DysektProcessor;

// =============================================================================
//  SfzFileBrowser — self-contained inline directory browser component
// =============================================================================
class SfzFileBrowser : public juce::Component,
                       public juce::ListBoxModel,
                       private juce::Timer
{
public:
    /** Called when the user double-clicks a .sfz or .sf2 file. */
    std::function<void (const juce::File&)> onFileChosen;
    /** Called after a new SF2/SFZ file has been accepted (any path). */
    std::function<void (const juce::File&)> onFileLoaded;

    /** Called when the user explicitly closes the browser (Esc / icon click). */
    std::function<void()> onDismiss;

    SfzFileBrowser();
    ~SfzFileBrowser() override;

    // juce::Component overrides
    void paint   (juce::Graphics&) override;
    void resized () override;
    void mouseDown  (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override { return false; /* handled via grabKeyboardFocus */ }

    // Open to a specific root directory (call before making visible)
    void setRootDirectory (const juce::File& dir);
    void showDrives   ();                        ///< Show the drive/volume picker list
    bool hasNavigated () const { return navigated; } ///< True after first navigation
    juce::File getCurrentDirectory() const { return currentDir; }

    // ── ListBoxModel ──────────────────────────────────────────────────────────
    int  getNumRows()                                                  override;
    void paintListBoxItem (int row, juce::Graphics& g,
                           int w, int h, bool selected)               override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&)  override;
    void listBoxItemClicked       (int row, const juce::MouseEvent&)  override;
    juce::String getTooltipForRow (int row)                           override;

private:
    void   navigateTo      (const juce::File& dir);
    void   navigateUp      ();
    void   navigateToRoots ();   ///< Jump to filesystem volume list (external drives)
    void   loadRow      (int row);
    juce::File fileForRow (int row) const;
    bool   isDirectory  (int row) const;

    // Async directory scanning
    void timerCallback() override;
    void rebuildList();

    juce::File currentDir;

    // Rows: sorted directories first, then matching files
    juce::Array<juce::File> rows;

    juce::ListBox list;

    // Breadcrumb / up-button zone (computed in resized)
    juce::Rectangle<int> breadcrumbZone;
    juce::Rectangle<int> upBtnZone;
    juce::Rectangle<int> driveBtnZone;   // ⏏ button — navigate to filesystem roots
    bool                 atVirtualRoot { false }; ///< true when showing the drive-list view
    bool                 navigated     { false }; ///< true after the first navigateTo/navigateToRoots call

    static constexpr int kBreadcrumbH = 22;
    static constexpr int kRowH        = 18;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SfzFileBrowser)
};

// =============================================================================
//  SfzDropdownPanel
// =============================================================================
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
    void panelDidShow();

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kStripH  = 36;
    static constexpr int kAdsrH   = 34;   ///< height of the ADSR knob row

    // ── Keyboard sub-component ────────────────────────────────────────────────
    KeysPanel keysPanel;

private:
    // ── Header-strip drawing ──────────────────────────────────────────────────
    void drawHeaderStrip (juce::Graphics& g) const;
    void drawAdsrStrip   (juce::Graphics& g) const;
    void drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                   float normalised, const juce::String& label,
                   const juce::String& valueStr) const;
    void drawMeter (juce::Graphics& g) const;
    void drawPresetPicker (juce::Graphics& g) const;

    // ── Layout zones (computed in resized) ────────────────────────────────────
    juce::Rectangle<int> nameZone,
                          volZone, transZone,
                          panZone, fineZone,
                          rvMixZone, rvSizeZone,
                          meterZone;

    // ADSR knob zones (second row, below header strip)
    juce::Rectangle<int> adsrAtkZone, adsrDecZone, adsrSusZone, adsrRelZone;

    // Sub-zones inside nameZone
    juce::Rectangle<int> presetDecBtn, presetLabel, presetIncBtn, folderIconZone;

    // ── Drag state for knobs ──────────────────────────────────────────────────
    enum class ActiveKnob { None, Volume, Transpose, Pan, FineTune, ReverbMix, ReverbSize,
                            AdsrAttack, AdsrDecay, AdsrSustain, AdsrRelease };
    ActiveKnob activeKnob  { ActiveKnob::None };
    int        dragStartY  { 0 };
    float      dragStartVal{ 0.f };

    // ── VU meter ──────────────────────────────────────────────────────────────
    float meterL { 0.f }, meterR { 0.f };
    float holdL  { 0.f }, holdR  { 0.f };
    static constexpr float kHoldDecay = 0.93f;

    // ── Cached preset list ────────────────────────────────────────────────────
    std::vector<Sf2PresetInfo> presetList;

    // ── Inline file browser ───────────────────────────────────────────────────
    SfzFileBrowser fileBrowser;
    bool           browserOpen { false };

    void openBrowser();
    void closeBrowser();
    void onFileChosen (const juce::File& f);

    // ── Value mapping helpers ─────────────────────────────────────────────────
    float volToNorm    (float linear) const;
    float normToVol    (float n)      const;
    float transToNorm  (int semi)     const;
    int   normToTrans  (float n)      const;
    float panToNorm    (float p)      const;
    float normToPan    (float n)      const;
    float fineToNorm   (float cents)  const;
    float normToFine   (float n)      const;

    // ── Preset navigation ─────────────────────────────────────────────────────
    void selectPreset (int delta);

    // ── Zone parsers ──────────────────────────────────────────────────────────
    static std::vector<KeysPanel::Keyzone> parseSfzZones (const juce::File& f);
    static std::vector<KeysPanel::Keyzone> parseSf2Zones (const juce::File& f,
                                                            int targetBank   = 0,
                                                            int targetPreset = 0);
    void reloadZones (const juce::File& f);
    void writeSfzZoneChange (const juce::File& f, int rowIndex,
                              const KeysPanel::Keyzone& updated);

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
