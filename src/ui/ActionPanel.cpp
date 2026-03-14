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

    // --- MUTUALLY EXCLUSIVE BUTTON LOGIC START ---
    addSliceBtn.setClickingTogglesState(true);
    lazyChopBtn.setClickingTogglesState(true);

    addSliceBtn.onClick = [this]
    {
        bool addActive = addSliceBtn.getToggleState();
        if (addActive)
        {
            // If enabling Add, turn off MIDI
            lazyChopBtn.setToggleState(false, juce::dontSendNotification);
            // Turn off MIDI logic if it's on
            if (processor.lazyChop.isActive())
            {
                DysektProcessor::Command cmd;
                cmd.type = DysektProcessor::CmdLazyChopStop;
                processor.pushCommand(cmd);
            }
        }
        // Apply add slice draw mode in waveform
        waveformView.setSliceDrawMode(addActive);
        repaint();
    };

    lazyChopBtn.onClick = [this]
    {
        bool midiActive = lazyChopBtn.getToggleState();
        if (midiActive)
        {
            // If enabling MIDI, turn off Add
            addSliceBtn.setToggleState(false, juce::dontSendNotification);
            waveformView.setSliceDrawMode(false);
        }
        // MIDI Slice logic as before
        const bool wasActive = processor.lazyChop.isActive();
        DysektProcessor::Command cmd;
        cmd.type = midiActive ? DysektProcessor::CmdLazyChopStart : DysektProcessor::CmdLazyChopStop;
        processor.pushCommand(cmd);

        if (midiActive)
            processor.midiSelectsSlice.store(true);

        repaint();
    };
    // --- MUTUALLY EXCLUSIVE BUTTON LOGIC END ---

    shortcutsBtn.onClick = [this] { if (onShortcutsToggle) onShortcutsToggle(); };
    shortcutsBtn.setTooltip ("Keyboard Shortcuts (⌘?)");
    addSliceBtn.setTooltip ("Add Slice (A / hold Alt)");
    lazyChopBtn.setTooltip ("MIDI Slice — chop by incoming MIDI notes (L)");

    addSliceBtn.setButtonText ("");   // icon drawn in paintOverChildren
    lazyChopBtn.setButtonText ("");   // icon drawn in paintOverChildren
    updateMidiButtonAppearance (false);
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
        btn.setColour (juce::TextButton::textColourOnId,  getTheme().accent);
        btn.setColour (juce::TextButton::textColourOffId, getTheme().accent);
    }
}

void ActionPanel::updateMidiButtonAppearance (bool /*active*/)
{
    // midiSelectBtn removed — MIDI Follow is now in HeaderBar icon slot.
}

void ActionPanel::resized()
{
    const int gap    = 4;
    const int h      = getHeight();
    const int thinW  = 30;
    const int addW   = 80;
    const int midiW  = 34;

    int right = getWidth();    right -= thinW + gap;

    addSliceBtn.setBounds (0,            0, addW, h);
    lazyChopBtn.setBounds (addW + gap,   0, midiW, h);

    shortcutsBtn.setVisible (false);
}

void ActionPanel::paint (juce::Graphics& g)
{
    for (auto* btn : { &addSliceBtn, &lazyChopBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  getTheme().button);
        btn->setColour (juce::TextButton::textColourOnId,  getTheme().foreground);
        btn->setColour (juce::TextButton::textColourOffId, getTheme().foreground);
    }
    if (waveformView.isSliceDrawModeActive())
    { g.setColour (getTheme().accent.withAlpha (0.25f)); g.fillRect (addSliceBtn.getBounds()); }

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

void ActionPanel::paintOverChildren (juce::Graphics& g)
{
    // ADD SLICE icon: Plus + Scissors
    {
        auto b  = addSliceBtn.getBounds().toFloat();
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();
        const float sz = b.getHeight() * 0.72f;
        const float alpha = addSliceBtn.isEnabled() ? 0.92f : 0.35f;
        const auto  col = getTheme().accent.withAlpha (alpha);
        ap_drawPlus     (g, cx - sz * 0.32f, cy, sz, col);
        ap_drawScissors (g, cx + sz * 0.36f, cy, sz, col);
    }

    // MIDI SLICE icon: Piano + Scissors
    {
        auto b  = lazyChopBtn.getBounds().toFloat();
        const float cx    = b.getCentreX();
        const float cy    = b.getCentreY();
        const float sz    = b.getHeight() * 0.72f;
        const float alpha = lazyChopBtn.isEnabled() ? 0.92f : 0.35f;
        const auto  col   = processor.lazyChop.isActive()
                              ? getTheme().accent
                              : getTheme().foreground.withAlpha (alpha);
        ap_drawPiano    (g, cx - sz * 0.30f, cy, sz, col);
        ap_drawScissors (g, cx + sz * 0.38f, cy, sz, col);
    }
}