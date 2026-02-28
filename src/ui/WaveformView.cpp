// From Page 4 through Page 8
void WaveformView::drawWaveform (juce::Graphics& g)
{
    const int cy = getHeight() / 2;
    const float scale = (float) getHeight() * UILayout::waveformVerticalScale;

    auto& peaks = cache.getPeaks();
    const int numPeaks = std::min (cache.getNumPeaks(), getWidth());
    if (numPeaks <= 0)
        return;

    float samplesPerPixel = 1.0f;
    if (paintViewStateActive && cachedPaintViewState.valid)
        samplesPerPixel = cachedPaintViewState.samplesPerPixel;
    else
    {
        const auto view = buildViewState (processor.sampleData.getSnapshot());
        if (view.valid)
            samplesPerPixel = view.samplesPerPixel;
    }

    // Build top/bottom path once (shared by both modes)
    juce::Path fillPath;
    if (samplesPerPixel >= 1.0f)
    {
        fillPath.startNewSubPath (0.0f, (float) cy - peaks[0].maxVal * scale);
        for (int px = 1; px < numPeaks; ++px)
            fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);
        for (int px = numPeaks - 1; px >= 0; --px)
            fillPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].minVal * scale);
        fillPath.closeSubPath();
    }

    if (! softWaveform)
    {
        // HARD MODE (original flat-fill rendering)
        if (samplesPerPixel < 1.0f)
        {
            g.setColour (getTheme().waveform.withAlpha (0.9f));
            juce::Path path;
            bool started = false;
            for (int px = 0; px < numPeaks; ++px)
            {
                float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                if (! started) { path.startNewSubPath ((float) px, y); started = true; }
                else path.lineTo ((float) px, y);
            }
            g.strokePath (path, juce::PathStrokeType (1.5f));
            if (samplesPerPixel < 0.125f)
            {
                const float dotR = 2.5f;
                for (int px = 0; px < numPeaks; ++px)
                {
                    float exactPos = (float) pixelToSample (0) + (float) px * samplesPerPixel;
                    float frac = exactPos - std::floor (exactPos);
                    if (frac < samplesPerPixel)
                    {
                        float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                        g.fillEllipse ((float) px - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
                    }
                }
            }
        }
        else // samplesPerPixel >= 1.0f
        {
            g.setColour (getTheme().waveform);
            g.fillPath (fillPath);
            if (samplesPerPixel < 8.0f)
            {
                juce::Path midPath;
                float midY0 = (float) cy - (peaks[0].maxVal + peaks[0].minVal) * 0.5f * scale;
                midPath.startNewSubPath (0.0f, midY0);
                for (int px = 1; px < numPeaks; ++px)
                {
                    float mid = (peaks[(size_t) px].maxVal + peaks[(size_t) px].minVal) * 0.5f;
                    midPath.lineTo ((float) px, (float) cy - mid * scale);
                }
                g.strokePath (midPath, juce::PathStrokeType (1.5f));
            }
        }
    }
    else // softWaveform == true
    {
        // SOFT MODE (TAL-style: gradient fill + bright outline stroke)
        const juce::Colour waveCol = getTheme().waveform;
        const juce::Colour bgCol = getTheme().waveformBg;
        const int h = getHeight();
        if (samplesPerPixel < 1.0f)
        {
            // Sub-sample zoom: single bright line, no fill needed
            g.setColour (waveCol.withAlpha (0.95f));
            juce::Path path;
            bool started = false;
            for (int px = 0; px < numPeaks; ++px)
            {
                float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                if (! started) { path.startNewSubPath ((float) px, y); started = true; }
                else path.lineTo ((float) px, y);
            }
            g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (samplesPerPixel < 0.125f)
            {
                g.setColour (waveCol);
                const float dotR = 3.0f;
                for (int px = 0; px < numPeaks; ++px)
                {
                    float exactPos = (float) pixelToSample (0) + (float) px * samplesPerPixel;
                    float frac = exactPos - std::floor (exactPos);
                    if (frac < samplesPerPixel)
                    {
                        float y = (float) cy - peaks[(size_t) px].maxVal * scale;
                        g.fillEllipse ((float) px - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
                    }
                }
            }
        }
        else // samplesPerPixel >= 1.0f
        {
            // 1. Translucent gradient fill
            const juce::ColourGradient grad (waveCol.withAlpha (0.0f), 0.0f, 0.0f, waveCol.withAlpha (0.0f), 0.0f, (float) h, false);
            grad.addColour (0.35, waveCol.withAlpha (0.18f));
            grad.addColour (0.5, waveCol.withAlpha (0.28f));
            grad.addColour (0.65, waveCol.withAlpha (0.18f));
            g.setGradientFill (grad);
            g.fillPath (fillPath);

            // 2. Top outline (max envelope)
            juce::Path topPath;
            topPath.startNewSubPath (0.0f, (float) cy - peaks[0].maxVal * scale);
            for (int px = 1; px < numPeaks; ++px)
                topPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].maxVal * scale);
            g.setColour (waveCol.withAlpha (0.25f));
            g.strokePath (topPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (waveCol.withAlpha (0.90f));
            g.strokePath (topPath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // 3. Bottom outline (min envelope)
            juce::Path botPath;
            botPath.startNewSubPath (0.0f, (float) cy - peaks[0].minVal * scale);
            for (int px = 1; px < numPeaks; ++px)
                botPath.lineTo ((float) px, (float) cy - peaks[(size_t) px].minVal * scale);
            g.setColour (waveCol.withAlpha (0.25f));
            g.strokePath (botPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (waveCol.withAlpha (0.90f));
            g.strokePath (botPath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // 4. Transition midline (close zoom)
            if (samplesPerPixel < 8.0f)
            {
                juce::Path midPath;
                float midY0 = (float) cy - (peaks[0].maxVal + peaks[0].minVal) * 0.5f * scale;
                midPath.startNewSubPath (0.0f, midY0);
                for (int px = 1; px < numPeaks; ++px)
                {
                    float mid = (peaks[(size_t) px].maxVal + peaks[(size_t) px].minVal) * 0.5f;
                    midPath.lineTo ((float) px, (float) cy - mid * scale);
                }
                g.setColour (waveCol.withAlpha (0.85f));
                g.strokePath (midPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
}
// Note: drawSlices and drawPlaybackCursors definitions are elsewhere in your code (Pages 8-10).
