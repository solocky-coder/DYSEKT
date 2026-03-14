#include "WaveformOverview.h"
#include "DysektLookAndFeel.h"
#include "UIHelpers.h"
#include "../PluginProcessor.h"
#include <cmath>
#include <algorithm>

WaveformOverview::WaveformOverview (DysektProcessor& p)
    : processor (p)
{
    setOpaque (false);
}

// ── Peak cache ────────────────────────────────────────────────────────────────

void WaveformOverview::rebuildPeaks()
{
    const int w = getWidth();
    if (w <= 0 || ! processor.sampleData.isLoaded()) { peaks.clear(); return; }

    const auto& buf  = processor.sampleData.getBuffer();
    const int frames = buf.getNumSamples();
    const int ch     = buf.getNumChannels();
    if (frames <= 0 || ch <= 0) { peaks.clear(); return; }

    peaks.resize ((size_t) w);
    const float step = (float) frames / (float) w;

    for (int px = 0; px < w; ++px)
    {
        const int s0 = (int) (px * step);
        const int s1 = std::min ((int) ((px + 1) * step), frames);
        float mx = 0.0f;
        for (int c = 0; c < ch; ++c)
        {
            const float* data = buf.getReadPointer (c);
            for (int s = s0; s < s1; ++s)
                mx = std::max (mx, std::abs (data[s]));
        }
        peaks[(size_t) px] = mx;
    }
    peakFrameCount = frames;
    peakWidth      = w;
}

void WaveformOverview::repaintOverview()
{
    // Rebuild if sample changed or component resized
    const int frames = processor.sampleData.isLoaded()
                       ? processor.sampleData.getBuffer().getNumSamples() : 0;
    if (frames != peakFrameCount || getWidth() != peakWidth)
        rebuildPeaks();
    repaint();
}

// ── Viewport rect ─────────────────────────────────────────────────────────────

juce::Rectangle<float> WaveformOverview::viewportRect() const
{
    const float z        = std::max (1.0f, processor.zoom.load());
    const float sc       = processor.scroll.load();
    const float viewFrac = 1.0f / z;
    const float vStart   = sc * (1.0f - viewFrac);
    const float W        = (float) getWidth();
    const float H        = (float) getHeight();
    return { vStart * W, 0.0f, viewFrac * W, H };
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void WaveformOverview::paint (juce::Graphics& g)
{
    const auto ac  = getTheme().accent;
    const int  w   = getWidth();
    const int  h   = getHeight();
    const float cy = h * 0.5f;

    // Waveform — slices coloured by their palette colours if loaded
    if (! peaks.empty() && (int) peaks.size() == w)
    {
        // Dim fill
        juce::Path fill;
        bool first = true;
        for (int px = 0; px < w; ++px)
        {
            const float amp = peaks[(size_t) px] * (cy - 2.0f);
            if (first) { fill.startNewSubPath ((float)px, cy - amp); first = false; }
            else        fill.lineTo           ((float)px, cy - amp);
        }
        for (int px = w - 1; px >= 0; --px)
            fill.lineTo ((float)px, cy + peaks[(size_t)px] * (cy - 2.0f));
        fill.closeSubPath();

        g.setColour (ac.withAlpha (0.10f));
        g.fillPath (fill);

        // Bright outline — top only
        juce::Path line;
        first = true;
        for (int px = 0; px < w; ++px)
        {
            const float amp = peaks[(size_t) px] * (cy - 2.0f);
            const float y   = cy - amp;
            if (first) { line.startNewSubPath ((float)px, y); first = false; }
            else        line.lineTo           ((float)px, y);
        }
        g.setColour (ac.withAlpha (0.55f));
        g.strokePath (line, juce::PathStrokeType (0.8f));

        // Centre zero line
        g.setColour (ac.withAlpha (0.06f));
        g.drawHorizontalLine ((int) cy, 0.0f, (float) w);
    }
    else
    {
        // No sample — dim centre line
        g.setColour (ac.withAlpha (0.06f));
        g.drawHorizontalLine ((int) cy, 0.0f, (float) w);
    }

    // ── Viewport box ──────────────────────────────────────────────────────────
    const auto vr = viewportRect();

    // Fill
    g.setColour (ac.withAlpha (0.10f));
    g.fillRect  (vr);

    // Left / right edges — bright accent verticals
    g.setColour (ac.withAlpha (0.70f));
    g.drawVerticalLine (juce::roundToInt (vr.getX()),     0.0f, (float) h);
    g.drawVerticalLine (juce::roundToInt (vr.getRight()), 0.0f, (float) h);

    // Top / bottom edges
    g.setColour (ac.withAlpha (0.35f));
    g.drawHorizontalLine (0,     vr.getX(), vr.getRight());
    g.drawHorizontalLine (h - 1, vr.getX(), vr.getRight());

    // Dim overlay outside viewport
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    if (vr.getX() > 0.0f)
        g.fillRect (0.0f, 0.0f, vr.getX(), (float) h);
    if (vr.getRight() < (float) w)
        g.fillRect (vr.getRight(), 0.0f, (float) w - vr.getRight(), (float) h);
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void WaveformOverview::mouseDown (const juce::MouseEvent& e)
{
    isDragging      = true;
    dragStartX      = e.x;
    dragStartY      = e.y;
    dragStartZoom   = processor.zoom.load();
    dragStartScroll = processor.scroll.load();

    const auto vr = viewportRect();
    draggingViewport = vr.contains (e.position);

    if (draggingViewport)
    {
        // Anchor: sample fraction under cursor
        const float z        = dragStartZoom;
        const float viewFrac = 1.0f / z;
        const float vStart   = dragStartScroll * (1.0f - viewFrac);
        dragAnchorPxFrac = getWidth() > 0 ? (float) e.x / (float) getWidth() : 0.5f;
        dragAnchorFrac   = vStart + dragAnchorPxFrac * viewFrac;
    }
    else
    {
        // Click outside viewport: jump-scroll so viewport centres on click
        if (getWidth() > 0)
        {
            const float clickFrac = (float) e.x / (float) getWidth();
            const float z         = processor.zoom.load();
            const float viewFrac  = 1.0f / z;
            float newStart        = clickFrac - viewFrac * 0.5f;
            const float maxScroll = 1.0f - viewFrac;
            if (maxScroll > 0.0f)
                processor.scroll.store (juce::jlimit (0.0f, 1.0f, newStart / maxScroll));
            repaint();
        }
        // Treat subsequent drag as viewport drag from new position
        dragStartScroll  = processor.scroll.load();
        const float z        = dragStartZoom;
        const float viewFrac = 1.0f / z;
        const float vStart   = dragStartScroll * (1.0f - viewFrac);
        dragAnchorPxFrac = getWidth() > 0 ? (float) e.x / (float) getWidth() : 0.5f;
        dragAnchorFrac   = vStart + dragAnchorPxFrac * viewFrac;
        draggingViewport = true;
    }
}

void WaveformOverview::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDragging || getWidth() <= 0) return;

    if (draggingViewport)
    {
        // Horizontal drag — scroll, keeping anchor under cursor
        const float newAnchorPxFrac = (float) e.x / (float) getWidth();
        const float z        = processor.zoom.load();
        const float viewFrac = 1.0f / z;

        // Move viewport so dragAnchorFrac stays under cursor
        const float newViewStart = dragAnchorFrac - newAnchorPxFrac * viewFrac;
        const float maxScroll    = 1.0f - viewFrac;
        if (maxScroll > 0.0f)
            processor.scroll.store (juce::jlimit (0.0f, 1.0f, newViewStart / maxScroll));
    }

    // Vertical drag always = zoom (drag up = zoom in), anchored to drag start
    const float deltaY  = (float) (e.y - dragStartY);
    const float newZoom = juce::jlimit (1.0f, 16384.0f,
                                        dragStartZoom * UIHelpers::computeZoomFactor (deltaY));
    processor.zoom.store (newZoom);

    repaint();
}

void WaveformOverview::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}

void WaveformOverview::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& w)
{
    const float curZoom  = processor.zoom.load();
    const float curScroll= processor.scroll.load();

    const float factor   = (w.deltaY > 0) ? 1.25f : 0.8f;
    const float newZoom  = juce::jlimit (1.0f, 16384.0f, curZoom * factor);

    const float curFrac  = 1.0f / curZoom;
    const float curStart = curScroll * (1.0f - curFrac);
    const float anchorFn = getWidth() > 0 ? (float) e.x / (float) getWidth() : 0.5f;
    const float anchor   = curStart + anchorFn * curFrac;

    const float newFrac  = 1.0f / newZoom;
    const float newStart = anchor - anchorFn * newFrac;
    const float maxSc    = 1.0f - newFrac;
    if (maxSc > 0.0f)
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, newStart / maxSc));

    processor.zoom.store (newZoom);
    repaint();
}
