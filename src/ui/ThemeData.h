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

    juce::Colour slicePalette[32];

    static ThemeData darkTheme()
    {
        ThemeData t;
        t.name          = "dark";
        t.background       = juce::Colour (0xFF000000);
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
        t.slicePalette[12]= juce::Colour (0xFFC63060); // Coral Crimson
        t.slicePalette[13]= juce::Colour (0xFF30A0C6); // Steel Teal
        t.slicePalette[14]= juce::Colour (0xFFB0C600); // Chartreuse
        t.slicePalette[15]= juce::Colour (0xFF8060C6); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFC20544); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF05C2B0); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFC29B05); // Gold
        t.slicePalette[19] = juce::Colour (0xFF0553C2); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFF8DC205); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFB305C2); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF059BC2); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFC22B05); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF13C239); // Spring
        t.slicePalette[25] = juce::Colour (0xFFC20529); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF053BC2); // Azure
        t.slicePalette[27] = juce::Colour (0xFFA7C205); // Honey
        t.slicePalette[28] = juce::Colour (0xFFC23931); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF3167C2); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF66C205); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFA25CC2); // Soft Violet
        return t;
    }
    static ThemeData shellTheme()
    {
        ThemeData t;
        t.name             = "shell";
        t.background       = juce::Colour (0xFF000000);
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
        t.slicePalette[12] = juce::Colour (0xFFCC3060); // Coral Crimson
        t.slicePalette[13] = juce::Colour (0xFF30A0CC); // Steel Teal
        t.slicePalette[14] = juce::Colour (0xFFAFCC00); // Chartreuse
        t.slicePalette[15] = juce::Colour (0xFF8060CC); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFC70546); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF05C7B5); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFC79F05); // Gold
        t.slicePalette[19] = juce::Colour (0xFF0555C7); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFF91C705); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFB805C7); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF05A0C7); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFC72C05); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF14C73C); // Spring
        t.slicePalette[25] = juce::Colour (0xFFC7052A); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF053CC7); // Azure
        t.slicePalette[27] = juce::Colour (0xFFACC705); // Honey
        t.slicePalette[28] = juce::Colour (0xFFC73B31); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF3164C7); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF63C705); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFA45CC7); // Soft Violet
        return t;
    }
    static ThemeData lazyTheme()
    {
        ThemeData t;
        t.name             = "lazy";
        t.background       = juce::Colour (0xFF000000);
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
        t.slicePalette[12]= juce::Colour (0xFFEA3870); // Coral Crimson
        t.slicePalette[13]= juce::Colour (0xFF38B8EA); // Steel Teal
        t.slicePalette[14]= juce::Colour (0xFFCAEA00); // Chartreuse
        t.slicePalette[15]= juce::Colour (0xFF9070EA); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFE2064F); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF06E2CD); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFE2B506); // Gold
        t.slicePalette[19] = juce::Colour (0xFF0661E2); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFFA3E206); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFD006E2); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF06B4E2); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFE23106); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF16E243); // Spring
        t.slicePalette[25] = juce::Colour (0xFFE20630); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF0644E2); // Azure
        t.slicePalette[27] = juce::Colour (0xFFC2E206); // Honey
        t.slicePalette[28] = juce::Colour (0xFFE24238); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF3873E2); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF71E206); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFB66AE2); // Soft Violet
        return t;
    }
    static ThemeData snowTheme()
    {
        ThemeData t;
        t.name             = "snow";
        t.background       = juce::Colour (0xFF000000);
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
        t.slicePalette[12]= juce::Colour (0xFFB72C58); // Coral Crimson
        t.slicePalette[13]= juce::Colour (0xFF2C90B7); // Steel Teal
        t.slicePalette[14]= juce::Colour (0xFF9EB700); // Chartreuse
        t.slicePalette[15]= juce::Colour (0xFF7058B7); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFB5053F); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF05B5A4); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFB59105); // Gold
        t.slicePalette[19] = juce::Colour (0xFF054DB5); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFF82B505); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFA705B5); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF0591B5); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFB52705); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF12B535); // Spring
        t.slicePalette[25] = juce::Colour (0xFFB50526); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF0536B5); // Azure
        t.slicePalette[27] = juce::Colour (0xFF9BB505); // Honey
        t.slicePalette[28] = juce::Colour (0xFFB5352D); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF2D5CB5); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF5BB505); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFF9155B5); // Soft Violet
        return t;
    }
    static ThemeData ghostTheme()
    {
        ThemeData t;
        t.name             = "ghost";
        t.background       = juce::Colour (0xFF000000);
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
        t.slicePalette[12]= juce::Colour (0xFFD83468); // Coral Crimson
        t.slicePalette[13]= juce::Colour (0xFF34AAD8); // Steel Teal
        t.slicePalette[14]= juce::Colour (0xFFBDD800); // Chartreuse
        t.slicePalette[15]= juce::Colour (0xFF8868D8); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFD2064A); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF06D2BE); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFD2A706); // Gold
        t.slicePalette[19] = juce::Colour (0xFF065AD2); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFF97D206); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFC206D2); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF06A7D2); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFD22E06); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF15D23E); // Spring
        t.slicePalette[25] = juce::Colour (0xFFD2062D); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF063FD2); // Azure
        t.slicePalette[27] = juce::Colour (0xFFB5D206); // Honey
        t.slicePalette[28] = juce::Colour (0xFFD23E34); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF346BD2); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF6CD206); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFAC63D2); // Soft Violet
        return t;
    }
    static ThemeData hackTheme()
    {
        ThemeData t;
        t.name             = "hack";
        t.background       = juce::Colour (0xFF000000);
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
        t.slicePalette[12]= juce::Colour (0xFFB22854); // Coral Crimson
        t.slicePalette[13]= juce::Colour (0xFF288CB2); // Steel Teal
        t.slicePalette[14]= juce::Colour (0xFF9AB200); // Chartreuse
        t.slicePalette[15]= juce::Colour (0xFF6850B2); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFB1053D); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF05B1A0); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFB18D05); // Gold
        t.slicePalette[19] = juce::Colour (0xFF054BB1); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFF80B105); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFA305B1); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF058DB1); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFB12605); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF12B135); // Spring
        t.slicePalette[25] = juce::Colour (0xFFB10525); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF0535B1); // Azure
        t.slicePalette[27] = juce::Colour (0xFF98B105); // Honey
        t.slicePalette[28] = juce::Colour (0xFFB13129); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF2959B1); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF59B105); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFF8B4EB1); // Soft Violet
        return t;
    }
    static ThemeData midnightTheme()
    {
        ThemeData t;
        t.name             = "midnight";
        t.background       = juce::Colour (0xFF000000);   // deep purple chassis
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
        t.slicePalette[12] = juce::Colour (0xFFFF5090); // Coral Crimson
        t.slicePalette[13] = juce::Colour (0xFF50C8FF); // Steel Teal
        t.slicePalette[14] = juce::Colour (0xFFD0FF00); // Chartreuse
        t.slicePalette[15] = juce::Colour (0xFFFF90D8); // Rose Quartz

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFF42F51); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF07EAF4); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFF4C407); // Gold
        t.slicePalette[19] = juce::Colour (0xFF5889F4); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFFB8F407); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFC869F4); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF07C3F4); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFF45423); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF31F46B); // Spring
        t.slicePalette[25] = juce::Colour (0xFFF43F56); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF074AF4); // Azure
        t.slicePalette[27] = juce::Colour (0xFFDDF407); // Honey
        t.slicePalette[28] = juce::Colour (0xFFF44F4D); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF4D81F4); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF6FF407); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFF486A4); // Soft Violet
        return t;
    }
    static ThemeData cr8Theme()
    {
        ThemeData t;
        t.name             = "cr8";
        t.background       = juce::Colour (0xFF0C0D0E);   // near-black warm chassis
        t.waveformBg       = juce::Colour (0xFF131518);   // dark steel waveform bg
        t.darkBar          = juce::Colour (0xFF181A1D);   // panel bars
        t.foreground       = juce::Colour (0xFFDDD5C8);   // warm off-white
        t.header           = juce::Colour (0xFF101214);   // top bar
        t.waveform         = juce::Colour (0xFFD96010);   // burnt orange
        t.selectionOverlay = juce::Colour (0xFF2E1800);   // deep amber tint
        t.lockActive       = juce::Colour (0xFFD96010);   // burnt orange
        t.lockInactive     = juce::Colour (0xFF2A2C30);   // steel inactive
        t.gridLine         = juce::Colour (0xFF1C1E22);   // barely-visible warm grid
        t.accent           = juce::Colour (0xFFD96010);   // burnt orange
        t.button           = juce::Colour (0xFF22252A);   // elevated steel panel
        t.buttonHover      = juce::Colour (0xFF2E3238);   // hover lift
        t.separator        = juce::Colour (0xFF282B30);   // steel divider
        t.slicePalette[0 ] = juce::Colour (0xFFD96010); // Burnt Orange (accent)
        t.slicePalette[1 ] = juce::Colour (0xFF10A8D9); // Steel Blue
        t.slicePalette[2 ] = juce::Colour (0xFFD9C010); // Warm Yellow
        t.slicePalette[3 ] = juce::Colour (0xFF10D978); // Mint
        t.slicePalette[4 ] = juce::Colour (0xFFD91060); // Crimson
        t.slicePalette[5 ] = juce::Colour (0xFF8810D9); // Violet
        t.slicePalette[6 ] = juce::Colour (0xFF10D9C0); // Teal
        t.slicePalette[7 ] = juce::Colour (0xFFD93010); // Alarm Red
        t.slicePalette[8 ] = juce::Colour (0xFF50D910); // Lime
        t.slicePalette[9 ] = juce::Colour (0xFFD910A0); // Acid Pink
        t.slicePalette[10] = juce::Colour (0xFF1080D9); // Reactor Blue
        t.slicePalette[11] = juce::Colour (0xFFD98010); // Hazard Amber
        t.slicePalette[12] = juce::Colour (0xFFD94070); // Coral
        t.slicePalette[13] = juce::Colour (0xFF40A8D9); // Ice Blue
        t.slicePalette[14] = juce::Colour (0xFFA8D910); // Chartreuse
        t.slicePalette[15] = juce::Colour (0xFF9060D9); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFD3A814); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF145DD3); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFA3D314); // Gold
        t.slicePalette[19] = juce::Colour (0xFF14D3BE); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFFD31419); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFCE14D3); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF14A3D3); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFD37A14); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF14D31F); // Spring
        t.slicePalette[25] = juce::Colour (0xFFD31455); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF1437D3); // Azure
        t.slicePalette[27] = juce::Colour (0xFFD3C614); // Honey
        t.slicePalette[28] = juce::Colour (0xFFD3483F); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF3F6CD3); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF5DD314); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFB85CD3); // Soft Violet
        return t;
    }
    static ThemeData pigmentsTheme()
    {
        ThemeData t;
        t.name             = "pigments";
        t.background       = juce::Colour (0xFF000000);   // blue-tinted chassis
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
        t.slicePalette[12] = juce::Colour (0xFFFF5070); // Coral Crimson
        t.slicePalette[13] = juce::Colour (0xFF5CFFCC); // Seafoam
        t.slicePalette[14] = juce::Colour (0xFFCCFF00); // Chartreuse
        t.slicePalette[15] = juce::Colour (0xFFAA70FF); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFF42F51); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF07EAF4); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFF4C407); // Gold
        t.slicePalette[19] = juce::Colour (0xFF5889F4); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFFB8F407); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFD055F4); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF07C3F4); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFF45423); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF31F46B); // Spring
        t.slicePalette[25] = juce::Colour (0xFFF43F56); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF074AF4); // Azure
        t.slicePalette[27] = juce::Colour (0xFFDDF407); // Honey
        t.slicePalette[28] = juce::Colour (0xFFF46E4D); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF58EBF4); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF6CF407); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFD66AF4); // Soft Violet
        return t;
    }
    static ThemeData dysektTheme()
    {
        ThemeData t;
        t.name             = "dysekt";
        t.background       = juce::Colour (0xFF000000);   // absolute black chassis
        t.waveformBg       = juce::Colour (0xFF060609);   // near-void waveform panel
        t.darkBar          = juce::Colour (0xFF0D0D12);   // charcoal panel bars
        t.foreground       = juce::Colour (0xFFB8C8D0);   // cool blue-grey text
        t.header           = juce::Colour (0xFF0A0A0F);   // top bar
        t.waveform         = juce::Colour (0xFF00FFE0);   // neon teal waveform
        t.selectionOverlay = juce::Colour (0xFF003830);   // deep teal selection tint
        t.lockActive       = juce::Colour (0xFF00FFE0);   // teal lock-on
        t.lockInactive     = juce::Colour (0xFF1A2028);   // dim steel lock-off
        t.gridLine         = juce::Colour (0xFF101418);   // barely-visible grid
        t.accent           = juce::Colour (0xFF00FFE0);   // #00ffe0 — the single glow colour
        t.button           = juce::Colour (0xFF141A20);   // elevated panel button
        t.buttonHover      = juce::Colour (0xFF1C2830);   // teal-tinted hover
        t.separator        = juce::Colour (0xFF181E24);   // steel divider
        // Slice palette: 16 vivid neons that read clearly against near-black
        t.slicePalette[0 ] = juce::Colour (0xFFFF0090); // Hot Magenta
        t.slicePalette[1 ] = juce::Colour (0xFF00FF6C); // Toxic Lime
        t.slicePalette[2 ] = juce::Colour (0xFFFF5C00); // Molten Orange
        t.slicePalette[3 ] = juce::Colour (0xFF00D4FF); // Ice Blue
        t.slicePalette[4 ] = juce::Colour (0xFFFFE000); // Radioactive Yellow
        t.slicePalette[5 ] = juce::Colour (0xFF7000FF); // UV Violet
        t.slicePalette[6 ] = juce::Colour (0xFF00FFE0); // Absinthe (= accent)
        t.slicePalette[7 ] = juce::Colour (0xFFFF1A30); // Alarm Red
        t.slicePalette[8 ] = juce::Colour (0xFF30FF10); // Neon Green
        t.slicePalette[9 ] = juce::Colour (0xFFFF0070); // Acid Pink
        t.slicePalette[10] = juce::Colour (0xFF0090FF); // Reactor Blue
        t.slicePalette[11] = juce::Colour (0xFFFFAA00); // Hazard Amber
        t.slicePalette[12] = juce::Colour (0xFFFF3060); // Coral Crimson
        t.slicePalette[13] = juce::Colour (0xFF30B8FF); // Steel Teal
        t.slicePalette[14] = juce::Colour (0xFFC8FF00); // Chartreuse
        t.slicePalette[15] = juce::Colour (0xFF8060FF); // Lavender

        // Bank B — slices 17–32 (hue-rotated from Bank A)
        t.slicePalette[16] = juce::Colour (0xFFF40734); // Deep Rose
        t.slicePalette[17] = juce::Colour (0xFF07F4C4); // Sea Green
        t.slicePalette[18] = juce::Colour (0xFFF4B607); // Gold
        t.slicePalette[19] = juce::Colour (0xFF0773F4); // Periwinkle
        t.slicePalette[20] = juce::Colour (0xFFB8F407); // Lime Pulse
        t.slicePalette[21] = juce::Colour (0xFFC807F4); // Indigo
        t.slicePalette[22] = juce::Colour (0xFF07B8F4); // Mint Fizz
        t.slicePalette[23] = juce::Colour (0xFFF45A1E); // Crimson Pop
        t.slicePalette[24] = juce::Colour (0xFF15F44B); // Spring
        t.slicePalette[25] = juce::Colour (0xFFF40716); // Fuchsia
        t.slicePalette[26] = juce::Colour (0xFF0734F4); // Azure
        t.slicePalette[27] = juce::Colour (0xFFEAF407); // Honey
        t.slicePalette[28] = juce::Colour (0xFFF44D31); // Dusty Rose
        t.slicePalette[29] = juce::Colour (0xFF3168F4); // Powder Blue
        t.slicePalette[30] = juce::Colour (0xFF68F407); // Acid Lime
        t.slicePalette[31] = juce::Colour (0xFFB45CF4); // Soft Violet
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
        if (text.contains("name: cr8"))      return cr8Theme();
        if (text.contains("name: dysekt"))   return dysektTheme();

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
                if (idx >= 0 && idx < 32)
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
        for (int i = 0; i < 32; ++i)
            s << "slice" << (i + 1) << ": " << colourToHex (slicePalette[i]) << "\n";
        return s;
    }
};
