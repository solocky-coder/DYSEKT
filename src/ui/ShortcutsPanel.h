#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

/// Modal overlay panel displaying all keyboard shortcuts, organised by category.
/// Shown via ⌘? or the "?" button in ActionPanel.
/// Dismissed by pressing Escape or clicking the close button.
class ShortcutsPanel : public juce::Component
{
public:
    ShortcutsPanel();
    ~ShortcutsPanel() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;

    /// Called when the panel should be dismissed.
    std::function<void()> onDismiss;

private:
    struct ShortcutEntry
    {
        juce::String keys;
        juce::String description;
    };

    struct ShortcutCategory
    {
        juce::String title;
        std::vector<ShortcutEntry> entries;
    };

    juce::TextButton   closeBtn { "×" };
    juce::TextEditor   searchBox;
    juce::Label        titleLabel;

    std::vector<ShortcutCategory> categories;
    juce::String currentFilter;

    void buildShortcutData();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShortcutsPanel)
};
