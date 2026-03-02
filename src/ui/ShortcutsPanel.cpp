#include "ShortcutsPanel.h"
#include "DysektLookAndFeel.h"

ShortcutsPanel::ShortcutsPanel()
{
    buildShortcutData();

    titleLabel.setText ("Keyboard Shortcuts", juce::dontSendNotification);
    titleLabel.setFont (DysektLookAndFeel::makeFont (15.0f, true));
    titleLabel.setColour (juce::Label::textColourId, getTheme().foreground);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    closeBtn.setColour (juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    closeBtn.setColour (juce::TextButton::textColourOffId, getTheme().foreground.withAlpha (0.75f));
    closeBtn.onClick = [this] { if (onDismiss) onDismiss(); };
    addAndMakeVisible (closeBtn);

    searchBox.setTextToShowWhenEmpty ("Search shortcuts...", getTheme().foreground.withAlpha (0.4f));
    searchBox.setFont (DysektLookAndFeel::makeFont (11.0f));
    searchBox.setColour (juce::TextEditor::backgroundColourId, getTheme().background.withAlpha (0.6f));
    searchBox.setColour (juce::TextEditor::textColourId,       getTheme().foreground);
    searchBox.setColour (juce::TextEditor::outlineColourId,    getTheme().accent.withAlpha (0.4f));
    searchBox.onTextChange = [this]
    {
        currentFilter = searchBox.getText().toLowerCase();
        repaint();
    };
    addAndMakeVisible (searchBox);

    setWantsKeyboardFocus (true);
}

ShortcutsPanel::~ShortcutsPanel() = default;

void ShortcutsPanel::buildShortcutData()
{
    categories.clear();

    {
        ShortcutCategory slicing;
        slicing.title = "Slicing";
        slicing.entries = {
            { "A",   "Add slice (toggle Add mode)" },
            { "L",   "Lazy Chop (auto-follow mode)" },
            { "D",   "Duplicate selected slice" },
            { "C",   "Auto Chop panel" },
            { "Del", "Delete selected slice" },
            { "Z",   "Snap to zero-crossing toggle" },
            { "Alt", "Hold for Alt (draw) mode" },
        };
        categories.push_back (std::move (slicing));
    }

    {
        ShortcutCategory nav;
        nav.title = "Navigation";
        nav.entries = {
            { "← / →",       "Select previous / next slice" },
            { "Tab",         "Select next slice" },
            { "Shift+Tab",   "Select previous slice" },
        };
        categories.push_back (std::move (nav));
    }

    {
        ShortcutCategory editing;
        editing.title = "Editing";
        editing.entries = {
            { "⌘Z",       "Undo" },
            { "⌘⇧Z",     "Redo" },
            { "F",        "Toggle MIDI-selects-slice mode" },
        };
        categories.push_back (std::move (editing));
    }

    {
        ShortcutCategory misc;
        misc.title = "General";
        misc.entries = {
            { "⌘?",    "Toggle this shortcuts panel" },
            { "Esc",   "Close panel / cancel operation" },
            { "Cmd",   "Command modifier for keyboard shortcuts" },
        };
        categories.push_back (std::move (misc));
    }
}

bool ShortcutsPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onDismiss) onDismiss();
        return true;
    }
    return false;
}

void ShortcutsPanel::paint (juce::Graphics& g)
{
    // Dim overlay behind the panel
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    // Panel background
    auto panel = getLocalBounds().reduced (40, 30);
    g.setColour (getTheme().header);
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);

    g.setColour (getTheme().accent.withAlpha (0.5f));
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);

    // Draw shortcut rows below the header controls
    auto content = panel.reduced (14, 6);
    content.removeFromTop (30 + 8 + 26 + 10); // title row + gap + search + gap

    const int colW    = content.getWidth() / 2;
    const int rowH    = 18;
    const int catGap  = 10;

    auto leftCol  = content.removeFromLeft (colW);
    auto rightCol = content;

    bool useLeft = true;
    for (const auto& cat : categories)
    {
        // Filter: skip categories with no matching entries
        bool hasMatch = currentFilter.isEmpty();
        if (! hasMatch)
        {
            for (const auto& e : cat.entries)
                if (e.keys.toLowerCase().contains (currentFilter) || e.description.toLowerCase().contains (currentFilter))
                    { hasMatch = true; break; }
        }
        if (! hasMatch) continue;

        auto& col = useLeft ? leftCol : rightCol;
        useLeft = ! useLeft;

        // Category heading
        g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
        g.setColour (getTheme().accent);
        g.drawText (cat.title.toUpperCase(), col.removeFromTop (rowH), juce::Justification::centredLeft);
        col.removeFromTop (2);

        for (const auto& entry : cat.entries)
        {
            if (! currentFilter.isEmpty()
                && ! entry.keys.toLowerCase().contains (currentFilter)
                && ! entry.description.toLowerCase().contains (currentFilter))
                continue;

            auto row = col.removeFromTop (rowH);
            const int keysW = 72;

            // Keys badge
            auto keyRect = row.removeFromLeft (keysW);
            g.setColour (getTheme().button.withAlpha (0.9f));
            g.fillRoundedRectangle (keyRect.reduced (0, 2).toFloat(), 3.0f);
            g.setFont (DysektLookAndFeel::makeFont (10.0f, true));
            g.setColour (getTheme().accent);
            g.drawText (entry.keys, keyRect, juce::Justification::centred);

            // Description
            row.removeFromLeft (6);
            g.setFont (DysektLookAndFeel::makeFont (10.5f));
            g.setColour (getTheme().foreground.withAlpha (0.85f));
            g.drawText (entry.description, row, juce::Justification::centredLeft);
        }

        col.removeFromTop (catGap);
    }
}

void ShortcutsPanel::resized()
{
    auto panel = getLocalBounds().reduced (40, 30);
    auto header = panel.reduced (14, 6);

    // Title row
    auto titleRow = header.removeFromTop (30);
    closeBtn.setBounds (titleRow.removeFromRight (30));
    titleLabel.setBounds (titleRow);

    header.removeFromTop (8);

    // Search box
    searchBox.setBounds (header.removeFromTop (26));
}
