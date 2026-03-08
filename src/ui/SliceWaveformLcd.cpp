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
    // Read raw param values - USE THE GETTERS!
    const float attackMs  = processor.getAttackParam()  ? processor.getAttackParam()->load()  : 5.0f;
    const float decayMs   = processor.getDecayParam()   ? processor.getDecayParam()->load()   : 100.0f;
    const float sustainPc = processor.getSustainParam() ? processor.getSustainParam()->load() : 100.0f;
    const float releaseMs = processor.getReleaseParam() ? processor.getReleaseParam()->load() : 20.0f;

    // Map to X positions across the waveform (total budget = 1.0)
    // Attack  [0   → ax]
    // Decay   [ax  → dx]
    // Sustain [dx  → rx]  (plateau)
    // Release [rx  → 1.0]
    const float totalMs = attackMs + decayMs + 200.0f + releaseMs;  // 200ms budget for sustain plateau
    const float scale   = (totalMs > 0.0f) ? 1.0f / totalMs : 1.0f;

    env.ax = juce::jlimit (0.04f, 0.30f, attackMs  * scale);
    env.dx = juce::jlimit (env.ax + 0.04f, 0.70f, (attackMs + decayMs) * scale);
    env.rx = juce::jlimit (env.dx + 0.04f, 0.95f, (attackMs + decayMs + 200.0f) * scale);
    env.sy = juce::jlimit (0.05f, 0.95f, 1.0f - (sustainPc / 100.0f));  // flip: 0=top=loud
    env.ay = 0.08f;  // attack peak always near top (full amplitude)

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

// ── The rest of your code: Draw, Mouse, Paint, etc.
// (no changes required—full file continues here as per your repo.)