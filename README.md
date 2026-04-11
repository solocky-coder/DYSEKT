# DYSEKT Embedded Themes — Drop-in Package

## Folder structure

```
DYSEKT_themes/
├── src/
│   └── EmbeddedThemes.h        ← drop into your src/ folder
└── Resources/
    └── themes/                 ← add to Projucer as Binary Resources (optional)
        ├── midnight.dysektstyle
        ├── pigments.dysektstyle
        ├── shell.dysektstyle
        ├── snow.dysektstyle
        ├── dark.dysektstyle
        ├── ghost.dysektstyle
        ├── hack.dysektstyle
        ├── lazy.dysektstyle
        └── KR8TIVE.dysektstyle
```

---

## Option A — Header-only (recommended, zero Projucer changes)

1. Copy `src/EmbeddedThemes.h` → your project's `src/` folder.
2. In any file that needs themes:

```cpp
#include "EmbeddedThemes.h"

// Populate a dropdown
for (auto& t : DYSEKT::kAllThemes)
    themeCombo.addItem (juce::String (t.name), ...);

// Apply a theme by name
if (auto* t = DYSEKT::findTheme ("midnight"))
{
    auto bg     = DYSEKT::ThemeColors::toColour (t->background);
    auto accent = DYSEKT::ThemeColors::toColour (t->accent);
    // pass colours to your LookAndFeel / ThemeManager as usual
}
```

All colours are `constexpr` — no file I/O, no parsing at runtime.

---

## Option B — JUCE BinaryData

Use this if you want the raw `.dysektstyle` files accessible via `BinaryData::`.

1. In **Projucer** → File Explorer → right-click your project root →  
   **"Add existing files…"** → select all files in `Resources/themes/`.
2. For each file, tick **"Add to Binary Resources"** in the properties panel.
3. Re-save the Projucer project (Ctrl+S) — this regenerates `BinaryData.h/.cpp`.
4. Load at runtime:

```cpp
#include "EmbeddedThemes.h"   // still needed for ThemeColors struct
#include <BinaryData.h>

int   size = 0;
auto* data = BinaryData::getNamedResource ("midnight_dysektstyle", size);
// 'data' is the raw text of the .dysektstyle file
```

---

## Compiler requirement

`EmbeddedThemes.h` uses C++17 designated initialisers and `std::string_view`.  
Make sure your Projucer project has **C++ Language Standard** set to **C++17** or later.
