#include "PadGridView.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../params/ParamIds.h"
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
    const int barW = getWidth();
    const int btnW = (barW - kPadPadX * 2 - 6) / 2;
    const int btnY = (kBankBarH - 20) / 2;

    bankAButtonBounds = { kPadPadX,               btnY, btnW, 20 };
    bankBButtonBounds = { kPadPadX + btnW + 6,    btnY, btnW, 20 };
}

void PadGridView::drawBankBar (juce::Graphics& g) const
{
    const auto& th = getTheme();

    auto barRect = juce::Rectangle<int> (0, 0, getWidth(), kBankBarH).toFloat();
    g.setColour (th.background.darker (0.08f));
    g.fillRect (barRect);

    g.setColour (th.separator.withAlpha (0.40f));
    g.drawHorizontalLine (kBankBarH - 1, 0.0f, (float) getWidth());

    auto drawBtn = [&] (juce::Rectangle<int> r, int bankIndex, const char* label)
    {
        const bool active = (currentBank == bankIndex);

        g.setColour (active ? th.accent : th.button);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        g.setColour (active ? th.background : th.separator);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);

        g.setFont (DysektLookAndFeel::makeFont (11.0f, true));
        g.setColour (active ? th.background : th.foreground.withAlpha (0.70f));
        g.drawText (label, r, juce::Justification::centred);
    };

    drawBtn (bankAButtonBounds, 0, "Bank A");
    drawBtn (bankBButtonBounds, 1, "Bank B");
}

//==============================================================================
// Grid geometry
//==============================================================================

juce::Rectangle<int> PadGridView::cellBounds (int absIndex) const noexcept
{
    const int bankStart = currentBank * kPadsPerBank;
    const int bankEnd   = bankStart + kPadsPerBank;
    if (absIndex < bankStart || absIndex >= bankEnd) return {};

    const int localIdx = absIndex - bankStart;
    const int row = localIdx / kNumCols;
    const int col = localIdx % kNumCols;

    const int gridTop = kBankBarH + kPadPadY;
    const int gridH   = getHeight() - gridTop - kPadPadY;
    const int gridW   = getWidth()  - kPadPadX * 2;
    if (gridW <= 0 || gridH <= 0) return {};

    const int cellW = (gridW - (kNumCols - 1) * kPadGap) / kNumCols;
    const int cellH = (gridH - (kNumRows - 1) * kPadGap) / kNumRows;

    const int x = kPadPadX + col * (cellW + kPadGap);
    const int y = gridTop  + row * (cellH + kPadGap);

    return { x, y, cellW, cellH };
}

int PadGridView::padIndexAt (juce::Point<int> p) const noexcept
{
    if (p.y < kBankBarH) return -1;

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
    const int oct = (note / 12) - 2;  // matches LCD: MIDI 60 = C3
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

    // ── Background: full pad filled with the slice color ─────────────────────
    if (isEmpty)
    {
        // Background gradient — same recipe as DualLcdControlFrame
        {
            auto bgTop = th.darkBar.darker (0.45f);
            auto bgBot = th.darkBar.darker (0.65f);
            juce::ColourGradient grad (bgTop, 0, (float) bounds.getY(),
                                       bgBot, 0, (float) bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
        }
        // Outer glow
        g.setColour (th.accent.withAlpha (0.08f));
        g.drawRoundedRectangle (bounds.toFloat().expanded (1.0f), 5.0f, 1.0f);
        // Inner border
        g.setColour (th.accent.withAlpha (0.20f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    const auto& slice    = ui.slices[(size_t) absIndex];
    const juce::Colour sliceCol = slice.colour;

    // Darkened version of the slice color for the pad body
    // Selected pads are a bit brighter so they stand out clearly.
    juce::Colour padBg = sliceCol.darker (sel ? 0.38f : 0.58f);
    if (hov) padBg = padBg.brighter (0.12f);

    // Background fill — darkened slice color with subtle gradient
    {
        juce::ColourGradient grad (padBg.brighter (0.08f), 0, (float) bounds.getY(),
                                   padBg.darker (0.12f),  0, (float) bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    }

    // Outer glow — always accent color (matches every other frame in the UI)
    g.setColour (th.accent.withAlpha (sel ? 0.28f : 0.14f));
    g.drawRoundedRectangle (bounds.toFloat().expanded (1.0f), 5.0f, 1.0f);

    // Inner border — accent at full strength when selected, dimmed otherwise
    g.setColour (th.accent.withAlpha (sel ? 0.90f : (hov ? 0.55f : 0.35f)));
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, sel ? 1.5f : 1.0f);

    // ── Inner layout ──────────────────────────────────────────────────────────
    auto inner     = bounds.reduced (3, 3);

    // Top row: note name left + slice name centred  (fixed height = 11 px)
    auto topRow    = inner.removeFromTop (11);

    // Remaining space = waveform canvas (no meter strip any more)
    auto waveArea  = inner;

    // ── MIDI note name — top-left (replaces old pad number) ──────────────────
    {
        g.setFont (DysektLookAndFeel::makeMonoFont (7.5f));
        g.setColour (sliceCol.brighter (0.55f).withAlpha (0.90f));
        g.drawText (midiNoteName (slice.midiNote),
                    topRow.withWidth (28),
                    juce::Justification::centredLeft);
    }

    // ── Slice name — top-centre ───────────────────────────────────────────────
    {
        const juce::String displayName = slice.name.isNotEmpty()
                                       ? slice.name
                                       : ("Slice " + juce::String (absIndex + 1));
        g.setFont (DysektLookAndFeel::makeFont (8.5f, true));
        g.setColour (juce::Colours::white.withAlpha (sel ? 1.0f : 0.88f));
        g.drawText (displayName, topRow, juce::Justification::centred, true);
    }

    // ── Slice waveform — fills the middle canvas (8-mode, SliceWaveformLcd style) ──
    if (waveArea.getWidth() > 4 && waveArea.getHeight() > 4 && ui.sampleLoaded)
    {
        const int startSamp = slice.startSample;
        const int endSamp   = processor.sliceManager.getEndForSlice (absIndex, ui.sampleNumFrames);
        const int sliceLen  = endSamp - startSamp;

        if (sliceLen > 0)
        {
            // Build per-pixel peak array for this slice
            const int W = waveArea.getWidth();
            const int H = waveArea.getHeight();
            std::vector<float> peaks ((size_t) W, 0.0f);
            for (int x = 0; x < W; ++x)
            {
                const int samp = startSamp + juce::roundToInt ((float) x / (float) W * (float) sliceLen);
                peaks[(size_t) x] = processor.getWaveformPeakAt (samp);
            }

            // Clip drawing to waveArea
            g.saveState();
            g.reduceClipRegion (waveArea);

            const float ox    = (float) waveArea.getX();
            const float oy    = (float) waveArea.getY();
            const float cy    = oy + (float) H * 0.5f;
            const float scale = (float) H * 0.44f;

            // Use slice color (brightened slightly) to mirror SliceWaveformLcd
            const juce::Colour waveCol = sliceCol.brighter (0.25f);

            auto px2x = [&] (int px) -> float { return ox + (float) px; };

            switch (waveformMode)
            {
                // ── Mode 0 : Hard (SliceWaveformLcd default — fill + glow lines) ──
                default:
                case 0:
                {
                    juce::Path fill, lineTop, lineBot;
                    bool first = true;
                    for (int px = 0; px < W; ++px)
                    {
                        const float amp = peaks[(size_t) px] * scale;
                        const float yT  = cy - amp;
                        const float yB  = cy + amp;
                        if (first) { lineTop.startNewSubPath (px2x (px), yT);
                                     lineBot.startNewSubPath (px2x (px), yB); first = false; }
                        else       { lineTop.lineTo (px2x (px), yT);
                                     lineBot.lineTo (px2x (px), yB); }
                    }
                    fill = lineTop;
                    for (int px = W - 1; px >= 0; --px)
                        fill.lineTo (px2x (px), cy + peaks[(size_t) px] * scale);
                    fill.closeSubPath();

                    // Phosphor-style fill + glow — matches SliceWaveformLcd
                    g.setColour (waveCol.withAlpha (0.12f));
                    g.fillPath (fill);
                    g.setColour (waveCol.withAlpha (0.22f));
                    g.strokePath (lineTop, juce::PathStrokeType (2.5f));
                    g.strokePath (lineBot, juce::PathStrokeType (2.5f));
                    g.setColour (waveCol.withAlpha (0.85f));
                    g.strokePath (lineTop, juce::PathStrokeType (1.1f));
                    g.strokePath (lineBot, juce::PathStrokeType (1.1f));
                    break;
                }

                // ── Mode 1 : Soft ─────────────────────────────────────────────
                case 1:
                {
                    juce::Path fillPath;
                    fillPath.startNewSubPath (px2x (0), cy - peaks[0] * scale);
                    for (int px = 1; px < W; ++px)
                        fillPath.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                    for (int px = W - 1; px >= 0; --px)
                        fillPath.lineTo (px2x (px), cy + peaks[(size_t) px] * scale);
                    fillPath.closeSubPath();
                    g.setColour (waveCol.withAlpha (0.60f));
                    g.fillPath (fillPath);
                    juce::Path topLine;
                    topLine.startNewSubPath (px2x (0), cy - peaks[0] * scale);
                    for (int px = 1; px < W; ++px)
                        topLine.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                    g.setColour (waveCol.withAlpha (0.95f));
                    g.strokePath (topLine, juce::PathStrokeType (1.2f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }

                // ── Mode 2 : Outline ──────────────────────────────────────────
                case 2:
                {
                    juce::Path topPath, botPath;
                    topPath.startNewSubPath (px2x (0), cy - peaks[0] * scale);
                    botPath.startNewSubPath (px2x (0), cy + peaks[0] * scale);
                    for (int px = 1; px < W; ++px)
                    {
                        topPath.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                        botPath.lineTo (px2x (px), cy + peaks[(size_t) px] * scale);
                    }
                    g.setColour (waveCol.withAlpha (0.25f));
                    g.strokePath (topPath, juce::PathStrokeType (2.5f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.strokePath (botPath, juce::PathStrokeType (2.5f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (waveCol.withAlpha (0.90f));
                    g.strokePath (topPath, juce::PathStrokeType (1.0f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.strokePath (botPath, juce::PathStrokeType (1.0f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }

                // ── Mode 3 : Rectified ────────────────────────────────────────
                case 3:
                {
                    const float baseline  = oy + (float) H * 0.85f;
                    const float rectScale = scale * 1.6f;
                    juce::Path rectPath;
                    rectPath.startNewSubPath (px2x (0), baseline - peaks[0] * rectScale);
                    for (int px = 1; px < W; ++px)
                        rectPath.lineTo (px2x (px), baseline - peaks[(size_t) px] * rectScale);
                    rectPath.lineTo (px2x (W - 1), baseline);
                    rectPath.lineTo (px2x (0), baseline);
                    rectPath.closeSubPath();
                    juce::ColourGradient grad (waveCol.withAlpha (0.60f), 0.0f, oy,
                                               waveCol.withAlpha (0.05f), 0.0f, oy + (float) H, false);
                    g.setGradientFill (grad);
                    g.fillPath (rectPath);
                    juce::Path topLine;
                    topLine.startNewSubPath (px2x (0), baseline - peaks[0] * rectScale);
                    for (int px = 1; px < W; ++px)
                        topLine.lineTo (px2x (px), baseline - peaks[(size_t) px] * rectScale);
                    g.setColour (waveCol.withAlpha (0.90f));
                    g.strokePath (topLine, juce::PathStrokeType (1.1f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }

                // ── Mode 4 : Mirrored ─────────────────────────────────────────
                case 4:
                {
                    juce::Path upper, lower;
                    upper.startNewSubPath (px2x (0), cy);
                    lower.startNewSubPath (px2x (0), cy);
                    for (int px = 0; px < W; ++px)
                    {
                        upper.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                        lower.lineTo (px2x (px), cy + peaks[(size_t) px] * scale);
                    }
                    upper.lineTo (px2x (W - 1), cy); upper.closeSubPath();
                    lower.lineTo (px2x (W - 1), cy); lower.closeSubPath();
                    g.setColour (waveCol.withAlpha (0.75f)); g.fillPath (upper);
                    g.setColour (waveCol.withAlpha (0.35f)); g.fillPath (lower);
                    juce::Path edge;
                    edge.startNewSubPath (px2x (0), cy - peaks[0] * scale);
                    for (int px = 1; px < W; ++px)
                        edge.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                    g.setColour (waveCol.withAlpha (0.90f));
                    g.strokePath (edge, juce::PathStrokeType (1.0f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }

                // ── Mode 5 : Bars ─────────────────────────────────────────────
                case 5:
                {
                    for (int px = 0; px < W; ++px)
                    {
                        const float amp   = peaks[(size_t) px] * scale;
                        const float alpha = 0.4f + peaks[(size_t) px] * 0.55f;
                        g.setColour (waveCol.withAlpha (alpha));
                        g.fillRect (px2x (px), cy - amp, 1.0f, juce::jmax (1.0f, amp * 2.0f));
                    }
                    break;
                }

                // ── Mode 6 : RMS ──────────────────────────────────────────────
                case 6:
                {
                    juce::Path rmsPath;
                    rmsPath.startNewSubPath (px2x (0), cy - peaks[0] * scale);
                    for (int px = 1; px < W; ++px)
                        rmsPath.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                    for (int px = W - 1; px >= 0; --px)
                        rmsPath.lineTo (px2x (px), cy + peaks[(size_t) px] * scale);
                    rmsPath.closeSubPath();
                    juce::ColourGradient grad (waveCol.withAlpha (0.0f), 0.0f, 0.0f,
                                               waveCol.withAlpha (0.0f), 0.0f, oy + (float) H, false);
                    grad.addColour (0.35, waveCol.withAlpha (0.22f));
                    grad.addColour (0.50, waveCol.withAlpha (0.36f));
                    grad.addColour (0.65, waveCol.withAlpha (0.22f));
                    g.setGradientFill (grad);
                    g.fillPath (rmsPath);
                    juce::Path rmsLine;
                    rmsLine.startNewSubPath (px2x (0), cy - peaks[0] * scale);
                    for (int px = 1; px < W; ++px)
                        rmsLine.lineTo (px2x (px), cy - peaks[(size_t) px] * scale);
                    g.setColour (waveCol.withAlpha (0.28f));
                    g.strokePath (rmsLine, juce::PathStrokeType (3.0f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (waveCol.withAlpha (0.95f));
                    g.strokePath (rmsLine, juce::PathStrokeType (1.1f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }

                // ── Mode 7 : Stepped ──────────────────────────────────────────
                case 7:
                {
                    const int stepW = juce::jmax (2, W / juce::jmax (1, W / 4));
                    juce::Path upper;
                    bool started = false;
                    float lastY = cy;
                    for (int px = 0; px < W; px += stepW)
                    {
                        float y = cy - peaks[(size_t) px] * scale;
                        if (! started) { upper.startNewSubPath (px2x (px), y); started = true; }
                        else { upper.lineTo (px2x (px), lastY); upper.lineTo (px2x (px), y); }
                        upper.lineTo (px2x (juce::jmin (px + stepW, W - 1)), y);
                        lastY = y;
                    }
                    juce::Path lower;
                    bool startedL = false; float lastYL = cy;
                    for (int px = 0; px < W; px += stepW)
                    {
                        float y = cy + peaks[(size_t) px] * scale;
                        if (! startedL) { lower.startNewSubPath (px2x (px), y); startedL = true; }
                        else { lower.lineTo (px2x (px), lastYL); lower.lineTo (px2x (px), y); }
                        lower.lineTo (px2x (juce::jmin (px + stepW, W - 1)), y);
                        lastYL = y;
                    }
                    juce::Path uFill = upper;
                    uFill.lineTo (px2x (W - 1), cy); uFill.lineTo (px2x (0), cy); uFill.closeSubPath();
                    juce::Path lFill = lower;
                    lFill.lineTo (px2x (W - 1), cy); lFill.lineTo (px2x (0), cy); lFill.closeSubPath();
                    g.setColour (waveCol.withAlpha (0.70f)); g.fillPath (uFill);
                    g.setColour (waveCol.withAlpha (0.30f)); g.fillPath (lFill);
                    g.setColour (waveCol.withAlpha (0.95f));
                    g.strokePath (upper, juce::PathStrokeType (1.0f));
                    break;
                }
            }

            g.restoreState();
        }
    }

    // Playhead removed from pad view by design.
}

//==============================================================================
// Component overrides
//==============================================================================

void PadGridView::paint (juce::Graphics& g)
{
    const auto& th = getTheme();
    const auto& ui = processor.getUiSliceSnapshot();

    g.fillAll (th.background);

    drawBankBar (g);

    const int bankStart = currentBank * kPadsPerBank;
    const int bankEnd   = bankStart + kPadsPerBank;

    for (int absIdx = bankStart; absIdx < bankEnd; ++absIdx)
    {
        const bool isEmpty = (absIdx >= ui.numSlices);
        const auto r = cellBounds (absIdx);
        if (r.isEmpty()) continue;
        drawPad (g, r, absIdx, isEmpty);
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

    const int idx = padIndexAt (pt);
    if (idx < 0) return;

    const auto& ui = processor.getUiSliceSnapshot();
    if (idx >= ui.numSlices) return;

    // ── Right-click: context menu ─────────────────────────────────────────────
    if (e.mods.isRightButtonDown())
    {
        // Select the pad first so the menu acts on the right slice
        DysektProcessor::Command selCmd;
        selCmd.type      = DysektProcessor::CmdSelectSlice;
        selCmd.intParam1 = idx;
        processor.pushCommand (selCmd);
        repaint();

        showPadContextMenu (idx, { e.getScreenX(), e.getScreenY() });
        return;
    }

    // ── Left-click: select + play ─────────────────────────────────────────────
    DysektProcessor::Command cmd;
    cmd.type      = DysektProcessor::CmdSelectSlice;
    cmd.intParam1 = idx;
    processor.pushCommand (cmd);

    // Fire noteOn so the pad plays its slice
    const int midiNote = processor.sliceManager.getSlice (idx).midiNote;
    processor.uiNoteOnRequest.store (midiNote, std::memory_order_relaxed);

    repaint();
}

void PadGridView::mouseUp (const juce::MouseEvent& e)
{
    const juce::Point<int> pt = e.getPosition();
    if (pt.y < kBankBarH) return;

    const int idx = padIndexAt (pt);
    if (idx < 0) return;

    const auto& ui = processor.getUiSliceSnapshot();
    if (idx >= ui.numSlices) return;

    // Send noteOff so voice can stop if the sampler is in gated mode
    const int midiNote = processor.sliceManager.getSlice (idx).midiNote;
    processor.uiNoteOffRequest.store (midiNote, std::memory_order_relaxed);
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
// Pad right-click context menu
//==============================================================================

void PadGridView::showPadContextMenu (int idx, juce::Point<int> screenPos)
{
    const auto& snap  = processor.getUiSliceSnapshot();
    if (idx < 0 || idx >= snap.numSlices) return;
    const auto& slice = snap.slices[(size_t) idx];

    float ms     = DysektLookAndFeel::getMenuScale();
    int   itemH  = (int) (24 * ms);
    auto* topLvl = getTopLevelComponent();

    // ── Volume sub-menu (preset dB steps) ────────────────────────────────────
    juce::PopupMenu volMenu;
    {
        static const float  kVolSteps[]  = { -100.f, -24.f, -18.f, -12.f, -6.f, 0.f, 3.f, 6.f, 12.f };
        static const char*  kVolLabels[] = { "-inf dB", "-24 dB", "-18 dB", "-12 dB",
                                             "-6 dB",   "0 dB",   "+3 dB",  "+6 dB", "+12 dB" };
        for (int i = 0; i < 9; ++i)
        {
            const bool isCur = (std::abs (slice.volume - kVolSteps[i]) < 0.5f);
            volMenu.addItem (200 + i, juce::String (kVolLabels[i]), true, isCur);
        }
    }

    // ── Pitch sub-menu (semitone steps) ──────────────────────────────────────
    juce::PopupMenu pitchMenu;
    {
        static const float kPitchSteps[]  = { -24.f,-12.f,-7.f,-5.f,-3.f,-2.f,-1.f,
                                                0.f, 1.f, 2.f, 3.f, 5.f, 7.f,12.f,24.f };
        static const char* kPitchLabels[] = { "-24","-12","-7","-5","-3","-2","-1",
                                               "0",  "+1","+2","+3","+5","+7","+12","+24" };
        for (int i = 0; i < 15; ++i)
        {
            const bool isCur = (std::abs (slice.pitchSemitones - kPitchSteps[i]) < 0.5f);
            pitchMenu.addItem (300 + i, juce::String (kPitchLabels[i]) + " st", true, isCur);
        }
    }

    // ── Mute-group sub-menu ───────────────────────────────────────────────────
    juce::PopupMenu muteMenu;
    {
        const int curMute = (slice.lockMask & kLockMuteGroup)
                          ? slice.muteGroup
                          : (int) processor.apvts.getRawParameterValue (ParamIds::defaultMuteGroup)->load();
        muteMenu.addItem (400, "Off  (0)", true, curMute == 0);
        for (int g = 1; g <= 16; ++g)
            muteMenu.addItem (400 + g, "Group " + juce::String (g), true, curMute == g);
    }

    // ── Output-bus sub-menu ───────────────────────────────────────────────────
    juce::PopupMenu outMenu;
    {
        const int curOut = (slice.lockMask & kLockOutputBus) ? slice.outputBus : 0;
        outMenu.addItem (500, "Main  (0)", true, curOut == 0);
        for (int o = 1; o <= 15; ++o)
            outMenu.addItem (500 + o, "Output " + juce::String (o), true, curOut == o);
    }

    // ── Root menu ─────────────────────────────────────────────────────────────
    juce::PopupMenu menu;
    menu.addItem (1, "Rename Slice...");
    menu.addItem (3, "Pad Color...");
    menu.addSeparator();
    menu.addSubMenu ("Volume",     volMenu);
    menu.addSubMenu ("Pitch",      pitchMenu);
    menu.addSeparator();
    menu.addSubMenu ("Mute Group", muteMenu);
    menu.addSubMenu ("Output",     outMenu);
    menu.addSeparator();
    menu.addItem (2, "Delete Slice");

    // Screen-space bounds of this pad (used to anchor the color picker later)
    const juce::Rectangle<int> padScreenBounds =
        cellBounds (idx) + getScreenPosition() - getPosition();

    menu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 })
            .withParentComponent (topLvl)
            .withStandardItemHeight (itemH),
        [this, idx, padScreenBounds] (int result)
        {
            if (result == 0) return;

            // ── Rename ────────────────────────────────────────────────────
            if (result == 1)
            {
                const auto& snap2 = processor.getUiSliceSnapshot();
                if (idx < snap2.numSlices && onRenameRequest)
                    onRenameRequest (idx, snap2.slices[(size_t) idx].name);
                return;
            }

            // ── Delete ────────────────────────────────────────────────────
            if (result == 2)
            {
                DysektProcessor::Command cmd;
                cmd.type      = DysektProcessor::CmdDeleteSlice;
                cmd.intParam1 = idx;
                processor.pushCommand (cmd);
                repaint();
                return;
            }

            // ── Color picker ─────────────────────────────────────────────
            if (result == 3)
            {
                launchColorPicker (idx, padScreenBounds);
                return;
            }

            // ── Volume ────────────────────────────────────────────────────
            if (result >= 200 && result < 300)
            {
                static const float kVolSteps[] = { -100.f,-24.f,-18.f,-12.f,-6.f,0.f,3.f,6.f,12.f };
                DysektProcessor::Command cmd;
                cmd.type        = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1   = idx;
                cmd.intParam2   = DysektProcessor::FieldVolume;
                cmd.floatParam1 = kVolSteps[result - 200];
                processor.pushCommand (cmd);
                return;
            }

            // ── Pitch ─────────────────────────────────────────────────────
            if (result >= 300 && result < 400)
            {
                static const float kPitchSteps[] = { -24.f,-12.f,-7.f,-5.f,-3.f,-2.f,-1.f,
                                                       0.f,  1.f, 2.f, 3.f, 5.f, 7.f,12.f,24.f };
                DysektProcessor::Command cmd;
                cmd.type        = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1   = idx;
                cmd.intParam2   = DysektProcessor::FieldPitch;
                cmd.floatParam1 = kPitchSteps[result - 300];
                processor.pushCommand (cmd);
                return;
            }

            // ── Mute group ────────────────────────────────────────────────
            if (result >= 400 && result < 500)
            {
                DysektProcessor::Command cmd;
                cmd.type        = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1   = idx;
                cmd.intParam2   = DysektProcessor::FieldMuteGroup;
                cmd.floatParam1 = (float) (result - 400);  // 0=off, 1-16=groups
                processor.pushCommand (cmd);
                return;
            }

            // ── Output bus ────────────────────────────────────────────────
            if (result >= 500 && result < 600)
            {
                DysektProcessor::Command cmd;
                cmd.type        = DysektProcessor::CmdSetSliceParam;
                cmd.intParam1   = idx;
                cmd.intParam2   = DysektProcessor::FieldOutputBus;
                cmd.floatParam1 = (float) (result - 500);  // 0=main, 1-15=outputs
                processor.pushCommand (cmd);
                return;
            }
        });
}

//------------------------------------------------------------------------------

void PadGridView::launchColorPicker (int idx, juce::Rectangle<int> padScreenBounds)
{
    const auto& snap = processor.getUiSliceSnapshot();
    if (idx < 0 || idx >= snap.numSlices) return;

    const juce::Colour current = snap.slices[(size_t) idx].colour;

    auto* selector = new juce::ColourSelector (
        juce::ColourSelector::showColourAtTop |
        juce::ColourSelector::showSliders     |
        juce::ColourSelector::showColourspace, 4, 8);

    selector->setName ("padColor");
    selector->setCurrentColour (current);
    selector->setSize (280, 320);

    // Pre-fill the 32 swatches with the same palette used by Slice.h
    static const juce::uint32 kPal[32] = {
        0xFFD82626, 0xFFF45F3D, 0xFFAD541E, 0xFFF28D0C,
        0xFFE0BC51, 0xFFC1B60A, 0xFFC2D826, 0xFFBBF43D,
        0xFF66AD1E, 0xFF54F20C, 0xFF63E051, 0xFF0AC115, 0xFF26D852,
        0xFF3DF48D, 0xFF1EAD77, 0xFF0CF2C7, 0xFF51E0E0, 0xFF0A9FC1,
        0xFF2695D8, 0xFF3D8DF4, 0xFF1E42AD, 0xFF0C1BF2,
        0xFF6351E0, 0xFF430AC1, 0xFF7F26D8, 0xFFBB3DF4, 0xFF9B1EAD,
        0xFFF20CE3, 0xFFE051BC, 0xFFC10A71, 0xFFD82669, 0xFFF43D5F,
    };
    selector->setNumSwatches (32);
    for (int i = 0; i < 32; ++i)
        selector->setSwatchColour (i, juce::Colour (kPal[i]));

    // Inline ChangeListener that forwards color changes to the processor
    struct ColourListener : public juce::ChangeListener
    {
        DysektProcessor& proc;
        int              sliceIdx;
        ColourListener (DysektProcessor& p, int i) : proc (p), sliceIdx (i) {}
        void changeListenerCallback (juce::ChangeBroadcaster* src) override
        {
            if (auto* cs = dynamic_cast<juce::ColourSelector*> (src))
            {
                DysektProcessor::Command cmd;
                cmd.type      = DysektProcessor::CmdSetSliceColour;
                cmd.intParam1 = sliceIdx;
                cmd.intParam2 = (int)(unsigned) cs->getCurrentColour().withAlpha (1.0f).getARGB();
                proc.pushCommand (cmd);
            }
        }
    };

    // Ownership: the selector outlives us, so heap-allocate the listener and
    // let the selector's ComponentListener chain clean it up via deleteWhenParentDeleted.
    auto* listener = new ColourListener (processor, idx);
    selector->addChangeListener (listener);

    // Store raw pointer in properties so the selector can delete it on destruction
    // by using a custom deleter component as a child.
    struct ListenerDeleter : public juce::Component
    {
        juce::ChangeListener* target;
        juce::ChangeBroadcaster* broadcaster;
        ListenerDeleter (juce::ChangeListener* t, juce::ChangeBroadcaster* b)
            : target (t), broadcaster (b) {}
        ~ListenerDeleter() override
        {
            broadcaster->removeChangeListener (target);
            delete target;
        }
    };
    auto* deleter = new ListenerDeleter (listener, selector);
    deleter->setSize (0, 0);
    selector->addAndMakeVisible (deleter);

    juce::CallOutBox::launchAsynchronously (
        std::unique_ptr<juce::Component> (selector),
        padScreenBounds, nullptr);
}

//==============================================================================

{
    const int activePad = processor.sliceManager.selectedSlice
                              .load (std::memory_order_relaxed);

    if (activePad >= 0 && activePad < kMaxPads)
    {
        const int sliceBank = activePad / kPadsPerBank;
        if (sliceBank != currentBank)
        {
            currentBank = sliceBank;
            hoveredPad  = -1;
        }
    }

    repaint();
}
