#include "DualLcdControlFrame.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../params/ParamIds.h"

DualLcdControlFrame::DualLcdControlFrame (DysektProcessor& p)
    : processor (p)
{
    // No child components — all drawing is manual.
}

// ── drawIcon ──────────────────────────────────────────────────────────────────

void DualLcdControlFrame::drawIcon (juce::Graphics& g, juce::Rectangle<float> b,
                                    int type, bool active)
{
    const auto accent = getTheme().accent;
    const auto fg     = getTheme().foreground;

    if (active)
    {
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (b.reduced (2.0f), 4.0f);
        g.setColour (accent.withAlpha (0.70f));
        g.drawRoundedRectangle (b.reduced (1.5f), 4.0f, 1.2f);
    }

    float cx  = b.getCentreX();
    float cy2 = b.getCentreY();
    auto  col = active ? accent : fg.withAlpha (0.55f);
    g.setColour (col);

    if (type == 0) // Folder / Browser
    {
        g.fillRoundedRectangle (cx - 8, cy2 - 3, 7, 3, 1.0f);
        g.fillRoundedRectangle (cx - 9, cy2 - 2, 18, 11, 1.5f);
    }
    else if (type == 1) // Waveform
    {
        float pts[] = { -8,-1, -6,-4, -4,0, -2,4, 0,-6, 2,6, 4,-2, 6,2, 8,0 };
        juce::Path p;
        for (int i = 0; i < 9; i++)
        {
            float px = cx + pts[i*2], py = cy2 + pts[i*2+1];
            i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
        }
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    else if (type == 2) // Piano / Chromatic
    {
        for (int k = 0; k < 5; ++k)
            g.fillRect ((int)(cx - 9 + k*4), (int)(cy2 - 4), 3, 9);
        g.setColour (active ? accent.darker (0.6f) : fg.withAlpha (0.20f));
        for (int kb : {0, 1, 3, 4})
            g.fillRect ((int)(cx - 7 + kb*4), (int)(cy2 - 4), 2, 5);
    }
    else // type == 3: Bode / frequency-response curve
    {
        // LP filter Bode shape: flat shelf rolling off to the right
        juce::Path p;
        p.startNewSubPath (cx - 10, cy2 - 6);
        p.lineTo          (cx - 2,  cy2 - 6);
        p.cubicTo         (cx + 2,  cy2 - 6,
                           cx + 4,  cy2 + 1,
                           cx + 6,  cy2 + 6);
        p.lineTo          (cx + 10, cy2 + 6);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        // -3dB dot at the knee
        g.fillEllipse (cx - 0.5f, cy2 - 2.5f, 3.5f, 3.5f);
    }
}

// ── paint ─────────────────────────────────────────────────────────────────────

void DualLcdControlFrame::paint (juce::Graphics& g)
{
    const auto accent = getTheme().accent;
    const auto fg     = getTheme().foreground;
    const int  w      = getWidth();
    const int  h      = getHeight();
    const int  half   = h / 2;

    // ── Background + border ───────────────────────────────────────────────────
    {
        juce::ColourGradient grad (juce::Colour (0xFF131313), 0, 0,
                                   juce::Colour (0xFF0E0E0E), 0, (float) h, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (accent.withAlpha (0.20f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
    }

    // ── Divider ───────────────────────────────────────────────────────────────
    g.setColour (accent.withAlpha (0.10f));
    g.drawHorizontalLine (half, 6.0f, (float) w - 6.0f);

    // ── Top row: four icons + slice-count chip ────────────────────────────────
    {
        const int btnSz   = 28;
        const int btnY    = (half - btnSz) / 2;
        const int chipW   = 28;
        const int chipGap = 5;
        // Distribute 4 icons across the width, chip on the far right
        const int iconsW  = w - chipW - chipGap * 2;
        const int gap     = (iconsW - 4 * btnSz) / 5;

        filIconArea  = { gap,                       btnY, btnSz, btnSz };
        waIconArea   = { gap * 2 + btnSz,           btnY, btnSz, btnSz };
        chIconArea   = { gap * 3 + btnSz * 2,       btnY, btnSz, btnSz };
        bodeIconArea = { gap * 4 + btnSz * 3,       btnY, btnSz, btnSz };

        drawIcon (g, filIconArea .toFloat(), 0, browserActive);
        drawIcon (g, waIconArea  .toFloat(), 1, waveActive);
        drawIcon (g, chIconArea  .toFloat(), 2, chromaticActive);
        drawIcon (g, bodeIconArea.toFloat(), 3, bodeActive);

        // Slice-count chip — right side
        const auto& ui  = processor.getUiSliceSnapshot();
        int chipH = 14;
        int chipX = w - chipW - chipGap;
        int chipY = (half - chipH) / 2;
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle ((float) chipX, (float) chipY,
                                (float) chipW, (float) chipH, 3.0f);
        g.setColour (accent.withAlpha (0.40f));
        g.drawRoundedRectangle ((float) chipX, (float) chipY,
                                (float) chipW, (float) chipH, 3.0f, 0.8f);
        g.setFont (DysektLookAndFeel::makeFont (9.0f, true));
        g.setColour (accent.withAlpha (0.80f));
        g.drawText (juce::String (ui.numSlices), chipX, chipY, chipW, chipH,
                    juce::Justification::centred);
    }

    // ── Bottom row: ROOT | PITCH | VOL knobs ─────────────────────────────────
    {
        const auto& ui = processor.getUiSliceSnapshot();
        float gPitch = processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();
        float gVol   = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
        float rootN  = (float) ui.rootNote / 127.0f;
        float pitchN = (gPitch + 48.0f) / 96.0f;
        float volN   = (gVol + 100.0f) / 124.0f;

        static const char* noteNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        int rn = juce::jlimit (0, 127, ui.rootNote);
        juce::String rootStr  = juce::String (noteNames[rn % 12]) + juce::String (rn / 12 - 2);
        juce::String pitchStr = (gPitch >= 0.0f ? "+" : "") + juce::String ((int) std::round (gPitch));
        juce::String volStr   = (gVol >= 0.0f ? "+" : "") + juce::String (gVol, 1);

        const int kr  = 8;
        const float kStart = juce::MathConstants<float>::pi * 1.25f;
        const float kEnd   = juce::MathConstants<float>::pi * 2.75f;

        int kcy  = half + (h - half) / 2 - 5;
        int k1cx = w / 6;
        int k2cx = w / 2;
        int k3cx = w * 5 / 6;

        struct Knob { int cx; float norm; juce::String lbl; juce::String val; };
        Knob knobs[] = {
            { k1cx, rootN,  "ROOT",  rootStr  },
            { k2cx, pitchN, "PITCH", pitchStr },
            { k3cx, volN,   "VOL",   volStr   },
        };

        for (auto& k : knobs)
        {
            float angle = kStart + k.norm * (kEnd - kStart);

            juce::Path track;
            track.addCentredArc ((float)k.cx,(float)kcy,(float)kr,(float)kr,0.f,kStart,kEnd,true);
            g.setColour (getTheme().darkBar.brighter (0.3f));
            g.strokePath (track, juce::PathStrokeType (1.5f));

            juce::Path arc;
            arc.addCentredArc ((float)k.cx,(float)kcy,(float)kr,(float)kr,0.f,kStart,angle,true);
            g.setColour (accent);
            g.strokePath (arc, juce::PathStrokeType (2.2f));

            float lr = (float) kr - 2.0f;
            g.setColour (accent.brighter (0.3f));
            g.drawLine ((float)k.cx, (float)kcy,
                        (float)k.cx + lr * std::cos (angle),
                        (float)kcy  + lr * std::sin (angle), 1.3f);

            g.setFont (DysektLookAndFeel::makeFont (7.5f, true));
            g.setColour (fg.withAlpha (0.45f));
            g.drawText (k.lbl, k.cx - 16, kcy + kr + 2, 32, 9, juce::Justification::centred);

            g.setFont (DysektLookAndFeel::makeFont (8.0f));
            g.setColour (accent.withAlpha (0.80f));
            g.drawText (k.val, k.cx - 18, kcy + kr + 11, 36, 9, juce::Justification::centred);
        }

        rootKnobArea  = { k1cx - kr - 5, kcy - kr - 3, (kr + 5) * 2, (kr + 5) * 2 };
        pitchKnobArea = { k2cx - kr - 5, kcy - kr - 3, (kr + 5) * 2, (kr + 5) * 2 };
        volKnobArea   = { k3cx - kr - 5, kcy - kr - 3, (kr + 5) * 2, (kr + 5) * 2 };
    }
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void DualLcdControlFrame::mouseDown (const juce::MouseEvent& e)
{
    if (textEditor) textEditor.reset();

    auto pos = e.getPosition();
    dragTarget = DragTarget::None;

    // Icon toggles
    if (filIconArea.contains (pos))
    {
        browserActive = ! browserActive;
        repaint();
        if (onBrowserToggle) onBrowserToggle();
        return;
    }
    if (waIconArea.contains (pos))
    {
        waveActive = ! waveActive;
        repaint();
        if (onWaveToggle) onWaveToggle();
        return;
    }
    if (chIconArea.contains (pos))
    {
        chromaticActive = ! chromaticActive;
        repaint();
        if (onChromaticToggle) onChromaticToggle();
        return;
    }
    if (bodeIconArea.contains (pos))
    {
        bodeActive = ! bodeActive;
        repaint();
        if (onBodeToggle) onBodeToggle();
        return;
    }

    // Knobs
    if (rootKnobArea.contains (pos))
    {
        dragTarget     = DragTarget::Root;
        dragStartY     = pos.y;
        dragStartValue = (float) processor.getUiSliceSnapshot().rootNote;
        return;
    }
    if (pitchKnobArea.contains (pos))
    {
        dragTarget     = DragTarget::Pitch;
        dragStartY     = pos.y;
        dragStartValue = processor.apvts.getRawParameterValue (ParamIds::defaultPitch)->load();
        if (auto* p = processor.apvts.getParameter (ParamIds::defaultPitch))
            p->beginChangeGesture();
        return;
    }
    if (volKnobArea.contains (pos))
    {
        dragTarget     = DragTarget::Volume;
        dragStartY     = pos.y;
        dragStartValue = processor.apvts.getRawParameterValue (ParamIds::masterVolume)->load();
        if (auto* p = processor.apvts.getParameter (ParamIds::masterVolume))
            p->beginChangeGesture();
        return;
    }
}

void DualLcdControlFrame::mouseDrag (const juce::MouseEvent& e)
{
    const float delta = (float) (dragStartY - e.y);

    switch (dragTarget)
    {
        case DragTarget::Root:
        {
            float sens  = e.mods.isShiftDown() ? 0.1f : 0.4f;
            int newRoot = juce::jlimit (0, 127, (int) std::round (dragStartValue + delta * sens));
            DysektProcessor::Command cmd;
            cmd.type      = DysektProcessor::CmdSetRootNote;
            cmd.intParam1 = newRoot;
            processor.pushCommand (cmd);
            repaint();
            break;
        }
        case DragTarget::Pitch:
        {
            float sens = e.mods.isShiftDown() ? 0.05f : 0.5f;
            float newV = juce::jlimit (-48.0f, 48.0f, dragStartValue + delta * sens);
            if (auto* p = processor.apvts.getParameter (ParamIds::defaultPitch))
                p->setValueNotifyingHost (p->convertTo0to1 (newV));
            repaint();
            break;
        }
        case DragTarget::Volume:
        {
            float sens = e.mods.isShiftDown() ? 0.1f : 1.0f;
            float newV = juce::jlimit (-100.0f, 24.0f, dragStartValue + delta * sens);
            if (auto* p = processor.apvts.getParameter (ParamIds::masterVolume))
                p->setValueNotifyingHost (p->convertTo0to1 (newV));
            repaint();
            break;
        }
        default: break;
    }
}

void DualLcdControlFrame::mouseUp (const juce::MouseEvent&)
{
    if (dragTarget == DragTarget::Pitch)
        if (auto* p = processor.apvts.getParameter (ParamIds::defaultPitch))
            p->endChangeGesture();
    if (dragTarget == DragTarget::Volume)
        if (auto* p = processor.apvts.getParameter (ParamIds::masterVolume))
            p->endChangeGesture();
    dragTarget = DragTarget::None;
}

void DualLcdControlFrame::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! rootKnobArea.contains (e.getPosition())) return;

    const auto& ui = processor.getUiSliceSnapshot();
    textEditor = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (*textEditor);
    textEditor->setBounds (rootKnobArea.getX(), rootKnobArea.getBottom() - 16,
                           rootKnobArea.getWidth(), 16);
    textEditor->setFont (DysektLookAndFeel::makeFont (12.0f));
    textEditor->setColour (juce::TextEditor::backgroundColourId,
                           getTheme().header.brighter (0.2f));
    textEditor->setColour (juce::TextEditor::textColourId,    juce::Colours::white);
    textEditor->setColour (juce::TextEditor::outlineColourId, getTheme().accent);
    textEditor->setText (juce::String (ui.rootNote), false);
    textEditor->selectAll();
    textEditor->grabKeyboardFocus();

    textEditor->onReturnKey = [this] {
        if (! textEditor) return;
        int val = juce::jlimit (0, 127, textEditor->getText().getIntValue());
        DysektProcessor::Command cmd;
        cmd.type      = DysektProcessor::CmdSetRootNote;
        cmd.intParam1 = val;
        processor.pushCommand (cmd);
        textEditor.reset();
        repaint();
    };
    textEditor->onEscapeKey = [this] { textEditor.reset(); repaint(); };
    textEditor->onFocusLost = [this] { textEditor.reset(); repaint(); };
}
