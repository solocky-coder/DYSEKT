#include "ShortcutsPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "MidiLearnDialog.h"

ShortcutsPanel::ShortcutsPanel (DysektProcessor& proc)
    : processor (proc)
{
    buildShortcutData();

    titleLabel.setText ("Settings & Shortcuts", juce::dontSendNotification);
    titleLabel.setFont (DysektLookAndFeel::makeFont (15.0f, true));
    titleLabel.setColour (juce::Label::textColourId, getTheme().foreground);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    closeBtn.setColour (juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    closeBtn.setColour (juce::TextButton::textColourOffId, getTheme().foreground.withAlpha (0.75f));
    closeBtn.onClick = [this] { if (onDismiss) onDismiss(); };
    addAndMakeVisible (closeBtn);

    themeBtn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
    themeBtn.setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    themeBtn.setTooltip ("Open the theme colour editor");
    themeBtn.onClick = [this] { if (onThemeRequest) onThemeRequest(); };
    addAndMakeVisible (themeBtn);

    auto styleScaleBtn = [this] (juce::TextButton& btn)
    {
        btn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn.setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    };

    styleScaleBtn (scaleDownBtn);
    scaleDownBtn.onClick = [this]
    {
        if (auto* p = processor.apvts.getParameter (ParamIds::uiScale))
        {
            float cur = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
            float nxt = juce::jlimit (0.5f, 3.0f, cur - 0.25f);
            p->setValueNotifyingHost (p->convertTo0to1 (nxt));
            updateScaleLcd();
        }
    };
    addAndMakeVisible (scaleDownBtn);

    styleScaleBtn (scaleUpBtn);
    scaleUpBtn.onClick = [this]
    {
        if (auto* p = processor.apvts.getParameter (ParamIds::uiScale))
        {
            float cur = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
            float nxt = juce::jlimit (0.5f, 3.0f, cur + 0.25f);
            p->setValueNotifyingHost (p->convertTo0to1 (nxt));
            updateScaleLcd();
        }
    };
    addAndMakeVisible (scaleUpBtn);

    scaleLcd.setFont (DysektLookAndFeel::makeMonoFont (11.0f));
    scaleLcd.setColour (juce::Label::textColourId,       getTheme().foreground);
    scaleLcd.setColour (juce::Label::backgroundColourId, getTheme().background.withAlpha (0.6f));
    scaleLcd.setJustificationType (juce::Justification::centred);
    updateScaleLcd();
    addAndMakeVisible (scaleLcd);

    searchBox.setTextToShowWhenEmpty ("Search shortcuts...", getTheme().foreground.withAlpha (0.4f));
    searchBox.setFont (DysektLookAndFeel::makeFont (11.0f));
    searchBox.setColour (juce::TextEditor::backgroundColourId, getTheme().background.withAlpha (0.6f));
    searchBox.setColour (juce::TextEditor::textColourId,       getTheme().foreground);
    searchBox.setColour (juce::TextEditor::outlineColourId,    getTheme().accent.withAlpha (0.4f));
    searchBox.onTextChange = [this]
    {
        currentFilter = searchBox.getText().toLowerCase();
        repaint();
    };
    addAndMakeVisible (searchBox);

    setupManualViewer();   // ← PDF viewer setup
    setWantsKeyboardFocus (true);
}

ShortcutsPanel::~ShortcutsPanel() = default;

//==============================================================================
// Finds the bundled user manual PDF from common locations.
juce::File ShortcutsPanel::findManualPdf()
{
    // 1) macOS: inside the plugin/app bundle Resources folder
    auto bundle = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    auto f = bundle.getChildFile ("Contents/Resources/DYSEKT_Manual.pdf");
    if (f.existsAsFile()) return f;

    // 2) Next to the VST3 / component / DLL on Windows & macOS
    f = bundle.getParentDirectory().getChildFile ("DYSEKT_Manual.pdf");
    if (f.existsAsFile()) return f;

    // 3) User application data folder  (~/Library/Application Support/DYSEKT/ etc.)
    f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("DYSEKT").getChildFile ("DYSEKT_Manual.pdf");
    if (f.existsAsFile()) return f;

    return {};
}

void ShortcutsPanel::setupManualViewer()
{
    manualViewer = std::make_unique<juce::WebBrowserComponent>();

    auto pdf = findManualPdf();
    if (pdf.existsAsFile())
        manualViewer->goToURL ("file://" + pdf.getFullPathName());
    else
        // Graceful placeholder — swap for a real URL if you host the manual online
        manualViewer->goToURL ("about:blank");

    addAndMakeVisible (*manualViewer);
}

//==============================================================================
void ShortcutsPanel::updateScaleLcd()
{
    float cur = processor.apvts.getRawParameterValue (ParamIds::uiScale)->load();
    scaleLcd.setText (juce::String (cur, 2) + "x", juce::dontSendNotification);
}

void ShortcutsPanel::drawScaleSection (juce::Graphics& /*g*/, juce::Rectangle<int>& area)
{
    area.removeFromTop (24);  // scale button row
    area.removeFromTop (4);   // gap before next section
}

void ShortcutsPanel::buildShortcutData()
{
    categories.clear();

    {
        ShortcutCategory slicing;
        slicing.title = "Slicing";
        slicing.entries = {
            { "Double-click", "Add slice at position"  },
            { "L",            "MIDI Slice"              },
            { "Del",          "Delete selected slice"   },
        };
        categories.push_back (std::move (slicing));
    }

    {
        ShortcutCategory nav;
        nav.title = "Navigation";
        nav.entries = {
            { "Left / Right", "Select previous / next slice" },
        };
        categories.push_back (std::move (nav));
    }

    {
        ShortcutCategory editing;
        editing.title = "Editing";
        editing.entries = {
            { "Ctrl+Z", "Undo"                          },
            { "F",      "Toggle MIDI-selects-slice mode" },
            { "M",      "Toggle MIDI Learn dialog"       },
        };
        categories.push_back (std::move (editing));
    }

    {
        ShortcutCategory misc;
        misc.title = "General";
        misc.entries = {
            { "?  (QWERTZ: Shift+\xc3\x9f)", "Toggle this panel"        },
            { "Esc",                           "Close panel / cancel"    },
        };
        categories.push_back (std::move (misc));
    }
}

bool ShortcutsPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onDismiss) onDismiss();
        return true;
    }
    return false;
}

void ShortcutsPanel::mouseDown (const juce::MouseEvent& e)
{
    // ── Trim preference ───────────────────────────────────────────────────────
    const int pref    = processor.trimPreference.load (std::memory_order_relaxed);
    int       newPref = pref;

    if (trimAlwaysRect.contains (e.getPosition()))
        newPref = DysektProcessor::TrimPrefAlways;
    else if (trimNeverRect.contains (e.getPosition()))
        newPref = DysektProcessor::TrimPrefNever;
    else if (trimLongRect.contains (e.getPosition()))
        newPref = DysektProcessor::TrimPrefAsk;

    if (newPref != pref)
    {
        processor.trimPreference.store (newPref, std::memory_order_relaxed);
        repaint();
        return;
    }

    // ── Interface mode ────────────────────────────────────────────────────────
    int newMode = currentUiMode;

    if (uiModeWaveRect.contains (e.getPosition()))
        newMode = 0;
    else if (uiModeGridRect.contains (e.getPosition()))
        newMode = 1;

    if (newMode != currentUiMode)
    {
        currentUiMode = newMode;
        repaint();
        if (onUiModeChanged)
            onUiModeChanged (currentUiMode);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ShortcutsPanel::drawTrimPrefsSection (juce::Graphics& g, juce::Rectangle<int>& area)
{
    const int pref  = processor.trimPreference.load (std::memory_order_relaxed);
    const int rowH  = 22;
    const int btnH  = 18;
    const int gap   = 6;

    g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
    g.setColour (getTheme().accent);
    g.drawText ("TRIM ON LOAD", area.removeFromTop (rowH), juce::Justification::centredLeft);
    area.removeFromTop (2);

    struct Option { juce::String label; int value; juce::Rectangle<int>* rect; };
    Option opts[] = {
        { "Always Trim (Default)", DysektProcessor::TrimPrefAlways, &trimAlwaysRect },
        { "Trim Long Samples",     DysektProcessor::TrimPrefAsk,    &trimLongRect   },
        { "Never Trim",            DysektProcessor::TrimPrefNever,  &trimNeverRect  },
    };

    for (auto& opt : opts)
    {
        auto row = area.removeFromTop (btnH);
        area.removeFromTop (gap);

        const bool active = (pref == opt.value);
        *opt.rect = row;

        const int dotR = 5;
        auto dotArea = row.removeFromLeft (dotR * 2 + 6);
        juce::Rectangle<float> dot (dotArea.getX() + 2.0f,
                                    dotArea.getCentreY() - (float) dotR,
                                    (float) dotR * 2.0f, (float) dotR * 2.0f);

        g.setColour (active ? getTheme().accent : getTheme().button);
        g.fillEllipse (dot);
        g.setColour (getTheme().accent.withAlpha (active ? 1.0f : 0.35f));
        g.drawEllipse (dot.reduced (0.5f), 1.0f);
        if (active)
        {
            g.setColour (getTheme().header);
            g.fillEllipse (dot.reduced (3.0f));
        }

        g.setFont (DysektLookAndFeel::makeFont (10.5f));
        g.setColour (active ? getTheme().foreground : getTheme().foreground.withAlpha (0.6f));
        g.drawText (opt.label, row.removeFromLeft (200), juce::Justification::centredLeft);
    }

    area.removeFromTop (4);
}

// ─────────────────────────────────────────────────────────────────────────────
void ShortcutsPanel::drawInterfaceSection (juce::Graphics& g, juce::Rectangle<int>& area)
{
    const int rowH = 22;
    const int btnH = 18;
    const int gap  = 6;

    g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
    g.setColour (getTheme().accent);
    g.drawText ("INTERFACE", area.removeFromTop (rowH), juce::Justification::centredLeft);
    area.removeFromTop (2);

    struct Option { juce::String label; int value; juce::Rectangle<int>* rect; };
    Option opts[] = {
        { "Waveform View", 0, &uiModeWaveRect },
        { "Pad Grid",      1, &uiModeGridRect },
    };

    for (auto& opt : opts)
    {
        auto row = area.removeFromTop (btnH);
        area.removeFromTop (gap);

        const bool active = (currentUiMode == opt.value);
        *opt.rect = row;

        const int dotR = 5;
        auto dotArea = row.removeFromLeft (dotR * 2 + 6);
        juce::Rectangle<float> dot (dotArea.getX() + 2.0f,
                                    dotArea.getCentreY() - (float) dotR,
                                    (float) dotR * 2.0f, (float) dotR * 2.0f);

        g.setColour (active ? getTheme().accent : getTheme().button);
        g.fillEllipse (dot);
        g.setColour (getTheme().accent.withAlpha (active ? 1.0f : 0.35f));
        g.drawEllipse (dot.reduced (0.5f), 1.0f);
        if (active)
        {
            g.setColour (getTheme().header);
            g.fillEllipse (dot.reduced (3.0f));
        }

        g.setFont (DysektLookAndFeel::makeFont (10.5f));
        g.setColour (active ? getTheme().foreground : getTheme().foreground.withAlpha (0.6f));
        g.drawText (opt.label, row.removeFromLeft (200), juce::Justification::centredLeft);
    }

    area.removeFromTop (4);
}

// ─────────────────────────────────────────────────────────────────────────────
void ShortcutsPanel::paint (juce::Graphics& g)
{
    // Dim overlay
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    auto panel = getLocalBounds().reduced (40, 30);
    g.setColour (getTheme().header);
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);
    g.setColour (getTheme().accent.withAlpha (0.5f));
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto content = panel.reduced (14, 6);
    content.removeFromTop (30 + 8 + 26 + 10); // title + gap + search + gap

    const int colW = content.getWidth() / 2;
    auto leftCol  = content.removeFromLeft (colW);
    auto rightCol = content;

    // ── Left column: settings + ALL shortcuts ────────────────────────────
    g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
    g.setColour (getTheme().accent);
    g.drawText ("UI SCALE", leftCol.removeFromTop (18), juce::Justification::centredLeft);
    drawScaleSection (g, leftCol);

    drawTrimPrefsSection  (g, leftCol);
    drawInterfaceSection  (g, leftCol);

    g.setColour (getTheme().separator.withAlpha (0.4f));
    g.drawHorizontalLine (leftCol.getY() + 2, (float) leftCol.getX(), (float) leftCol.getRight() - 8);
    leftCol.removeFromTop (10);

    const int rowH   = 18;
    const int catGap = 10;
    const int keysMin = 52, keysMax = 120;
    juce::Font keyFont = DysektLookAndFeel::makeFont (9.5f, true);

    // ── ALL categories go to the left column now ─────────────────────────
    for (const auto& cat : categories)
    {
        bool hasMatch = currentFilter.isEmpty();
        if (! hasMatch)
            for (const auto& e : cat.entries)
                if (e.keys.toLowerCase().contains (currentFilter)
                    || e.description.toLowerCase().contains (currentFilter))
                    { hasMatch = true; break; }
        if (! hasMatch) continue;

        g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
        g.setColour (getTheme().accent);
        g.drawText (cat.title.toUpperCase(), leftCol.removeFromTop (rowH),
                    juce::Justification::centredLeft);
        leftCol.removeFromTop (2);

        for (const auto& entry : cat.entries)
        {
            if (! currentFilter.isEmpty()
                && ! entry.keys.toLowerCase().contains (currentFilter)
                && ! entry.description.toLowerCase().contains (currentFilter))
                continue;

            const int textW  = (int) std::ceil (keyFont.getStringWidthFloat (entry.keys));
            const int keysW  = juce::jlimit (keysMin, keysMax, textW + 10);

            auto row     = leftCol.removeFromTop (rowH);
            auto keyRect = row.removeFromLeft (keysW);
            g.setColour (getTheme().button.withAlpha (0.9f));
            g.fillRoundedRectangle (keyRect.reduced (0, 2).toFloat(), 3.0f);
            g.setFont (keyFont);
            g.setColour (getTheme().accent);
            g.drawText (entry.keys, keyRect, juce::Justification::centred);

            row.removeFromLeft (6);
            g.setFont (DysektLookAndFeel::makeFont (10.5f));
            g.setColour (getTheme().foreground.withAlpha (0.85f));
            g.drawText (entry.description, row, juce::Justification::centredLeft);
        }
        leftCol.removeFromTop (catGap);
    }

    // ── Right column: "USER MANUAL" heading (viewer is a child component) ─
    rightCol.removeFromLeft (8);   // left inset to match child component
    g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
    g.setColour (getTheme().accent);
    g.drawText ("USER MANUAL", rightCol.removeFromTop (22), juce::Justification::centredLeft);

    // Draw a thin border around the viewer area so it looks intentional
    // when the PDF hasn't loaded yet
    if (manualViewer != nullptr)
    {
        g.setColour (getTheme().accent.withAlpha (0.25f));
        g.drawRoundedRectangle (manualViewer->getBounds().toFloat().expanded (1.0f), 4.0f, 1.0f);
    }
}

void ShortcutsPanel::resized()
{
    auto panel = getLocalBounds().reduced (40, 30);
    auto header = panel.reduced (14, 6);

    auto titleRow = header.removeFromTop (30);
    closeBtn.setBounds (titleRow.removeFromRight (30));
    themeBtn.setBounds (titleRow.removeFromRight (120));
    titleRow.removeFromRight (6);
    titleLabel.setBounds (titleRow);

    header.removeFromTop (8);
    searchBox.setBounds (header.removeFromTop (26));

    // ── Derive the same content rect used in paint() ──────────────────────
    auto content = panel.reduced (14, 6);
    content.removeFromTop (30 + 8 + 26 + 10);

    auto leftCol  = content.removeFromLeft (content.getWidth() / 2);
    auto rightCol = content;

    // ── Scale buttons (left column) ───────────────────────────────────────
    leftCol.removeFromTop (18); // "UI SCALE" heading

    auto scaleRow = leftCol.removeFromTop (24);
    const int btnW = 26;
    scaleDownBtn.setBounds (scaleRow.removeFromLeft (btnW));
    scaleRow.removeFromLeft (4);
    scaleLcd.setBounds (scaleRow.removeFromLeft (52));
    scaleRow.removeFromLeft (4);
    scaleUpBtn.setBounds (scaleRow.removeFromLeft (btnW));

    // ── PDF viewer (right column) ─────────────────────────────────────────
    if (manualViewer != nullptr)
    {
        rightCol.removeFromLeft   (8);  // left inset
        rightCol.removeFromRight  (4);  // right inset
        rightCol.removeFromTop    (22); // space for the "USER MANUAL" heading drawn in paint()
        rightCol.removeFromBottom (6);  // bottom inset
        manualViewer->setBounds (rightCol);
    }
}
