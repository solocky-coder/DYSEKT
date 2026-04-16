#include "PadGridView.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include <cmath>

//==============================================================================
PadGridView::PadGridView (DysektProcessor& proc)
    : processor (proc)
{
    setRepaintsOnMouseActivity (false);
}

PadGridView::~PadGridView() = default;

//==============================================================================
// Bank-switcher
//==============================================================================

void PadGridView::layoutBankButtons()
{
    // Divide the bank-bar into two equal buttons with a small gap between them.
    const int barW    = getWidth();
    const int btnW    = (barW - kPadPadX * 2 - 6) / 2;  // 6px centre gap
    const int btnY    = (kBankBarH - 20) / 2;

    bankAButtonBounds = { kPadPadX,          btnY, btnW, 20 };
    bankBButtonBounds = { kPadPadX + btnW + 6, btnY, btnW, 20 };
}

void PadGridView::drawBankBar (juce::Graphics& g) const
{
    const auto& th = getTheme();

    // Bar background
    auto barRect = juce::Rectangle<int> (0, 0, getWidth(), kBankBarH).toFloat();
    g.setColour (th.background.darker (0.08f));
    g.fillRect (barRect);

    // Separator line
    g.setColour (th.separator.withAlpha (0.40f));
    g.drawHorizontalLine (kBankBarH - 1, 0.0f, (float) getWidth());

    // Draw Bank A button
    auto drawBtn = [&] (juce::Rectangle<int> r, int bankIndex, const char* label)
    {
        const bool active = (currentBank == bankIndex);

        juce::Colour bg = active ? th.accent : th.button;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        g.setColour (active ? th.background : th.separator);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);

        g.setFont (DysektLookAndFeel::makeFont (11.0f, true));
        g.setColour (active ? th.background : th.foreground.withAlpha (0.70f));
        g.drawText (label, r, juce::Justification::centred);
    };

    drawBtn (bankAButtonBounds, 0, "Bank A  (1–16)");
    drawBtn (bankBButtonBounds, 1, "Bank B  (17–32)");
}

//==============================================================================
// Grid geometry
//==============================================================================

juce::Rectangle<int> PadGridView::cellBounds (int absIndex) const noexcept
{
    // Only show pads belonging to the current bank.
    const int bankStart = currentBank * kPadsPerBank;
    const int bankEnd   = bankStart + kPadsPerBank;
    if (absIndex < bankStart || absIndex >= bankEnd) return {};

    const int localIdx = absIndex - bankStart;
    const int row = localIdx / kNumCols;
    const int col = localIdx % kNumCols;

    const int gridTop  = kBankBarH + kPadPadY;
    const int gridH    = getHeight() - gridTop - kPadPadY;
    const int gridW    = getWidth()  - kPadPadX * 2;
    if (gridW <= 0 || gridH <= 0) return {};

    const int cellW = (gridW - (kNumCols - 1) * kPadGap) / kNumCols;
    const int cellH = (gridH - (kNumRows - 1) * kPadGap) / kNumRows;

    const int x = kPadPadX + col * (cellW + kPadGap);
    const int y = gridTop  + row * (cellH + kPadGap);

    return { x, y, cellW, cellH };
}

int PadGridView::padIndexAt (juce::Point<int> p) const noexcept
{
    if (p.y < kBankBarH) return -1;  // hit-test is inside the bank bar

    const int bankStart = currentBank * kPadsPerBank;
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        const int absIdx = bankStart + i;
        const auto r = cellBounds (absIdx);
        if (! r.isEmpty() && r.contains (p))
            return absIdx;
    }
    return -1;
}

//==============================================================================
// Drawing helpers
//==============================================================================

juce::String PadGridView::midiNoteName (int note)
{
    static const char* kNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    const int oct = (note / 12) - 1;
    return juce::String (kNames[note % 12]) + juce::String (oct);
}

void PadGridView::drawPad (juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            int absIndex,
                            bool isEmpty) const
{
    const auto& th  = getTheme();
    const auto& ui  = processor.getUiSliceSnapshot();
    const bool  sel = (ui.selectedSlice == absIndex);
    const bool  hov = (hoveredPad == absIndex);

    // ── Background ────────────────────────────────────────────────────────────
    juce::Colour bgCol = isEmpty ? th.button.withAlpha (0.35f)
                                 : (hov ? th.buttonHover : th.button);
    if (sel && ! isEmpty)
        bgCol = th.button.brighter (0.22f);

    g.setColour (bgCol);
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    // ── Border ────────────────────────────────────────────────────────────────
    if (sel && ! isEmpty)
    {
        g.setColour (th.accent.withAlpha (0.92f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.5f);
    }
    else if (hov && ! isEmpty)
    {
        g.setColour (th.accent.withAlpha (0.40f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
    }
    else
    {
        g.setColour (th.separator.withAlpha (isEmpty ? 0.18f : 0.45f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 0.75f);
    }

    // ── Pad number (always shown, top-left, faint) ────────────────────────────
    {
        g.setFont (DysektLookAndFeel::makeMonoFont (8.5f));
        g.setColour (th.foreground.withAlpha (isEmpty ? 0.12f : 0.28f));
        g.drawText (juce::String (absIndex + 1),
                    bounds.reduced (5, 4),
                    juce::Justification::topLeft);
    }

    if (isEmpty) return;

    const auto& slice     = ui.slices[(size_t) absIndex];
    const juce::Colour padColour = slice.colour;

    // ── Colour accent bar (left edge) ─────────────────────────────────────────
    auto barRect = bounds.removeFromLeft (kBarW).reduced (0, 3).toFloat();
    g.setColour (padColour.withAlpha (sel ? 1.0f : 0.82f));
    g.fillRoundedRectangle (barRect, 2.0f);

    // Content area (remaining width after accent bar + small padding)
    auto content = bounds.reduced (5, 4);

    // ── Slice name ────────────────────────────────────────────────────────────
    {
        const juce::String displayName = slice.name.isNotEmpty()
                                       ? slice.name
                                       : ("Slice " + juce::String (absIndex + 1));
        auto nameR = content.removeFromTop (content.getHeight() / 2);
        g.setFont (DysektLookAndFeel::makeFont (10.5f, true));
        g.setColour (sel ? th.foreground : th.foreground.withAlpha (0.88f));
        g.drawText (displayName, nameR, juce::Justification::centredLeft, true);
    }

    // ── MIDI note (bottom-left) ───────────────────────────────────────────────
    {
        auto noteR = content.removeFromBottom (content.getHeight() / 2);
        g.setFont (DysektLookAndFeel::makeMonoFont (8.5f));
        g.setColour (th.foreground.withAlpha (0.48f));
        g.drawText (midiNoteName (slice.midiNote), noteR, juce::Justification::bottomLeft);
    }

    // ── Peak meters (bottom-right corner) ────────────────────────────────────
    if (absIndex < (int) processor.slicePeakL.size())
    {
        const float peakL = processor.slicePeakL[(size_t) absIndex].load (std::memory_order_relaxed);
        const float peakR = processor.slicePeakR[(size_t) absIndex].load (std::memory_order_relaxed);

        const int meterW = juce::jmin (50, bounds.getWidth() / 3);
        const int meterH = 3;
        const int meterX = bounds.getRight() - meterW - 3;
        const int meterY = bounds.getBottom() - 4 - meterH * 2 - 2;

        // L
        g.setColour (th.separator.withAlpha (0.35f));
        g.fillRect (meterX, meterY, meterW, meterH);
        g.setColour (padColour.withAlpha (0.82f));
        g.fillRect (meterX, meterY, juce::roundToInt (peakL * meterW), meterH);

        // R
        g.setColour (th.separator.withAlpha (0.35f));
        g.fillRect (meterX, meterY + meterH + 2, meterW, meterH);
        g.setColour (padColour.withAlpha (0.62f));
        g.fillRect (meterX, meterY + meterH + 2, juce::roundToInt (peakR * meterW), meterH);
    }
}

//==============================================================================
// Component overrides
//==============================================================================

void PadGridView::paint (juce::Graphics& g)
{
    const auto& th = getTheme();
    const auto& ui = processor.getUiSliceSnapshot();

    g.fillAll (th.background);

    // Bank switcher bar
    drawBankBar (g);

    // Pads for the current bank
    const int bankStart = currentBank * kPadsPerBank;
    const int bankEnd   = bankStart + kPadsPerBank;

    for (int absIdx = bankStart; absIdx < bankEnd; ++absIdx)
    {
        const bool isEmpty = (absIdx >= ui.numSlices);
        const auto r = cellBounds (absIdx);
        if (r.isEmpty()) continue;
        drawPad (g, r, absIdx, isEmpty);
    }

    // "No slices" hint
    if (ui.numSlices == 0)
    {
        g.setFont (DysektLookAndFeel::makeFont (12.0f));
        g.setColour (th.foreground.withAlpha (0.35f));
        g.drawText ("No slices — load a sample and add markers",
                    getLocalBounds().withTrimmedTop (kBankBarH),
                    juce::Justification::centred);
    }
}

void PadGridView::resized()
{
    layoutBankButtons();
}

//==============================================================================
// Input
//==============================================================================

void PadGridView::mouseDown (const juce::MouseEvent& e)
{
    const juce::Point<int> pt = e.getPosition();

    // Bank-switcher hit-test (inside the bank bar)
    if (pt.y < kBankBarH)
    {
        if (bankAButtonBounds.contains (pt) && currentBank != 0)
        {
            currentBank = 0;
            hoveredPad  = -1;
            repaint();
        }
        else if (bankBButtonBounds.contains (pt) && currentBank != 1)
        {
            currentBank = 1;
            hoveredPad  = -1;
            repaint();
        }
        return;
    }

    // Pad hit-test
    const int idx = padIndexAt (pt);
    if (idx < 0) return;

    const auto& ui = processor.getUiSliceSnapshot();
    if (idx >= ui.numSlices) return;

    DysektProcessor::Command cmd;
    cmd.type      = DysektProcessor::CmdSelectSlice;
    cmd.intParam1 = idx;
    processor.pushCommand (cmd);
    repaint();
}

void PadGridView::mouseMove (const juce::MouseEvent& e)
{
    const int idx = (e.getPosition().y < kBankBarH) ? -1
                                                     : padIndexAt (e.getPosition());
    if (idx != hoveredPad)
    {
        hoveredPad = idx;
        repaint();
    }
}

void PadGridView::mouseExit (const juce::MouseEvent&)
{
    if (hoveredPad != -1)
    {
        hoveredPad = -1;
        repaint();
    }
}

//==============================================================================
void PadGridView::repaintGrid()
{
    // Auto-switch bank if the currently active (triggered/selected) slice
    // lives on a different bank than the one presently displayed.
    // processor.sliceManager.selectedSlice is updated atomically by the audio
    // thread whenever a MIDI note triggers a slice, so reading it here on the
    // timer tick (~30 ms) gives near-instant bank switching.
    const int activePad = processor.sliceManager.selectedSlice
                              .load (std::memory_order_relaxed);

    if (activePad >= 0 && activePad < kMaxPads)
    {
        const int sliceBank = activePad / kPadsPerBank;
        if (sliceBank != currentBank)
        {
            currentBank = sliceBank;
            // hoveredPad may now refer to a pad on the wrong bank — clear it.
            hoveredPad = -1;
        }
    }

    repaint();
}
