#include "SliceWaveformLcd.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../params/ParamIds.h"

// ── Theme-derived colours ─────────────────────────────────────────────────────
static juce::Colour lcd2Bg()       { return getTheme().darkBar.darker (0.55f); }
static juce::Colour lcd2Phosphor() { return getTheme().accent; }
static juce::Colour lcd2Dim()      { return getTheme().accent.withAlpha (0.15f).overlaidWith (lcd2Bg()); }
static juce::Colour lcd2Bright()   { return getTheme().accent.brighter (0.45f); }

const juce::Colour SliceWaveformLcd::kBg       { 0xFF050F0E };
const juce::Colour SliceWaveformLcd::kBezel    { 0xFF0D1E1C };
const juce::Colour SliceWaveformLcd::kPhosphor { 0xFF2AFFD0 };
const juce::Colour SliceWaveformLcd::kDim      { 0xFF0A2A22 };
const juce::Colour SliceWaveformLcd::kBright   { 0xFF8AFFF0 };
const juce::Colour SliceWaveformLcd::kLabel    { 0xFF1A7060 };

// Toxic Candy node colours (match ThemeData palette)
static const juce::Colour kColAttack  { 0xFF00FF87 };   // Toxic Lime
static const juce::Colour kColDecay   { 0xFFFFE800 };   // Radioactive Yellow
static const juce::Colour kColSustain { 0xFF00C8FF };   // Ice Blue
static const juce::Colour kColRelease { 0xFFFF6B00 };   // Molten Orange

// ── Constructor ───────────────────────────────────────────────────────────────

SliceWaveformLcd::SliceWaveformLcd (DysektProcessor& p)
    : processor (p)
{
    setOpaque (true);
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SliceWaveformLcd::resized()
{
    screenArea = getLocalBounds().reduced (4).toFloat();
}

void SliceWaveformLcd::repaintLcd()
{
    repaint();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

juce::String SliceWaveformLcd::midiNoteName (int note)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    note = juce::jlimit (0, 127, note);
    return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
}

juce::String SliceWaveformLcd::formatMs (float ms)
{
    if (ms < 1000.0f) return juce::String (juce::roundToInt (ms)) + "ms";
    return juce::String (ms / 1000.0f, 2) + "s";
}

// ── Data building ─────────────────────────────────────────────────────────────

void SliceWaveformLcd::buildDisplayData()
{
    data = {};

    const auto& snap = processor.getUiSliceSnapshot();
    data.hasSample   = snap.sampleLoaded && ! snap.sampleMissing;
    data.numSlices   = snap.numSlices;
    data.sampleName  = snap.sampleFileName;
    data.totalFrames = snap.sampleNumFrames;
    data.sampleRate  = processor.getSampleRate() > 0.0
                           ? processor.getSampleRate() : 44100.0;

    if (! data.hasSample || snap.selectedSlice < 0 || snap.selectedSlice >= snap.numSlices)
        return;

    data.hasSlice    = true;
    data.sliceIndex  = snap.selectedSlice;

    const auto& sl      = snap.slices[(size_t) snap.selectedSlice];
    data.startSample    = sl.startSample;
    data.endSample      = processor.sliceManager.getEndForSlice (
                              snap.selectedSlice, data.totalFrames);
    data.midiNote       = sl.midiNote;
    data.volume         = sl.volume;
    data.pan            = sl.pan;
    data.pitchSemitones = sl.pitchSemitones;
    data.algorithm      = sl.algorithm;

    const int kPeaks  = 256;
    data.peaks.clearQuick();
    data.peaks.insertMultiple (-1, 0.0f, kPeaks);

    const int sliceLen = data.endSample - data.startSample;
    if (sliceLen <= 0) return;

    for (int i = 0; i < kPeaks; i++)
    {
        const float t   = (float) i / (float) kPeaks;
        const int   pos = data.startSample + (int) (t * (float) sliceLen);
        data.peaks.set (i, processor.getWaveformPeakAt (pos));
    }
}

// ── Envelope: read params → normalised nodes ──────────────────────────────────

void SliceWaveformLcd::buildEnvelopeNodes()
{
    // Read raw param values (getRawParameterValue gives denormalised units)
    const float attackMs  = processor.attackParam  ? processor.attackParam ->load() : 5.0f;
    const float decayMs   = processor.decayParam   ? processor.decayParam  ->load() : 100.0f;
    const float sustainPc = processor.sustainParam ? processor.sustainParam->load() : 100.0f;
    const float releaseMs = processor.releaseParam ? processor.releaseParam->load() : 20.0f;

    // Fixed per-segment X budgets (must match commitNodes exactly):
    //   Attack  segment: [0   , kAX ]  — param max 1000 ms
    //   Decay   segment: [kAX , kDX ]  — param max 5000 ms
    //   Sustain segment: [kDX , kRX ]  — fixed plateau
    //   Release segment: [kRX , 1.0 ]  — param max 5000 ms
    static constexpr float kAX = 0.20f;   // end of attack zone
    static constexpr float kDX = 0.45f;   // end of decay zone
    static constexpr float kRX = 0.80f;   // end of sustain zone / start of release

    env.ax = juce::jlimit (0.02f, kAX - 0.02f,  (attackMs  / 1000.0f) * kAX);
    env.dx = juce::jlimit (kAX + 0.02f, kDX - 0.02f,
                            kAX + (decayMs / 5000.0f) * (kDX - kAX));
    env.rx = juce::jlimit (kDX + 0.02f, kRX - 0.02f,
                            kDX + 0.5f * (kRX - kDX));   // fixed mid-point, not param-driven
    env.sy = juce::jlimit (0.05f, 0.95f, 1.0f - (sustainPc / 100.0f));  // 0=top=loud
    env.ay = 0.08f;  // attack peak always near top

    // Rebuild node list
    envNodes.clear();

    EnvNode a; a.xn = env.ax; a.yn = env.ay; a.role = NodeRole::Attack;
    a.colour = kColAttack;  a.label = "A"; envNodes.add (a);

    EnvNode d; d.xn = env.dx; d.yn = env.sy; d.role = NodeRole::Decay;
    d.colour = kColDecay;   d.label = "D"; envNodes.add (d);

    // Sustain handle: mid of plateau
    EnvNode s;
    s.xn = (env.dx + env.rx) * 0.5f; s.yn = env.sy; s.role = NodeRole::Sustain;
    s.colour = kColSustain; s.label = "S"; envNodes.add (s);

    EnvNode r; r.xn = env.rx; r.yn = env.sy; r.role = NodeRole::Release;
    r.colour = kColRelease; r.label = "R"; envNodes.add (r);
}

// Write normalised nodes back to ADSR params ──────────────────────────────────

void SliceWaveformLcd::commitNodes()
{
    // Inverse-map using the same fixed segment budgets as buildEnvelopeNodes
    static constexpr float kAX = 0.20f;
    static constexpr float kDX = 0.45f;
    static constexpr float kRX = 0.80f;

    const float attackMs  = juce::jlimit (0.0f, 1000.0f, (env.ax / kAX) * 1000.0f);
    const float decayMs   = juce::jlimit (0.0f, 5000.0f,
                                ((env.dx - kAX) / (kDX - kAX)) * 5000.0f);
    const float sustainPc = juce::jlimit (0.0f, 100.0f,  (1.0f - env.sy) * 100.0f);
    const float releaseMs = juce::jlimit (0.0f, 5000.0f,
                                ((1.0f - env.rx) / (1.0f - kRX)) * 5000.0f);

    auto setParam = [&] (const juce::String& id, float val)
    {
        if (auto* p = processor.apvts.getParameter (id))
        {
            const float norm = p->convertTo0to1 (val);
            p->setValueNotifyingHost (norm);
        }
    };

    setParam (ParamIds::defaultAttack,  attackMs);
    setParam (ParamIds::defaultDecay,   decayMs);
    setParam (ParamIds::defaultSustain, sustainPc);
    setParam (ParamIds::defaultRelease, releaseMs);
}

// Envelope Y at normalised X (linear interpolation between nodes) ─────────────

float SliceWaveformLcd::envAt (float xn) const
{
    // Polyline: P0(0,1) → P1(ax,ay) → P2(dx,sy) → P3(rx,sy) → P4(1,1)
    struct Pt { float x, y; };
    const Pt pts[] = {
        { 0.0f,   1.0f   },
        { env.ax, env.ay },
        { env.dx, env.sy },
        { env.rx, env.sy },
        { 1.0f,   1.0f   }
    };
    constexpr int N = 5;

    for (int i = 0; i < N - 1; ++i)
    {
        if (xn >= pts[i].x && xn <= pts[i+1].x)
        {
            const float span = pts[i+1].x - pts[i].x;
            const float t    = (span > 0.0f) ? (xn - pts[i].x) / span : 0.0f;
            return pts[i].y + t * (pts[i+1].y - pts[i].y);
        }
    }
    return 1.0f;
}

// ── Hit testing ───────────────────────────────────────────────────────────────

SliceWaveformLcd::NodeRole SliceWaveformLcd::hitTest (juce::Point<float> pos) const
{
    if (screenArea.isEmpty()) return NodeRole::None;

    const float W = screenArea.getWidth();
    const float H = screenArea.getHeight();
    const float ox = screenArea.getX();
    const float oy = screenArea.getY();

    NodeRole best = NodeRole::None;
    float    bestD2 = kHitR * kHitR;

    for (const auto& n : envNodes)
    {
        const float nx = ox + n.xn * W;
        const float ny = oy + n.yn * H;
        const float dx = pos.x - nx;
        const float dy = pos.y - ny;
        const float d2 = dx*dx + dy*dy;
        if (d2 < bestD2) { bestD2 = d2; best = n.role; }
    }
    return best;
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void SliceWaveformLcd::mouseMove (const juce::MouseEvent& e)
{
    const auto newHov = hitTest (e.position);
    if (newHov != hovRole)
    {
        hovRole = newHov;
        setMouseCursor (hovRole != NodeRole::None
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SliceWaveformLcd::mouseDown (const juce::MouseEvent& e)
{
    dragRole = hitTest (e.position);
}

void SliceWaveformLcd::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRole == NodeRole::None || screenArea.isEmpty()) return;

    const float W  = screenArea.getWidth();
    const float H  = screenArea.getHeight();
    const float ox = screenArea.getX();
    const float oy = screenArea.getY();

    const float xn = juce::jlimit (0.01f, 0.99f, (e.position.x - ox) / W);
    const float yn = juce::jlimit (0.02f, 0.98f, (e.position.y - oy) / H);

    if      (dragRole == NodeRole::Attack)
    {
        env.ax = juce::jlimit (0.02f, env.dx - 0.04f, xn);
        env.ay = yn;
    }
    else if (dragRole == NodeRole::Decay)
    {
        env.dx = juce::jlimit (env.ax + 0.04f, env.rx - 0.04f, xn);
        env.sy = yn;
    }
    else if (dragRole == NodeRole::Sustain)
    {
        env.sy = yn;
    }
    else if (dragRole == NodeRole::Release)
    {
        env.rx = juce::jlimit (env.dx + 0.04f, 0.97f, xn);
    }

    // Update envNodes[] positions from the new env.* values WITHOUT re-reading params
    // (buildEnvelopeNodes would overwrite env.* from params — wrong during drag)
    envNodes.clear();
    EnvNode a; a.xn = env.ax; a.yn = env.ay; a.role = NodeRole::Attack;
    a.colour = kColAttack; a.label = "A"; envNodes.add (a);
    EnvNode d; d.xn = env.dx; d.yn = env.sy; d.role = NodeRole::Decay;
    d.colour = kColDecay;  d.label = "D"; envNodes.add (d);
    EnvNode s; s.xn = (env.dx + env.rx) * 0.5f; s.yn = env.sy; s.role = NodeRole::Sustain;
    s.colour = kColSustain; s.label = "S"; envNodes.add (s);
    EnvNode r; r.xn = env.rx; r.yn = env.sy; r.role = NodeRole::Release;
    r.colour = kColRelease; r.label = "R"; envNodes.add (r);

    // Push to params
    commitNodes();

    repaint();
}

void SliceWaveformLcd::mouseUp (const juce::MouseEvent&)
{
    dragRole = NodeRole::None;
    // Re-sync env.* from params after drag ends (cleans up any float drift)
    buildEnvelopeNodes();
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void SliceWaveformLcd::drawBackground (juce::Graphics& g)
{
    const auto ac = getTheme().accent;
    auto b = getLocalBounds();

    juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
                                     juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
    g.setGradientFill (outerGrad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);
    g.setColour (ac.withAlpha (0.20f));
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

    auto screen = b.reduced (4);
    g.setColour (lcd2Bg());
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    juce::ColourGradient glow (lcd2Phosphor().withAlpha (0.07f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack, 0, (float) (screen.getY() + 18), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (juce::Colour (0xFF000000).withAlpha ((uint8_t) kScanlineAlpha));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    g.setColour (ac.withAlpha (0.12f));
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
}

void SliceWaveformLcd::drawWaveform (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    if (data.peaks.isEmpty()) return;

    const float cy = area.getCentreY();
    const float W  = area.getWidth();
    const float H  = area.getHeight();
    const int   n  = data.peaks.size();

    // Grid
    g.setColour (lcd2Phosphor().withAlpha (0.06f));
    g.drawHorizontalLine (juce::roundToInt (cy), area.getX(), area.getRight());

    // Build waveform paths — amplitude modulated by envelope shape
    juce::Path fill, lineTop, lineBot;
    bool first = true;

    for (int i = 0; i < n; i++)
    {
        const float xn    = (float) i / (float) n;
        const float x     = area.getX() + xn * W;
        const float eGain = 1.0f - envAt (xn);   // 0=silence 1=full
        const float amp   = juce::jlimit (0.0f, 1.0f, data.peaks[i]) * (H * 0.45f) * eGain;

        const float yT = cy - amp;
        const float yB = cy + amp;

        if (first)
        {
            lineTop.startNewSubPath (x, yT);
            lineBot.startNewSubPath (x, yB);
            first = false;
        }
        else
        {
            lineTop.lineTo (x, yT);
            lineBot.lineTo (x, yB);
        }
    }

    fill = lineTop;
    for (int i = n - 1; i >= 0; i--)
    {
        const float xn    = (float) i / (float) n;
        const float x     = area.getX() + xn * W;
        const float eGain = 1.0f - envAt (xn);
        const float amp   = juce::jlimit (0.0f, 1.0f, data.peaks[i]) * (H * 0.45f) * eGain;
        fill.lineTo (x, cy + amp);
    }
    fill.closeSubPath();

    g.setColour (lcd2Phosphor().withAlpha (0.12f));
    g.fillPath (fill);

    juce::PathStrokeType glow (2.5f);
    g.setColour (lcd2Phosphor().withAlpha (0.22f));
    g.strokePath (lineTop, glow);
    g.strokePath (lineBot, glow);

    juce::PathStrokeType sharp (1.1f);
    g.setColour (lcd2Phosphor().withAlpha (0.85f));
    g.strokePath (lineTop, sharp);
    g.strokePath (lineBot, sharp);

    // Slice boundary markers
    g.setColour (lcd2Phosphor().withAlpha (0.50f));
    g.drawVerticalLine (juce::roundToInt (area.getX() + 1),      area.getY(), area.getBottom());
    g.drawVerticalLine (juce::roundToInt (area.getRight() - 1),  area.getY(), area.getBottom());
}

void SliceWaveformLcd::drawSegmentLabel (juce::Graphics& g,
                                          float x0, float y0,
                                          float x1, float y1,
                                          const char* text,
                                          juce::Colour col,
                                          const juce::Rectangle<float>& area)
{
    const float mx = area.getX() + ((x0 + x1) * 0.5f) * area.getWidth();
    const float my = area.getY() + ((y0 + y1) * 0.5f) * area.getHeight() - 9.0f;
    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (col.withAlpha (0.40f));
    g.drawText (juce::String (text),
                juce::Rectangle<float> (mx - 30.0f, my - 6.0f, 60.0f, 12.0f),
                juce::Justification::centred, false);
}

void SliceWaveformLcd::drawEnvelope (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    const float W  = area.getWidth();
    const float H  = area.getHeight();
    const float ox = area.getX();
    const float oy = area.getY();

    auto px = [&] (float xn) { return ox + xn * W; };
    auto py = [&] (float yn) { return oy + yn * H; };

    // ── Filled envelope region ────────────────────────────────────────────────
    juce::Path envFill;
    envFill.startNewSubPath (px (0.0f), py (1.0f));
    envFill.lineTo (px (0.0f),   py (1.0f));
    envFill.lineTo (px (env.ax), py (env.ay));
    envFill.lineTo (px (env.dx), py (env.sy));
    envFill.lineTo (px (env.rx), py (env.sy));
    envFill.lineTo (px (1.0f),   py (1.0f));
    envFill.closeSubPath();

    juce::ColourGradient fillGrad (kColDecay.withAlpha (0.08f), 0, oy,
                                    kColDecay.withAlpha (0.00f), 0, oy + H, false);
    g.setGradientFill (fillGrad);
    g.fillPath (envFill);

    // ── Envelope polyline ─────────────────────────────────────────────────────
    juce::Path envLine;
    envLine.startNewSubPath (px (0.0f),   py (1.0f));
    envLine.lineTo          (px (env.ax), py (env.ay));
    envLine.lineTo          (px (env.dx), py (env.sy));
    envLine.lineTo          (px (env.rx), py (env.sy));
    envLine.lineTo          (px (1.0f),   py (1.0f));

    // Glow pass
    juce::PathStrokeType glowStroke (2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.strokePath (envLine, glowStroke);

    // Main line (dashed via path flattening)
    juce::Path dashedLine;
    {
        juce::PathStrokeType stroke (1.0f);
        float dashes[] = { 3.0f, 5.0f };
        stroke.createDashedStroke (dashedLine, envLine, dashes, 2);
    }
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.fillPath (dashedLine);

    // Sustain plateau highlighted
    juce::Path susLine;
    susLine.startNewSubPath (px (env.dx), py (env.sy));
    susLine.lineTo          (px (env.rx), py (env.sy));
    g.setColour (kColSustain.withAlpha (0.35f));
    g.strokePath (susLine, juce::PathStrokeType (1.0f));

    // ── Segment labels ────────────────────────────────────────────────────────
    drawSegmentLabel (g, 0.0f, 1.0f, env.ax, env.ay, "FADE IN",  kColAttack,  area);
    drawSegmentLabel (g, env.ax, env.ay, env.dx, env.sy, "DECAY", kColDecay,   area);
    drawSegmentLabel (g, env.rx, env.sy, 1.0f, 1.0f, "FADE OUT", kColRelease, area);
}

void SliceWaveformLcd::drawNodes (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    const float W  = area.getWidth();
    const float H  = area.getHeight();
    const float ox = area.getX();
    const float oy = area.getY();

    for (const auto& node : envNodes)
    {
        const float cx = ox + node.xn * W;
        const float cy = oy + node.yn * H;
        const bool  hov = (node.role == hovRole || node.role == dragRole);
        const float r   = hov ? kNodeR + 2.5f : kNodeR;

        // Glow ring
        g.setColour (node.colour.withAlpha (hov ? 0.55f : 0.25f));
        g.drawEllipse (cx - r, cy - r, r * 2, r * 2, hov ? 1.5f : 1.0f);

        // Inner dot
        const float dr = hov ? 3.0f : 2.5f;
        g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.80f));
        g.fillEllipse (cx - dr, cy - dr, dr * 2, dr * 2);

        // Label above
        g.setFont (DysektLookAndFeel::makeFont (hov ? 9.5f : 8.0f, true));
        g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.70f));
        g.drawText (juce::String (node.label),
                    juce::Rectangle<float> (cx - 12.0f, cy - r - 14.0f, 24.0f, 12.0f),
                    juce::Justification::centred, false);

        // Tick line down to envelope (for vertical nodes)
        if (node.role != NodeRole::Sustain)
        {
            g.setColour (node.colour.withAlpha (0.18f));
            g.drawVerticalLine (juce::roundToInt (cx),
                                cy + r,
                                oy + H);
        }
    }
}

void SliceWaveformLcd::drawNoData (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (4);
    g.setFont (DysektLookAndFeel::makeFont (10.0f));
    g.setColour (lcd2Dim().brighter (0.4f));

    if (! data.hasSample)
        g.drawText ("-- NO SAMPLE LOADED --", b, juce::Justification::centred);
    else
        g.drawText ("-- SELECT A SLICE --",   b, juce::Justification::centred);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SliceWaveformLcd::paint (juce::Graphics& g)
{
    buildDisplayData();
    drawBackground (g);

    if (! data.hasSample || ! data.hasSlice)
    {
        drawNoData (g);
        return;
    }

    // Rebuild envelope nodes from current param values (unless mid-drag)
    if (dragRole == NodeRole::None)
        buildEnvelopeNodes();

    const auto area = getLocalBounds().reduced (4).toFloat().reduced (2.0f);
    screenArea = area;

    drawWaveform (g, area);
    drawEnvelope (g, area);
    drawNodes    (g, area);
}
