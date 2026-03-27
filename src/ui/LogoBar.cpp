#include "LogoBar.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

LogoBar::LogoBar (DysektProcessor& p) : processor (p) {}

void LogoBar::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().header);

    const auto accent = getTheme().accent;
    const auto fg     = getTheme().foreground;
    const int  w  = getWidth();
    const int  h  = getHeight();
    const int  cy = h / 2;

    // ── Measure wordmark so we can centre the whole block ─────────────────
    const float wordmarkSize = 18.0f;
    auto wordmarkFont = DysektLookAndFeel::makeFont (wordmarkSize, true);

    const juce::String dy   = "DY";
    const juce::String sekt = "SEKT";
    const int dyW   = wordmarkFont.getStringWidth (dy);
    const int sektW = wordmarkFont.getStringWidth (sekt);
    const int wordW = dyW + sektW;

    // Icon: 5 bars
    const int barW    = 3;
    const int gap     = 2;
    const int barStep = barW + gap;
    const int iconW   = 5 * barStep - gap;   // 19px
    const int iconGap = 7;

    // Total block width → centre it
    const int blockW  = iconW + iconGap + wordW;
    const int startX  = (w - blockW) / 2;

    // ── Waveform-slice icon ───────────────────────────────────────────────
    const float barH = (float)(h - 16);
    const float heights[5] = { 0.55f, 0.90f, 0.48f, 0.80f, 0.52f };
    const int   activeBar  = 1;

    g.setColour (accent.withAlpha (0.12f));
    g.drawHorizontalLine (cy, (float)startX, (float)(startX + iconW));

    for (int i = 0; i < 5; ++i)
    {
        const int bx = startX + i * barStep;
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

    // ── Wordmark ──────────────────────────────────────────────────────────
    const int textX = startX + iconW + iconGap;

    g.setFont (wordmarkFont);

    // "DY" — foreground
    g.setColour (fg.withAlpha (0.90f));
    g.drawText (dy, textX, 0, dyW + 2, h, juce::Justification::centredLeft);

    // "SEKT" — accent
    g.setColour (accent);
    g.drawText (sekt, textX + dyW, 0, sektW + 2, h, juce::Justification::centredLeft);

    // ── Tagline ───────────────────────────────────────────────────────────
    g.setFont (DysektLookAndFeel::makeFont (7.5f));
    g.setColour (fg.withAlpha (0.55f));
    g.drawText ("sample slicer", textX, cy + 7, wordW, 11,
                juce::Justification::centredRight);
}
