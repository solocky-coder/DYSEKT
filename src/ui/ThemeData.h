// src/ui/ThemeData.h

#pragma once
#include <juce_graphics/juce_graphics.h>

class ThemeData {
public:
    juce::Colour accent;         // Used for accent elements
    juce::Colour foreground;     // Used for text and main UI foreground
    juce::Colour background;     // Used for general background
    juce::Colour header;         // Used for panel or header backgrounds
    juce::Colour button;         // Used for button backgrounds
    juce::Colour gridLine;       // Used for grid line visual elements
    juce::Colour lockInactive;   // Used for inactive locked states
    juce::Colour waveformBg;     // Used for waveform background
    juce::Colour waveformColour; // Used for waveform lines
    // Add more colour members as needed

    ThemeData()
        : accent(juce::Colours::orange),
          foreground(juce::Colours::white),
          background(juce::Colours::black),
          header(juce::Colours::darkgrey),
          button(juce::Colours::grey),
          gridLine(juce::Colours::grey),
          lockInactive(juce::Colours::lightgrey),
          waveformBg(juce::Colours::purple),
          waveformColour(juce::Colours::aqua)
    {
        // You can customize the default colours above as desired.
    }
};