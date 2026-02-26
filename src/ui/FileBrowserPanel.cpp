#include "FileBrowserPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

FileBrowserPanel::FileBrowserPanel (DysektProcessor& p)
    : processor (p),
      fileFilter ("*.wav;*.aif;*.aiff;*.ogg;*.flac;*.mp3", "*", "Audio Files"),
      dirList (&fileFilter, ioThread),
      browser (juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles,
               juce::File::getSpecialLocation (juce::File::userHomeDirectory),
               &fileFilter, nullptr)
{
    ioThread.startThread();
    browser.addListener (this);
    addAndMakeVisible (browser);
}

FileBrowserPanel::~FileBrowserPanel()
{
    browser.removeListener (this);
    ioThread.stopThread (2000);
}

void FileBrowserPanel::resized()
{
    browser.setBounds (getLocalBounds());
}

void FileBrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().darkBar);
}

void FileBrowserPanel::fileDoubleClicked (const juce::File& f)
{
    if (f.existsAsFile())
        processor.loadFileAsync (f);
}
