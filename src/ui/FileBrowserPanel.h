#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DysektLookAndFeel.h"

class DysektProcessor;

// ── Tiny LookAndFeel override to shrink the file-list font ───────────────────
class SmallListLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SmallListLookAndFeel() {}

    // Call this whenever the theme changes to push new colours through
    void refreshTheme()
    {
        const auto& t = getTheme();
        setColour (juce::ListBox::backgroundColourId,
                   t.darkBar.darker (0.3f));
        setColour (juce::DirectoryContentsDisplayComponent::textColourId,
                   t.foreground.withAlpha (0.75f));
        setColour (juce::DirectoryContentsDisplayComponent::highlightColourId,
                   t.accent.withAlpha (0.12f));
        setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId,
                   t.accent);
        setColour (juce::FileChooserDialogBox::titleTextColourId,
                   t.foreground.withAlpha (0.75f));
        setColour (juce::TextEditor::backgroundColourId,  t.darkBar.darker (0.3f));
        setColour (juce::TextEditor::textColourId,        t.accent);
        setColour (juce::TextEditor::outlineColourId,     t.separator);
        setColour (juce::Label::backgroundColourId,       t.darkBar.darker (0.3f));
        setColour (juce::Label::textColourId,             t.foreground.withAlpha (0.75f));
    }

    // ── Override popup menu drawing — no unicode symbols ─────────────────────
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (getTheme().darkBar);
        g.setColour (getTheme().separator);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool, const juce::String& text,
                            const juce::String&, const juce::Drawable*,
                            const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour (getTheme().separator);
            g.fillRect (area.reduced (4, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }
        if (isHighlighted && isActive)
        {
            g.setColour (getTheme().accent.withAlpha (0.15f));
            g.fillRect (area);
        }
        const int dotZone = 16;
        if (isTicked)
        {
            g.setColour (getTheme().accent);
            g.fillRect (area.getX() + 6, area.getCentreY() - 2, 4, 4);
        }
        const auto textCol = isTicked ? getTheme().accent
                           : isActive ? getTheme().foreground
                                      : getTheme().foreground.withAlpha (0.4f);
        g.setColour (textCol);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f)));
        g.drawText (text,
                    area.withLeft (area.getX() + dotZone)
                        .withRight (area.getRight() - 4),
                    juce::Justification::centredLeft);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions{}.withHeight (13.0f));
    }

    void drawFileBrowserRow (juce::Graphics& g, int width, int height,
                             const juce::File& /*file*/,
                             const juce::String& filename,
                             juce::Image* /*icon*/,
                             const juce::String& fileSizeDescription,
                             const juce::String& /*fileTimeDescription*/,
                             bool /*isDirectory*/, bool isItemSelected,
                             int /*itemIndex*/,
                             juce::DirectoryContentsDisplayComponent& /*dcc*/) override
    {
        // Read live from theme — always current regardless of when theme changed
        const auto& t = getTheme();
        const auto bgHighlight  = t.accent.withAlpha (0.12f);
        const auto textNormal   = t.foreground.withAlpha (0.75f);
        const auto textSelected = t.accent;

        if (isItemSelected)
        {
            g.setColour (bgHighlight);
            g.fillAll();
        }

        const auto textCol = isItemSelected ? textSelected : textNormal;
        g.setColour (textCol);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
        g.drawText (filename, 2, 0, width - 4, height, juce::Justification::centredLeft, true);

        if (! fileSizeDescription.isEmpty())
        {
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));
            g.setColour (textCol.withAlpha (0.6f));
            g.drawText (fileSizeDescription,
                        width - 80, 0, 78, height,
                        juce::Justification::centredRight, true);
        }
    }

    int smallRowHeight() const { return 18; }
};

class FileBrowserPanel : public juce::Component,
                         private juce::FileBrowserListener,
                         private juce::ChangeListener
{
public:
    explicit FileBrowserPanel (DysektProcessor& p);
    ~FileBrowserPanel() override;

    void resized() override;
    void paint   (juce::Graphics& g) override;

    // Refresh all theme-dependent colours — call whenever theme changes
    void refreshTheme();

    // Called after double-click load so editor can close the panel
    std::function<void()> onFileLoaded;
    // Called when user double-clicks an audio file (not SF2/SFZ); editor wires this to showTrimDialog
    std::function<void (const juce::File&)> onLoadRequest;

private:
    // FileBrowserListener
    void selectionChanged() override {}
    void fileClicked       (const juce::File& f, const juce::MouseEvent&) override;
    void fileDoubleClicked (const juce::File& f) override;
    void browserRootChanged (const juce::File&) override {}

    // ChangeListener (transport state changes)
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void startPreview (const juce::File& f);
    void stopPreview();
    void updatePlayButton();

    DysektProcessor& processor;

    // File browser
    SmallListLookAndFeel           smallLAF;
    juce::WildcardFileFilter       fileFilter;
    juce::TimeSliceThread          ioThread  { "FileBrowserIO" };
    juce::DirectoryContentsList    dirList;
    juce::FileBrowserComponent     browser;

    // Preview audio engine
    juce::AudioDeviceManager       deviceManager;
    juce::AudioFormatManager       formatManager;
    juce::AudioTransportSource     transport;
    juce::AudioSourcePlayer        sourcePlayer;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // Preview UI bar (shown when a file is selected)
    juce::File                     previewFile;
    bool                           previewVisible = false;

    juce::TextButton               playStopBtn { "PLAY" };
    juce::Slider                   volumeSlider;
    juce::Label                    fileNameLabel;

    static constexpr int kBarH = 36;
};
