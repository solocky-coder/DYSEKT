#include "OscilloscopeView.h"
#include "../PluginProcessor.h"
#include "DysektLookAndFeel.h"
#include <array>
#include <cmath>

static constexpr int kDisplaySamples = 256;
static constexpr int kStatsBoxW      = 68;
static constexpr int kGridSpacing    = 20; // samples between vertical grid lines

OscilloscopeView::OscilloscopeView (DysektProcessor& p) : processor (p) {}

void OscilloscopeView::paint (juce::Graphics& g)
{
    const auto& theme = getTheme();
    auto bounds = getLocalBounds();

    // Background
    g.fillAll (theme.waveformBg);

    // Border
    g.setColour (theme.separator);
    g.drawRect (bounds, 1);

    // Split waveform area and stats box
    auto waveArea  = bounds.reduced (1, 1);
    auto statsArea = waveArea.removeFromRight (kStatsBoxW);

    // ── Vertical grid lines every kGridSpacing samples ───────────────────────
    const float pxPerSample = (float) waveArea.getWidth() / (float) kDisplaySamples;
    g.setColour (theme.gridLine);
    for (int s = kGridSpacing; s < kDisplaySamples; s += kGridSpacing)
    {
        const float x = (float) waveArea.getX() + (float) s * pxPerSample;
        g.drawVerticalLine ((int) x, (float) waveArea.getY(), (float) waveArea.getBottom());
    }

    // ── Centre horizontal line ────────────────────────────────────────────────
    const int cy = waveArea.getCentreY();
    g.drawHorizontalLine (cy, (float) waveArea.getX(), (float) waveArea.getRight());

    // ── Read last kDisplaySamples from ring buffer ────────────────────────────
    const int ringSize  = DysektProcessor::kOscRingBufferSize;
    const int writeHead = processor.oscRingWriteHead.load (std::memory_order_acquire);

    std::array<float, kDisplaySamples> samples;
    float peakLevel = 0.0f;
    for (int i = 0; i < kDisplaySamples; ++i)
    {
        const int idx = (writeHead - kDisplaySamples + i + ringSize) & (ringSize - 1);
        const float s = processor.oscRingBuffer[(size_t) idx];
        samples[(size_t) i] = s;
        const float a = s < 0.0f ? -s : s;
        if (a > peakLevel) peakLevel = a;
    }

    // ── Draw waveform path ────────────────────────────────────────────────────
    const float cy_f   = (float) cy;
    const float halfH  = (float) (waveArea.getHeight() / 2 - 2);
    const float originX = (float) waveArea.getX();

    juce::Path path;
    for (int i = 0; i < kDisplaySamples; ++i)
    {
        const float x = originX + (float) i * pxPerSample;
        const float y = cy_f - samples[(size_t) i] * halfH;
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (theme.accent);
    g.strokePath (path, juce::PathStrokeType (1.5f));

    // ── Stats box ─────────────────────────────────────────────────────────────
    g.setColour (theme.gridLine);
    g.drawVerticalLine (statsArea.getX(), (float) statsArea.getY(), (float) statsArea.getBottom());

    const float peakDb  = peakLevel > 1e-4f ? 20.0f * std::log10 (peakLevel) : -100.0f;
    const juce::String peakStr = peakDb > -99.0f
                                    ? (juce::String (peakDb, 1) + " dB")
                                    : "-inf";

    const auto statsInner = statsArea.reduced (4, 4);
    g.setColour (theme.foreground.withAlpha (0.6f));
    g.setFont (DysektLookAndFeel::makeFont (8.5f));
    g.drawText ("PEAK", statsInner.withHeight (12), juce::Justification::centredTop);

    g.setColour (theme.accent.withAlpha (0.9f));
    g.setFont (DysektLookAndFeel::makeFont (9.5f, true));
    g.drawText (peakStr, statsInner, juce::Justification::centred);
}
