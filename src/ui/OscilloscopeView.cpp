#include "OscilloscopeView.h"
#include "../PluginProcessor.h"
#include "DysektLookAndFeel.h"

OscilloscopeView::OscilloscopeView (DysektProcessor& p)
    : processor (p)
{
    setOpaque (true);
}

void OscilloscopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const int  W      = bounds.getWidth();
    const int  H      = bounds.getHeight();

    // ── Background ────────────────────────────────────────────────────────────
    g.fillAll (getTheme().waveformBg);

    // ── Centre-line ───────────────────────────────────────────────────────────
    const float cy = H * 0.5f;
    g.setColour (getTheme().gridLine.withAlpha (0.3f));
    g.drawHorizontalLine (juce::roundToInt (cy), 0.0f, (float) W);

    // ── Read ring buffer ──────────────────────────────────────────────────────
    const int   ringSize  = DysektProcessor::kOscRingBufferSize;   // 4096
    const int   writeHead = processor.oscRingWriteHead.load (std::memory_order_acquire);

    // How many samples we want to display — use the full width in pixels,
    // clamped to the ring buffer size.
    const int   numSamples = juce::jmin (W, ringSize);
    const float xScale     = (float) W / (float) numSamples;
    const float yScale     = cy * 0.85f;

    // ── Build path ─────────────────────────────────────────────────────────────
    juce::Path path;
    bool first = true;

    for (int i = 0; i < numSamples; i++)
    {
        // Walk backwards from the write head so the newest sample is on the right
        const int   ringIdx = (writeHead - numSamples + i + ringSize) & (ringSize - 1);
        const float sample  = processor.oscRingBuffer[(size_t) ringIdx];
        const float x       = (float) i * xScale;
        const float y       = cy - sample * yScale;

        if (first) { path.startNewSubPath (x, y); first = false; }
        else        path.lineTo (x, y);
    }

    // ── Draw waveform with glow ───────────────────────────────────────────────
    const auto waveColour = getTheme().waveform;

    // Soft glow pass (wider, more transparent)
    g.setColour (waveColour.withAlpha (0.18f));
    g.strokePath (path, juce::PathStrokeType (3.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // Main line
    g.setColour (waveColour.withAlpha (0.75f));
    g.strokePath (path, juce::PathStrokeType (1.2f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // ── Top / bottom border ───────────────────────────────────────────────────
    g.setColour (getTheme().gridLine.withAlpha (0.4f));
    g.drawHorizontalLine (0,     0.0f, (float) W);
    g.drawHorizontalLine (H - 1, 0.0f, (float) W);
}
