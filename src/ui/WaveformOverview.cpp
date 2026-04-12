#include "WaveformOverview.h"
#include "../PluginProcessor.h"

//==============================================================================
WaveformOverview::WaveformOverview (DysektProcessor& p)
    : processor (p)
{
    setOpaque (false);
}

void WaveformOverview::repaintOverview()
{
    repaint();
}

//==============================================================================
// Viewport rect
// Mirrors WaveformView::buildViewState():
//   visibleLen   = numFrames / zoom
//   maxStart     = numFrames - visibleLen
//   visibleStart = scroll * maxStart
// Then maps to pixel fractions of our component width.
//==============================================================================
juce::Rectangle<float> WaveformOverview::viewportRect() const
{
    auto snap = processor.sampleData.getSnapshot();
    if (! snap || snap->buffer.getNumSamples() == 0)
        return { 0.0f, 0.0f, (float) getWidth(), (float) getHeight() };

    const float W   = (float) getWidth();
    const float H   = (float) getHeight();
    const int   nf  = snap->buffer.getNumSamples();
    const float z   = juce::jmax (1.0f, processor.zoom.load());
    const float sc  = processor.scroll.load();

    const int visLen   = juce::jlimit (1, nf, (int) ((float) nf / z));
    const int maxStart = juce::jmax (0, nf - visLen);
    const int visStart = juce::jlimit (0, maxStart, (int) (sc * (float) maxStart));

    const float x0 = (float) visStart        / (float) nf * W;
    const float x1 = (float) (visStart + visLen) / (float) nf * W;

    return { x0, 0.0f, juce::jmax (1.0f, x1 - x0), H };
}

//==============================================================================
// Peak cache
//==============================================================================
void WaveformOverview::rebuildPeaks()
{
    auto snap = processor.sampleData.getSnapshot();
    if (! snap || snap->buffer.getNumSamples() == 0 || getWidth() <= 0)
    {
        peaks.clear();
        peakNumFrames = 0;
        peakWidth     = 0;
        return;
    }

    const int W  = getWidth();
    const int nf = snap->buffer.getNumSamples();
    const int ch = snap->buffer.getNumChannels();

    peaks.assign ((size_t) W, 0.0f);

    for (int px = 0; px < W; ++px)
    {
        int s0 = (int) ((float) px       / (float) W * (float) nf);
        int s1 = (int) ((float) (px + 1) / (float) W * (float) nf);
        s1 = juce::jmax (s0 + 1, s1);
        s1 = juce::jmin (s1, nf);

        float mx = 0.0f;
        for (int c = 0; c < ch; ++c)
        {
            const float* buf = snap->buffer.getReadPointer (c);
            for (int s = s0; s < s1; ++s)
                mx = juce::jmax (mx, std::abs (buf[s]));
        }
        peaks[(size_t) px] = mx;
    }

    peakNumFrames = nf;
    peakWidth     = W;
}

//==============================================================================
// Paint
//==============================================================================
void WaveformOverview::paint (juce::Graphics& g)
{
    auto snap = processor.sampleData.getSnapshot();
    const bool hasSample = snap && snap->buffer.getNumSamples() > 0;

    if (hasSample)
    {
        const int nf = snap->buffer.getNumSamples();
        if (nf != peakNumFrames || getWidth() != peakWidth)
            rebuildPeaks();
    }

    const auto  bounds = getLocalBounds().toFloat();
    const float W      = bounds.getWidth();
    const float H      = bounds.getHeight();
    const float mid    = H * 0.5f;

    // Background
    g.setColour (juce::Colour (0xff0c0d0e));
    g.fillRoundedRectangle (bounds, 2.0f);

    if (hasSample && ! peaks.empty())
    {
        const auto vr = viewportRect();

        // Draw waveform — two colours: brighter inside viewport
        for (int px = 0; px < (int) peaks.size(); ++px)
        {
            const float amp    = peaks[(size_t) px] * (mid * 0.85f);
            const bool  inside = ((float) px >= vr.getX() && (float) px < vr.getRight());
            g.setColour (inside ? juce::Colour (0xff4a5568)
                                : juce::Colour (0xff252930));
            g.drawVerticalLine (px, mid - amp, mid + amp + 1.0f);
        }

        // Dim areas outside viewport
        g.setColour (juce::Colour (0x44000000));
        if (vr.getX() > 0.0f)
            g.fillRect (0.0f, 0.0f, vr.getX(), H);
        if (vr.getRight() < W)
            g.fillRect (vr.getRight(), 0.0f, W - vr.getRight(), H);

        // Viewport fill
        g.setColour (juce::Colour (0x15d96010));
        g.fillRect (vr);

        // Viewport border
        g.setColour (juce::Colour (0xffd96010));
        g.drawRect (vr, 1.0f);

        // Left handle
        const juce::Rectangle<float> lh (vr.getX(), 0.0f, (float) kHandleW, H);
        g.setColour (juce::Colour (0xffd96010));
        g.fillRect (lh);
        g.setColour (juce::Colour (0x55000000));
        for (int i = -1; i <= 1; ++i)
            g.drawHorizontalLine ((int) (mid + (float) i * 3.0f),
                                  lh.getX() + 1.5f, lh.getRight() - 1.5f);

        // Right handle
        const juce::Rectangle<float> rh (vr.getRight() - (float) kHandleW, 0.0f, (float) kHandleW, H);
        g.setColour (juce::Colour (0xffd96010));
        g.fillRect (rh);
        g.setColour (juce::Colour (0x55000000));
        for (int i = -1; i <= 1; ++i)
            g.drawHorizontalLine ((int) (mid + (float) i * 3.0f),
                                  rh.getX() + 1.5f, rh.getRight() - 1.5f);
    }
    else
    {
        g.setColour (juce::Colour (0xff1c1e22));
        g.drawHorizontalLine ((int) mid, 0.0f, W);
    }

    // Outer border
    g.setColour (juce::Colour (0xff1c1e22));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);
}

//==============================================================================
// Mouse
//==============================================================================
void WaveformOverview::mouseDown (const juce::MouseEvent& e)
{
    auto snap = processor.sampleData.getSnapshot();
    if (! snap || snap->buffer.getNumSamples() == 0) return;

    isDragging      = true;
    dragStartScroll = processor.scroll.load();
    dragStartZoom   = juce::jmax (1.0f, processor.zoom.load());
    dragStartX      = e.x;

    const float W  = (float) getWidth();
    const float mx = (float) e.x;
    const auto  vr = viewportRect();
    const int   nf = snap->buffer.getNumSamples();

    const juce::Rectangle<float> lh (vr.getX(),
                                     0.0f, (float) kHandleW, (float) getHeight());
    const juce::Rectangle<float> rh (vr.getRight() - (float) kHandleW,
                                     0.0f, (float) kHandleW, (float) getHeight());

    if (lh.contains (mx, (float) e.y))
    {
        dragMode       = DragMode::ResizeLeft;
        dragFixedFrac  = vr.getRight() / W;          // right edge stays fixed (as fraction of nf)
        dragMovingFrac = vr.getX()     / W;           // left edge moves
    }
    else if (rh.contains (mx, (float) e.y))
    {
        dragMode       = DragMode::ResizeRight;
        dragFixedFrac  = vr.getX()     / W;           // left edge stays fixed
        dragMovingFrac = vr.getRight() / W;            // right edge moves
    }
    else if (vr.contains (mx, (float) e.y))
    {
        dragMode       = DragMode::Scroll;
        // Store click offset within viewport as fraction of component width
        dragFixedFrac  = (mx - vr.getX()) / W;
    }
    else
    {
        // Click outside: jump-scroll so viewport centres on click
        dragMode = DragMode::Scroll;

        const int   visLen   = juce::jlimit (1, nf, (int) ((float) nf / dragStartZoom));
        const int   maxStart = juce::jmax (1, nf - visLen);
        const float clickSample = (mx / W) * (float) nf;
        const float newStart    = juce::jlimit (0.0f, (float) maxStart,
                                                 clickSample - (float) visLen * 0.5f);
        processor.scroll.store (newStart / (float) maxStart);
        dragStartScroll = processor.scroll.load();
        dragFixedFrac   = (float) visLen * 0.5f / W;
    }

    repaint();
}

void WaveformOverview::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDragging) return;

    auto snap = processor.sampleData.getSnapshot();
    if (! snap || snap->buffer.getNumSamples() == 0) return;

    const float W   = (float) getWidth();
    const int   nf  = snap->buffer.getNumSamples();
    const float dxF = (float) (e.x - dragStartX) / W;   // movement as fraction of component width

    if (dragMode == DragMode::Scroll)
    {
        // dxF is fraction of numFrames; convert to scroll (0..1 over maxStart)
        const int   visLen   = juce::jlimit (1, nf, (int) ((float) nf / dragStartZoom));
        const int   maxStart = juce::jmax (1, nf - visLen);
        const float dScroll  = dxF * (float) nf / (float) maxStart;
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, dragStartScroll + dScroll));
    }
    else if (dragMode == DragMode::ResizeLeft)
    {
        // dragFixedFrac = right edge (fixed), dragMovingFrac = left edge at drag start
        const float rightFrac = dragFixedFrac;
        const float leftFrac  = juce::jlimit (0.0f,
                                               rightFrac - kMinViewFrac,
                                               dragMovingFrac + dxF);
        const float viewFrac  = rightFrac - leftFrac;
        const float newZoom   = juce::jmax (1.0f, 1.0f / viewFrac);

        const int visLen   = juce::jlimit (1, nf, (int) ((float) nf / newZoom));
        const int maxStart = juce::jmax (1, nf - visLen);
        // leftFrac = visStart / numFrames  =>  visStart = leftFrac * nf
        const float newStart = leftFrac * (float) nf;
        processor.zoom.store   (newZoom);
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, newStart / (float) maxStart));
    }
    else if (dragMode == DragMode::ResizeRight)
    {
        // dragFixedFrac = left edge (fixed), dragMovingFrac = right edge at drag start
        const float leftFrac  = dragFixedFrac;
        const float rightFrac = juce::jlimit (leftFrac + kMinViewFrac,
                                               1.0f,
                                               dragMovingFrac + dxF);
        const float viewFrac  = rightFrac - leftFrac;
        const float newZoom   = juce::jmax (1.0f, 1.0f / viewFrac);

        const int visLen   = juce::jlimit (1, nf, (int) ((float) nf / newZoom));
        const int maxStart = juce::jmax (1, nf - visLen);
        const float newStart = leftFrac * (float) nf;
        processor.zoom.store   (newZoom);
        processor.scroll.store (juce::jlimit (0.0f, 1.0f, newStart / (float) maxStart));
    }

    repaint();
}

void WaveformOverview::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
    dragMode   = DragMode::None;
    repaint();
}

void WaveformOverview::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& w)
{
    auto snap = processor.sampleData.getSnapshot();
    if (! snap || snap->buffer.getNumSamples() == 0) return;

    const float W        = (float) getWidth();
    const int   nf       = snap->buffer.getNumSamples();
    const float oldZoom  = juce::jmax (1.0f, processor.zoom.load());
    const float oldSc    = processor.scroll.load();

    const float newZoom  = juce::jmax (1.0f, oldZoom * (1.0f + w.deltaY * kZoomWheelSens));

    // Keep the sample under the cursor stationary
    const float anchorFrac  = (float) e.x / W;                         // fraction of component
    const float anchorSample = anchorFrac * (float) nf;                // sample index

    const float newVisLen   = (float) nf / newZoom;
    const float newMaxStart = juce::jmax (1.0f, (float) nf - newVisLen);
    // newStart = anchorSample - anchorFrac * newVisLen  (keeps cursor-sample fixed)
    const float newStart    = anchorSample - anchorFrac * newVisLen;

    processor.zoom.store   (newZoom);
    processor.scroll.store (juce::jlimit (0.0f, 1.0f, newStart / newMaxStart));

    repaint();
}
