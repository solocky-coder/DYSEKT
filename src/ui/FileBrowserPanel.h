#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

class DysektProcessor;

// ── Tiny LookAndFeel override to shrink the file-list font ───────────────────
class SmallListLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SmallListLookAndFeel()
    {
        // Very dark list background
        setColour (juce::ListBox::backgroundColourId,         juce::Colour (0xFF060709));
        setColour (juce::DirectoryContentsDisplayComponent::textColourId,
                   juce::Colour (0xFF8aacbc));
        setColour (juce::DirectoryContentsDisplayComponent::highlightColourId,
                   juce::Colour (0xFF0a1822));
        setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId,
                   juce::Colour (0xFF00d4e8));
        setColour (juce::FileChooserDialogBox::titleTextColourId,
                   juce::Colour (0xFF8aacbc));
    }
private:
public:
    void drawFileBrowserRow (juce::Graphics& g, int width, int height,
                             const juce::File& /*file*/,
                             const juce::String& filename,
                             juce::Image* /*icon*/,
                             const juce::String& fileSizeDescription,
                             const juce::String& fileTimeDescription,
                             bool isDirectory, bool isItemSelected,
                             int /*itemIndex*/,
                             juce::DirectoryContentsDisplayComponent& /*dcc*/) override
    {
        if (isItemSelected)
            g.fillAll (findColour (juce::DirectoryContentsDisplayComponent::highlightColourId)
                           .withAlpha (0.6f));

        auto textCol = isItemSelected
                     ? findColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId)
                     : findColour (juce::DirectoryContentsDisplayComponent::textColourId);

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

    // Row height — not a virtual override, just call it directly where needed
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

    // Called after double-click load so editor can close the panel
    std::function<void()> onFileLoaded;

    // Called when a file is selected (by double-click); overrides direct loadFileAsync if set
    std::function<void(const juce::File&)> onFileSelected;

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
