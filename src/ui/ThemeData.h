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
        t.background    = juce::Colour (0xFF484644);
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
        t.slicePalette[0] = juce::Colour (0xFFC6008C); // Hot Magenta
        t.slicePalette[1] = juce::Colour (0xFF00C669); // Toxic Lime
        t.slicePalette[2] = juce::Colour (0xFFC65300); // Molten Orange
        t.slicePalette[3] = juce::Colour (0xFF009CC6); // Ice Blue
        t.slicePalette[4] = juce::Colour (0xFFC6B400); // Radioactive Yellow
        t.slicePalette[5] = juce::Colour (0xFF6C00C6); // UV Violet
        t.slicePalette[6] = juce::Colour (0xFF00C6A5); // Absinthe
        t.slicePalette[7] = juce::Colour (0xFFC60023); // Alarm Red
        t.slicePalette[8] = juce::Colour (0xFF2CC60F); // Neon Green
        t.slicePalette[9] = juce::Colour (0xFFC60070); // Acid Pink
        t.slicePalette[10]= juce::Colour (0xFF0083C6); // Reactor Blue
        t.slicePalette[11]= juce::Colour (0xFFC69800); // Hazard Amber
        t.slicePalette[12]= juce::Colour (0xFFC6008C); // Hot Magenta
        t.slicePalette[13]= juce::Colour (0xFF00C669); // Toxic Lime
        t.slicePalette[14]= juce::Colour (0xFFC65300); // Molten Orange
        t.slicePalette[15]= juce::Colour (0xFF009CC6); // Ice Blue
        return t;
    }

    // ── SHELL ─────────────────────────────────────────────────────────────────
    // Deep green tint · Phosphor terminal · Organic warmth
    static ThemeData shellTheme()
    {
        ThemeData t;
        t.name             = "shell";
        t.background       = juce::Colour (0xFF425248);
        t.waveformBg       = juce::Colour (0xFF080F0C);
        t.darkBar          = juce::Colour (0xFF0E1A14);
        t.foreground       = juce::Colour (0xFF8ECBA0);
        t.header           = juce::Colour (0xFF0A1610);
        t.waveform         = juce::Colour (0xFF3DCC6A);
        t.selectionOverlay = juce::Colour (0xFF1A4428);
        t.lockActive       = juce::Colour (0xFF3DCC6A);
        t.lockInactive     = juce::Colour (0xFF1E3228);
        t.gridLine         = juce::Colour (0xFF142018);
        t.accent           = juce::Colour (0xFF3DCC6A);
        t.button           = juce::Colour (0xFF122018);
        t.buttonHover      = juce::Colour (0xFF1C3022);
        t.separator        = juce::Colour (0xFF1A2A1E);
        t.slicePalette[0 ] = juce::Colour (0xFFCC0090); // Hot Magenta
        t.slicePalette[1 ] = juce::Colour (0xFF00CC6C); // Toxic Lime
        t.slicePalette[2 ] = juce::Colour (0xFFCC5500); // Molten Orange
        t.slicePalette[3 ] = juce::Colour (0xFF00A0CC); // Ice Blue
        t.slicePalette[4 ] = juce::Colour (0xFFCCB900); // Radioactive Yellow
        t.slicePalette[5 ] = juce::Colour (0xFF6F00CC); // UV Violet
        t.slicePalette[6 ] = juce::Colour (0xFF00CCA9); // Absinthe
        t.slicePalette[7 ] = juce::Colour (0xFFCC0024); // Alarm Red
        t.slicePalette[8 ] = juce::Colour (0xFF2DCC10); // Neon Green
        t.slicePalette[9 ] = juce::Colour (0xFFCC0073); // Acid Pink
        t.slicePalette[10] = juce::Colour (0xFF0086CC); // Reactor Blue
        t.slicePalette[11] = juce::Colour (0xFFCC9C00); // Hazard Amber
        t.slicePalette[12] = juce::Colour (0xFFCC0090); // Hot Magenta
        t.slicePalette[13] = juce::Colour (0xFF00CC6C); // Toxic Lime
        t.slicePalette[14] = juce::Colour (0xFFCC5500); // Molten Orange
        t.slicePalette[15] = juce::Colour (0xFF00A0CC); // Ice Blue
        return t;
    }


    // ── LAZY ─────────────────────────────────────────────────────────────────
    // Electric blue accent · Deep charcoal panels · High contrast
    static ThemeData lazyTheme()
    {
        ThemeData t;
        t.name             = "lazy";
        t.background       = juce::Colour (0xFF484D5A);
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
        t.slicePalette[0] = juce::Colour (0xFFEA00A5); // Hot Magenta
        t.slicePalette[1] = juce::Colour (0xFF00EA7C); // Toxic Lime
        t.slicePalette[2] = juce::Colour (0xFFEA6200); // Molten Orange
        t.slicePalette[3] = juce::Colour (0xFF00B8EA); // Ice Blue
        t.slicePalette[4] = juce::Colour (0xFFEAD500); // Radioactive Yellow
        t.slicePalette[5] = juce::Colour (0xFF7F00EA); // UV Violet
        t.slicePalette[6] = juce::Colour (0xFF00EAC3); // Absinthe
        t.slicePalette[7] = juce::Colour (0xFFEA002A); // Alarm Red
        t.slicePalette[8] = juce::Colour (0xFF34EA12); // Neon Green
        t.slicePalette[9] = juce::Colour (0xFFEA0084); // Acid Pink
        t.slicePalette[10]= juce::Colour (0xFF009AEA); // Reactor Blue
        t.slicePalette[11]= juce::Colour (0xFFEAB400); // Hazard Amber
        t.slicePalette[12]= juce::Colour (0xFFEA00A5); // Hot Magenta
        t.slicePalette[13]= juce::Colour (0xFF00EA7C); // Toxic Lime
        t.slicePalette[14]= juce::Colour (0xFFEA6200); // Molten Orange
        t.slicePalette[15]= juce::Colour (0xFF00B8EA); // Ice Blue
        return t;
    }

    // ── SNOW ─────────────────────────────────────────────────────────────────
    // Warm orange accent · Dark slate panels · Professional warmth
    static ThemeData snowTheme()
    {
        ThemeData t;
        t.name             = "snow";
        t.background       = juce::Colour (0xFF505258);
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
        t.slicePalette[0] = juce::Colour (0xFFB70081); // Hot Magenta
        t.slicePalette[1] = juce::Colour (0xFF00B761); // Toxic Lime
        t.slicePalette[2] = juce::Colour (0xFFB74D00); // Molten Orange
        t.slicePalette[3] = juce::Colour (0xFF0090B7); // Ice Blue
        t.slicePalette[4] = juce::Colour (0xFFB7A700); // Radioactive Yellow
        t.slicePalette[5] = juce::Colour (0xFF6400B7); // UV Violet
        t.slicePalette[6] = juce::Colour (0xFF00B798); // Absinthe
        t.slicePalette[7] = juce::Colour (0xFFB70021); // Alarm Red
        t.slicePalette[8] = juce::Colour (0xFF29B70E); // Neon Green
        t.slicePalette[9] = juce::Colour (0xFFB70067); // Acid Pink
        t.slicePalette[10]= juce::Colour (0xFF0078B7); // Reactor Blue
        t.slicePalette[11]= juce::Colour (0xFFB78D00); // Hazard Amber
        t.slicePalette[12]= juce::Colour (0xFFB70081); // Hot Magenta
        t.slicePalette[13]= juce::Colour (0xFF00B761); // Toxic Lime
        t.slicePalette[14]= juce::Colour (0xFFB74D00); // Molten Orange
        t.slicePalette[15]= juce::Colour (0xFF0090B7); // Ice Blue
        return t;
    }

    // ── GHOST ────────────────────────────────────────────────────────────────
    // Bright teal/mint accent · Near-black · Ultra-minimal
    static ThemeData ghostTheme()
    {
        ThemeData t;
        t.name             = "ghost";
        t.background       = juce::Colour (0xFF424850);
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
        t.slicePalette[0] = juce::Colour (0xFFD80099); // Hot Magenta
        t.slicePalette[1] = juce::Colour (0xFF00D872); // Toxic Lime
        t.slicePalette[2] = juce::Colour (0xFFD85A00); // Molten Orange
        t.slicePalette[3] = juce::Colour (0xFF00AAD8); // Ice Blue
        t.slicePalette[4] = juce::Colour (0xFFD8C500); // Radioactive Yellow
        t.slicePalette[5] = juce::Colour (0xFF7600D8); // UV Violet
        t.slicePalette[6] = juce::Colour (0xFF00D8B4); // Absinthe
        t.slicePalette[7] = juce::Colour (0xFFD80027); // Alarm Red
        t.slicePalette[8] = juce::Colour (0xFF30D811); // Neon Green
        t.slicePalette[9] = juce::Colour (0xFFD8007A); // Acid Pink
        t.slicePalette[10]= juce::Colour (0xFF008ED8); // Reactor Blue
        t.slicePalette[11]= juce::Colour (0xFFD8A600); // Hazard Amber
        t.slicePalette[12]= juce::Colour (0xFFD80099); // Hot Magenta
        t.slicePalette[13]= juce::Colour (0xFF00D872); // Toxic Lime
        t.slicePalette[14]= juce::Colour (0xFFD85A00); // Molten Orange
        t.slicePalette[15]= juce::Colour (0xFF00AAD8); // Ice Blue
        return t;
    }

    // ── HACK ─────────────────────────────────────────────────────────────────
    // Red accent · Pure black · Industrial — classic Akai MPC
    static ThemeData hackTheme()
    {
        ThemeData t;
        t.name             = "hack";
        t.background       = juce::Colour (0xFF484442);
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
        t.slicePalette[0] = juce::Colour (0xFFB2007D); // Hot Magenta
        t.slicePalette[1] = juce::Colour (0xFF00B25E); // Toxic Lime
        t.slicePalette[2] = juce::Colour (0xFFB24A00); // Molten Orange
        t.slicePalette[3] = juce::Colour (0xFF008CB2); // Ice Blue
        t.slicePalette[4] = juce::Colour (0xFFB2A200); // Radioactive Yellow
        t.slicePalette[5] = juce::Colour (0xFF6100B2); // UV Violet
        t.slicePalette[6] = juce::Colour (0xFF00B294); // Absinthe
        t.slicePalette[7] = juce::Colour (0xFFB20020); // Alarm Red
        t.slicePalette[8] = juce::Colour (0xFF27B20E); // Neon Green
        t.slicePalette[9] = juce::Colour (0xFFB20064); // Acid Pink
        t.slicePalette[10]= juce::Colour (0xFF0075B2); // Reactor Blue
        t.slicePalette[11]= juce::Colour (0xFFB28900); // Hazard Amber
        t.slicePalette[12]= juce::Colour (0xFFB2007D); // Hot Magenta
        t.slicePalette[13]= juce::Colour (0xFF00B25E); // Toxic Lime
        t.slicePalette[14]= juce::Colour (0xFFB24A00); // Molten Orange
        t.slicePalette[15]= juce::Colour (0xFF008CB2); // Ice Blue
        return t;
    }


    // ── MIDNIGHT ─────────────────────────────────────────────────────────────
    // Deep purple-black · Violet accent · Serum / Vital premium synth feel
    // Sharp 2px radius · Zero glow · Crisp 1px borders — Direction D
    static ThemeData midnightTheme()
    {
        ThemeData t;
        t.name             = "midnight";
        t.background       = juce::Colour (0xFF403D58);   // deep purple chassis
        t.waveformBg       = juce::Colour (0xFF08060F);   // near-void
        t.darkBar          = juce::Colour (0xFF100E1A);   // panel bars
        t.foreground       = juce::Colour (0xFFD8D0F0);   // cool purple-tinted white
        t.header           = juce::Colour (0xFF0A0812);   // top bar
        t.waveform         = juce::Colour (0xFF9B6FFF);   // violet
        t.selectionOverlay = juce::Colour (0xFF1A0F38);   // selection tint
        t.lockActive       = juce::Colour (0xFF9B6FFF);
        t.lockInactive     = juce::Colour (0xFF1C1838);
        t.gridLine         = juce::Colour (0xFF161228);
        t.accent           = juce::Colour (0xFF9B6FFF);   // electric violet
        t.button           = juce::Colour (0xFF18152A);   // elevated panel
        t.buttonHover      = juce::Colour (0xFF221F38);
        t.separator        = juce::Colour (0xFF2A2545);
        t.slicePalette[0 ] = juce::Colour (0xFFFF2DA0); // Hot Magenta
        t.slicePalette[1 ] = juce::Colour (0xFF00FFAA); // Mint
        t.slicePalette[2 ] = juce::Colour (0xFFFF6B00); // Amber
        t.slicePalette[3 ] = juce::Colour (0xFF5CCCFF); // Cyan
        t.slicePalette[4 ] = juce::Colour (0xFFFFE000); // Solar Yellow
        t.slicePalette[5 ] = juce::Colour (0xFF9B6FFF); // Violet
        t.slicePalette[6 ] = juce::Colour (0xFF00FFD4); // Aqua
        t.slicePalette[7 ] = juce::Colour (0xFFFF2040); // Alarm Red
        t.slicePalette[8 ] = juce::Colour (0xFF40FF30); // Neon Green
        t.slicePalette[9 ] = juce::Colour (0xFFFF40A0); // Acid Pink
        t.slicePalette[10] = juce::Colour (0xFF00A8FF); // Sky Blue
        t.slicePalette[11] = juce::Colour (0xFFFFB800); // Gold
        t.slicePalette[12] = juce::Colour (0xFFFF2DA0); // Hot Magenta
        t.slicePalette[13] = juce::Colour (0xFF00FFAA); // Mint
        t.slicePalette[14] = juce::Colour (0xFFFF6B00); // Amber
        t.slicePalette[15] = juce::Colour (0xFF9B6FFF); // Violet
        return t;
    }

    // ── PIGMENTS ──────────────────────────────────────────────────────────────
    // Deep navy-black · Electric cyan accent · Arturia Pigments-inspired
    static ThemeData pigmentsTheme()
    {
        ThemeData t;
        t.name             = "pigments";
        t.background       = juce::Colour (0xFF3E3E52);   // blue-tinted chassis
        t.waveformBg       = juce::Colour (0xFF09090F);   // deeper for waveform
        t.darkBar          = juce::Colour (0xFF0C0C16);   // panel bars
        t.foreground       = juce::Colour (0xFFDDE6F5);   // cool off-white
        t.header           = juce::Colour (0xFF0A0A12);   // top bar
        t.waveform         = juce::Colour (0xFF5CCCFF);   // Pigments cyan
        t.selectionOverlay = juce::Colour (0xFF0A1E38);   // selection tint
        t.lockActive       = juce::Colour (0xFF5CCCFF);
        t.lockInactive     = juce::Colour (0xFF1C2040);
        t.gridLine         = juce::Colour (0xFF14142A);
        t.accent           = juce::Colour (0xFF5CCCFF);   // electric cyan
        t.button           = juce::Colour (0xFF181828);   // elevated panel
        t.buttonHover      = juce::Colour (0xFF22223A);
        t.separator        = juce::Colour (0xFF28284A);
        t.slicePalette[0 ] = juce::Colour (0xFFFF2DA0); // Hot Magenta
        t.slicePalette[1 ] = juce::Colour (0xFF00FFAA); // Mint
        t.slicePalette[2 ] = juce::Colour (0xFFFF6B00); // Amber
        t.slicePalette[3 ] = juce::Colour (0xFF5CCCFF); // Pigments Cyan
        t.slicePalette[4 ] = juce::Colour (0xFFFFE000); // Solar Yellow
        t.slicePalette[5 ] = juce::Colour (0xFF9B59FF); // UV Violet
        t.slicePalette[6 ] = juce::Colour (0xFF00FFD4); // Aqua
        t.slicePalette[7 ] = juce::Colour (0xFFFF2040); // Alarm Red
        t.slicePalette[8 ] = juce::Colour (0xFF40FF30); // Neon Green
        t.slicePalette[9 ] = juce::Colour (0xFFFF40A0); // Acid Pink
        t.slicePalette[10] = juce::Colour (0xFF00A8FF); // Sky Blue
        t.slicePalette[11] = juce::Colour (0xFFFFB800); // Gold
        t.slicePalette[12] = juce::Colour (0xFFFF2DA0); // Hot Magenta
        t.slicePalette[13] = juce::Colour (0xFF00FFAA); // Mint
        t.slicePalette[14] = juce::Colour (0xFFFF6B00); // Amber
        t.slicePalette[15] = juce::Colour (0xFF5CCCFF); // Pigments Cyan
        return t;
    }

    static juce::Colour parseHex (const juce::String& hex)
    {
        return juce::Colour ((juce::uint32) (0xFF000000 | hex.getHexValue32()));
    }

    static ThemeData fromThemeFile (const juce::String& text)
    {
        ThemeData t = darkTheme(); // defaults
        if (text.contains("name: midnight")) return midnightTheme();
        if (text.contains("name: pigments")) return pigmentsTheme();

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
