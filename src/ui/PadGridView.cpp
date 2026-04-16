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

    drawBtn (bankAButtonBounds, 0, "Bank A  (1–16)");
    drawBtn (bankBButtonBounds, 1, "Bank B  (17–32)");
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

    // ── Background: full pad filled with the slice colour ─────────────────────
    if (isEmpty)
    {
        g.setColour (th.button.withAlpha (0.30f));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        // Faint border
        g.setColour (th.separator.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 0.75f);
        return;
    }

    const auto& slice    = ui.slices[(size_t) absIndex];
    const juce::Colour sliceCol = slice.colour;

    // Darkened version of the slice colour for the pad body
    // Selected pads are a bit brighter so they stand out clearly.
    juce::Colour padBg = sliceCol.darker (sel ? 0.38f : 0.58f);
    if (hov) padBg = padBg.brighter (0.12f);

    g.setColour (padBg);
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    // Slice-colour border — bolder when selected
    {
        const float alpha = sel ? 1.0f : (hov ? 0.70f : 0.50f);
        const float thick = sel ? 1.5f : 0.75f;
        g.setColour (sliceCol.withAlpha (alpha));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, thick);
    }

    // ── Inner layout ──────────────────────────────────────────────────────────
    auto inner     = bounds.reduced (5, 4);

    // Top row: note name left + slice name centred  (fixed height = 14 px)
    auto topRow    = inner.removeFromTop (14);

    // Remaining space = waveform canvas (no meter strip any more)
    auto waveArea  = inner;

    // ── MIDI note name — top-left (replaces old pad number) ──────────────────
    {
        g.setFont (DysektLookAndFeel::makeMonoFont (8.0f));
        g.setColour (sliceCol.brighter (0.55f).withAlpha (0.90f));
        g.drawText (midiNoteName (slice.midiNote),
                    topRow.withWidth (30),
                    juce::Justification::centredLeft);
    }

    // ── Slice name — top-centre ───────────────────────────────────────────────
    {
        const juce::String displayName = slice.name.isNotEmpty()
                                       ? slice.name
                                       : ("Slice " + juce::String (absIndex + 1));
        g.setFont (DysektLookAndFeel::makeFont (9.5f, true));
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

            // Use slice colour (brightened slightly) to mirror SliceWaveformLcd
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

    // ── Playhead — one per active voice playing this slice ───────────────────
    if (ui.sampleLoaded && waveArea.getWidth() > 4)
    {
        const int startSamp = slice.startSample;
        const int endSamp   = processor.sliceManager.getEndForSlice (absIndex, ui.sampleNumFrames);
        const int sliceLen  = endSamp - startSamp;

        if (sliceLen > 0)
        {
            auto& vp = processor.voicePool;

            for (int i = 0; i < VoicePool::kMaxVoices; ++i)
            {
                const auto& v = vp.getVoice (i);
                if (! v.active || v.sliceIdx != absIndex)
                    continue;

                const float rawPos = vp.voicePositions[i].load (std::memory_order_relaxed);
                float xn = (rawPos - (float) startSamp) / (float) sliceLen;
                xn = juce::jlimit (0.0f, 1.0f, xn);

                const float x  = (float) waveArea.getX() + xn * (float) waveArea.getWidth();
                const float y1 = (float) waveArea.getY();
                const float y2 = (float) waveArea.getBottom();

                // Phosphor glow halo (matches SliceWaveformLcd style)
                g.setColour (sliceCol.brighter (0.6f).withAlpha (0.15f));
                g.drawLine (x - 1.5f, y1, x - 1.5f, y2, 1.0f);
                g.drawLine (x + 1.5f, y1, x + 1.5f, y2, 1.0f);

                // Main playhead line
                g.setColour (sliceCol.brighter (0.8f).withAlpha (0.90f));
                g.drawLine (x, y1, x, y2, 1.5f);

                // Small downward triangle cap at top
                const float capH = 4.5f;
                juce::Path cap;
                cap.addTriangle (x - 3.0f, y1,
                                 x + 3.0f, y1,
                                 x,        y1 + capH);
                g.fillPath (cap);

                break;  // show only the most recently triggered voice (same as LCD)
            }
        }
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
