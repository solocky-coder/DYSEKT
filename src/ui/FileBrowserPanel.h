#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

class DysektProcessor;

class FileBrowserPanel : public juce::Component,
                         private juce::FileBrowserListener,
                         private juce::ChangeListener
{
public:
    explicit FileBrowserPanel (DysektProcessor& p);
    ~FileBrowserPanel() override;

    void resized() override;
    void paint   (juce::Graphics& g) override;

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
