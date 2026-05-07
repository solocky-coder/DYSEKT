#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>   // required for WebBrowserComponent
#include <functional>
#include <vector>

class DysektProcessor;

/// Modal overlay panel displaying keyboard shortcuts, global preferences,
/// and an embedded PDF user manual viewer.
///
/// The panel has two tabs:
///   "Settings & Shortcuts"  — the existing preferences / shortcut list
///   "User Manual"           — WebBrowserComponent rendering the bundled PDF
///
/// Shown via ? or the "?" button in ActionPanel.
/// Dismissed by pressing Escape or clicking the close button.
class ShortcutsPanel : public juce::Component
{
public:
    explicit ShortcutsPanel (DysektProcessor& proc);
    ~ShortcutsPanel() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;

    /// Called when the panel should be dismissed.
    std::function<void()> onDismiss;

    /// Called when the user clicks "Theme Editor" — opens ThemeEditorPanel.
    std::function<void()> onThemeRequest;

    /// Called when the user selects an interface mode.
    /// Argument: 0 = Waveform View, 1 = Pad Grid.
    std::function<void(int)> onUiModeChanged;

    /// Set this to the current UI mode before making the panel visible,
    /// so the correct radio button appears selected.
    int currentUiMode = 0;

private:
    DysektProcessor& processor;

    // ── Tab state ─────────────────────────────────────────────────────────────
    enum class Tab { Settings, Manual };
    Tab activeTab = Tab::Settings;

    juce::TextButton tabSettingsBtn { "Settings & Shortcuts" };
    juce::TextButton tabManualBtn   { "User Manual"          };

    // ── Manual viewer ─────────────────────────────────────────────────────────
#if JUCE_WEB_BROWSER
    std::unique_ptr<juce::WebBrowserComponent> manualViewer;
#endif
    bool manualLoaded = false;

    /// Returns the URL to load in the manual viewer.
    /// Looks for DYSEKT_User_Manual.pdf next to the VST3, then in
    /// %AppData%\DYSEKT\, and falls back to the GitHub releases page.
    juce::URL findManualPdf() const;

    /// Creates manualViewer (if needed) and navigates it to findManualPdf().
    void setupManualViewer();

    // ── Settings-tab widgets ──────────────────────────────────────────────────
    struct ShortcutEntry    { juce::String keys, description; };
    struct ShortcutCategory { juce::String title; std::vector<ShortcutEntry> entries; };

    juce::TextButton closeBtn     { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };
    juce::TextButton themeBtn     { "Theme Editor..." };
    juce::TextButton scaleDownBtn { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) };
    juce::TextButton scaleUpBtn   { "+" };
    juce::Label      scaleLcd;
    juce::TextEditor searchBox;
    juce::Label      titleLabel;

    std::vector<ShortcutCategory> categories;
    juce::String currentFilter;

    juce::Rectangle<int> trimAlwaysRect, trimNeverRect, trimLongRect;
    juce::Rectangle<int> uiModeWaveRect, uiModeGridRect;

    void buildShortcutData();
    void drawTrimPrefsSection (juce::Graphics& g, juce::Rectangle<int>& area);
    void drawScaleSection     (juce::Graphics& g, juce::Rectangle<int>& area);
    void drawInterfaceSection (juce::Graphics& g, juce::Rectangle<int>& area);
    void updateScaleLcd();
    void applyTabLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShortcutsPanel)
};
