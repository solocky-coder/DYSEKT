#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

class DysektProcessor;

/// Modal overlay panel displaying keyboard shortcuts and global preferences.
/// Shown via ⌘? or the "?" button in ActionPanel.
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

private:
    DysektProcessor& processor;

    struct ShortcutEntry    { juce::String keys, description; };
    struct ShortcutCategory { juce::String title; std::vector<ShortcutEntry> entries; };

    juce::TextButton closeBtn { "×" };
    juce::TextEditor searchBox;
    juce::Label      titleLabel;

    std::vector<ShortcutCategory> categories;
    juce::String currentFilter;

    // Trim-preference button hit rects (updated each paint)
    juce::Rectangle<int> trimAlwaysRect, trimNeverRect, trimLongRect;

    void buildShortcutData();
    void drawTrimPrefsSection (juce::Graphics& g, juce::Rectangle<int>& area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShortcutsPanel)
};
