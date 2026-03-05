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
    g.fillAll (getTheme().background.darker (0.15f));

    // ── Centre-line ───────────────────────────────────────────────────────────
    const float cy = H * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (juce::roundToInt (cy), 0.0f, (float) W);

    // ── Read actual sample waveform for the selected slice ────────────────────
    auto sampleSnap = processor.sampleData.getSnapshot();
    if (sampleSnap == nullptr)
    {
        // No sample loaded — draw flat line
        g.setColour (getTheme().accent.withAlpha (0.2f));
        g.drawHorizontalLine (juce::roundToInt (cy), 0.0f, (float) W);
        g.setColour (getTheme().darkBar);
        g.drawHorizontalLine (0,     0.0f, (float) W);
        g.drawHorizontalLine (H - 1, 0.0f, (float) W);
        return;
    }

    const auto& ui       = processor.getUiSliceSnapshot();
    const int   totalFrames = sampleSnap->buffer.getNumSamples();
    const int   numChans    = sampleSnap->buffer.getNumChannels();
    if (totalFrames <= 0 || numChans <= 0)
        return;

    // Use selected slice boundaries; fall back to whole file if nothing selected
    int startSample = 0;
    int endSample   = totalFrames;
    const int sel   = ui.selectedSlice;
    if (sel >= 0 && sel < ui.numSlices)
    {
        startSample = ui.slices[(size_t) sel].startSample;
        endSample   = ui.slices[(size_t) sel].endSample;
    }
    startSample = juce::jlimit (0, totalFrames - 1, startSample);
    endSample   = juce::jlimit (startSample + 1, totalFrames, endSample);

    const int   sliceLen = endSample - startSample;
    const float yScale   = cy * 0.85f;

    // Mix down to mono on-the-fly: average all channels at each sample
    // Draw a min/max peak envelope when many samples map to few pixels,
    // or an interpolated line when few samples map to many pixels.
    const float samplesPerPixel = (float) sliceLen / (float) W;
    const auto* readL = sampleSnap->buffer.getReadPointer (0);
    const auto* readR = (numChans > 1) ? sampleSnap->buffer.getReadPointer (1) : readL;

    juce::Path path;

    if (samplesPerPixel >= 1.0f)
    {
        // ── Peak envelope (zoomed out) ────────────────────────────────────────
        for (int px = 0; px < W; ++px)
        {
            const int s0 = startSample + (int) ((float) px       * samplesPerPixel);
            const int s1 = startSample + (int) ((float)(px + 1)  * samplesPerPixel);
            const int iEnd = juce::jmin (s1, endSample);

            float mn = 0.f, mx = 0.f;
            for (int s = s0; s < iEnd; ++s)
            {
                const float v = (readL[s] + readR[s]) * 0.5f;
                mn = juce::jmin (mn, v);
                mx = juce::jmax (mx, v);
            }

            const float yTop = cy - mx * yScale;
            const float yBot = cy - mn * yScale;

            if (px == 0)
                path.startNewSubPath ((float) px, (yTop + yBot) * 0.5f);

            // Draw thin spike: move to top, then line to bottom
            path.lineTo ((float) px, yTop);
            path.lineTo ((float) px, yBot);
        }
    }
    else
    {
        // ── Interpolated line (zoomed in) ─────────────────────────────────────
        const float pixelsPerSample = (float) W / (float) sliceLen;
        bool first = true;
        for (int s = startSample; s < endSample; ++s)
        {
            const float v = (readL[s] + readR[s]) * 0.5f;
            const float x = (float)(s - startSample) * pixelsPerSample;
            const float y = cy - v * yScale;
            if (first) { path.startNewSubPath (x, y); first = false; }
            else        path.lineTo (x, y);
        }
    }

    // ── Draw waveform with glow ───────────────────────────────────────────────
    const auto waveColour = getTheme().accent;

    // Soft glow pass
    g.setColour (waveColour.withAlpha (0.18f));
    g.strokePath (path, juce::PathStrokeType (3.0f,
                                              juce::PathStrokeType::mitre,
                                              juce::PathStrokeType::square));

    // Main line
    g.setColour (waveColour.withAlpha (0.80f));
    g.strokePath (path, juce::PathStrokeType (1.0f,
                                              juce::PathStrokeType::mitre,
                                              juce::PathStrokeType::square));

    // ── Top / bottom border ───────────────────────────────────────────────────
    g.setColour (getTheme().darkBar);
    g.drawHorizontalLine (0,     0.0f, (float) W);
    g.drawHorizontalLine (H - 1, 0.0f, (float) W);
}
