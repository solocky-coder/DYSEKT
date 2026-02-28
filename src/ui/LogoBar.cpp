#include "LogoBar.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include <cmath>

LogoBar::LogoBar (DysektProcessor& p) : processor (p) {}

void LogoBar::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().header);

    const auto accent   = getTheme().accent;
    const auto fg       = getTheme().foreground;
    const int  h        = getHeight();
    const int  cy       = h / 2;

    // ── Waveform-slice icon ───────────────────────────────────────────────
    // 5 bars of varying height, drawn left-to-right starting at x=10
    const int iconX  = 10;
    const int iconW  = 28;   // total icon width
    const int barW   = 3;
    const int gap    = 2;
    const int barStep = barW + gap;

    // Heights as fraction of bar area height (26px)
    const float barH  = (float) (h - 10);   // available vertical space
    // [left … right]: fraction of barH for each bar
    const float heights[5] = { 0.55f, 0.90f, 0.48f, 0.80f, 0.52f };
    // bar 1 (index 1) is the "active" highlighted bar
    const int activeBar = 1;

    // faint horizontal centre line
    g.setColour (accent.withAlpha (0.12f));
    g.drawHorizontalLine (cy, (float) iconX, (float) (iconX + iconW));

    for (int i = 0; i < 5; ++i)
    {
        const int bx = iconX + i * barStep;
        const int bh = juce::roundToInt (heights[i] * barH);
        const int by = cy - bh / 2;

        if (i == activeBar)
        {
            // filled with accent + brighter stroke
            g.setColour (accent.withAlpha (0.22f));
            g.fillRect (bx, by, barW, bh);
            g.setColour (accent);
        }
        else
        {
            g.setColour (accent.withAlpha (0.45f));
        }
        g.drawRect (bx, by, barW, bh, 1);
    }

    // ── Wordmark: "DY" (foreground) + "SEKT" (accent) ───────────────────
    const int textX = iconX + iconW + 8;

    g.setFont (DysektLookAndFeel::makeFont (20.0f, true));

    // "DY" — foreground colour
    g.setColour (fg.withAlpha (0.90f));
    const juce::String dy = "DY";
    const int dyW = g.getCurrentFont().getStringWidth (dy);
    g.drawText (dy, textX, 0, dyW + 2, h, juce::Justification::centredLeft);

    // "SEKT" — accent colour
    g.setColour (accent);
    g.drawText ("SEKT", textX + dyW, 0, 60, h, juce::Justification::centredLeft);

    // ── Tagline: "sample slicer" ──────────────────────────────────────────
    const int tagX = textX;
    // estimate wordmark total width to position tagline underneath right edge
    const int sektW = g.getCurrentFont().getStringWidth ("SEKT");
    const int wordW = dyW + sektW;

    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (fg.withAlpha (0.22f));
    // right-align tagline under the wordmark
    g.drawText ("sample slicer", tagX, cy + 8, wordW, 12,
                juce::Justification::centredRight);
}
