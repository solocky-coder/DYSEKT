// src/ui/ThemeData.h

#pragma once
#include <juce_graphics/juce_graphics.h>

class ThemeData {
public:
    juce::String name;               // Placeholder for theme name
    juce::Colour accent;             // Used for accent elements
    juce::Colour foreground;         // Used for text and main UI foreground
    juce::Colour background;         // Used for general background
    juce::Colour header;             // Used for panel or header backgrounds
    juce::Colour button;             // Used for button backgrounds
    juce::Colour buttonHover;        // Used for hover state on buttons
    juce::Colour separator;          // Used for separators
    juce::Colour gridLine;           // Used for grid line visual elements
    juce::Colour lockActive;         // Used for actively-locked states
    juce::Colour lockInactive;       // Used for inactive locked states
    juce::Colour darkBar;            // Used for dark bar backgrounds
    juce::Colour waveformBg;         // Used for waveform background
    juce::Colour waveformColour;     // Used for waveform lines

    ThemeData()
        : name("Default"),
          accent(juce::Colours::orange),
          foreground(juce::Colours::white),
          background(juce::Colours::black),
          header(juce::Colours::darkgrey),
          button(juce::Colours::grey),
          buttonHover(juce::Colours::yellow.withAlpha(0.7f)),
          separator(juce::Colours::grey.withAlpha(0.9f)),
          gridLine(juce::Colours::grey),
          lockActive(juce::Colours::red.withAlpha(0.7f)),
          lockInactive(juce::Colours::lightgrey),
          darkBar(juce::Colours::darkslategrey),
          waveformBg(juce::Colours::purple),
          waveformColour(juce::Colours::aqua)
    {
        // Additional customization if needed.
    }
};