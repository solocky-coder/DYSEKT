#pragma once

#include <juce_graphics/juce_graphics.h>

// =============================================================================
//  ThemeData  —  all colours and palette for one visual theme
// =============================================================================
class ThemeData
{
public:
    juce::String name;

    juce::Colour background;
    juce::Colour foreground;
    juce::Colour accent;
    juce::Colour button;
    juce::Colour buttonHover;
    juce::Colour header;
    juce::Colour darkBar;
    juce::Colour separator;
    juce::Colour lockActive;
    juce::Colour lockInactive;
    juce::Colour waveformColour;

    juce::Array<juce::Colour> slicePalette;

    juce::String toThemeFile() const
    {
        auto hex = [] (juce::Colour c) { return c.toDisplayString (true); };
        juce::String s;
        s << "name: "         << name                 << "\n"
          << "background: "   << hex (background)     << "\n"
          << "foreground: "   << hex (foreground)     << "\n"
          << "accent: "       << hex (accent)         << "\n"
          << "button: "       << hex (button)         << "\n"
          << "buttonHover: "  << hex (buttonHover)    << "\n"
          << "header: "       << hex (header)         << "\n"
          << "darkBar: "      << hex (darkBar)        << "\n"
          << "separator: "    << hex (separator)      << "\n"
          << "lockActive: "   << hex (lockActive)     << "\n"
          << "lockInactive: " << hex (lockInactive)   << "\n"
          << "waveform: "     << hex (waveformColour) << "\n";
        for (int i = 0; i < slicePalette.size(); ++i)
            s << "slice" << i << ": " << hex (slicePalette[i]) << "\n";
        return s;
    }

    static ThemeData fromThemeFile (const juce::String& text)
    {
        ThemeData t;
        auto parseLine = [&] (const juce::String& key) -> juce::Colour
        {
            for (auto& line : juce::StringArray::fromLines (text))
            {
                auto l = line.trim();
                if (l.startsWith (key + ":"))
                {
                    auto hex = l.fromFirstOccurrenceOf (":", false, false).trim();
                    if (! hex.startsWith ("#")) hex = "#" + hex;
                    return juce::Colour::fromString (hex);
                }
            }
            return juce::Colours::black;
        };
        for (auto& line : juce::StringArray::fromLines (text))
        {
            auto l = line.trim();
            if (l.startsWith ("name:"))
                t.name = l.fromFirstOccurrenceOf (":", false, false).trim();
        }
        t.background     = parseLine ("background");
        t.foreground     = parseLine ("foreground");
        t.accent         = parseLine ("accent");
        t.button         = parseLine ("button");
        t.buttonHover    = parseLine ("buttonHover");
        t.header         = parseLine ("header");
        t.darkBar        = parseLine ("darkBar");
        t.separator      = parseLine ("separator");
        t.lockActive     = parseLine ("lockActive");
        t.lockInactive   = parseLine ("lockInactive");
        t.waveformColour = parseLine ("waveform");
        for (int i = 0; i < 16; ++i)
        {
            juce::String key = "slice" + juce::String (i);
            bool found = false;
            for (auto& line : juce::StringArray::fromLines (text))
            {
                auto l = line.trim();
                if (l.startsWith (key + ":"))
                {
                    auto hex = l.fromFirstOccurrenceOf (":", false, false).trim();
                    if (! hex.startsWith ("#")) hex = "#" + hex;
                    t.slicePalette.add (juce::Colour::fromString (hex));
                    found = true; break;
                }
            }
            if (! found) break;
        }
        if (t.slicePalette.isEmpty())
            t.slicePalette = defaultSlicePalette();
        return t;
    }

    static ThemeData darkTheme()
    {
        ThemeData t; t.name = "dark";
        t.background = juce::Colour (0xff1a1a1a); t.foreground  = juce::Colour (0xffe0e0e0);
        t.accent     = juce::Colour (0xff4fc3f7); t.button      = juce::Colour (0xff2c2c2c);
        t.buttonHover= juce::Colour (0xff3a3a3a); t.header      = juce::Colour (0xff252525);
        t.darkBar    = juce::Colour (0xff121212); t.separator   = juce::Colour (0xff333333);
        t.lockActive = juce::Colour (0xff4fc3f7); t.lockInactive= juce::Colour (0xff444444);
        t.waveformColour = juce::Colour (0xff4fc3f7);
        t.slicePalette = defaultSlicePalette(); return t;
    }
    static ThemeData lightTheme()
    {
        ThemeData t; t.name = "light";
        t.background = juce::Colour (0xfff0f0f0); t.foreground  = juce::Colour (0xff1a1a1a);
        t.accent     = juce::Colour (0xff0277bd); t.button      = juce::Colour (0xffdcdcdc);
        t.buttonHover= juce::Colour (0xffc8c8c8); t.header     = juce::Colour (0xffe4e4e4);
        t.darkBar    = juce::Colour (0xffb0b0b0); t.separator   = juce::Colour (0xffbdbdbd);
        t.lockActive = juce::Colour (0xff0277bd); t.lockInactive= juce::Colour (0xff9e9e9e);
        t.waveformColour = juce::Colour (0xff0277bd);
        t.slicePalette = defaultSlicePalette(); return t;
    }
    static ThemeData lazyTheme()
    {
        ThemeData t; t.name = "lazy";
        t.background = juce::Colour (0xff1b1f23); t.foreground  = juce::Colour (0xffcdd9e5);
        t.accent     = juce::Colour (0xff58a6ff); t.button      = juce::Colour (0xff21262d);
        t.buttonHover= juce::Colour (0xff30363d); t.header      = juce::Colour (0xff161b22);
        t.darkBar    = juce::Colour (0xff0d1117); t.separator   = juce::Colour (0xff30363d);
        t.lockActive = juce::Colour (0xff58a6ff); t.lockInactive= juce::Colour (0xff484f58);
        t.waveformColour = juce::Colour (0xff58a6ff);
        t.slicePalette = defaultSlicePalette(); return t;
    }
    static ThemeData snowTheme()
    {
        ThemeData t; t.name = "snow";
        t.background = juce::Colour (0xfffafafa); t.foreground  = juce::Colour (0xff212121);
        t.accent     = juce::Colour (0xff90caf9); t.button      = juce::Colour (0xffeeeeee);
        t.buttonHover= juce::Colour (0xffe0e0e0); t.header      = juce::Colour (0xffe3f2fd);
        t.darkBar    = juce::Colour (0xffbbdefb); t.separator   = juce::Colour (0xffcfd8dc);
        t.lockActive = juce::Colour (0xff42a5f5); t.lockInactive= juce::Colour (0xffb0bec5);
        t.waveformColour = juce::Colour (0xff42a5f5);
        t.slicePalette = defaultSlicePalette(); return t;
    }
    static ThemeData ghostTheme()
    {
        ThemeData t; t.name = "ghost";
        t.background = juce::Colour (0xff0f0f0f); t.foreground  = juce::Colour (0xffaaaaaa);
        t.accent     = juce::Colour (0xffeeeeee); t.button      = juce::Colour (0xff1c1c1c);
        t.buttonHover= juce::Colour (0xff2a2a2a); t.header      = juce::Colour (0xff141414);
        t.darkBar    = juce::Colour (0xff080808); t.separator   = juce::Colour (0xff2a2a2a);
        t.lockActive = juce::Colour (0xffcccccc); t.lockInactive= juce::Colour (0xff3a3a3a);
        t.waveformColour = juce::Colour (0xff888888);
        t.slicePalette = defaultSlicePalette(); return t;
    }
    static ThemeData hackTheme()
    {
        ThemeData t; t.name = "hack";
        t.background = juce::Colour (0xff001100); t.foreground  = juce::Colour (0xff00ff41);
        t.accent     = juce::Colour (0xff00ff41); t.button      = juce::Colour (0xff002200);
        t.buttonHover= juce::Colour (0xff003300); t.header      = juce::Colour (0xff000d00);
        t.darkBar    = juce::Colour (0xff000800); t.separator   = juce::Colour (0xff004400);
        t.lockActive = juce::Colour (0xff00ff41); t.lockInactive= juce::Colour (0xff005500);
        t.waveformColour = juce::Colour (0xff00cc33);
        t.slicePalette = defaultSlicePalette(); return t;
    }

private:
    static juce::Array<juce::Colour> defaultSlicePalette()
    {
        return {
            juce::Colour (0xffef5350), juce::Colour (0xffec407a),
            juce::Colour (0xffab47bc), juce::Colour (0xff7e57c2),
            juce::Colour (0xff42a5f5), juce::Colour (0xff26c6da),
            juce::Colour (0xff26a69a), juce::Colour (0xff66bb6a),
            juce::Colour (0xffd4e157), juce::Colour (0xffffca28),
            juce::Colour (0xffffa726), juce::Colour (0xffff7043),
            juce::Colour (0xff8d6e63), juce::Colour (0xffbdbdbd),
            juce::Colour (0xff78909c), juce::Colour (0xff80deea)
        };
    }
};
