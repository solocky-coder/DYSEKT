#include "GlobalEqPanel.h"
#include "../PluginProcessor.h"
#include "../params/ParamIds.h"
#include "ThemeData.h"
#include <cmath>

// ── Constructor ───────────────────────────────────────────────────────────────

GlobalEqPanel::GlobalEqPanel (DysektProcessor& p)
    : processor (p)
{
    nodes[Low]  = { Low,  0.f, 200.f,  1.f, {} };
    nodes[Mid]  = { Mid,  0.f, 1000.f, 1.f, {} };
    nodes[High] = { High, 0.f, 8000.f, 1.f, {} };

    setOpaque (false);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void GlobalEqPanel::paint (juce::Graphics& g)
{
    auto& theme = ThemeData::current();
    auto  bounds = getLocalBounds().toFloat();
    auto  plot   = plotArea();

    // Background
    g.setColour (theme.darkBar);
    g.fillRoundedRectangle (bounds, 4.f);

    // Border
    g.setColour (theme.separator);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.f, 1.f);

    // Title
    g.setColour (theme.foreground.withAlpha (0.55f));
    g.setFont (juce::Font ("Barlow Condensed", 11.f, juce::Font::plain));
    g.drawText ("GLOBAL EQ", bounds.removeFromTop (18.f).translated (8.f, 0.f),
                juce::Justification::centredLeft, false);

    // ── Grid ─────────────────────────────────────────────────────────────────
    // Horizontal dB lines
    g.setColour (theme.gridLine);
    for (int db : { -12, -6, 0, 6, 12 })
    {
        float y = gainToY ((float) db);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }
    // 0 dB line — brighter
    {
        float y = gainToY (0.f);
        g.setColour (theme.separator);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    // Vertical frequency lines (decade markers)
    g.setColour (theme.gridLine);
    for (float hz : { 200.f, 500.f, 1000.f, 2000.f, 5000.f, 8000.f })
    {
        float x = freqToX (hz);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    // dB labels (right edge)
    g.setColour (theme.foreground.withAlpha (0.35f));
    g.setFont (juce::Font ("Barlow Condensed", 9.f, juce::Font::plain));
    for (int db : { -12, -6, 6, 12 })
    {
        float y = gainToY ((float) db);
        juce::String label = (db > 0 ? "+" : "") + juce::String (db);
        g.drawText (label,
                    juce::Rectangle<float> (plot.getRight() + 2.f, y - 6.f, 22.f, 12.f),
                    juce::Justification::centredLeft, false);
    }

    // Freq labels (bottom edge)
    for (std::pair<float, const char*> fLabel : {
            std::pair<float,const char*>{200.f, "200"}, {500.f,"500"}, {1000.f,"1k"},
            {2000.f,"2k"}, {5000.f,"5k"}, {8000.f,"8k"} })
    {
        float x = freqToX (fLabel.first);
        g.drawText (fLabel.second,
                    juce::Rectangle<float> (x - 12.f, plot.getBottom() + 2.f, 24.f, 10.f),
                    juce::Justification::centred, false);
    }

    // ── EQ curve ─────────────────────────────────────────────────────────────
    auto curve = buildCurve();

    // Filled area under curve
    {
        auto filled = curve;
        filled.lineTo (plot.getRight(), gainToY (0.f));
        filled.lineTo (plot.getX(),     gainToY (0.f));
        filled.closeSubPath();
        g.setColour (theme.accent.withAlpha (0.08f));
        g.fillPath (filled);
    }

    // Curve line
    g.setColour (theme.accent.withAlpha (0.80f));
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    // ── Node handles ─────────────────────────────────────────────────────────
    const juce::Colour nodeColours[3] = {
        theme.accent.withAlpha (0.85f),
        theme.accent,
        theme.accent.withAlpha (0.85f)
    };
    const char* bandLabels[3] = { "L", "M", "H" };

    for (int i = 0; i < 3; ++i)
    {
        auto& n = nodes[i];
        float x = n.pos.x;
        float y = n.pos.y;

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (x - kNodeRadius + 1.f, y - kNodeRadius + 1.f,
                       kNodeRadius * 2.f, kNodeRadius * 2.f);

        // Fill
        g.setColour (nodeColours[i]);
        g.fillEllipse (x - kNodeRadius, y - kNodeRadius,
                       kNodeRadius * 2.f, kNodeRadius * 2.f);

        // Border
        g.setColour (theme.foreground.withAlpha (0.6f));
        g.drawEllipse (x - kNodeRadius, y - kNodeRadius,
                       kNodeRadius * 2.f, kNodeRadius * 2.f, 1.f);

        // Label
        g.setColour (theme.background);
        g.setFont (juce::Font ("Barlow Condensed", 8.f, juce::Font::bold));
        g.drawText (bandLabels[i],
                    juce::Rectangle<float> (x - kNodeRadius, y - kNodeRadius,
                                            kNodeRadius * 2.f, kNodeRadius * 2.f),
                    juce::Justification::centred, false);
    }

    // Tooltip-style readout for Mid node (shows freq + gain)
    {
        auto& mid = nodes[Mid];
        juce::String tip = juce::String ((int) mid.freqHz) + " Hz  "
                         + (mid.gainDb >= 0 ? "+" : "")
                         + juce::String (mid.gainDb, 1) + " dB";
        g.setColour (theme.foreground.withAlpha (0.50f));
        g.setFont (juce::Font ("Barlow Condensed", 9.f, juce::Font::plain));
        g.drawText (tip, plot.withTrimmedBottom (4.f), juce::Justification::bottomRight, false);
    }
}

// ── Resized ───────────────────────────────────────────────────────────────────

void GlobalEqPanel::resized()
{
    layout();
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void GlobalEqPanel::mouseDown (const juce::MouseEvent& e)
{
    Band hit = hitTest (e.position);
    if (hit == NoBand) return;

    dragBand      = hit;
    dragStartPos  = e.position;
    dragStartGain = nodes[hit].gainDb;
    dragStartFreq = nodes[hit].freqHz;
}

void GlobalEqPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand == NoBand) return;

    auto& n   = nodes[dragBand];
    auto  plot = plotArea();

    // Y → gain
    float rawGain = yToGain (e.position.y);
    n.gainDb = juce::jlimit (-kGainMax, kGainMax, rawGain);

    // X → freq (Mid only)
    if (dragBand == Mid)
    {
        float rawFreq = xToFreq (e.position.x);
        n.freqHz = juce::jlimit (kFreqLo, kFreqHi, rawFreq);
    }

    // Write to APVTS
    auto setParam = [&] (const juce::String& id, float val)
    {
        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (val));
    };

    switch (dragBand)
    {
        case Low:
            setParam (ParamIds::globalEqLowGain,  n.gainDb);
            break;
        case Mid:
            setParam (ParamIds::globalEqMidGain, n.gainDb);
            setParam (ParamIds::globalEqMidFreq, n.freqHz);
            break;
        case High:
            setParam (ParamIds::globalEqHighGain, n.gainDb);
            break;
        default: break;
    }

    layout();
    repaint();
}

void GlobalEqPanel::mouseUp (const juce::MouseEvent&)
{
    dragBand = NoBand;
}

void GlobalEqPanel::mouseMove (const juce::MouseEvent& e)
{
    Band hit = hitTest (e.position);
    setMouseCursor (hit != NoBand ? juce::MouseCursor::PointingHandCursor
                                  : juce::MouseCursor::NormalCursor);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void GlobalEqPanel::layout()
{
    // Sync node values from APVTS
    auto getParam = [&] (const juce::String& id) -> float
    {
        if (auto* p = processor.apvts.getRawParameterValue (id))
            return p->load();
        return 0.f;
    };

    nodes[Low].gainDb  = getParam (ParamIds::globalEqLowGain);
    nodes[Low].freqHz  = 200.f;

    nodes[Mid].gainDb  = getParam (ParamIds::globalEqMidGain);
    nodes[Mid].freqHz  = getParam (ParamIds::globalEqMidFreq);
    nodes[Mid].q       = getParam (ParamIds::globalEqMidQ);

    nodes[High].gainDb = getParam (ParamIds::globalEqHighGain);
    nodes[High].freqHz = 8000.f;

    // Compute screen positions
    nodes[Low].pos  = { freqToX (nodes[Low].freqHz),  gainToY (nodes[Low].gainDb)  };
    nodes[Mid].pos  = { freqToX (nodes[Mid].freqHz),  gainToY (nodes[Mid].gainDb)  };
    nodes[High].pos = { freqToX (nodes[High].freqHz), gainToY (nodes[High].gainDb) };
}

juce::Rectangle<float> GlobalEqPanel::plotArea() const
{
    // Leave room for title, labels, and padding
    return getLocalBounds().toFloat()
           .reduced (8.f)
           .withTrimmedTop (16.f)
           .withTrimmedBottom (14.f)
           .withTrimmedRight (26.f);
}

float GlobalEqPanel::gainToY (float dB) const
{
    auto r = plotArea();
    float t = (dB + kGainMax) / (kGainMax * 2.f);
    // t=1 → top (positive gain), t=0 → bottom
    return r.getBottom() - t * r.getHeight();
}

float GlobalEqPanel::yToGain (float y) const
{
    auto r = plotArea();
    float t = (r.getBottom() - y) / r.getHeight();
    return t * (kGainMax * 2.f) - kGainMax;
}

float GlobalEqPanel::freqToX (float hz) const
{
    auto r = plotArea();
    float logLo = std::log10 (kFreqLo);
    float logHi = std::log10 (kFreqHi);
    float t = (std::log10 (juce::jlimit (kFreqLo, kFreqHi, hz)) - logLo) / (logHi - logLo);
    return r.getX() + t * r.getWidth();
}

float GlobalEqPanel::xToFreq (float x) const
{
    auto r = plotArea();
    float t = (x - r.getX()) / r.getWidth();
    t = juce::jlimit (0.f, 1.f, t);
    float logLo = std::log10 (kFreqLo);
    float logHi = std::log10 (kFreqHi);
    return std::pow (10.f, logLo + t * (logHi - logLo));
}

// Biquad magnitude response (one band only, normalised output).
float GlobalEqPanel::filterMagnitudeAt (int type, float evalHz, float filterHz,
                                         float gainDb, float q, float sampleRate)
{
    // Convert gain to linear amplitude
    const float A  = std::pow (10.f, gainDb / 40.f);   // for shelves: sqrt(10^(dB/20))
    const float Ap = std::pow (10.f, gainDb / 20.f);   // for peak

    const float w0 = juce::MathConstants<float>::twoPi * filterHz / sampleRate;
    const float w  = juce::MathConstants<float>::twoPi * evalHz   / sampleRate;
    const float cw = std::cos (w);
    const float sw = std::sin (w);
    const float cw0= std::cos (w0);
    const float sw0= std::sin (w0);

    float b0, b1, b2, a0, a1, a2;

    if (type == 0) // low shelf
    {
        float alpha = sw0 / 2.f * std::sqrt ((A + 1.f / A) * (1.f / 1.f - 1.f) + 2.f);
        b0 = A * ((A + 1.f) - (A - 1.f) * cw0 + 2.f * std::sqrt (A) * alpha);
        b1 = 2.f * A * ((A - 1.f) - (A + 1.f) * cw0);
        b2 = A * ((A + 1.f) - (A - 1.f) * cw0 - 2.f * std::sqrt (A) * alpha);
        a0 = (A + 1.f) + (A - 1.f) * cw0 + 2.f * std::sqrt (A) * alpha;
        a1 = -2.f * ((A - 1.f) + (A + 1.f) * cw0);
        a2 = (A + 1.f) + (A - 1.f) * cw0 - 2.f * std::sqrt (A) * alpha;
    }
    else if (type == 2) // high shelf
    {
        float alpha = sw0 / 2.f * std::sqrt ((A + 1.f / A) * (1.f / 1.f - 1.f) + 2.f);
        b0 = A * ((A + 1.f) + (A - 1.f) * cw0 + 2.f * std::sqrt (A) * alpha);
        b1 = -2.f * A * ((A - 1.f) + (A + 1.f) * cw0);
        b2 = A * ((A + 1.f) + (A - 1.f) * cw0 - 2.f * std::sqrt (A) * alpha);
        a0 = (A + 1.f) - (A - 1.f) * cw0 + 2.f * std::sqrt (A) * alpha;
        a1 = 2.f * ((A - 1.f) - (A + 1.f) * cw0);
        a2 = (A + 1.f) - (A - 1.f) * cw0 - 2.f * std::sqrt (A) * alpha;
    }
    else // peak
    {
        float alpha = sw0 / (2.f * q);
        b0 = 1.f + alpha * Ap;
        b1 = -2.f * cw0;
        b2 = 1.f - alpha * Ap;
        a0 = 1.f + alpha / Ap;
        a1 = -2.f * cw0;
        a2 = 1.f - alpha / Ap;
    }

    // H(z) magnitude at w = 2π*evalHz/sr, using the bilinear form
    // |H(e^jw)|² = |B(e^jw)|² / |A(e^jw)|²
    float cosW = cw, cos2W = std::cos (2.f * w);
    float bRe = b0 + b1 * cosW + b2 * cos2W;
    float bIm = b1 * std::sin (w) + b2 * std::sin (2.f * w);
    float aRe = a0 + a1 * cosW + a2 * cos2W;
    float aIm = a1 * std::sin (w) + a2 * std::sin (2.f * w);

    float num = bRe * bRe + bIm * bIm;
    float den = aRe * aRe + aIm * aIm;

    if (den < 1e-10f) return 0.f;
    return std::sqrt (num / den);
}

juce::Path GlobalEqPanel::buildCurve() const
{
    auto plot = plotArea();
    int  nPts = juce::jmax (2, (int) plot.getWidth());
    juce::Path path;

    for (int i = 0; i < nPts; ++i)
    {
        float x   = plot.getX() + (float) i;
        float hz  = xToFreq (x);

        // Combined magnitude from all three bands
        float magLin = filterMagnitudeAt (0, hz, nodes[Low].freqHz,
                                          nodes[Low].gainDb, 1.f, kSampleRate)
                     * filterMagnitudeAt (1, hz, nodes[Mid].freqHz,
                                          nodes[Mid].gainDb, nodes[Mid].q, kSampleRate)
                     * filterMagnitudeAt (2, hz, nodes[High].freqHz,
                                          nodes[High].gainDb, 1.f, kSampleRate);

        float dB  = 20.f * std::log10 (juce::jmax (1e-6f, magLin));
        float y   = gainToY (juce::jlimit (-kGainMax, kGainMax, dB));

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    return path;
}

GlobalEqPanel::Band GlobalEqPanel::hitTest (juce::Point<float> p) const
{
    float bestDist = kNodeRadius * 2.f;
    Band  best     = NoBand;

    for (int i = 0; i < 3; ++i)
    {
        float d = nodes[i].pos.getDistanceFrom (p);
        if (d < bestDist)
        {
            bestDist = d;
            best     = static_cast<Band> (i);
        }
    }
    return best;
}
