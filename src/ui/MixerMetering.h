#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "DysektLookAndFeel.h"
#include "ThemeData.h"

/**
 * MixerMetering — a dual-channel VU-style peak meter widget.
 *
 * Features:
 *  - L/R stereo bars (green → yellow → red colour coding)
 *  - Peak-hold indicator with configurable decay
 *  - Designed for vertical orientation inside a mixer strip
 */
class MixerMetering : public juce::Component
{
public:
    MixerMetering()
    {
        peakHoldL = peakHoldR = 0.0f;
        peakHoldDecayL = peakHoldDecayR = 0.0f;
    }

    /** Called from the UI timer to update levels (0..1 linear). */
    void setLevels (float left, float right)
    {
        levelL = juce::jlimit (0.0f, 1.0f, left);
        levelR = juce::jlimit (0.0f, 1.0f, right);

        // Update peak hold
        if (levelL >= peakHoldDecayL)
        {
            peakHoldL      = levelL;
            peakHoldDecayL = levelL;
        }
        else
        {
            peakHoldDecayL = juce::jmax (0.0f, peakHoldDecayL - kPeakDecayRate);
            peakHoldL      = peakHoldDecayL;
        }

        if (levelR >= peakHoldDecayR)
        {
            peakHoldR      = levelR;
            peakHoldDecayR = levelR;
        }
        else
        {
            peakHoldDecayR = juce::jmax (0.0f, peakHoldDecayR - kPeakDecayRate);
            peakHoldR      = peakHoldDecayR;
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = getTheme();
        const auto bounds = getLocalBounds();
        const int barW = (bounds.getWidth() - 3) / 2;  // 1px gap between + border
        const int barH = bounds.getHeight() - 2;
        const int x0   = 1;
        const int x1   = x0 + barW + 1;
        const int y0   = 1;

        g.setColour (theme.waveformBg.darker (0.3f));
        g.fillRect (bounds);
        g.setColour (theme.gridLine);
        g.drawRect (bounds, 1);

        drawBar (g, x0, y0, barW, barH, levelL, peakHoldL);
        drawBar (g, x1, y0, barW, barH, levelR, peakHoldR);
    }

private:
    static constexpr float kPeakDecayRate = 0.008f;  // per-frame decay

    // Decay rate ticks; colours
    static constexpr float kYellowThreshold = 0.7f;  // linear ~-3 dBFS
    static constexpr float kRedThreshold    = 0.9f;  // linear ~-0.9 dBFS

    float levelL { 0.0f }, levelR { 0.0f };
    float peakHoldL { 0.0f }, peakHoldR { 0.0f };
    float peakHoldDecayL { 0.0f }, peakHoldDecayR { 0.0f };

    void drawBar (juce::Graphics& g, int x, int y, int w, int h,
                  float level, float peak) const
    {
        const auto& theme = getTheme();

        // Draw filled bar
        const int fillH = juce::roundToInt (level * (float) h);
        if (fillH > 0)
        {
            // Gradient: bottom = green, middle = yellow, top = red
            const int greenH  = juce::roundToInt (kYellowThreshold * (float) h);
            const int yellowH = juce::roundToInt (kRedThreshold    * (float) h);

            // Green zone
            if (fillH > 0 && greenH > 0)
            {
                g.setColour (juce::Colour (0xFF22CC44));
                g.fillRect (x, y + h - std::min (fillH, greenH),
                            w, std::min (fillH, greenH));
            }
            // Yellow zone
            if (fillH > greenH)
            {
                const int yZoneH = std::min (fillH, yellowH) - greenH;
                g.setColour (juce::Colour (0xFFDDCC00));
                g.fillRect (x, y + h - std::min (fillH, yellowH), w, yZoneH);
            }
            // Red zone
            if (fillH > yellowH)
            {
                const int rZoneH = fillH - yellowH;
                g.setColour (juce::Colour (0xFFDD2200));
                g.fillRect (x, y + h - fillH, w, rZoneH);
            }
        }

        // Peak hold tick
        if (peak > 0.01f)
        {
            const int peakY = y + h - juce::roundToInt (peak * (float) h);
            juce::Colour pkColour =
                peak >= kRedThreshold    ? juce::Colour (0xFFDD2200) :
                peak >= kYellowThreshold ? juce::Colour (0xFFDDCC00) :
                                           juce::Colour (0xFF22CC44);
            g.setColour (pkColour);
            g.drawHorizontalLine (juce::jlimit (y, y + h - 1, peakY), (float) x, (float) (x + w));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerMetering)
};
