#pragma once
#include <juce_graphics/juce_graphics.h>

struct ThemeData
{
    juce::String name;

    juce::Colour background;
    juce::Colour waveformBg;
    juce::Colour darkBar;
    juce::Colour foreground;
    juce::Colour header;
    juce::Colour waveform;
    juce::Colour selectionOverlay;
    juce::Colour lockActive;
    juce::Colour lockInactive;
    juce::Colour gridLine;
    juce::Colour accent;
    juce::Colour button;
    juce::Colour buttonHover;
    juce::Colour separator;

    juce::Colour slicePalette[16];

    static ThemeData darkTheme()
    {
        ThemeData t;
        t.name          = "dark";
        t.background    = juce::Colour (0xFF0A0A0E);
        t.waveformBg    = juce::Colour (0xFF060608);
        t.darkBar       = juce::Colour (0xFF0E0E13);
        t.foreground    = juce::Colour (0xFFCCD0D8);
        t.header    = juce::Colour (0xFF0D0D14);
        t.waveform = juce::Colour::fromFloatRGBA (0.70f, 0.78f, 0.85f, 1.0f);
        t.selectionOverlay = juce::Colour::fromFloatRGBA (0.25f, 0.35f, 0.55f, 1.0f);
        t.lockActive      = juce::Colour::fromFloatRGBA (0.90f, 0.35f, 0.22f, 1.0f);
        t.lockInactive       = juce::Colour::fromFloatRGBA (0.30f, 0.30f, 0.34f, 1.0f);
        t.gridLine      = juce::Colour::fromFloatRGBA (0.14f, 0.14f, 0.18f, 1.0f);
        t.accent        = juce::Colour::fromFloatRGBA (0.25f, 0.85f, 0.85f, 1.0f);
        t.button        = juce::Colour (0xFF1C2028);
        t.buttonHover   = juce::Colour (0xFF2A3040);
        t.separator     = juce::Colour::fromFloatRGBA (0.20f, 0.20f, 0.25f, 1.0f);
        t.slicePalette[0]  = juce::Colour (0xFF1A8FD1); // Ocean Blue
        t.slicePalette[1]  = juce::Colour (0xFFD13A6E); // Deep Rose
        t.slicePalette[2]  = juce::Colour (0xFF1AC17A); // Emerald
        t.slicePalette[3]  = juce::Colour (0xFFD17A1A); // Amber
        t.slicePalette[4]  = juce::Colour (0xFF7A1AD1); // Electric Violet
        t.slicePalette[5]  = juce::Colour (0xFF1AC1C1); // Cyan
        t.slicePalette[6]  = juce::Colour (0xFFC13A1A); // Vermillion
        t.slicePalette[7]  = juce::Colour (0xFF1A6ED1); // Cobalt
        t.slicePalette[8]  = juce::Colour (0xFFD1C11A); // Gold
        t.slicePalette[9]  = juce::Colour (0xFF3AD16E); // Mint
        t.slicePalette[10] = juce::Colour (0xFFD11AA8); // Magenta
        t.slicePalette[11] = juce::Colour (0xFF1AA8D1); // Sky
        t.slicePalette[12] = juce::Colour (0xFF1A8FD1); // Ocean Blue
        t.slicePalette[13] = juce::Colour (0xFFD13A6E); // Deep Rose
        t.slicePalette[14] = juce::Colour (0xFF1AC17A); // Emerald
        t.slicePalette[15] = juce::Colour (0xFFD17A1A); // Amber
        return t;
    }

    static ThemeData lightTheme()
    {
        ThemeData t;
        t.name          = "light";
        t.background    = juce::Colour (0xFFF0F0F4);
        t.waveformBg    = juce::Colour (0xFFFAFAFE);
        t.darkBar       = juce::Colour (0xFFE8E8F0);
        t.foreground    = juce::Colour (0xFF1A1A2E);
        t.header    = juce::Colour (0xFFE0E0EC);
        t.waveform = juce::Colour (0xFF2A4060);
        t.selectionOverlay = juce::Colour (0xFF8090B8);
        t.lockActive      = juce::Colour (0xFFCC4422);
        t.lockInactive       = juce::Colour (0xFF9999A8);
        t.gridLine      = juce::Colour (0xFFD8D8E0);
        t.accent        = juce::Colour (0xFF1A8888);
        t.button        = juce::Colour (0xFFD0D4DC);
        t.buttonHover   = juce::Colour (0xFFBCC0CC);
        t.separator     = juce::Colour (0xFFC0C0CC);
        t.slicePalette[0]  = juce::Colour (0xFF1070B8); // Ocean Blue
        t.slicePalette[1]  = juce::Colour (0xFFB82055); // Deep Rose
        t.slicePalette[2]  = juce::Colour (0xFF10A060); // Emerald
        t.slicePalette[3]  = juce::Colour (0xFFB86010); // Amber
        t.slicePalette[4]  = juce::Colour (0xFF6010B8); // Electric Violet
        t.slicePalette[5]  = juce::Colour (0xFF10A0A0); // Cyan
        t.slicePalette[6]  = juce::Colour (0xFFB83010); // Vermillion
        t.slicePalette[7]  = juce::Colour (0xFF1055B8); // Cobalt
        t.slicePalette[8]  = juce::Colour (0xFFB0A010); // Gold
        t.slicePalette[9]  = juce::Colour (0xFF20B055); // Mint
        t.slicePalette[10] = juce::Colour (0xFFB01090); // Magenta
        t.slicePalette[11] = juce::Colour (0xFF1090B8); // Sky
        t.slicePalette[12] = juce::Colour (0xFF1070B8); // Ocean Blue
        t.slicePalette[13] = juce::Colour (0xFFB82055); // Deep Rose
        t.slicePalette[14] = juce::Colour (0xFF10A060); // Emerald
        t.slicePalette[15] = juce::Colour (0xFFB86010); // Amber
        return t;
    }


    // ── LAZY ─────────────────────────────────────────────────────────────────
    // Electric blue accent · Deep charcoal panels · High contrast
    static ThemeData lazyTheme()
    {
        ThemeData t;
        t.name             = "lazy";
        t.background       = juce::Colour (0xFF1A1C22);
        t.waveformBg       = juce::Colour (0xFF13151A);
        t.darkBar          = juce::Colour (0xFF16181F);
        t.foreground       = juce::Colour (0xFFC8D8E8);
        t.header           = juce::Colour (0xFF13151A);
        t.waveform         = juce::Colour (0xFF4A9EFF);
        t.selectionOverlay = juce::Colour (0xFF1E3050);
        t.lockActive       = juce::Colour (0xFF4A9EFF);
        t.lockInactive     = juce::Colour (0xFF2A3545);
        t.gridLine         = juce::Colour (0xFF1E2028);
        t.accent           = juce::Colour (0xFF4A9EFF);
        t.button           = juce::Colour (0xFF1E2028);
        t.buttonHover      = juce::Colour (0xFF2A3545);
        t.separator        = juce::Colour (0xFF252830);
        t.slicePalette[0]  = juce::Colour (0xFF20A8F0); // Ocean Blue
        t.slicePalette[1]  = juce::Colour (0xFFF03080); // Deep Rose
        t.slicePalette[2]  = juce::Colour (0xFF20E090); // Emerald
        t.slicePalette[3]  = juce::Colour (0xFFF09020); // Amber
        t.slicePalette[4]  = juce::Colour (0xFF9020F0); // Electric Violet
        t.slicePalette[5]  = juce::Colour (0xFF20E0E0); // Cyan
        t.slicePalette[6]  = juce::Colour (0xFFE04020); // Vermillion
        t.slicePalette[7]  = juce::Colour (0xFF2080F0); // Cobalt
        t.slicePalette[8]  = juce::Colour (0xFFE0D020); // Gold
        t.slicePalette[9]  = juce::Colour (0xFF40E080); // Mint
        t.slicePalette[10] = juce::Colour (0xFFE020C8); // Magenta
        t.slicePalette[11] = juce::Colour (0xFF20C8E0); // Sky
        t.slicePalette[12] = juce::Colour (0xFF20A8F0); // Ocean Blue
        t.slicePalette[13] = juce::Colour (0xFFF03080); // Deep Rose
        t.slicePalette[14] = juce::Colour (0xFF20E090); // Emerald
        t.slicePalette[15] = juce::Colour (0xFFF09020); // Amber
        return t;
    }

    // ── SNOW ─────────────────────────────────────────────────────────────────
    // Warm orange accent · Dark slate panels · Professional warmth
    static ThemeData snowTheme()
    {
        ThemeData t;
        t.name             = "snow";
        t.background       = juce::Colour (0xFF232428);
        t.waveformBg       = juce::Colour (0xFF1C1D21);
        t.darkBar          = juce::Colour (0xFF1E1F23);
        t.foreground       = juce::Colour (0xFFE8E0D4);
        t.header           = juce::Colour (0xFF1C1D21);
        t.waveform         = juce::Colour (0xFFE87C2A);
        t.selectionOverlay = juce::Colour (0xFF3A2810);
        t.lockActive       = juce::Colour (0xFFE87C2A);
        t.lockInactive     = juce::Colour (0xFF383940);
        t.gridLine         = juce::Colour (0xFF2A2B30);
        t.accent           = juce::Colour (0xFFE87C2A);
        t.button           = juce::Colour (0xFF2A2B30);
        t.buttonHover      = juce::Colour (0xFF383940);
        t.separator        = juce::Colour (0xFF2E3035);
        t.slicePalette[0]  = juce::Colour (0xFF1A8FD1); // Ocean Blue
        t.slicePalette[1]  = juce::Colour (0xFFD13A6E); // Deep Rose
        t.slicePalette[2]  = juce::Colour (0xFF1AC17A); // Emerald
        t.slicePalette[3]  = juce::Colour (0xFFD17A1A); // Amber
        t.slicePalette[4]  = juce::Colour (0xFF7A1AD1); // Electric Violet
        t.slicePalette[5]  = juce::Colour (0xFF1AC1C1); // Cyan
        t.slicePalette[6]  = juce::Colour (0xFFC13A1A); // Vermillion
        t.slicePalette[7]  = juce::Colour (0xFF1A6ED1); // Cobalt
        t.slicePalette[8]  = juce::Colour (0xFFD1C11A); // Gold
        t.slicePalette[9]  = juce::Colour (0xFF3AD16E); // Mint
        t.slicePalette[10] = juce::Colour (0xFFD11AA8); // Magenta
        t.slicePalette[11] = juce::Colour (0xFF1AA8D1); // Sky
        t.slicePalette[12] = juce::Colour (0xFF1A8FD1); // Ocean Blue
        t.slicePalette[13] = juce::Colour (0xFFD13A6E); // Deep Rose
        t.slicePalette[14] = juce::Colour (0xFF1AC17A); // Emerald
        t.slicePalette[15] = juce::Colour (0xFFD17A1A); // Amber
        return t;
    }

    // ── GHOST ────────────────────────────────────────────────────────────────
    // Bright teal/mint accent · Near-black · Ultra-minimal
    static ThemeData ghostTheme()
    {
        ThemeData t;
        t.name             = "ghost";
        t.background       = juce::Colour (0xFF0E1014);
        t.waveformBg       = juce::Colour (0xFF090B0E);
        t.darkBar          = juce::Colour (0xFF0B0D10);
        t.foreground       = juce::Colour (0xFFC8E8E0);
        t.header           = juce::Colour (0xFF090B0E);
        t.waveform         = juce::Colour (0xFF2DD4A8);
        t.selectionOverlay = juce::Colour (0xFF0A2A20);
        t.lockActive       = juce::Colour (0xFF2DD4A8);
        t.lockInactive     = juce::Colour (0xFF1E2830);
        t.gridLine         = juce::Colour (0xFF141820);
        t.accent           = juce::Colour (0xFF2DD4A8);
        t.button           = juce::Colour (0xFF141820);
        t.buttonHover      = juce::Colour (0xFF1E2830);
        t.separator        = juce::Colour (0xFF1A1E24);
        t.slicePalette[0]  = juce::Colour (0xFF18A0E8); // Ocean Blue
        t.slicePalette[1]  = juce::Colour (0xFFE83878); // Deep Rose
        t.slicePalette[2]  = juce::Colour (0xFF18D888); // Emerald
        t.slicePalette[3]  = juce::Colour (0xFFE89018); // Amber
        t.slicePalette[4]  = juce::Colour (0xFF8818E8); // Electric Violet
        t.slicePalette[5]  = juce::Colour (0xFF18D8D8); // Cyan
        t.slicePalette[6]  = juce::Colour (0xFFD84818); // Vermillion
        t.slicePalette[7]  = juce::Colour (0xFF1878E8); // Cobalt
        t.slicePalette[8]  = juce::Colour (0xFFD8C818); // Gold
        t.slicePalette[9]  = juce::Colour (0xFF38D878); // Mint
        t.slicePalette[10] = juce::Colour (0xFFD818C0); // Magenta
        t.slicePalette[11] = juce::Colour (0xFF18C0D8); // Sky
        t.slicePalette[12] = juce::Colour (0xFF18A0E8); // Ocean Blue
        t.slicePalette[13] = juce::Colour (0xFFE83878); // Deep Rose
        t.slicePalette[14] = juce::Colour (0xFF18D888); // Emerald
        t.slicePalette[15] = juce::Colour (0xFFE89018); // Amber
        return t;
    }

    // ── HACK ─────────────────────────────────────────────────────────────────
    // Red accent · Pure black · Industrial — classic Akai MPC
    static ThemeData hackTheme()
    {
        ThemeData t;
        t.name             = "hack";
        t.background       = juce::Colour (0xFF0A0A0A);
        t.waveformBg       = juce::Colour (0xFF050505);
        t.darkBar          = juce::Colour (0xFF070707);
        t.foreground       = juce::Colour (0xFFC8C8C8);
        t.header           = juce::Colour (0xFF050505);
        t.waveform         = juce::Colour (0xFFCC2200);
        t.selectionOverlay = juce::Colour (0xFF2A0500);
        t.lockActive       = juce::Colour (0xFFCC2200);
        t.lockInactive     = juce::Colour (0xFF252525);
        t.gridLine         = juce::Colour (0xFF141414);
        t.accent           = juce::Colour (0xFFCC2200);
        t.button           = juce::Colour (0xFF0F0F0F);
        t.buttonHover      = juce::Colour (0xFF1A0000);
        t.separator        = juce::Colour (0xFF1A1A1A);
        t.slicePalette[0]  = juce::Colour (0xFFCC2200); // Red
        t.slicePalette[1]  = juce::Colour (0xFF888888); // Grey
        t.slicePalette[2]  = juce::Colour (0xFF666666); // Dark Grey
        t.slicePalette[3]  = juce::Colour (0xFF999999); // Silver
        t.slicePalette[4]  = juce::Colour (0xFF777777); // Mid Grey
        t.slicePalette[5]  = juce::Colour (0xFF555555); // Charcoal
        t.slicePalette[6]  = juce::Colour (0xFF888888); // Grey
        t.slicePalette[7]  = juce::Colour (0xFFAAAAAA); // Light Grey
        t.slicePalette[8]  = juce::Colour (0xFF444444); // Dim
        t.slicePalette[9]  = juce::Colour (0xFF993300); // Dark Red
        t.slicePalette[10] = juce::Colour (0xFF666666); // Dark Grey
        t.slicePalette[11] = juce::Colour (0xFF777777); // Mid Grey
        t.slicePalette[12] = juce::Colour (0xFF555555); // Charcoal
        t.slicePalette[13] = juce::Colour (0xFF888888); // Grey
        t.slicePalette[14] = juce::Colour (0xFF444444); // Dim
        t.slicePalette[15] = juce::Colour (0xFF333333); // Near Black
        return t;
    }

    static juce::Colour parseHex (const juce::String& hex)
    {
        return juce::Colour ((juce::uint32) (0xFF000000 | hex.getHexValue32()));
    }

    static ThemeData fromThemeFile (const juce::String& text)
    {
        ThemeData t = darkTheme(); // defaults

        for (auto line : juce::StringArray::fromLines (text))
        {
            line = line.trim();
            if (line.isEmpty() || line.startsWith ("#"))
                continue;

            int colonIdx = line.indexOf (":");
            if (colonIdx < 0)
                continue;

            auto key = line.substring (0, colonIdx).trim();
            auto val = line.substring (colonIdx + 1).trim().unquoted();

            // Strip inline comments (  # ...)
            int hashIdx = val.indexOf (" #");
            if (hashIdx >= 0)
                val = val.substring (0, hashIdx).trimEnd();

            if (key == "name")            t.name = val;
            else if (key == "background")    t.background = parseHex (val);
            else if (key == "waveformBg")    t.waveformBg = parseHex (val);
            else if (key == "darkBar")       t.darkBar = parseHex (val);
            else if (key == "foreground")    t.foreground = parseHex (val);
            else if (key == "header")    t.header = parseHex (val);
            else if (key == "waveform") t.waveform = parseHex (val);
            else if (key == "selectionOverlay") t.selectionOverlay = parseHex (val);
            else if (key == "lockActive")      t.lockActive = parseHex (val);
            else if (key == "lockInactive")       t.lockInactive = parseHex (val);
            else if (key == "gridLine")      t.gridLine = parseHex (val);
            else if (key == "accent")        t.accent = parseHex (val);
            else if (key == "button")        t.button = parseHex (val);
            else if (key == "buttonHover")   t.buttonHover = parseHex (val);
            else if (key == "separator")     t.separator = parseHex (val);
            else if (key.startsWith ("slice"))
            {
                int idx = key.substring (5).getIntValue() - 1;
                if (idx >= 0 && idx < 16)
                    t.slicePalette[idx] = parseHex (val);
            }
        }
        return t;
    }

    static juce::String colourToHex (juce::Colour c)
    {
        return juce::String::toHexString ((int) (c.getARGB() & 0x00FFFFFF)).paddedLeft ('0', 6);
    }

    juce::String toThemeFile() const
    {
        juce::String s;
        s << "name: " << name << "\n";
        s << "background: " << colourToHex (background) << "\n";
        s << "waveformBg: " << colourToHex (waveformBg) << "\n";
        s << "darkBar: " << colourToHex (darkBar) << "\n";
        s << "foreground: " << colourToHex (foreground) << "\n";
        s << "header: " << colourToHex (header) << "\n";
        s << "waveform: " << colourToHex (waveform) << "\n";
        s << "selectionOverlay: " << colourToHex (selectionOverlay) << "\n";
        s << "lockActive: " << colourToHex (lockActive) << "\n";
        s << "lockInactive: " << colourToHex (lockInactive) << "\n";
        s << "gridLine: " << colourToHex (gridLine) << "\n";
        s << "accent: " << colourToHex (accent) << "\n";
        s << "button: " << colourToHex (button) << "\n";
        s << "buttonHover: " << colourToHex (buttonHover) << "\n";
        s << "separator: " << colourToHex (separator) << "\n";
        for (int i = 0; i < 16; ++i)
            s << "slice" << (i + 1) << ": " << colourToHex (slicePalette[i]) << "\n";
        return s;
    }
};
