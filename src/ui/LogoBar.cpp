#include "LogoBar.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

LogoBar::LogoBar (DysektProcessor& p) : processor (p) {}

void LogoBar::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().header);

    const auto accent = getTheme().accent;
    const auto fg     = getTheme().foreground;
    const int  h      = getHeight();
    const int  cy     = h / 2;

    // ── Measure wordmark widths first so we can centre everything ─────────
    g.setFont (DysektLookAndFeel::makeFont (20.0f, true));
    const juce::String dy   = "DY";
    const int          dyW  = g.getCurrentFont().getStringWidth (dy);
    const int          sektW = g.getCurrentFont().getStringWidth ("SEKT");

    // Total content: icon (28px) + gap (8px) + wordmark
    const int iconW        = 28;
    const int iconTextGap  = 8;
    const int contentW     = iconW + iconTextGap + dyW + sektW;
    const int offsetX      = (getWidth() - contentW) / 2;
    const int iconX        = juce::jmax (0, offsetX);  // never clip left

    // ── Waveform-slice icon ───────────────────────────────────────────────
    const int   barW  = 3;
    const int   gap   = 2;
    const int   barStep = barW + gap;
    const float barH  = (float) (h - 10);

    const float heights[5] = { 0.55f, 0.90f, 0.48f, 0.80f, 0.52f };
    const int   activeBar  = 1;

    g.setColour (accent.withAlpha (0.12f));
    g.drawHorizontalLine (cy, (float) iconX, (float) (iconX + iconW));

    for (int i = 0; i < 5; ++i)
    {
        const int bx = iconX + i * barStep;
        const int bh = juce::roundToInt (heights[i] * barH);
        const int by = cy - bh / 2;

        if (i == activeBar)
        {
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

    // ── Wordmark: "DY" (foreground) + "SEKT" (accent) ────────────────────
    const int textX = iconX + iconW + iconTextGap;

    g.setFont (DysektLookAndFeel::makeFont (20.0f, true));

    g.setColour (fg.withAlpha (0.90f));
    g.drawText (dy, textX, 0, dyW + 2, h, juce::Justification::centredLeft);

    g.setColour (accent);
    g.drawText ("SEKT", textX + dyW, 0, sektW + 4, h, juce::Justification::centredLeft);

    // ── Tagline: "sample slicer" — right-aligned under wordmark ──────────
    const int wordW = dyW + sektW;
    g.setFont (DysektLookAndFeel::makeFont (8.0f));
    g.setColour (fg.withAlpha (0.90f));
    g.drawText ("sample slicer", textX, cy + 8, wordW, 12,
                juce::Justification::centredRight);
}
