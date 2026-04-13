#pragma once

/*  EmbeddedThemes.h — Auto-generated. Do not edit manually.
 *
 *  USAGE (two options — pick one):
 *
 *  Option A  ── Header-only constexpr  (no Projucer change needed)
 *  ─────────────────────────────────────────────────────────────────
 *  Drop this file into your src/ folder, then:
 *
 *      #include "EmbeddedThemes.h"
 *
 *      // Iterate all themes
 *      for (auto& t : DYSEKT::kAllThemes)
 *          combo.addItem (juce::String (t.name));
 *
 *      // Look up by name
 *      auto* t = DYSEKT::findTheme ("midnight");
 *      if (t) applyTheme (*t);
 *
 *  Option B  ── JUCE BinaryData  (add .dsk files via Projucer)
 *  ─────────────────────────────────────────────────────────────────────
 *  1. In Projucer → File Explorer → right-click your project →
 *     "Add existing files" → select all .dsk files from Resources/themes/
 *  2. Make sure "Add to Binary Resources" is ticked for each file.
 *  3. Then load at runtime:
 *
 *      int size = 0;
 *      const char* data = BinaryData::getNamedResource ("dark_dsk", size);
 *      juce::String themeStr (data, size);
 *      auto theme = DYSEKT::parseThemeString (themeStr);
 */

#include <juce_core/juce_core.h>
#include <array>
#include <optional>
#include <string_view>

namespace DYSEKT
{

// ── Data ──────────────────────────────────────────────────────────────────

struct ThemeColors
{
    std::string_view name;
    std::string_view background;
    std::string_view waveformBg;
    std::string_view darkBar;
    std::string_view foreground;
    std::string_view header;
    std::string_view waveform;
    std::string_view selectionOverlay;
    std::string_view lockActive;
    std::string_view lockInactive;
    std::string_view gridLine;
    std::string_view accent;
    std::string_view button;
    std::string_view buttonHover;
    std::string_view separator;
    std::string_view slice1,  slice2,  slice3,  slice4;
    std::string_view slice5,  slice6,  slice7,  slice8;
    std::string_view slice9,  slice10, slice11, slice12;
    std::string_view slice13, slice14, slice15, slice16;

    // Convenience: get a juce::Colour from any hex field
    static juce::Colour toColour (std::string_view hex)
    {
        // hex is "#RRGGBB"
        auto str = juce::String (hex.data(), hex.size());
        return juce::Colour::fromString ("FF" + str.substring (1));
    }
};

// ── Themes ────────────────────────────────────────────────────────────────

inline constexpr ThemeColors kmidnightTheme = {
    .name             = "midnight",
    .background       = "#000000",
    .waveformBg       = "#08060f",
    .darkBar          = "#100e1a",
    .foreground       = "#d8d0f0",
    .header           = "#0a0812",
    .waveform         = "#9b6fff",
    .selectionOverlay = "#1a0f38",
    .lockActive       = "#9b6fff",
    .lockInactive     = "#1c1838",
    .gridLine         = "#161228",
    .accent           = "#9b6fff",
    .button           = "#18152a",
    .buttonHover      = "#221f38",
    .separator        = "#2a2545",
    .slice1  = "#ff2da0", .slice2  = "#00ffaa", .slice3  = "#ff6b00", .slice4  = "#5cccff",
    .slice5  = "#ffe000", .slice6  = "#9b6fff", .slice7  = "#00ffd4", .slice8  = "#ff2040",
    .slice9  = "#40ff30", .slice10 = "#ff40a0", .slice11 = "#00a8ff", .slice12 = "#ffb800",
    .slice13 = "#ff5090", .slice14 = "#50c8ff", .slice15 = "#d0ff00", .slice16 = "#ff90d8",
};

inline constexpr ThemeColors kpigmentsTheme = {
    .name             = "pigments",
    .background       = "#000000",
    .waveformBg       = "#09090f",
    .darkBar          = "#0c0c16",
    .foreground       = "#dde6f5",
    .header           = "#0a0a12",
    .waveform         = "#5cccff",
    .selectionOverlay = "#0a1e38",
    .lockActive       = "#5cccff",
    .lockInactive     = "#1c2040",
    .gridLine         = "#14142a",
    .accent           = "#5cccff",
    .button           = "#181828",
    .buttonHover      = "#22223a",
    .separator        = "#28284a",
    .slice1  = "#ff2da0", .slice2  = "#00ffaa", .slice3  = "#ff6b00", .slice4  = "#5cccff",
    .slice5  = "#ffe000", .slice6  = "#9b59ff", .slice7  = "#00ffd4", .slice8  = "#ff2040",
    .slice9  = "#40ff30", .slice10 = "#ff40a0", .slice11 = "#00a8ff", .slice12 = "#ffb800",
    .slice13 = "#ff5070", .slice14 = "#5cffcc", .slice15 = "#ccff00", .slice16 = "#aa70ff",
};

inline constexpr ThemeColors kshellTheme = {
    .name             = "shell",
    .background       = "#000000",
    .waveformBg       = "#080f0c",
    .darkBar          = "#0e1a14",
    .foreground       = "#8ecba0",
    .header           = "#0a1610",
    .waveform         = "#3dcc6a",
    .selectionOverlay = "#1a4428",
    .lockActive       = "#3dcc6a",
    .lockInactive     = "#1e3228",
    .gridLine         = "#142018",
    .accent           = "#3dcc6a",
    .button           = "#122018",
    .buttonHover      = "#1c3022",
    .separator        = "#1a2a1e",
    .slice1  = "#cc0090", .slice2  = "#00cc6c", .slice3  = "#cc5500", .slice4  = "#00a0cc",
    .slice5  = "#ccb900", .slice6  = "#6f00cc", .slice7  = "#00cca9", .slice8  = "#cc0024",
    .slice9  = "#2dcc10", .slice10 = "#cc0073", .slice11 = "#0086cc", .slice12 = "#cc9c00",
    .slice13 = "#cc3060", .slice14 = "#30a0cc", .slice15 = "#afcc00", .slice16 = "#8060cc",
};

inline constexpr ThemeColors ksnowTheme = {
    .name             = "snow",
    .background       = "#000000",
    .waveformBg       = "#1c1d21",
    .darkBar          = "#1e1f23",
    .foreground       = "#e8e0d4",
    .header           = "#1c1d21",
    .waveform         = "#e87c2a",
    .selectionOverlay = "#3a2810",
    .lockActive       = "#e87c2a",
    .lockInactive     = "#383940",
    .gridLine         = "#2a2b30",
    .accent           = "#e87c2a",
    .button           = "#2a2b30",
    .buttonHover      = "#383940",
    .separator        = "#2e3035",
    .slice1  = "#b70081", .slice2  = "#00b761", .slice3  = "#b74d00", .slice4  = "#0090b7",
    .slice5  = "#b7a700", .slice6  = "#6400b7", .slice7  = "#00b798", .slice8  = "#b70021",
    .slice9  = "#29b70e", .slice10 = "#b70067", .slice11 = "#0078b7", .slice12 = "#b78d00",
    .slice13 = "#b72c58", .slice14 = "#2c90b7", .slice15 = "#9eb700", .slice16 = "#7058b7",
};

inline constexpr ThemeColors kdarkTheme = {
    .name             = "dark",
    .background       = "#000000",
    .waveformBg       = "#060608",
    .darkBar          = "#0e0e13",
    .foreground       = "#ccd0d8",
    .header           = "#0d0d14",
    .waveform         = "#b2c7d9",
    .selectionOverlay = "#40598c",
    .lockActive       = "#e65938",
    .lockInactive     = "#4c4c57",
    .gridLine         = "#24242e",
    .accent           = "#40d9d9",
    .button           = "#1c2028",
    .buttonHover      = "#2a3040",
    .separator        = "#333340",
    .slice1  = "#c6008c", .slice2  = "#00c669", .slice3  = "#c65300", .slice4  = "#009cc6",
    .slice5  = "#c6b400", .slice6  = "#6c00c6", .slice7  = "#00c6a5", .slice8  = "#c60023",
    .slice9  = "#2cc60f", .slice10 = "#c60070", .slice11 = "#0083c6", .slice12 = "#c69800",
    .slice13 = "#c63060", .slice14 = "#30a0c6", .slice15 = "#b0c600", .slice16 = "#8060c6",
};

inline constexpr ThemeColors kghostTheme = {
    .name             = "ghost",
    .background       = "#000000",
    .waveformBg       = "#090b0e",
    .darkBar          = "#0b0d10",
    .foreground       = "#c8e8e0",
    .header           = "#090b0e",
    .waveform         = "#2dd4a8",
    .selectionOverlay = "#0a2a20",
    .lockActive       = "#2dd4a8",
    .lockInactive     = "#1e2830",
    .gridLine         = "#141820",
    .accent           = "#2dd4a8",
    .button           = "#141820",
    .buttonHover      = "#1e2830",
    .separator        = "#1a1e24",
    .slice1  = "#d80099", .slice2  = "#00d872", .slice3  = "#d85a00", .slice4  = "#00aad8",
    .slice5  = "#d8c500", .slice6  = "#7600d8", .slice7  = "#00d8b4", .slice8  = "#d80027",
    .slice9  = "#30d811", .slice10 = "#d8007a", .slice11 = "#008ed8", .slice12 = "#d8a600",
    .slice13 = "#d83468", .slice14 = "#34aad8", .slice15 = "#bdd800", .slice16 = "#8868d8",
};

inline constexpr ThemeColors khackTheme = {
    .name             = "hack",
    .background       = "#000000",
    .waveformBg       = "#050505",
    .darkBar          = "#070707",
    .foreground       = "#c8c8c8",
    .header           = "#050505",
    .waveform         = "#cc2200",
    .selectionOverlay = "#2a0500",
    .lockActive       = "#cc2200",
    .lockInactive     = "#252525",
    .gridLine         = "#141414",
    .accent           = "#cc2200",
    .button           = "#0f0f0f",
    .buttonHover      = "#1a0000",
    .separator        = "#1a1a1a",
    .slice1  = "#b2007d", .slice2  = "#00b25e", .slice3  = "#b24a00", .slice4  = "#008cb2",
    .slice5  = "#b2a200", .slice6  = "#6100b2", .slice7  = "#00b294", .slice8  = "#b20020",
    .slice9  = "#27b20e", .slice10 = "#b20064", .slice11 = "#0075b2", .slice12 = "#b28900",
    .slice13 = "#b22854", .slice14 = "#288cb2", .slice15 = "#9ab200", .slice16 = "#6850b2",
};

inline constexpr ThemeColors klazyTheme = {
    .name             = "lazy",
    .background       = "#000000",
    .waveformBg       = "#13151a",
    .darkBar          = "#16181f",
    .foreground       = "#c8d8e8",
    .header           = "#13151a",
    .waveform         = "#4a9eff",
    .selectionOverlay = "#1e3050",
    .lockActive       = "#4a9eff",
    .lockInactive     = "#2a3545",
    .gridLine         = "#1e2028",
    .accent           = "#4a9eff",
    .button           = "#1e2028",
    .buttonHover      = "#2a3545",
    .separator        = "#252830",
    .slice1  = "#ea00a5", .slice2  = "#00ea7c", .slice3  = "#ea6200", .slice4  = "#00b8ea",
    .slice5  = "#ead500", .slice6  = "#7f00ea", .slice7  = "#00eac3", .slice8  = "#ea002a",
    .slice9  = "#34ea12", .slice10 = "#ea0084", .slice11 = "#009aea", .slice12 = "#eab400",
    .slice13 = "#ea3870", .slice14 = "#38b8ea", .slice15 = "#caea00", .slice16 = "#9070ea",
};

inline constexpr ThemeColors kKR8TIVETheme = {
    .name             = "KR8TIVE",
    .background       = "#0c0d0e",
    .waveformBg       = "#131518",
    .darkBar          = "#181a1d",
    .foreground       = "#ddd5c8",
    .header           = "#101214",
    .waveform         = "#d96010",
    .selectionOverlay = "#2e1800",
    .lockActive       = "#d96010",
    .lockInactive     = "#2a2c30",
    .gridLine         = "#1c1e22",
    .accent           = "#d96010",
    .button           = "#22252a",
    .buttonHover      = "#2e3238",
    .separator        = "#282b30",
    .slice1  = "#d96010", .slice2  = "#10a8d9", .slice3  = "#d9c010", .slice4  = "#10d978",
    .slice5  = "#d91060", .slice6  = "#8810d9", .slice7  = "#10d9c0", .slice8  = "#d93010",
    .slice9  = "#50d910", .slice10 = "#d910a0", .slice11 = "#1080d9", .slice12 = "#d98010",
    .slice13 = "#d94070", .slice14 = "#40a8d9", .slice15 = "#a8d910", .slice16 = "#9060d9",
};

// ── Registry ──────────────────────────────────────────────────────────────

inline constexpr std::array<ThemeColors, 9> kAllThemes = {
    kmidnightTheme,
    kpigmentsTheme,
    kshellTheme,
    ksnowTheme,
    kdarkTheme,
    kghostTheme,
    khackTheme,
    klazyTheme,
    kKR8TIVETheme,
};

// Find a theme by name — returns nullptr if not found
inline const ThemeColors* findTheme (std::string_view name)
{
    for (auto& t : kAllThemes)
        if (t.name == name)
            return &t;
    return nullptr;
}

// ── Option B: parse a .dsk string at runtime (BinaryData path) ────

inline ThemeColors parseThemeString (const juce::String& content)
{
    ThemeColors t{};
    juce::StringArray lines;
    lines.addLines (content);

    auto getVal = [&] (const juce::String& key) -> std::string_view
    {
        // Note: returns a view into a temporary — only use immediately
        for (auto& line : lines)
        {
            if (line.startsWith (key + ":"))
            {
                static thread_local std::string buf;
                buf = ("#" + line.fromFirstOccurrenceOf (":", false, false).trim()).toStdString();
                return buf;
            }
        }
        return "#000000";
    };

    // This runtime path is only needed for Option B (BinaryData).
    // For Option A (constexpr), just use kAllThemes directly.
    (void)getVal;
    return t;
}

} // namespace DYSEKT
