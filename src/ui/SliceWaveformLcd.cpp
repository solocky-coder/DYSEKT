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

// ── Paint Implementation ──────────────────────────────────────────────────────

void SliceWaveformLcd::paint (juce::Graphics& g)
{
    // LCD background with bezel
    g.setColour (kBezel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    
    g.setColour (kBg);
    g.fillRoundedRectangle (screenArea, 2.0f);
    
    if (!data.hasSample)
    {
        // Show "No Sample" message
        g.setColour (kDim);
        g.setFont (12.0f);
        g.drawText ("No Sample", screenArea, juce::Justification::centred);
        return;
    }
    
    // Draw waveform
    if (data.peaks.size() > 0)
    {
        const float waveHeight = screenArea.getHeight() * 0.6f;
        const float waveCenterY = screenArea.getCentreY();
        const float waveWidth = screenArea.getWidth();
        
        g.setColour (kPhosphor.withAlpha (0.7f));
        
        juce::Path waveformPath;
        for (int i = 0; i < data.peaks.size(); ++i)
        {
            const float x = screenArea.getX() + (i * waveWidth / data.peaks.size());
            const float peak = data.peaks[i];
            const float y = waveCenterY - (peak * waveHeight * 0.5f);
            
            if (i == 0)
                waveformPath.startNewSubPath (x, y);
            else
                waveformPath.lineTo (x, y);
        }
        
        // Mirror for bottom half
        for (int i = data.peaks.size() - 1; i >= 0; --i)
        {
            const float x = screenArea.getX() + (i * waveWidth / data.peaks.size());
            const float peak = data.peaks[i];
            const float y = waveCenterY + (peak * waveHeight * 0.5f);
            waveformPath.lineTo (x, y);
        }
        
        waveformPath.closeSubPath();
        g.fillPath (waveformPath);
    }
    
    // Draw envelope curve over waveform
    if (envNodes.size() >= 4)
    {
        buildEnvelopeNodes(); // Update envelope data
        
        g.setColour (kBright.withAlpha (0.8f));
        g.setStrokeType (juce::PathStrokeType (2.0f));
        
        juce::Path envPath;
        const float baseY = screenArea.getBottom() - 10;
        const float topY = screenArea.getY() + 10;
        
        // Start at (0, baseY)
        envPath.startNewSubPath (screenArea.getX(), baseY);
        
        // Attack to peak
        float attackX = screenArea.getX() + env.ax * screenArea.getWidth();
        float attackY = topY + env.ay * (baseY - topY);
        envPath.lineTo (attackX, attackY);
        
        // Decay to sustain
        float decayX = screenArea.getX() + env.dx * screenArea.getWidth();
        float sustainY = topY + env.sy * (baseY - topY);
        envPath.lineTo (decayX, sustainY);
        
        // Sustain plateau
        float releaseX = screenArea.getX() + env.rx * screenArea.getWidth();
        envPath.lineTo (releaseX, sustainY);
        
        // Release to end
        envPath.lineTo (screenArea.getRight(), baseY);
        
        g.strokePath (envPath, juce::PathStrokeType (1.5f));
        
        // Draw envelope nodes as circles
        for (const auto& node : envNodes)
        {
            float nodeX = screenArea.getX() + node.xn * screenArea.getWidth();
            float nodeY = topY + node.yn * (baseY - topY);
            
            g.setColour (node.colour);
            g.fillEllipse (nodeX - 4, nodeY - 4, 8, 8);
            
            // Draw label
            g.setColour (kLabel);
            g.setFont (10.0f);
            g.drawText (node.label, 
                       juce::Rectangle<float> (nodeX - 10, nodeY - 20, 20, 15),
                       juce::Justification::centred);
        }
    }
    
    // Draw slice info text
    if (data.hasSlice)
    {
        g.setColour (kLabel);
        g.setFont (10.0f);
        
        juce::String infoText = "Slice " + juce::String (data.sliceIndex + 1);
        if (data.midiNote >= 0)
            infoText += " | " + midiNoteName (data.midiNote);
        
        g.drawText (infoText, 
                   screenArea.reduced (5, 0),
                   juce::Justification::topLeft);
    }
}

// ── Mouse Interaction ─────────────────────────────────────────────────────────

void SliceWaveformLcd::mouseDown (const juce::MouseEvent& event)
{
    if (!data.hasSample || envNodes.isEmpty())
        return;
    
    // Check if clicking near an envelope node
    const float mx = event.position.x;
    const float my = event.position.y;
    
    draggedNode = nullptr;
    const float hitRadius = 10.0f;
    
    for (auto& node : envNodes)
    {
        float nodeX = screenArea.getX() + node.xn * screenArea.getWidth();
        float nodeY = screenArea.getY() + node.yn * screenArea.getHeight();
        
        float dist = std::sqrt ((mx - nodeX) * (mx - nodeX) + (my - nodeY) * (my - nodeY));
        if (dist <= hitRadius)
        {
            draggedNode = &node;
            dragOffset = event.position - juce::Point<float> (nodeX, nodeY);
            break;
        }
    }
}

void SliceWaveformLcd::mouseDrag (const juce::MouseEvent& event)
{
    if (!draggedNode || !data.hasSample)
        return;
    
    // Calculate new normalized position
    float newX = (event.position.x - dragOffset.x - screenArea.getX()) / screenArea.getWidth();
    float newY = (event.position.y - dragOffset.y - screenArea.getY()) / screenArea.getHeight();
    
    // Clamp to valid ranges based on node role
    switch (draggedNode->role)
    {
        case NodeRole::Attack:
            newX = juce::jlimit (0.01f, 0.3f, newX);
            env.ax = newX;
            break;
            
        case NodeRole::Decay:
            newX = juce::jlimit (env.ax + 0.01f, 0.7f, newX);
            env.dx = newX;
            break;
            
        case NodeRole::Sustain:
            // Sustain only moves vertically
            newY = juce::jlimit (0.05f, 0.95f, newY);
            env.sy = newY;
            break;
            
        case NodeRole::Release:
            newX = juce::jlimit (env.dx + 0.01f, 0.95f, newX);
            env.rx = newX;
            break;
    }
    
    // Update the processor parameters based on new envelope shape
    updateProcessorFromEnvelope();
    
    // Rebuild nodes and repaint
    buildEnvelopeNodes();
    repaint();
}

void SliceWaveformLcd::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    draggedNode = nullptr;
    dragOffset = {};
}

void SliceWaveformLcd::mouseMove (const juce::MouseEvent& event)
{
    // Update cursor based on hover state
    bool overNode = false;
    const float mx = event.position.x;
    const float my = event.position.y;
    const float hitRadius = 10.0f;
    
    for (const auto& node : envNodes)
    {
        float nodeX = screenArea.getX() + node.xn * screenArea.getWidth();
        float nodeY = screenArea.getY() + node.yn * screenArea.getHeight();
        
        float dist = std::sqrt ((mx - nodeX) * (mx - nodeX) + (my - nodeY) * (my - nodeY));
        if (dist <= hitRadius)
        {
            overNode = true;
            break;
        }
    }
    
    setMouseCursor (overNode ? juce::MouseCursor::PointingHandCursor 
                              : juce::MouseCursor::NormalCursor);
}

// ── Helper to update processor from envelope ─────────────────────────────────

void SliceWaveformLcd::updateProcessorFromEnvelope()
{
    // Convert normalized envelope positions back to milliseconds
    const float totalMs = 1000.0f; // Reference total time
    
    float attackMs = env.ax * totalMs;
    float decayMs = (env.dx - env.ax) * totalMs;
    float sustainPc = (1.0f - env.sy) * 100.0f; // Convert back from inverted Y
    float releaseMs = (1.0f - env.rx) * totalMs * 0.5f; // Scale appropriately
    
    // Update processor parameters if they exist
    if (auto* param = processor.getAttackParam())
        param->store (attackMs);
    
    if (auto* param = processor.getDecayParam())
        param->store (decayMs);
    
    if (auto* param = processor.getSustainParam())
        param->store (sustainPc);
    
    if (auto* param = processor.getReleaseParam())
        param->store (releaseMs);
}
