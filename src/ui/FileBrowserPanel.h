#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class FileBrowserPanel : public juce::Component,
                         private juce::FileBrowserListener
{
public:
    explicit FileBrowserPanel (DysektProcessor& p);
    ~FileBrowserPanel() override;

    void resized() override;
    void paint   (juce::Graphics& g) override;

private:
    // FileBrowserListener
    void selectionChanged() override {}
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File& f) override;
    void browserRootChanged (const juce::File&) override {}

    DysektProcessor& processor;

    juce::WildcardFileFilter fileFilter;
    juce::TimeSliceThread    ioThread  { "FileBrowserIO" };
    juce::DirectoryContentsList dirList;
    juce::FileBrowserComponent  browser;
};
