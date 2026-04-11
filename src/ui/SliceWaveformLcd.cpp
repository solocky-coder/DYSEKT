#include "SliceWaveformLcd.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../params/ParamIds.h"
#include "../audio/Slice.h"

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
static const juce::Colour kColHold    { 0xFFFF00FF };   // Hot Magenta

// ── Constructor ───────────────────────────────────────────────────────────────

SliceWaveformLcd::SliceWaveformLcd (DysektProcessor& p)
    : processor (p)
{
    setOpaque (false); // rounded corners — must not claim full opaque coverage
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SliceWaveformLcd::resized()
{
    screenArea = getLocalBounds().reduced (4).toFloat();
}

void SliceWaveformLcd::repaintLcd()
{
    if (dragRole == NodeRole::None)
    {
        if (postCommitGuard > 0)
        {
            --postCommitGuard;
        }
        else
        {
            const int ver = processor.getUiSliceSnapshotVersion();
            if (ver != lastEnvSnapVer)
            {
                buildEnvelopeNodes();
                lastEnvSnapVer = ver;
            }
        }
    }
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
    data.sampleName  = snap.isDefaultSample ? juce::String() : snap.sampleFileName;
    data.isDefault   = snap.isDefaultSample;
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

// ── getSliceDurMs ─────────────────────────────────────────────────────────────
// Returns the duration (in ms) of the currently selected slice.
// Used by buildEnvelopeNodes / commitNodes to scale ADSR positions relative
// to actual slice length so dragging is intuitive regardless of slice size.
float SliceWaveformLcd::getSliceDurMs() const
{
    static constexpr float kDefaultMs = 1000.0f;   // fallback if no slice loaded

    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel < 0 || sel >= processor.sliceManager.getNumSlices())
        return kDefaultMs;

    const auto& s      = processor.sliceManager.getSlice (sel);
    const int   total  = processor.sampleData.getNumFrames();
    if (total <= 0)
        return kDefaultMs;

    const int sliceEnd = processor.sliceManager.getEndForSlice (sel, total);
    const int len      = sliceEnd - s.startSample;
    if (len <= 0)
        return kDefaultMs;

    const float sr = (float) processor.voicePool.getSampleRate();
    return (float) len / sr * 1000.0f;
}

void SliceWaveformLcd::buildEnvelopeNodes()
{
    // Mirror SCB logic exactly: unlocked fields read from global APVTS (same units),
    // locked fields read from per-slice values.
    // APVTS params: Attack 0-120000ms, Decay 0-120000ms, Sustain 0-100%, Release 0-120000ms
    float attackMs  = processor.attackParam  ? processor.attackParam ->load() : 5.0f;
    float holdMs    = processor.holdParam    ? processor.holdParam   ->load() : 0.0f;
    float decayMs   = processor.decayParam   ? processor.decayParam  ->load() : 100.0f;
    float sustainPc = processor.sustainParam ? processor.sustainParam->load() : 100.0f;
    float releaseMs = processor.releaseParam ? processor.releaseParam->load() : 20.0f;

    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel >= 0 && sel < processor.sliceManager.getNumSlices())
    {
        const auto& s = processor.sliceManager.getSlice (sel);
        // Per-field: if locked use slice value, otherwise use global APVTS value above
        if (s.lockMask & kLockAttack)  attackMs  = s.attackSec    * 1000.0f;
        if (s.lockMask & kLockHold)    holdMs    = s.holdSec      * 1000.0f;
        if (s.lockMask & kLockDecay)   decayMs   = s.decaySec     * 1000.0f;
        if (s.lockMask & kLockSustain) sustainPc = s.sustainLevel  * 100.0f;
        if (s.lockMask & kLockRelease) releaseMs = s.releaseSec   * 1000.0f;
    }

    // Layout (display-only proportions — must match commitNodes exactly):
    //   Attack  : [0       .. ax    ]  attack peak always at top
    //   Decay   : [ax      .. dx    ]  falls from peak to sustain
    //   Sustain : [dx      .. kSEnd ]  flat plateau (kSEnd is fixed)
    //   Release : [kSEnd   .. rEnd  ]  falls from sustain to silence
    //
    //   A node: (ax,    TOP )  drag right = more attack
    //   D node: (dx,    sy  )  drag right = more decay
    //   S node: (mid,   sy  )  drag up/down = sustain level
    //   R node: (rEnd,  BOT )  drag right = more release tail
    // Attack spans up to 85% of display; D/S/R share remaining space proportionally
    static constexpr float kAX   = 0.85f;   // attack can span entire slice
    static constexpr float kRMax = 0.99f;

    // Fixed view windows matching the SCB knob ranges:
    //   Attack  → 0-1000 ms  (knob max = 1.0s)
    //   Decay   → 0-5000 ms  (knob max = 5.0s)
    //   Release → 0-5000 ms  (knob max = 5.0s)
    // Using sliceDurMs as the window caused the node to pin at the right edge
    // once the value exceeded the slice length (e.g. 464ms slice + 5000ms decay).
    const float sliceDurMs_   = juce::jmax (1.0f, getSliceDurMs());
    const float kAttackViewMs  = sliceDurMs_;
    const float kHoldViewMs    = sliceDurMs_;
    const float kDecayViewMs   = sliceDurMs_;
    const float releaseViewMs  = sliceDurMs_;
    const float attackNorm  = std::sqrt (juce::jmin (attackMs  / kAttackViewMs,  1.0f));
    const float holdNorm    = std::sqrt (juce::jmin (holdMs    / kHoldViewMs,    1.0f));
    const float decayNorm   = std::sqrt (juce::jmin (decayMs   / kDecayViewMs,   1.0f));

    env.ax  = juce::jlimit (0.0f, kAX, attackNorm * kAX);

    // Remaining space after attack — same proportions as original at low attack
    const float remain    = kRMax - env.ax;
    const float kHX_eff   = env.ax + remain * 0.20f;   // hold end (up to 20% of space)
    const float kDX_eff   = env.ax + remain * 0.47f;   // decay zone end
    const float kSEnd_eff = env.ax + remain * 0.65f;   // sustain plateau end

    env.hx  = juce::jlimit (env.ax, kHX_eff,
                            env.ax + holdNorm * (kHX_eff - env.ax));

    env.dx    = juce::jlimit (env.hx + 0.01f, kDX_eff,
                              env.hx + decayNorm * (kDX_eff - env.hx));
    env.sy    = juce::jlimit (0.04f, 0.94f, 1.0f - (sustainPc / 100.0f));
    env.ay    = 0.04f;   // attack peak near top (standard ADSR visual)
    env.sxEnd = kSEnd_eff;
    // Release node represents fade-out START (standard ADSR), not fade-out end.
    env.rx    = juce::jlimit (0.0f, 1.0f,
                              1.0f - (releaseMs / releaseViewMs));

    // Rebuild node list
    envNodes.clear();

    EnvNode a; a.xn = env.ax; a.yn = env.ay; a.role = NodeRole::Attack;
    a.colour = kColAttack;  a.label = "A"; envNodes.add (a);

    // Hold node: X-only drag — stays at peak level (ay)
    EnvNode h; h.xn = env.hx; h.yn = env.ay; h.role = NodeRole::Hold;
    h.colour = kColHold;    h.label = "H"; envNodes.add (h);

    // Decay node sits on the sustain line at decay end (standard ADSR visual).
    EnvNode d; d.xn = env.dx; d.yn = env.sy; d.role = NodeRole::Decay;
    d.colour = kColDecay;   d.label = "D"; envNodes.add (d);

    // Sustain handle: mid of plateau [dx .. kSEnd]
    EnvNode s;
    s.xn = (env.dx + env.sxEnd) * 0.5f; s.yn = env.sy; s.role = NodeRole::Sustain;
    s.colour = kColSustain; s.label = "S"; envNodes.add (s);

    // Release node: fade-out start, on sustain level
    EnvNode r; r.xn = env.rx; r.yn = env.sy; r.role = NodeRole::Release;
    r.colour = kColRelease; r.label = "R"; envNodes.add (r);
}

// Write normalised nodes back to ADSR params ──────────────────────────────────

void SliceWaveformLcd::commitNodes()
{
    // Inverse-map (must match buildEnvelopeNodes constants exactly)
    // Inverse-map: must match buildEnvNodes dynamic layout
    static constexpr float kAX   = 0.85f;
    static constexpr float kRMax = 0.99f;

    // Inverse of slice-relative sqrt mapping in buildEnvelopeNodes:
    //   ms = ratio^2 * kXxxViewMs  (inverse of  norm = sqrt(ms/kXxxViewMs))
    const float sliceDurMs_c  = juce::jmax (1.0f, getSliceDurMs());
    const float kAttackViewMs  = sliceDurMs_c;
    const float kHoldViewMs    = sliceDurMs_c;
    const float kDecayViewMs   = sliceDurMs_c;
    const float releaseViewMs  = sliceDurMs_c;
    const float remain_c  = kRMax - env.ax;
    const float kHX_eff   = env.ax + remain_c * 0.20f;
    const float kDX_eff   = env.ax + remain_c * 0.47f;
    const float kSEnd_eff = env.ax + remain_c * 0.65f;
    const float aRatio = env.ax / kAX;
    const float hRatio = (kHX_eff > env.ax) ? (env.hx - env.ax) / (kHX_eff - env.ax) : 0.0f;
    const float dRatio = (kDX_eff > env.hx) ? (env.dx - env.hx) / (kDX_eff - env.hx) : 0.0f;
    const float attackMs  = juce::jlimit (0.0f, kAttackViewMs,  aRatio * aRatio * kAttackViewMs);
    const float holdMs    = juce::jlimit (0.0f, kHoldViewMs,    hRatio * hRatio * kHoldViewMs);
    const float decayMs   = juce::jlimit (0.0f, kDecayViewMs,   dRatio * dRatio * kDecayViewMs);
    const float sustainPc = juce::jlimit (0.0f, 100.0f,     (1.0f - env.sy) * 100.0f);
    const float releaseMs = juce::jlimit (0.0f, releaseViewMs, (1.0f - env.rx) * releaseViewMs);

    // Write to selected slice via CmdSetSliceParam (per-slice, not global)
    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel < 0 || sel >= processor.sliceManager.getNumSlices()) return;

    auto sendField = [&] (DysektProcessor::SliceParamField field, float val)
    {
        DysektProcessor::Command cmd;
        cmd.type        = DysektProcessor::CmdSetSliceParam;
        cmd.intParam1   = (int) field;   // CmdSetSliceParam: intParam1=field, floatParam1=val
        cmd.floatParam1 = val;
        processor.pushCommand (cmd);
    };

    sendField (DysektProcessor::FieldAttack,  attackMs  / 1000.0f);   // seconds
    sendField (DysektProcessor::FieldHold,    holdMs    / 1000.0f);
    sendField (DysektProcessor::FieldDecay,   decayMs   / 1000.0f);
    sendField (DysektProcessor::FieldSustain, sustainPc / 100.0f);    // 0-1
    sendField (DysektProcessor::FieldRelease, releaseMs / 1000.0f);
    // NOTE: do NOT touch the global APVTS params here — CmdSetSliceParam locks the
    // per-slice value and sets the lock bit, so the SCB knob reads s.xxxSec directly.
    // Writing to the global default would silently override every unlocked slice.

    // Give the processor time to echo the new values before rebuilding
    postCommitGuard = 6;
    lastEnvSnapVer  = -1;   // force rebuild once guard expires
}

// Envelope Y at normalised X (linear interpolation between nodes) ─────────────

float SliceWaveformLcd::envAt (float xn) const
{
    // Polyline: P0(0,1) → P1(ax,top) → P2(dx,sy) → P3(kSEnd,sy) → P4(rx,sy) → P5(1,1)
    // kSEnd is dynamic — stored in env.sxEnd by buildEnvNodes
    const float kSEnd = env.sxEnd;
    struct Pt { float x, y; };
    const Pt pts[] = {
        { 0.0f,   1.0f   },
        { env.ax, env.ay },   // attack peak (env.ay = 0.04, near top)
        { env.hx, env.ay },   // hold plateau end (same peak height)
        { env.dx, env.sy },   // end of decay / sustain level
        { kSEnd,  env.sy },   // dynamic end of sustain plateau
        { env.rx, env.sy },   // release start
        { 1.0f,   1.0f   }    // end of release (silence)
    };
    constexpr int N = 7;

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
    const NodeRole hit = hitTest (e.position);

    if (e.mods.isRightButtonDown())
    {
        // Right-click on a node: toggle that ADSR field's lock for the selected slice
        uint32_t bit = 0;
        if      (hit == NodeRole::Attack)  bit = kLockAttack;
        else if (hit == NodeRole::Hold)    bit = kLockHold;
        else if (hit == NodeRole::Decay)   bit = kLockDecay;
        else if (hit == NodeRole::Sustain) bit = kLockSustain;
        else if (hit == NodeRole::Release) bit = kLockRelease;

        if (bit != 0)
        {
            const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
            if (sel >= 0 && sel < processor.sliceManager.getNumSlices())
            {
                const auto& s = processor.sliceManager.getSlice (sel);
                const bool currentlyLocked = (s.lockMask & bit) != 0;

                if (currentlyLocked)
                {
                    DysektProcessor::Command cmd;
                    cmd.type      = DysektProcessor::CmdToggleLock;
                    cmd.intParam1 = (int) bit;
                    processor.pushCommand (cmd);
                }
                else
                {
                    // Preserve the current node value when locking so it doesn't jump.
                    static constexpr float kAX   = 0.85f;
                    static constexpr float kRMax = 0.99f;
                    const float sliceDurMs_l = juce::jmax (1.0f, getSliceDurMs());
                    const float kAttackViewMs = sliceDurMs_l;
                    const float kHoldViewMs   = sliceDurMs_l;
                    const float kDecayViewMs  = sliceDurMs_l;
                    const float releaseViewMs = sliceDurMs_l;

                    const float remain_c  = kRMax - env.ax;
                    const float kHX_eff   = env.ax + remain_c * 0.20f;
                    const float kDX_eff   = env.ax + remain_c * 0.47f;
                    const float kSEnd_eff = env.ax + remain_c * 0.65f;

                    const float aRatio = env.ax / kAX;
                    const float hRatio = (kHX_eff > env.ax) ? (env.hx - env.ax) / (kHX_eff - env.ax) : 0.0f;
                    const float dRatio = (kDX_eff > env.hx) ? (env.dx - env.hx) / (kDX_eff - env.hx) : 0.0f;

                    const float attackSec  = juce::jlimit (0.0f, kAttackViewMs, aRatio * aRatio * kAttackViewMs) / 1000.0f;
                    const float holdSec    = juce::jlimit (0.0f, kHoldViewMs,   hRatio * hRatio * kHoldViewMs) / 1000.0f;
                    const float decaySec   = juce::jlimit (0.0f, kDecayViewMs,  dRatio * dRatio * kDecayViewMs) / 1000.0f;
                    const float sustainVal = juce::jlimit (0.0f, 1.0f, 1.0f - env.sy);
                    const float releaseSec = juce::jlimit (0.0f, releaseViewMs, (1.0f - env.rx) * releaseViewMs) / 1000.0f;

                    auto lockFieldWithValue = [&] (DysektProcessor::SliceParamField field, float value)
                    {
                        DysektProcessor::Command c;
                        c.type        = DysektProcessor::CmdSetSliceParam;
                        c.intParam1   = (int) field;
                        c.floatParam1 = value;
                        processor.pushCommand (c);
                    };

                    if      (hit == NodeRole::Attack)  lockFieldWithValue (DysektProcessor::FieldAttack,  attackSec);
                    else if (hit == NodeRole::Hold)    lockFieldWithValue (DysektProcessor::FieldHold,    holdSec);
                    else if (hit == NodeRole::Decay)   lockFieldWithValue (DysektProcessor::FieldDecay,   decaySec);
                    else if (hit == NodeRole::Sustain) lockFieldWithValue (DysektProcessor::FieldSustain, sustainVal);
                    else if (hit == NodeRole::Release) lockFieldWithValue (DysektProcessor::FieldRelease, releaseSec);
                }
            }
            postCommitGuard = 6;
            repaint();
        }
        return;   // don't start a drag on right-click
    }

    dragRole = hit;
}

void SliceWaveformLcd::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRole == NodeRole::None || screenArea.isEmpty()) return;

    const float W  = screenArea.getWidth();
    const float H  = screenArea.getHeight();
    const float ox = screenArea.getX();
    const float oy = screenArea.getY();

    const float xn  = juce::jlimit (0.01f, 0.99f, (e.position.x - ox) / W);
    const float rxn = juce::jlimit (0.0f,  1.0f,  (e.position.x - ox) / W);  // full-range for R
    const float yn  = juce::jlimit (0.02f, 0.98f, (e.position.y - oy) / H);

    // Dynamic layout — must match buildEnvelopeNodes exactly
    static constexpr float kAX   = 0.85f;   // attack spans up to 85% of display
    static constexpr float kRMax = 0.99f;

    if (dragRole == NodeRole::Attack)
    {
        // A: X only — peak height is always maximum, no Y drag
        env.ax = juce::jlimit (0.0f, kAX, xn);
    }

    // Recalculate dynamic zones every drag tick (attack movement shifts D/S/R zones)
    const float remain    = kRMax - env.ax;
    const float kDX_eff   = env.ax + remain * 0.47f;
    const float kSEnd_eff = env.ax + remain * 0.65f;
    env.sxEnd = kSEnd_eff;

    // Clamp D into its zone; R is free to span the full display width
    env.dx = juce::jlimit (env.ax + 0.01f, kDX_eff, env.dx);
    env.rx = juce::jlimit (0.0f,            1.0f,    env.rx);

    if (dragRole == NodeRole::Hold)
    {
        // H: X-only drag — hold end must stay between ax and dx
        const float kHX_eff = env.ax + (kRMax - env.ax) * 0.20f;
        env.hx = juce::jlimit (env.ax, juce::jmin (kHX_eff, env.dx - 0.01f), xn);
    }
    if (dragRole == NodeRole::Decay)
    {
        // D: X only — controls how far decay extends before sustain
        env.dx = juce::jlimit (env.ax + 0.01f, kDX_eff, xn);
    }
    else if (dragRole == NodeRole::Sustain)
    {
        // S: Y only — sustain level
        env.sy = juce::jlimit (0.04f, 0.94f, yn);
    }
    else if (dragRole == NodeRole::Release)
    {
        // R: X only — drag left = longer release tail (earlier fade start)
        // Full range [0,1]: R at 1.0 = no fade (clean cut at slice end)
        //                   R at 0.0 = fade starts at slice beginning
        env.rx = rxn;
    }

    // Rebuild envNodes[] from updated env.* (no param read during drag)
    envNodes.clear();
    EnvNode a; a.xn = env.ax; a.yn = env.ay; a.role = NodeRole::Attack;
    a.colour = kColAttack;  a.label = "A"; envNodes.add (a);

    EnvNode h; h.xn = env.hx; h.yn = env.ay; h.role = NodeRole::Hold;
    h.colour = kColHold;    h.label = "H"; envNodes.add (h);
    EnvNode d; d.xn = env.dx; d.yn = env.sy; d.role = NodeRole::Decay;   // X-only drag, on sustain line
    d.colour = kColDecay;   d.label = "D"; envNodes.add (d);
    EnvNode s; s.xn = (env.dx + kSEnd_eff) * 0.5f; s.yn = env.sy; s.role = NodeRole::Sustain;
    s.colour = kColSustain; s.label = "S"; envNodes.add (s);
    EnvNode r; r.xn = env.rx; r.yn = env.sy; r.role = NodeRole::Release;
    r.colour = kColRelease; r.label = "R"; envNodes.add (r);

    // Push to params — also updates APVTS knobs so display stays in sync
    commitNodes();

    repaint();
}

void SliceWaveformLcd::mouseUp (const juce::MouseEvent&)
{
    dragRole = NodeRole::None;
    // Don't rebuild immediately — processor hasn't echoed the new values yet.
    // The guard lets a few paints pass before we re-read from the snapshot.
    postCommitGuard = 6;
    repaint();
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void SliceWaveformLcd::drawBackground (juce::Graphics& g)
{
    const auto ac = getTheme().accent;
    auto b = getLocalBounds();

    auto bgTop = getTheme().darkBar.darker (0.45f);
    auto bgBot = getTheme().darkBar.darker (0.65f);
    juce::ColourGradient outerGrad (bgTop, 0, 0, bgBot, 0, (float) b.getHeight(), false);
    g.setGradientFill (outerGrad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);
    g.setColour (ac.withAlpha (0.18f));
    g.drawRoundedRectangle (b.toFloat().expanded (1.0f), 5.0f, 1.0f);
    g.setColour (ac.withAlpha (0.60f));
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.5f);

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

    g.setColour (ac.withAlpha (0.30f));
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);
}

void SliceWaveformLcd::drawWaveform (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    if (data.peaks.isEmpty()) return;

    const float cy = area.getCentreY();
    const float W  = area.getWidth();
    const float H  = area.getHeight();
    const int   n  = data.peaks.size();

    // Zero line
    g.setColour (lcd2Phosphor().withAlpha (0.20f));
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

    // Use selected slice colour for waveform rendering
    juce::Colour sliceCol = lcd2Phosphor();  // default = theme accent
    {
        const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
        if (sel >= 0 && sel < processor.sliceManager.getNumSlices())
            sliceCol = processor.sliceManager.getSlice (sel).colour;
    }

    g.setColour (sliceCol.withAlpha (0.12f));
    g.fillPath (fill);

    juce::PathStrokeType glow (2.5f);
    g.setColour (sliceCol.withAlpha (0.22f));
    g.strokePath (lineTop, glow);
    g.strokePath (lineBot, glow);

    juce::PathStrokeType sharp (1.1f);
    g.setColour (sliceCol.withAlpha (0.85f));
    g.strokePath (lineTop, sharp);
    g.strokePath (lineBot, sharp);

    // Slice boundary markers
    g.setColour (sliceCol.withAlpha (0.50f));
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
    envFill.lineTo (px (env.dx),    py (env.sy));
    envFill.lineTo (px (env.sxEnd), py (env.sy));
    envFill.lineTo (px (env.rx),    py (env.sy));
    envFill.lineTo (px (1.0f),      py (1.0f));
    envFill.closeSubPath();

    juce::ColourGradient fillGrad (kColDecay.withAlpha (0.08f), 0, oy,
                                    kColDecay.withAlpha (0.00f), 0, oy + H, false);
    g.setGradientFill (fillGrad);
    g.fillPath (envFill);

    // ── Envelope polyline ─────────────────────────────────────────────────────
    juce::Path envLine;
    envLine.startNewSubPath (px (0.0f),   py (1.0f));
    envLine.lineTo          (px (env.ax), py (env.ay));
    envLine.lineTo          (px (env.dx),    py (env.sy));
    envLine.lineTo          (px (env.sxEnd), py (env.sy));
    envLine.lineTo          (px (env.rx),    py (env.sy));
    envLine.lineTo          (px (1.0f),      py (1.0f));

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
    susLine.startNewSubPath (px (env.dx),    py (env.sy));
    susLine.lineTo          (px (env.sxEnd), py (env.sy));
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

    // Read lock state for selected slice
    uint32_t lockMask = 0;
    const int sel = processor.sliceManager.selectedSlice.load (std::memory_order_relaxed);
    if (sel >= 0 && sel < processor.sliceManager.getNumSlices())
        lockMask = processor.sliceManager.getSlice (sel).lockMask;

    for (const auto& node : envNodes)
    {
        const float cx = ox + node.xn * W;
        const bool  hov    = (node.role == hovRole || node.role == dragRole);
        const float r      = hov ? kNodeR + 2.5f : kNodeR;
        const float cy = juce::jmax (oy + r + 4.0f,
                             juce::jmin (oy + H - r - 4.0f, oy + node.yn * H));

        // Determine if this field is locked
        uint32_t fieldBit = 0;
        if      (node.role == NodeRole::Attack)  fieldBit = kLockAttack;
        else if (node.role == NodeRole::Hold)    fieldBit = kLockHold;
        else if (node.role == NodeRole::Decay)   fieldBit = kLockDecay;
        else if (node.role == NodeRole::Sustain) fieldBit = kLockSustain;
        else if (node.role == NodeRole::Release) fieldBit = kLockRelease;
        const bool locked = (fieldBit != 0) && ((lockMask & fieldBit) != 0);

        // Tick line down to envelope
        if (node.role != NodeRole::Sustain && node.role != NodeRole::Hold)
        {
            g.setColour (node.colour.withAlpha (locked ? 0.30f : 0.18f));
            g.drawVerticalLine (juce::roundToInt (cx), cy + r, oy + H);
        }

        if (locked)
        {
            // ── LOCKED: solid filled ring + glow + padlock pip ────────────────
            // Outer glow
            g.setColour (node.colour.withAlpha (hov ? 0.40f : 0.20f));
            g.drawEllipse (cx - r - 3.0f, cy - r - 3.0f,
                           (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f, 1.0f);

            // Filled ring
            g.setColour (node.colour.withAlpha (hov ? 0.90f : 0.70f));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

            // White centre dot
            g.setColour (juce::Colours::white.withAlpha (hov ? 0.95f : 0.75f));
            g.fillEllipse (cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);

            // Padlock icon above label  (tiny — 5px body)
            {
                const float px = cx;
                const float py = cy - r - 10.0f;   // sits just above ring
                // shackle arc
                juce::Path shackle;
                shackle.addCentredArc (px, py - 2.5f, 2.5f, 2.5f, 0.0f,
                                       juce::MathConstants<float>::pi,
                                       0.0f, true);
                g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.85f));
                g.strokePath (shackle, juce::PathStrokeType (1.2f));
                // body rect
                g.setColour (node.colour.withAlpha (hov ? 0.70f : 0.50f));
                g.fillRoundedRectangle (px - 3.0f, py, 6.0f, 5.0f, 1.0f);
                g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.80f));
                g.drawRoundedRectangle (px - 3.0f, py, 6.0f, 5.0f, 1.0f, 0.8f);
            }

            // Label BELOW node (always inside frame)
            g.setFont (DysektLookAndFeel::makeFont (hov ? 11.0f : 9.5f, true));
            g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.85f));
            g.drawText (juce::String (node.label),
                        juce::Rectangle<float> (cx - 14.0f, cy + r + 2.0f, 28.0f, 12.0f),
                        juce::Justification::centred, false);

            // Hover tooltip below label
            if (hov)
            {
                g.setFont (DysektLookAndFeel::makeFont (8.5f));
                g.setColour (node.colour.withAlpha (0.75f));
                g.drawText ("right-click to unlock",
                            juce::Rectangle<float> (cx - 48.0f, cy + r + 15.0f, 96.0f, 12.0f),
                            juce::Justification::centred, false);
            }
        }
        else
        {
            // ── UNLOCKED: hollow ring ─────────────────────────────────────────
            g.setColour (node.colour.withAlpha (hov ? 0.55f : 0.25f));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, hov ? 1.5f : 1.0f);

            // Inner dot
            const float dr = hov ? 3.0f : 2.5f;
            g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.80f));
            g.fillEllipse (cx - dr, cy - dr, dr * 2.0f, dr * 2.0f);

            // Label BELOW node (always inside frame)
            g.setFont (DysektLookAndFeel::makeFont (hov ? 11.0f : 9.5f, true));
            g.setColour (node.colour.withAlpha (hov ? 1.0f : 0.70f));
            g.drawText (juce::String (node.label),
                        juce::Rectangle<float> (cx - 14.0f, cy + r + 2.0f, 28.0f, 12.0f),
                        juce::Justification::centred, false);

            if (hov)
            {
                g.setFont (DysektLookAndFeel::makeFont (8.5f));
                g.setColour (node.colour.withAlpha (0.60f));
                g.drawText ("right-click to lock",
                            juce::Rectangle<float> (cx - 48.0f, cy + r + 15.0f, 96.0f, 12.0f),
                            juce::Justification::centred, false);
            }
        }
    }
}

void SliceWaveformLcd::drawNoData (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (4);

    if (! data.hasSample || data.isDefault)
    {
        // Show "EMPTY" prominently when no real sample is loaded
        g.setFont (DysektLookAndFeel::makeFont (18.0f, true));
        g.setColour (lcd2Phosphor().withAlpha (0.18f));
        g.drawText ("EMPTY", b, juce::Justification::centred);

        g.setFont (DysektLookAndFeel::makeFont (7.5f));
        g.setColour (lcd2Dim().brighter (0.5f));
        g.drawText ("drag a sample here or use the browser",
                    b.removeFromBottom (18), juce::Justification::centred);
    }
    else
    {
        g.setFont (DysektLookAndFeel::makeFont (10.0f));
        g.setColour (lcd2Dim().brighter (0.4f));
        g.drawText ("-- SELECT A SLICE --", b, juce::Justification::centred);
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SliceWaveformLcd::drawPlayhead (juce::Graphics& g, const juce::Rectangle<float>& area)
{
    if (data.sliceIndex < 0) return;

    const int   totalRange = data.endSample - data.startSample;
    if (totalRange <= 0) return;

    auto& vp = processor.voicePool;

    for (int i = 0; i < VoicePool::kMaxVoices; ++i)
    {
        const auto& v = vp.getVoice (i);
        if (! v.active || v.sliceIdx != data.sliceIndex) continue;

        const float rawPos = vp.voicePositions[i].load (std::memory_order_relaxed);
        float xn = (rawPos - (float) data.startSample) / (float) totalRange;
        xn = juce::jlimit (0.0f, 1.0f, xn);

        const float x = area.getX() + xn * area.getWidth();

        // Soft glow halo
        g.setColour (lcd2Phosphor().withAlpha (0.12f));
        g.drawLine (x - 1.5f, area.getY(), x - 1.5f, area.getBottom(), 1.0f);
        g.drawLine (x + 1.5f, area.getY(), x + 1.5f, area.getBottom(), 1.0f);

        // Main playhead line
        g.setColour (lcd2Phosphor().withAlpha (0.85f));
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.5f);

        // Small triangle cap at top
        const float capH = 5.0f;
        juce::Path cap;
        cap.addTriangle (x - 3.5f, area.getY(),
                         x + 3.5f, area.getY(),
                         x,        area.getY() + capH);
        g.fillPath (cap);

        // Only draw the most-recently-hit voice
        break;
    }
}


void SliceWaveformLcd::paint (juce::Graphics& g)
{
    // Clip to rounded LCD boundary — stops accent glow artefacts showing as
    // black corner notches against the plugin background.
    {
        juce::Path clipPath;
        clipPath.addRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.reduceClipRegion (clipPath);
    }
    buildDisplayData();
    drawBackground (g);

    // isDefault (Empty.wav) always shows EMPTY — even if an auto-slice exists
    if (! data.hasSample || ! data.hasSlice || data.isDefault)
    {
        drawNoData (g);
        return;
    }

    // Nodes are rebuilt in repaintLcd() (timer-driven), not here.
    // During drag, dragRole != None so mouseDrag maintains envNodes directly.

    const auto area = getLocalBounds().reduced (4).toFloat().reduced (2.0f);
    screenArea = area;

    drawWaveform (g, area);
    drawEnvelope (g, area);
    drawNodes    (g, area);
    drawPlayhead (g, area);
}
