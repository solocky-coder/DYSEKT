// =============================================================================
//  SfzDropdownPanel.cpp  —  SF2 / SFZ instrument strip with inline file browser
// =============================================================================
#include "SfzDropdownPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include <set>

// ── Layout constants (header strip) ──────────────────────────────────────────
static constexpr int kPickerW      = 160;   // narrowed to fit ADSR knobs in strip
static constexpr int kKnobW        = 52;
static constexpr int kMeterW       = 60;
static constexpr int kPresetArrowW = 18;
static constexpr int kFolderIconW  = 20;
static constexpr int kPad          = 6;
static constexpr int kKnobGap      = 4;

// =============================================================================
//  SfzFileBrowser
// =============================================================================

SfzFileBrowser::SfzFileBrowser()
{
    list.setModel (this);
    list.setRowHeight (kRowH);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    // Default to user's Music directory, falling back to home
    auto startDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    navigateTo (startDir);
}

SfzFileBrowser::~SfzFileBrowser()
{
    stopTimer();
    list.setModel (nullptr);
}

// ── paint ────────────────────────────────────────────────────────────────────

void SfzFileBrowser::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();

    // Background
    g.setColour (theme.darkBar.darker (0.45f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    // Breadcrumb bar background
    g.setColour (theme.darkBar.darker (0.20f));
    g.fillRect (breadcrumbZone);

    // Up-button
    const bool upHover = upBtnZone.contains (getMouseXYRelative());
    const bool canGoUp = showingDrives ? false
                                       : true;  // always allow up (to drives list or parent)
    if (upHover && canGoUp)
    {
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (upBtnZone.toFloat(), 2.0f);
    }
    g.setFont (DysektLookAndFeel::makeFont (11.0f));
    g.setColour (canGoUp ? theme.accent.withAlpha (0.80f)
                         : theme.foreground.withAlpha (0.20f));
    g.drawText (u8"\u2191", upBtnZone, juce::Justification::centred, false);

    // Current path text (truncated from left)
    {
        const auto pathArea = breadcrumbZone.withTrimmedLeft (upBtnZone.getWidth() + 4)
                                            .withTrimmedRight (4);
        g.setFont (DysektLookAndFeel::makeFont (9.5f));
        g.setColour (theme.foreground.withAlpha (0.55f));

        juce::String display;
        if (showingDrives)
        {
            display = "Computer";
        }
        else
        {
            // Show last 2 path segments so it fits
            const auto parts = juce::StringArray::fromTokens (
                currentDir.getFullPathName(), juce::File::getSeparatorString(), "");
            const int n = parts.size();
            if      (n == 0) display = "/";
            else if (n <= 2) display = currentDir.getFullPathName();
            else             display = u8"\u2026" + juce::File::getSeparatorString()
                                     + parts[n - 2] + juce::File::getSeparatorString()
                                     + parts[n - 1];
        }

        g.drawText (display, pathArea, juce::Justification::centredLeft, true);
    }

    // Separator between breadcrumb and list
    g.setColour (theme.accent.withAlpha (0.12f));
    g.fillRect (0, breadcrumbZone.getBottom(), getWidth(), 1);
}

// ── resized ───────────────────────────────────────────────────────────────────

void SfzFileBrowser::resized()
{
    constexpr int upW = 24;
    breadcrumbZone = { 0, 0, getWidth(), kBreadcrumbH };
    upBtnZone      = { 2, 1, upW, kBreadcrumbH - 2 };

    list.setBounds (0, kBreadcrumbH + 1, getWidth(), getHeight() - kBreadcrumbH - 1);
}

// ── mouseDown ────────────────────────────────────────────────────────────────

void SfzFileBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (upBtnZone.contains (e.getPosition()))
    {
        navigateUp();
        return;
    }
    // Clicks below the breadcrumb are handled by the ListBox itself
}

// ── navigation ───────────────────────────────────────────────────────────────

void SfzFileBrowser::navigateTo (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    showingDrives = false;
    currentDir = dir;
    rebuildList();
    repaint();
}

void SfzFileBrowser::navigateUp()
{
    if (showingDrives) return;

    const auto parent = currentDir.getParentDirectory();
    if (parent == currentDir)
    {
        // Already at a filesystem root — show the drives list
        showDrivesList();
    }
    else
    {
        navigateTo (parent);
    }
}

void SfzFileBrowser::showDrivesList()
{
    showingDrives = true;
    rows.clear();

    juce::File::findFileSystemRoots (rows);
    // Remove any non-existent roots (unmounted, etc.)
    rows.removeIf ([] (const juce::File& f) { return ! f.isDirectory(); });

    list.updateContent();
    list.repaint();
    repaint();
}

void SfzFileBrowser::rebuildList()
{
    if (showingDrives) return;   // drives list is built in showDrivesList()

    rows.clear();

    // Directories first (hidden files excluded)
    auto dirs = currentDir.findChildFiles (
        juce::File::findDirectories, false, "*");
    dirs.removeIf ([] (const juce::File& f) { return f.isHidden(); });
    dirs.sort();

    // Matching files
    auto files = currentDir.findChildFiles (
        juce::File::findFiles, false, "*.sf2;*.sfz");
    files.removeIf ([] (const juce::File& f) { return f.isHidden(); });
    files.sort();

    rows.addArray (dirs);
    rows.addArray (files);

    list.updateContent();
    list.repaint();
    repaint();   // breadcrumb path has changed
}

void SfzFileBrowser::setRootDirectory (const juce::File& dir)
{
    navigateTo (dir);
}

// ── ListBoxModel ──────────────────────────────────────────────────────────────

int SfzFileBrowser::getNumRows() { return rows.size(); }

bool SfzFileBrowser::isDirectory (int row) const
{
    if (row < 0 || row >= rows.size()) return false;
    return rows[row].isDirectory();
}

juce::File SfzFileBrowser::fileForRow (int row) const
{
    if (row < 0 || row >= rows.size()) return {};
    return rows[row];
}

void SfzFileBrowser::paintListBoxItem (int row, juce::Graphics& g,
                                        int w, int h, bool selected)
{
    if (row < 0 || row >= rows.size()) return;

    const auto& theme = getTheme();
    const auto& f     = rows[row];
    const bool  isDir = f.isDirectory();

    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.14f));
        g.fillAll();
    }

    g.setFont (DysektLookAndFeel::makeFont (10.5f));

    if (isDir)
    {
        g.setColour (theme.accent.withAlpha (0.55f));
        // Use a drive icon for filesystem roots, folder icon otherwise
        const bool isRoot = (f == f.getParentDirectory());
        g.drawText (isRoot ? u8"\U0001F4BE" : u8"\U0001F4C1",
                    3, 0, 16, h, juce::Justification::centredLeft, false);
        g.setColour (selected ? theme.accent : theme.foreground.withAlpha (0.80f));
        // For drives show full path (e.g. "D:\"), for dirs show name only
        const juce::String label = isRoot ? f.getFullPathName() : f.getFileName();
        g.drawText (label, 22, 0, w - 26, h,
                    juce::Justification::centredLeft, true);
    }
    else
    {
        // Extension badge
        const auto ext = f.getFileExtension().toUpperCase().trimCharactersAtStart (".");
        const int  badgeW = 30;
        const auto badgeRect = juce::Rectangle<int> (w - badgeW - 4, (h - 12) / 2, badgeW, 12);
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (badgeRect.toFloat(), 2.0f);
        g.setFont (DysektLookAndFeel::makeFont (8.0f));
        g.setColour (theme.accent.withAlpha (0.80f));
        g.drawText (ext, badgeRect, juce::Justification::centred, false);

        // Filename
        g.setFont (DysektLookAndFeel::makeFont (10.5f));
        g.setColour (selected ? theme.accent : theme.foreground.withAlpha (0.80f));
        g.drawText (f.getFileNameWithoutExtension(), 6, 0, w - badgeW - 12, h,
                    juce::Justification::centredLeft, true);
    }
}

void SfzFileBrowser::listBoxItemClicked (int /*row*/, const juce::MouseEvent&)
{
    repaint();
}

void SfzFileBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    loadRow (row);
}

juce::String SfzFileBrowser::getTooltipForRow (int row)
{
    if (row < 0 || row >= rows.size()) return {};
    return rows[row].getFullPathName();
}

void SfzFileBrowser::loadRow (int row)
{
    if (row < 0 || row >= rows.size()) return;
    const auto& f = rows[row];

    if (f.isDirectory())
    {
        navigateTo (f);  // works for both regular dirs and drive roots
    }
    else if (onFileChosen)
    {
        onFileChosen (f);
    }
}

void SfzFileBrowser::timerCallback() {}

// =============================================================================
//  SfzDropdownPanel — constructor / destructor
// =============================================================================

SfzDropdownPanel::SfzDropdownPanel (DysektProcessor& p)
    : processor (p),
      keysPanel (p)
{
    addChildComponent (keysPanel);

    // ── Inline file browser ───────────────────────────────────────────────────
    fileBrowser.onFileChosen = [this] (const juce::File& f) { onFileChosen (f); };
    fileBrowser.onDismiss    = [this] { closeBrowser(); };
    addChildComponent (fileBrowser);

    startTimerHz (30);
}

SfzDropdownPanel::~SfzDropdownPanel() = default;

// =============================================================================
//  Layout
// =============================================================================

void SfzDropdownPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Strip order (left → right):
    // [picker 310] [gap] [TRN] [FINE] [REV] [CHO] [PAN] [VOL] [METER]
    auto strip = juce::Rectangle<int> (0, 0, w, kStripH).reduced (kPad, 0);
    strip.removeFromLeft (4);   // left margin

    // Preset picker (wider, no LOAD button)
    auto pickerSlot = strip.removeFromLeft (kPickerW);
    nameZone = pickerSlot.withSizeKeepingCentre (kPickerW, kStripH - 6);
    strip.removeFromLeft (kPad * 2);

    // Right-side knobs
    meterZone   = strip.removeFromRight (kMeterW);
    strip.removeFromRight (kPad);

    volZone    = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kKnobGap);
    panZone    = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kPad);

    chorusZone = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kKnobGap);
    reverbZone = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kPad);

    fineZone   = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kKnobGap);
    transZone  = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kPad);

    // ADSR knobs — now in the top strip, after picker
    adsrRelZone = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kKnobGap);
    adsrSusZone = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kKnobGap);
    adsrDecZone = strip.removeFromRight (kKnobW);
    strip.removeFromRight (kKnobGap);
    adsrAtkZone = strip.removeFromRight (kKnobW);

    // Sub-divide nameZone:
    //   [< arrow] [folder icon] [label] [> arrow]
    {
        auto z = nameZone;
        presetDecBtn  = z.removeFromLeft  (kPresetArrowW);
        presetIncBtn  = z.removeFromRight (kPresetArrowW);
        folderIconZone = z.removeFromRight (kFolderIconW);
        presetLabel   = z;
    }

    // ── Keyboard panel ────────────────────────────────────────────────────────
    const int kbY = kStripH;  // ADSR is now in the top strip, no extra row
    const int kbH = juce::jmax (60, h - kbY);
    keysPanel.setVisible (kbH > 0 && ! browserOpen);
    if (kbH > 0)
        keysPanel.setBounds (kPad, kbY, w - kPad * 2, kbH);
    else
        keysPanel.setBounds ({});

    // ── Inline browser overlay ────────────────────────────────────────────────
    if (browserOpen)
    {
        fileBrowser.setBounds (kPad, kStripH + 1, w - kPad * 2, h - kStripH - 1);
        fileBrowser.setVisible (true);
    }
    else
    {
        fileBrowser.setVisible (false);
        fileBrowser.setBounds ({});
    }
}

// =============================================================================
//  Browser open / close
// =============================================================================

void SfzDropdownPanel::openBrowser()
{
    if (browserOpen) return;
    browserOpen = true;

    // Navigate to the directory of the currently loaded file if there is one
    if (processor.sfzPlayer.isLoaded())
        fileBrowser.setRootDirectory (
            processor.sfzPlayer.getLoadedFile().getParentDirectory());

    resized();
    repaint();
}

void SfzDropdownPanel::closeBrowser()
{
    if (! browserOpen) return;
    browserOpen = false;
    resized();
    repaint();
}

void SfzDropdownPanel::onFileChosen (const juce::File& f)
{
    processor.sfzPlayer.loadFile (f);
    reloadZones (f);
    closeBrowser();
    repaint();
}

// =============================================================================
//  Paint
// =============================================================================

void SfzDropdownPanel::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    const int   w     = getWidth();
    const int   h     = getHeight();

    // Full background
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient bg (theme.darkBar.darker (0.35f), 0.f, 0.f,
                                  theme.darkBar.darker (0.10f), 0.f, (float) h, false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 4.0f);

        const int sepY = kStripH;
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRect (kPad, sepY, w - kPad * 2, 1);
    }

    drawHeaderStrip (g);
    drawAdsrStrip (g);

    g.setColour (theme.accent.withAlpha (0.45f));
    g.fillRect (0, 0, w, 1);
}

// =============================================================================
//  drawAdsrStrip
// =============================================================================

void SfzDropdownPanel::drawAdsrStrip (juce::Graphics& g) const
{
    // Attack: 0-30 s, normalised
    drawKnob (g, adsrAtkZone,
              juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzAttack()  / 30.0f),
              "ATK",
              juce::String (processor.sfzPlayer.getSfzAttack(), 2) + "s");

    // Decay: 0-30 s
    drawKnob (g, adsrDecZone,
              juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzDecay()   / 30.0f),
              "DEC",
              juce::String (processor.sfzPlayer.getSfzDecay(), 2) + "s");

    // Sustain: 0-100 %
    drawKnob (g, adsrSusZone,
              juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzSustain() / 100.0f),
              "SUS",
              juce::String (juce::roundToInt (processor.sfzPlayer.getSfzSustain())) + "%");

    // Release: 0-60 s
    drawKnob (g, adsrRelZone,
              juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzRelease() / 60.0f),
              "REL",
              juce::String (processor.sfzPlayer.getSfzRelease(), 2) + "s");
}

// =============================================================================
//  drawHeaderStrip
// =============================================================================

void SfzDropdownPanel::drawHeaderStrip (juce::Graphics& g) const
{
    drawPresetPicker (g);

    drawKnob (g, transZone, transToNorm (processor.sfzPlayer.getTranspose()),
              "TRN",
              [&]() -> juce::String {
                  const int s = processor.sfzPlayer.getTranspose();
                  return s == 0 ? "0st" : (s > 0 ? "+" : "") + juce::String (s) + "st";
              }());

    drawKnob (g, fineZone, fineToNorm (processor.sfzPlayer.getFineTune()),
              "FINE",
              [&]() -> juce::String {
                  const float c = processor.sfzPlayer.getFineTune();
                  return (c >= 0 ? "+" : "") + juce::String (c, 0) + "c";
              }());

    drawKnob (g, reverbZone, processor.sfzPlayer.getReverb(),
              "REV",
              juce::String (juce::roundToInt (processor.sfzPlayer.getReverb() * 100)) + "%");

    drawKnob (g, chorusZone, processor.sfzPlayer.getChorus(),
              "CHO",
              juce::String (juce::roundToInt (processor.sfzPlayer.getChorus() * 100)) + "%");

    drawKnob (g, panZone, panToNorm (processor.sfzPlayer.getPan()),
              "PAN",
              [&]() -> juce::String {
                  const float p = processor.sfzPlayer.getPan();
                  if (std::abs (p) < 0.01f) return "C";
                  const int pct = juce::roundToInt (std::abs (p) * 100);
                  return (p < 0 ? "L" : "R") + juce::String (pct);
              }());

    drawKnob (g, volZone, volToNorm (processor.sfzPlayer.getVolume()),
              "VOL",
              [&]() -> juce::String {
                  const float db = juce::Decibels::gainToDecibels (processor.sfzPlayer.getVolume());
                  return db <= -95.f ? "-inf" : juce::String (db, 1) + "dB";
              }());

    drawMeter (g);
}

// =============================================================================
//  drawPresetPicker
// =============================================================================

void SfzDropdownPanel::drawPresetPicker (juce::Graphics& g) const
{
    const auto& theme    = getTheme();
    const bool  isLoaded = processor.sfzPlayer.isLoaded();

    // Background
    {
        auto bg = nameZone.toFloat();
        g.setColour (browserOpen ? theme.accent.withAlpha (0.10f)
                                 : theme.darkBar.darker (0.12f));
        g.fillRoundedRectangle (bg, 3.0f);
        g.setColour (browserOpen ? theme.accent.withAlpha (0.55f)
                                 : theme.accent.withAlpha (0.20f));
        g.drawRoundedRectangle (bg.reduced (0.5f), 3.0f, 1.0f);
    }

    // Folder icon (always visible — this is the open/close toggle)
    {
        const bool hover = folderIconZone.contains (getMouseXYRelative());
        g.setFont (DysektLookAndFeel::makeFont (10.0f));
        g.setColour (browserOpen
                     ? theme.accent.withAlpha (0.90f)
                     : hover ? theme.accent.withAlpha (0.70f)
                             : theme.foreground.withAlpha (0.35f));
        g.drawText (u8"\U0001F4C1", folderIconZone, juce::Justification::centred, false);
    }

    // Arrow buttons (only useful when loaded + presets exist)
    auto drawArrow = [&] (juce::Rectangle<int> zone, const juce::String& sym)
    {
        const bool active = isLoaded && presetList.size() > 1 && ! browserOpen;
        const bool hover  = zone.contains (getMouseXYRelative()) && active;
        g.setColour (hover ? theme.accent.withAlpha (0.30f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (zone.toFloat(), 2.0f);
        g.setFont (DysektLookAndFeel::makeFont (11.0f));
        g.setColour (active ? theme.accent.withAlpha (0.75f)
                            : theme.foreground.withAlpha (0.20f));
        g.drawText (sym, zone, juce::Justification::centred, false);
    };
    drawArrow (presetDecBtn, "<");
    drawArrow (presetIncBtn, ">");

    // Label area
    {
        auto lbl = presetLabel;

        if (browserOpen)
        {
            // Browser is open — show a hint
            g.setFont (DysektLookAndFeel::makeFont (10.0f));
            g.setColour (theme.accent.withAlpha (0.70f));
            g.drawText ("browsing files \u2014 double-click to load", lbl,
                        juce::Justification::centred, true);
        }
        else if (! isLoaded)
        {
            g.setFont (DysektLookAndFeel::makeFont (10.5f));
            g.setColour (theme.foreground.withAlpha (0.38f));
            g.drawText ("click \U0001F4C1 or drop a file", lbl,
                        juce::Justification::centred, false);
        }
        else if (presetList.empty())
        {
            g.setFont (DysektLookAndFeel::makeFont (10.5f));
            g.setColour (theme.foreground.withAlpha (0.75f));
            g.drawText (processor.sfzPlayer.getLoadedFile().getFileNameWithoutExtension(),
                        lbl, juce::Justification::centred, true);
        }
        else
        {
            const int idx = juce::jlimit (0, (int) presetList.size() - 1,
                                          processor.sfzPlayer.getCurrentPresetIndex());
            const auto& info = presetList[(size_t) idx];

            // Top mini-label
            {
                auto topLine = lbl.removeFromTop (lbl.getHeight() / 2);
                g.setFont (DysektLookAndFeel::makeFont (8.5f));
                g.setColour (theme.foreground.withAlpha (0.38f));
                const auto caption =
                    processor.sfzPlayer.getLoadedFile().getFileNameWithoutExtension()
                    + "  B:" + juce::String (info.bank)
                    + " P:" + juce::String (info.preset);
                g.drawText (caption, topLine, juce::Justification::centred, true);
            }

            // Preset name
            g.setFont (DysektLookAndFeel::makeFont (11.0f));
            g.setColour (theme.foreground);
            g.drawText (info.name, lbl, juce::Justification::centred, true);
        }
    }
}

// =============================================================================
//  drawKnob
// =============================================================================

void SfzDropdownPanel::drawKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                                   float normalised, const juce::String& label,
                                   const juce::String& valueStr) const
{
    const auto& theme = getTheme();

    const int dia  = juce::jmin (bounds.getHeight() - 6, 26);
    const int cy   = bounds.getCentreY();
    const int cx   = bounds.getX() + 3 + dia / 2;
    const float r  = (float) dia * 0.5f;

    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float angle  = startA + normalised * (endA - startA);

    juce::Path track;
    track.addCentredArc ((float) cx, (float) cy, r - 1.f, r - 1.f, 0.f, startA, endA, true);
    g.setColour (theme.darkBar.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (2.0f));

    juce::Path fill;
    fill.addCentredArc ((float) cx, (float) cy, r - 1.f, r - 1.f, 0.f, startA, angle, true);
    g.setColour (theme.accent);
    g.strokePath (fill, juce::PathStrokeType (2.0f));

    const float tx = (float) cx + (r - 4.f) * std::cos (angle - juce::MathConstants<float>::halfPi);
    const float ty = (float) cy + (r - 4.f) * std::sin (angle - juce::MathConstants<float>::halfPi);
    g.setColour (theme.accent.brighter (0.3f));
    g.fillEllipse (tx - 2.f, ty - 2.f, 4.f, 4.f);

    const int textX = cx + (int) r + 5;
    const int textW = bounds.getRight() - textX;

    g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
    g.setColour (theme.foreground.withAlpha (0.38f));
    g.drawText (label,    textX, cy - 10, textW, 10, juce::Justification::centredLeft, false);

    g.setFont (DysektLookAndFeel::makeFont (8.5f));
    g.setColour (theme.foreground.withAlpha (0.82f));
    g.drawText (valueStr, textX, cy,      textW, 10, juce::Justification::centredLeft, false);
}

// =============================================================================
//  drawMeter
// =============================================================================

void SfzDropdownPanel::drawMeter (juce::Graphics& g) const
{
    const auto& theme = getTheme();
    auto area = meterZone.reduced (2, 6);

    const int barW = area.getWidth() / 2 - 2;
    const int barH = area.getHeight();

    auto leftBar  = juce::Rectangle<int> (area.getX(),              area.getY(), barW, barH);
    auto rightBar = juce::Rectangle<int> (area.getX() + barW + 4,  area.getY(), barW, barH);

    g.setColour (theme.darkBar.darker (0.2f));
    g.fillRoundedRectangle (leftBar.toFloat(),  2.0f);
    g.fillRoundedRectangle (rightBar.toFloat(), 2.0f);

    auto drawBar = [&] (juce::Rectangle<int> bar, float peak, float hold)
    {
        const int fillH = juce::roundToInt ((float) bar.getHeight() * juce::jlimit (0.f, 1.f, peak));
        if (fillH > 0)
        {
            juce::ColourGradient grad (theme.accent.withAlpha (0.85f), 0.f, (float) bar.getBottom(),
                                        theme.accent.brighter (0.5f),  0.f, (float) bar.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() - fillH).toFloat(), 2.0f);
        }
        const int holdY = bar.getBottom() - juce::roundToInt ((float) bar.getHeight()
                           * juce::jlimit (0.f, 1.f, hold));
        g.setColour (theme.accent.brighter (0.6f).withAlpha (0.7f));
        g.fillRect (bar.getX(), holdY - 1, bar.getWidth(), 2);
    };

    drawBar (leftBar,  meterL, holdL);
    drawBar (rightBar, meterR, holdR);
}

// =============================================================================
//  Timer
// =============================================================================

void SfzDropdownPanel::timerCallback()
{
    const float newL = processor.sfzPeakL.load (std::memory_order_relaxed);
    const float newR = processor.sfzPeakR.load (std::memory_order_relaxed);
    if (newL > holdL) holdL = newL;
    if (newR > holdR) holdR = newR;
    holdL *= kHoldDecay;
    holdR *= kHoldDecay;
    meterL = newL;
    meterR = newR;

    presetList = processor.sfzPlayer.getPresetList();

    // Deferred zone reload after preset switch (audio thread needs a few ticks
    // to apply the program change before we read the new zones).
    if (pendingZoneReloadTicks > 0)
    {
        --pendingZoneReloadTicks;
        if (pendingZoneReloadTicks == 0 && processor.sfzPlayer.isLoaded())
            reloadZones (processor.sfzPlayer.getLoadedFile());
    }

    repaint();
}

// =============================================================================
//  Preset navigation
// =============================================================================

void SfzDropdownPanel::selectPreset (int delta)
{
    if (presetList.empty()) return;

    const int cur  = processor.sfzPlayer.getCurrentPresetIndex();
    const int next = juce::jlimit (0, (int) presetList.size() - 1, cur + delta);

    if (next != cur)
    {
        processor.sfzPlayer.setPresetByIndex (next);
        // Don't reload zones immediately — the audio thread applies the program
        // change asynchronously. Schedule a reload a few timer ticks later.
        pendingZoneReloadTicks = 4;   // ~133ms at 30Hz
        repaint();
    }
}

// =============================================================================
//  Mouse events
// =============================================================================

void SfzDropdownPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();

    // ── Folder icon — toggle browser ─────────────────────────────────────────
    if (folderIconZone.contains (pos))
    {
        if (browserOpen) closeBrowser();
        else             openBrowser();
        return;
    }

    // ── Clicking the label area when browser is closed and no file loaded ─────
    if (presetLabel.contains (pos) && ! browserOpen
        && ! processor.sfzPlayer.isLoaded())
    {
        openBrowser();
        return;
    }

    // ── Clicking label when browser is open — close it ────────────────────────
    if (nameZone.contains (pos) && browserOpen)
    {
        closeBrowser();
        return;
    }

    // ── Preset arrows ─────────────────────────────────────────────────────────
    if (! browserOpen)
    {
        if (presetDecBtn.contains (pos)) { selectPreset (-1); return; }
        if (presetIncBtn.contains (pos)) { selectPreset (+1); return; }
    }

    // ── Knob drag start ───────────────────────────────────────────────────────
    struct { juce::Rectangle<int>& zone; ActiveKnob id; float val; } knobs[] =
    {
        { volZone,    ActiveKnob::Volume,      volToNorm   (processor.sfzPlayer.getVolume()) },
        { transZone,  ActiveKnob::Transpose,   transToNorm (processor.sfzPlayer.getTranspose()) },
        { panZone,    ActiveKnob::Pan,         panToNorm   (processor.sfzPlayer.getPan()) },
        { fineZone,   ActiveKnob::FineTune,    fineToNorm  (processor.sfzPlayer.getFineTune()) },
        { reverbZone, ActiveKnob::Reverb,      processor.sfzPlayer.getReverb() },
        { chorusZone, ActiveKnob::Chorus,      processor.sfzPlayer.getChorus() },
        { adsrAtkZone, ActiveKnob::AdsrAttack,  juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzAttack()  / 30.0f) },
        { adsrDecZone, ActiveKnob::AdsrDecay,   juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzDecay()   / 30.0f) },
        { adsrSusZone, ActiveKnob::AdsrSustain, juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzSustain() / 100.0f) },
        { adsrRelZone, ActiveKnob::AdsrRelease, juce::jlimit (0.f, 1.f, processor.sfzPlayer.getSfzRelease() / 60.0f) },
    };

    for (auto& k : knobs)
    {
        if (k.zone.contains (pos))
        {
            activeKnob   = k.id;
            dragStartY   = pos.y;
            dragStartVal = k.val;
            return;
        }
    }
}

void SfzDropdownPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (activeKnob == ActiveKnob::None) return;
    const float delta   = (float)(dragStartY - e.getPosition().y) / 120.0f;
    const float newNorm = juce::jlimit (0.f, 1.f, dragStartVal + delta);

    switch (activeKnob)
    {
        case ActiveKnob::Volume:      processor.sfzPlayer.setVolume    (normToVol   (newNorm)); break;
        case ActiveKnob::Transpose:   processor.sfzPlayer.setTranspose (normToTrans (newNorm)); break;
        case ActiveKnob::Pan:         processor.sfzPlayer.setPan       (normToPan   (newNorm)); break;
        case ActiveKnob::FineTune:    processor.sfzPlayer.setFineTune  (normToFine  (newNorm)); break;
        case ActiveKnob::Reverb:      processor.sfzPlayer.setReverb    (newNorm);               break;
        case ActiveKnob::Chorus:      processor.sfzPlayer.setChorus    (newNorm);               break;
        case ActiveKnob::AdsrAttack:  processor.sfzPlayer.setSfzAttack  (newNorm * 30.0f);      break;
        case ActiveKnob::AdsrDecay:   processor.sfzPlayer.setSfzDecay   (newNorm * 30.0f);      break;
        case ActiveKnob::AdsrSustain: processor.sfzPlayer.setSfzSustain (newNorm * 100.0f);     break;
        case ActiveKnob::AdsrRelease: processor.sfzPlayer.setSfzRelease (newNorm * 60.0f);      break;
        default: break;
    }
    repaint();
}

void SfzDropdownPanel::mouseUp (const juce::MouseEvent&)
{
    activeKnob = ActiveKnob::None;
}

void SfzDropdownPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    if (volZone.contains    (pos)) { processor.sfzPlayer.setVolume    (1.0f);  repaint(); }
    if (transZone.contains  (pos)) { processor.sfzPlayer.setTranspose (0);     repaint(); }
    if (panZone.contains    (pos)) { processor.sfzPlayer.setPan       (0.0f);  repaint(); }
    if (fineZone.contains   (pos)) { processor.sfzPlayer.setFineTune  (0.0f);  repaint(); }
    if (reverbZone.contains (pos)) { processor.sfzPlayer.setReverb    (0.4f);  repaint(); }
    if (chorusZone.contains (pos)) { processor.sfzPlayer.setChorus    (0.2f);  repaint(); }
    // ADSR defaults
    if (adsrAtkZone.contains (pos)) { processor.sfzPlayer.setSfzAttack  (0.005f);  repaint(); }
    if (adsrDecZone.contains (pos)) { processor.sfzPlayer.setSfzDecay   (0.1f);    repaint(); }
    if (adsrSusZone.contains (pos)) { processor.sfzPlayer.setSfzSustain (100.0f);  repaint(); }
    if (adsrRelZone.contains (pos)) { processor.sfzPlayer.setSfzRelease (0.05f);   repaint(); }
}

void SfzDropdownPanel::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& w)
{
    if (browserOpen) return;

    const auto  pos  = e.getPosition();
    const float step = w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f);

    if (nameZone.contains (pos))
    {
        if (w.deltaY > 0.05f)       selectPreset (+1);
        else if (w.deltaY < -0.05f) selectPreset (-1);
        return;
    }

    auto adjustNorm = [&] (float current, float s) {
        return juce::jlimit (0.f, 1.f, current + s);
    };

    if (volZone.contains (pos))
        processor.sfzPlayer.setVolume (normToVol (adjustNorm (volToNorm (processor.sfzPlayer.getVolume()), step)));
    else if (transZone.contains (pos))
        processor.sfzPlayer.setTranspose (normToTrans (adjustNorm (transToNorm (processor.sfzPlayer.getTranspose()), step)));
    else if (panZone.contains (pos))
        processor.sfzPlayer.setPan (normToPan (adjustNorm (panToNorm (processor.sfzPlayer.getPan()), step)));
    else if (fineZone.contains (pos))
        processor.sfzPlayer.setFineTune (normToFine (adjustNorm (fineToNorm (processor.sfzPlayer.getFineTune()), step)));
    else if (reverbZone.contains (pos))
        processor.sfzPlayer.setReverb (adjustNorm (processor.sfzPlayer.getReverb(), step));
    else if (chorusZone.contains (pos))
        processor.sfzPlayer.setChorus (adjustNorm (processor.sfzPlayer.getChorus(), step));
    else if (adsrAtkZone.contains (pos))
        processor.sfzPlayer.setSfzAttack  (juce::jlimit (0.f, 30.f,  adjustNorm (processor.sfzPlayer.getSfzAttack()  / 30.0f,  step) * 30.0f));
    else if (adsrDecZone.contains (pos))
        processor.sfzPlayer.setSfzDecay   (juce::jlimit (0.f, 30.f,  adjustNorm (processor.sfzPlayer.getSfzDecay()   / 30.0f,  step) * 30.0f));
    else if (adsrSusZone.contains (pos))
        processor.sfzPlayer.setSfzSustain (juce::jlimit (0.f, 100.f, adjustNorm (processor.sfzPlayer.getSfzSustain() / 100.0f, step) * 100.0f));
    else if (adsrRelZone.contains (pos))
        processor.sfzPlayer.setSfzRelease (juce::jlimit (0.f, 60.f,  adjustNorm (processor.sfzPlayer.getSfzRelease() / 60.0f,  step) * 60.0f));

    repaint();
}

// =============================================================================
//  File drag-and-drop
// =============================================================================

bool SfzDropdownPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".sf2" || ext == ".sfz")
            return true;
    }
    return false;
}

void SfzDropdownPanel::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".sf2" || ext == ".sfz")
        {
            juce::File file (f);
            processor.sfzPlayer.loadFile (file);
            reloadZones (file);
            closeBrowser();
            repaint();
            return;
        }
    }
}

// =============================================================================
//  panelDidShow
// =============================================================================

void SfzDropdownPanel::panelDidShow()
{
    presetList = processor.sfzPlayer.getPresetList();

    // Force a full layout pass first so keysPanel and its viewport have
    // correct dimensions before rebuildZoneMatrix() is called.
    resized();

    if (processor.sfzPlayer.isLoaded())
        reloadZones (processor.sfzPlayer.getLoadedFile());
    repaint();
}

// =============================================================================
//  Value mapping
// =============================================================================

float SfzDropdownPanel::volToNorm   (float linear) const { return juce::jlimit (0.f, 1.f, linear * 0.5f); }
float SfzDropdownPanel::normToVol   (float n)       const { return n * 2.0f; }
float SfzDropdownPanel::transToNorm (int semi)       const { return ((float) semi + 24.0f) / 48.0f; }
int   SfzDropdownPanel::normToTrans (float n)        const { return juce::roundToInt (n * 48.0f - 24.0f); }
float SfzDropdownPanel::panToNorm   (float p)        const { return (p + 1.0f) * 0.5f; }
float SfzDropdownPanel::normToPan   (float n)        const { return n * 2.0f - 1.0f; }
float SfzDropdownPanel::fineToNorm  (float cents)    const { return (cents + 100.0f) / 200.0f; }
float SfzDropdownPanel::normToFine  (float n)        const { return n * 200.0f - 100.0f; }

// =============================================================================
//  Zone parsers
// =============================================================================

static juce::Colour zoneColourDP (int index)
{
    static const juce::Colour palette[] = {
        juce::Colour (0xFF4FC3F7), juce::Colour (0xFF81C784), juce::Colour (0xFFFFB74D),
        juce::Colour (0xFFE57373), juce::Colour (0xFFBA68C8), juce::Colour (0xFF4DD0E1),
        juce::Colour (0xFFF06292), juce::Colour (0xFFA1887F),
    };
    return palette[index % 8];
}

std::vector<KeysPanel::Keyzone> SfzDropdownPanel::parseSfzZones (const juce::File& f)
{
    std::vector<KeysPanel::Keyzone> zones;
    const auto lines = juce::StringArray::fromLines (f.loadFileAsString());

    int          loKey    = 0, hiKey = 127;
    bool         inRegion = false;
    int          colIdx   = 0;
    juce::String sampleName;

    auto flush = [&]
    {
        if (inRegion && hiKey >= loKey)
        {
            KeysPanel::Keyzone z;
            z.loKey    = loKey;
            z.hiKey    = hiKey;
            z.loVel    = 0;
            z.hiVel    = 127;
            z.rootPitch= -1;
            z.isLooped = false;
            z.colour   = zoneColourDP (colIdx++);
            // Use the sample filename (without extension) as the zone name,
            // falling back to a generic "Zone N" label if none was found.
            z.name     = sampleName.isNotEmpty()
                       ? sampleName
                       : "Zone " + juce::String (colIdx);
            z.isSfz = true;
          zones.push_back (z);
            loKey = 0; hiKey = 127; sampleName = {};
        }
        inRegion = false;
    };

    for (auto line : lines)
    {
        // Use the original (case-preserved) line for sample= value extraction,
        // since file paths may be case-sensitive.
        const auto lineLower = line.trim().toLowerCase();
        const auto lineOrig  = line.trim();

        if (lineLower.startsWith ("<region>")) { flush(); inRegion = true; loKey = 0; hiKey = 127; sampleName = {}; }
        else if (lineLower.startsWith ("<group>") || lineLower.startsWith ("<global>")) flush();

        if (inRegion)
        {
            auto loRaw = lineLower.indexOf ("lokey=");
            if (loRaw >= 0)
                loKey = juce::jlimit (0, 127,
                    lineLower.substring (loRaw + 6).upToFirstOccurrenceOf (" ", false, false).trim().getIntValue());
            auto hiRaw = lineLower.indexOf ("hikey=");
            if (hiRaw >= 0)
                hiKey = juce::jlimit (0, 127,
                    lineLower.substring (hiRaw + 6).upToFirstOccurrenceOf (" ", false, false).trim().getIntValue());
            auto kRaw = lineLower.indexOf ("key=");
            if (kRaw >= 0 && lineLower.indexOf ("lokey=") < 0)
            {
                const int k = juce::jlimit (0, 127,
                    lineLower.substring (kRaw + 4).upToFirstOccurrenceOf (" ", false, false).trim().getIntValue());
                loKey = hiKey = k;
            }
            // Extract sample= value — strip directory and extension to get bare name.
            auto sRaw = lineLower.indexOf ("sample=");
            if (sRaw >= 0 && sampleName.isEmpty())
            {
                auto rawPath = lineOrig.substring (sRaw + 7)
                                       .upToFirstOccurrenceOf (" ",  false, false)
                                       .upToFirstOccurrenceOf ("\t", false, false)
                                       .trim();
                // Handle both / and \ path separators.
                auto bare = rawPath.fromLastOccurrenceOf ("/",  false, false)
                                   .fromLastOccurrenceOf ("\\", false, false);
                // Strip file extension.
                if (bare.contains ("."))
                    bare = bare.upToLastOccurrenceOf (".", false, false);
                sampleName = bare.isNotEmpty() ? bare : rawPath;
            }
        }
    }
    flush();
    return zones;
}

std::vector<KeysPanel::Keyzone> SfzDropdownPanel::parseSf2Zones (const juce::File& f)
{
    // ── The SF2 zone chain ────────────────────────────────────────────────────
    // phdr[presetIdx] → pbag[pbag_start..pbag_end)
    //   each pbag → pgen range, scan for oper=41 (instrument index)
    //     inst[instIdx] → ibag[ibag_start..ibag_end)
    //       each ibag → igen range
    //         oper=43 keyRange (lo=byte2, hi=byte3)
    //         oper=53 sampleID (amount word = shdr index)
    // shdr[sampleID] → 20-char sample name
    // ─────────────────────────────────────────────────────────────────────────

    std::vector<KeysPanel::Keyzone> zones;
    juce::FileInputStream stream (f);
    if (stream.failedToOpen()) return zones;

    char riff[4]; stream.read (riff, 4);
    if (juce::String::fromUTF8 (riff, 4) != "RIFF") return zones;
    stream.readInt();
    char sfbk[4]; stream.read (sfbk, 4);
    if (juce::String::fromUTF8 (sfbk, 4) != "sfbk") return zones;

    // Read all pdta sub-chunks we need
    juce::MemoryBlock phdrData, pbagData, pgenData,
                      instData, ibagData, igenData, shdrData;

    while (! stream.isExhausted())
    {
        char id[4]; if (stream.read (id, 4) < 4) break;
        const auto chunkId = juce::String::fromUTF8 (id, 4);
        const int  sz      = stream.readInt();
        if (chunkId == "LIST")
        {
            char listId[4]; stream.read (listId, 4);
            if (juce::String::fromUTF8 (listId, 4) == "pdta")
            {
                const int64_t pdtaEnd = stream.getPosition() + sz - 4;
                while (stream.getPosition() < pdtaEnd && ! stream.isExhausted())
                {
                    char sub[4]; if (stream.read (sub, 4) < 4) break;
                    const auto subId = juce::String::fromUTF8 (sub, 4);
                    const int  subSz = stream.readInt();
                    auto readChunk = [&] (juce::MemoryBlock& mb)
                    {
                        mb.setSize ((size_t) subSz);
                        stream.read (mb.getData(), subSz);
                    };
                    if      (subId == "phdr") readChunk (phdrData);
                    else if (subId == "pbag") readChunk (pbagData);
                    else if (subId == "pgen") readChunk (pgenData);
                    else if (subId == "inst") readChunk (instData);
                    else if (subId == "ibag") readChunk (ibagData);
                    else if (subId == "igen") readChunk (igenData);
                    else if (subId == "shdr") readChunk (shdrData);
                    else stream.skipNextBytes (subSz);
                }
                break;
            }
            stream.skipNextBytes (sz - 4);
        }
        else stream.skipNextBytes (sz);
    }

    if (phdrData.isEmpty() || pbagData.isEmpty() || pgenData.isEmpty() ||
        instData.isEmpty() || ibagData.isEmpty() || igenData.isEmpty())
        return zones;

    // ── Helper lambdas to read little-endian words from raw blocks ────────────
    auto u16 = [] (const juce::MemoryBlock& mb, size_t byteOffset) -> uint16_t
    {
        const auto* d = static_cast<const uint8_t*> (mb.getData());
        return (uint16_t)(d[byteOffset] | (d[byteOffset + 1] << 8));
    };
    auto u8at = [] (const juce::MemoryBlock& mb, size_t byteOffset) -> uint8_t
    {
        return static_cast<const uint8_t*> (mb.getData())[byteOffset];
    };
    auto nameAt = [] (const juce::MemoryBlock& mb, size_t byteOffset) -> juce::String
    {
        const char* p = static_cast<const char*> (mb.getData()) + byteOffset;
        return juce::String::fromUTF8 (p, 20).trimEnd();
    };

    // ── Build sample-name table from shdr (46 bytes/record) ──────────────────
    std::vector<juce::String> sampleNames;
    if (! shdrData.isEmpty())
    {
        const size_t n = shdrData.getSize() / 46;
        sampleNames.reserve (n);
        for (size_t i = 0; i < n; ++i)
            sampleNames.push_back (nameAt (shdrData, i * 46));
    }

    // ── Get preset index from SfzPlayer ──────────────────────────────────────
    const int presetIdx = processor.sfzPlayer.getCurrentPresetIndex();

    // phdr record = 38 bytes: name(20) bank(2) preset(2) pbag_idx(2) lib(4) genre(4) morph(4)
    const size_t numPresets = phdrData.getSize() / 38;
    if ((size_t) presetIdx + 1 >= numPresets) return zones;   // need sentinel too

    const size_t pbagStart = u16 (phdrData, (size_t) presetIdx       * 38 + 20 + 2 + 2);
    const size_t pbagEnd   = u16 (phdrData, ((size_t) presetIdx + 1) * 38 + 20 + 2 + 2);

    // pbag record = 4 bytes: gen_idx(2) mod_idx(2)
    // pgen record = 4 bytes: oper(2) amount(2)
    const size_t numPbag = pbagData.getSize() / 4;
    const size_t numPgen = pgenData.getSize() / 4;

    // inst record = 22 bytes: name(20) ibag_idx(2)
    // ibag record = 4 bytes: gen_idx(2) mod_idx(2)
    // igen record = 4 bytes: oper(2) lo(1) hi(1)
    const size_t numInst = instData.getSize() / 22;
    const size_t numIbag = ibagData.getSize() / 4;
    const size_t numIgen = igenData.getSize() / 4;

    // Walk all preset bags for this preset
    for (size_t bi = pbagStart; bi < pbagEnd && bi + 1 < numPbag; ++bi)
    {
        const size_t genStart = u16 (pbagData, bi       * 4);
        const size_t genEnd   = u16 (pbagData, (bi + 1) * 4);

        // Scan pgens for oper=41 (instrument reference)
        for (size_t gi = genStart; gi < genEnd && gi < numPgen; ++gi)
        {
            const uint16_t oper   = u16 (pgenData, gi * 4);
            const uint16_t amount = u16 (pgenData, gi * 4 + 2);
            if (oper != 41) continue;   // not an instrument ref

            const size_t instIdx = amount;
            if (instIdx + 1 >= numInst) continue;

            const size_t ibagStart = u16 (instData, instIdx       * 22 + 20);
            const size_t ibagEnd   = u16 (instData, (instIdx + 1) * 22 + 20);

            // Walk instrument bags
            for (size_t ibi = ibagStart; ibi < ibagEnd && ibi + 1 < numIbag; ++ibi)
            {
                const size_t igStart = u16 (ibagData, ibi       * 4);
                const size_t igEnd   = u16 (ibagData, (ibi + 1) * 4);

                int  loKey = 0, hiKey = 127, sampleId = -1;
                bool hasKeyRange = false;

                for (size_t igi = igStart; igi < igEnd && igi < numIgen; ++igi)
                {
                    const uint16_t ioper = u16 (igenData, igi * 4);
                    if (ioper == 43)    // keyRange
                    {
                        loKey = u8at (igenData, igi * 4 + 2);
                        hiKey = u8at (igenData, igi * 4 + 3);
                        hasKeyRange = true;
                    }
                    else if (ioper == 53)   // sampleID
                    {
                        sampleId = (int) u16 (igenData, igi * 4 + 2);
                    }
                }

                if (! hasKeyRange || hiKey < loKey || sampleId < 0) continue;

                KeysPanel::Keyzone z;
                z.loKey     = loKey;
                z.hiKey     = hiKey;
                z.loVel     = 0;
                z.hiVel     = 127;
                z.rootPitch = -1;
                z.isLooped  = false;
                z.isSfz     = false;

                if (sampleId < (int) sampleNames.size())
                    z.name = sampleNames[(size_t) sampleId];
                if (z.name.isEmpty() || z.name == "EOS")
                    z.name = "Zone " + juce::String (zones.size() + 1);

                zones.push_back (z);
            }
        }
    }

    // Sort by loKey and assign colours
    std::sort (zones.begin(), zones.end(), [] (auto& a, auto& b) { return a.loKey < b.loKey; });

    // De-duplicate by (loKey, hiKey, name) — some SF2s layer L/R stereo pairs
    // that have identical key ranges. Keep the first occurrence only.
    std::set<std::pair<int,int>> seen;
    std::vector<KeysPanel::Keyzone> deduped;
    for (auto& z : zones)
    {
        auto key = std::make_pair (z.loKey, z.hiKey);
        if (seen.insert (key).second)
            deduped.push_back (z);
    }

    for (size_t i = 0; i < deduped.size(); ++i)
        deduped[i].colour = zoneColourDP ((int) i);

    return deduped;
}

void SfzDropdownPanel::reloadZones (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    const bool isSfz = (ext == ".sfz");

    auto zones = isSfz ? parseSfzZones (f)
               : (ext == ".sf2") ? parseSf2Zones (f)
               : std::vector<KeysPanel::Keyzone>{};

    keysPanel.setSfzEditable (isSfz);
    keysPanel.setKeyzones (zones);

    if (! zones.empty())
        keysPanel.autoScrollToZones();

    // Wire the edit callback — only fires for SFZ (sfzEditable == true)
    keysPanel.onZoneEdited = [this, f] (int rowIndex, const KeysPanel::Keyzone& updated)
    {
        writeSfzZoneChange (f, rowIndex, updated);
    };
}

// =============================================================================
//  writeSfzZoneChange  —  patch one <region> block in the SFZ text file
// =============================================================================

// Helper: set or replace an opcode value within a region line-block.
// 'lines' is the full file split by line. 'regionStart' is the line index of
// the <region> header. We search forward (until the next <region>/<group> or
// EOF) for the opcode and replace it, or append it to the <region> line.
static void setOpcode (juce::StringArray& lines, int regionStart,
                       const juce::String& opcode, const juce::String& value)
{
    const juce::String target = opcode + "=";

    // Search within this region's block
    for (int i = regionStart; i < lines.size(); ++i)
    {
        const auto lower = lines[i].toLowerCase().trim();
        if (i > regionStart && (lower.startsWith ("<region>") ||
                                lower.startsWith ("<group>") ||
                                lower.startsWith ("<global>")))
            break;  // reached next block — opcode not found, append

        const int pos = lines[i].toLowerCase().indexOf (target);
        if (pos >= 0)
        {
            // Replace the value in-place, preserving surrounding tokens
            // Find end of the value token (next space or end of string)
            const int valStart = pos + target.length();
            const auto rest = lines[i].substring (valStart);
            const int valEnd = rest.indexOfChar (' ');
            const juce::String newLine = lines[i].substring (0, valStart)
                                        + value
                                        + (valEnd >= 0 ? rest.substring (valEnd) : "");
            lines.set (i, newLine);
            return;
        }
    }

    // Opcode not present — append it to the <region> header line
    lines.set (regionStart, lines[regionStart].trimEnd() + " " + target + value);
}

void SfzDropdownPanel::writeSfzZoneChange (const juce::File& f,
                                            int rowIndex,
                                            const KeysPanel::Keyzone& z)
{
    if (! f.existsAsFile()) return;

    auto lines = juce::StringArray::fromLines (f.loadFileAsString());

    // Find the Nth <region> block (rowIndex is 0-based count of parsed regions)
    int regionCount = -1;
    int regionLine  = -1;

    for (int i = 0; i < lines.size(); ++i)
    {
        if (lines[i].trim().toLowerCase().startsWith ("<region>"))
        {
            ++regionCount;
            if (regionCount == rowIndex)
            {
                regionLine = i;
                break;
            }
        }
    }

    if (regionLine < 0) return;  // region not found — bail

    // Patch each editable opcode
    auto noteStr = [] (int note) -> juce::String
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
    };

    setOpcode (lines, regionLine, "lokey",  noteStr (z.loKey));
    setOpcode (lines, regionLine, "hikey",  noteStr (z.hiKey));
    setOpcode (lines, regionLine, "lovel",  juce::String (z.loVel));
    setOpcode (lines, regionLine, "hivel",  juce::String (z.hiVel));

    if (z.rootPitch >= 0)
        setOpcode (lines, regionLine, "pitch_keycenter", noteStr (z.rootPitch));

    // Write extended fields (only for SFZ zones)
    setOpcode (lines, regionLine, "tune",         juce::String ((int) z.tuneCents));
    setOpcode (lines, regionLine, "pan",          juce::String (juce::roundToInt (z.pan * 100.f)));
    setOpcode (lines, regionLine, "volume",       juce::String (z.volDb, 2));
    setOpcode (lines, regionLine, "ampeg_release",juce::String (z.releaseSec, 3));

    if (z.isLooped)
        setOpcode (lines, regionLine, "loop_mode", "loop_continuous");
    else
        setOpcode (lines, regionLine, "loop_mode", "no_loop");

    // Write back — join with \n (preserve original line endings best-effort)
    const bool crlf = f.loadFileAsString().contains ("\r\n");
    const auto newContent = lines.joinIntoString (crlf ? "\r\n" : "\n");
    f.replaceWithText (newContent);

    // Hot-reload the SFZ player so changes take effect immediately
    processor.sfzPlayer.loadFile (f);
}
