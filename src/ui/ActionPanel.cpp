#include "ActionPanel.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"

ActionPanel::ActionPanel (DysektProcessor& p, WaveformView& wv)
    : processor (p), waveformView (wv)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn, &shortcutsBtn })
    {
        addAndMakeVisible (btn);
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }

    addSliceBtn.setClickingTogglesState(true);
    lazyChopBtn.setClickingTogglesState(true);

    auto syncButtonColours = [this] {
        updateToggleBtn(addSliceBtn, addSliceBtn.getToggleState());
        updateToggleBtn(lazyChopBtn, lazyChopBtn.getToggleState());
    };

    addSliceBtn.onClick = [this, syncButtonColours]
    {
        bool addActive = addSliceBtn.getToggleState();
        bool midiActive = lazyChopBtn.getToggleState();

        if (addActive && midiActive)
        {
            lazyChopBtn.setToggleState(false, juce::dontSendNotification);
            updateToggleBtn(lazyChopBtn, false);

            if (processor.lazyChop.isActive())
            {
                DysektProcessor::Command cmd;
                cmd.type = DysektProcessor::CmdLazyChopStop;
                processor.pushCommand(cmd);
            }
        }

        waveformView.setSliceDrawMode(addActive);
        syncButtonColours();
        repaint();
    };

    lazyChopBtn.onClick = [this, syncButtonColours]
    {
        bool midiActive = lazyChopBtn.getToggleState();
        bool addActive = addSliceBtn.getToggleState();

        if (midiActive && addActive)
        {
            addSliceBtn.setToggleState(false, juce::dontSendNotification);
            updateToggleBtn(addSliceBtn, false);
            waveformView.setSliceDrawMode(false);
        }

        DysektProcessor::Command cmd;
        cmd.type = midiActive ? DysektProcessor::CmdLazyChopStart : DysektProcessor::CmdLazyChopStop;
        processor.pushCommand(cmd);

        if (midiActive)
            processor.midiSelectsSlice.store(true);

        syncButtonColours();
        repaint();
    };

    shortcutsBtn.onClick = [this] { if (onShortcutsToggle) onShortcutsToggle(); };
    shortcutsBtn.setTooltip ("Keyboard Shortcuts (⌘?)");
    addSliceBtn.setTooltip ("Add Slice (A / hold Alt)");
    lazyChopBtn.setTooltip ("MIDI Slice — chop by incoming MIDI notes (L)");

    addSliceBtn.setButtonText ("");
    lazyChopBtn.setButtonText ("");
    updateMidiButtonAppearance(false);

    syncButtonColours();
}

ActionPanel::~ActionPanel() = default;

void ActionPanel::updateToggleBtn (juce::TextButton& btn, bool active)
{
    if (active)
    {
        btn.setColour (juce::TextButton::buttonColourId,  getTheme().accent.withAlpha (0.2f));
        btn.setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn.setColour (juce::TextButton::textColourOffId, getTheme().accent);
    }
    else
    {
        btn.setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn.setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn.setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }
    btn.setEnabled(true); // Always clickable!
}

void ActionPanel::updateMidiButtonAppearance (bool /*active*/) {}

void ActionPanel::resized()
{
    const int gap    = 4;
    const int h      = getHeight();
    // Use the same width for both buttons!
    const int btnW   = 34;   // Was: addW=80, midiW=34
    // const int thinW  = 30; // Not used here

    addSliceBtn.setBounds (0,           0, btnW, h);
    lazyChopBtn.setBounds (btnW + gap,  0, btnW, h);

    shortcutsBtn.setVisible (false);
}

void ActionPanel::paint (juce::Graphics& g)
{
    if (waveformView.isSliceDrawModeActive())
    {
        g.setColour (getTheme().accent.withAlpha (0.25f));
        g.fillRect (addSliceBtn.getBounds());
    }
    if (processor.lazyChop.isActive())
    {
        g.setColour (juce::Colours::red.withAlpha (0.25f));
        g.fillRect (lazyChopBtn.getBounds());
    }
}

static void ap_drawScissors (juce::Graphics& g, float cx, float cy, float sz, juce::Colour col)
{
    const float hw = sz * 0.38f; const float hh = sz * 0.22f;
    g.setColour (col);
    juce::Path sc;
    sc.startNewSubPath (cx - hw, cy - hh * 0.2f); sc.lineTo (cx + hw * 0.55f, cy - hh);
    sc.startNewSubPath (cx - hw, cy + hh * 0.2f); sc.lineTo (cx + hw * 0.55f, cy + hh);
    g.strokePath (sc, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse (cx - hw * 0.05f, cy - sz * 0.06f, sz * 0.12f, sz * 0.12f);
}
static void ap_drawPlus (juce::Graphics& g, float cx, float cy, float sz, juce::Colour col)
{
    const float arm = sz * 0.28f; const float thk = sz * 0.08f;
    g.setColour (col);
    g.fillRect (cx - thk, cy - arm, thk * 2.f, arm * 2.f);
    g.fillRect (cx - arm, cy - thk, arm * 2.f, thk * 2.f);
}
static void ap_drawPiano (juce::Graphics& g, float cx, float cy, float sz, juce::Colour col)
{
    const float kw = sz * 0.11f; const float kh = sz * 0.44f; const float bh = kh * 0.58f;
    const float startX = cx - kw * 2.5f;
    g.setColour (col.withAlpha (0.85f));
    for (int i = 0; i < 5; ++i)
        g.fillRoundedRectangle (startX + i * (kw + 1.f), cy - kh * 0.5f, kw, kh, 0.8f);
    g.setColour (col.withAlpha (0.55f));
    for (int i : { 0, 1, 3 })
        g.fillRect  (startX + i * (kw + 1.f) + kw * 0.65f, cy - kh * 0.5f, kw * 0.68f, bh);
}

// --- SPACING CONSTANT ---
static constexpr float iconGap = 9.0f; // adjust for more/less space between icons

void ActionPanel::paintOverChildren (juce::Graphics& g)
{
    // --- ADD SLICE icon: Plus + Scissors (with gap) ---
    {
        auto b  = addSliceBtn.getBounds().toFloat();
        const float cy = b.getCentreY();
        const float sz = b.getHeight() * 0.72f;
        const float alpha = addSliceBtn.isEnabled() ? 0.92f : 0.35f;
        const auto col = waveformView.isSliceDrawModeActive()
            ? getTheme().accent
            : getTheme().foreground.withAlpha(alpha);

        // measure icon extents
        const float plusW = sz * 0.70f;     // approx icon bounds (for centering)
        const float scissorsW = sz * 0.70f; // same
        const float totalW = plusW + iconGap + scissorsW;

        const float left = b.getCentreX() - totalW/2.f;

        ap_drawPlus     (g, left + plusW/2.f,            cy, sz, col);
        ap_drawScissors (g, left + plusW + iconGap + scissorsW/2.f, cy, sz, col);
    }

    // --- MIDI SLICE icon: Piano + Scissors (with gap) ---
    {
        auto b  = lazyChopBtn.getBounds().toFloat();
        const float cy    = b.getCentreY();
        const float sz    = b.getHeight() * 0.72f;
        const float alpha = lazyChopBtn.isEnabled() ? 0.92f : 0.35f;
        const auto  col   = processor.lazyChop.isActive()
            ? getTheme().accent
            : getTheme().foreground.withAlpha(alpha);

        // measure icon extents
        const float pianoW = sz * 0.70f;
        const float scissorsW = sz * 0.70f;
        const float totalW = pianoW + iconGap + scissorsW;

        const float left = b.getCentreX() - totalW/2.f;

        ap_drawPiano    (g, left + pianoW/2.f,            cy, sz, col);
        ap_drawScissors (g, left + pianoW + iconGap + scissorsW/2.f, cy, sz, col);
    }
}