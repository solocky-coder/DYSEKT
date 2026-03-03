// src/ui/SliceLane.cpp

#include "SliceLane.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <array>

SliceLane::SliceLane(DysektProcessor& p)
    : processor(p)
{
}

void SliceLane::paint(juce::Graphics& g)
{
    // Draw background using theme color
    g.fillAll(getTheme().background);

    auto sampleSnap = processor.sampleData.getSnapshot();
    int numFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 0;
    if (numFrames <= 0)
        return;

    const auto& ui = processor.getUiSliceSnapshot();
    const float z = std::max(1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visLen = juce::jlimit(1, numFrames, (int)(numFrames / z));
    const int maxStart = juce::jmax(0, numFrames - visLen);
    const int visStart = juce::jlimit(0, maxStart, (int)(sc * (float)maxStart));

    int sel = ui.selectedSlice;
    int num = ui.numSlices;
    int w = getWidth();
    int h = getHeight();

    int previewIdx = -1, previewStart = 0, previewEnd = 0;
    const bool hasPreview = waveformView
        && waveformView->getActiveSlicePreview(previewIdx, previewStart, previewEnd);

    struct SliceInfo { int idx; int x1; int x2; bool selected; juce::Colour col; };
    std::array<SliceInfo, 64> visibleSlices {};  // assuming max 64 slices
    int visibleCount = 0;

    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[(size_t)i];
        if (!s.active) continue;
        int startSample = s.startSample;
        int endSample = s.endSample;
        if (hasPreview && i == previewIdx)
        {
            startSample = previewStart;
            endSample = previewEnd;
        }
        int x1 = (int)((float)(startSample - visStart) / visLen * w);
        int x2 = (int)((float)(endSample - visStart) / visLen * w);
        x1 = std::max(0, x1);
        x2 = std::min(w, x2);
        if (x2 - x1 < 2) continue;
        if (visibleCount < (int)visibleSlices.size())
            visibleSlices[visibleCount++] = { i, x1, x2, (i == sel), s.colour };
    }

    // Draw slice backgrounds and selection
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool drawSelected = (pass == 1);
        for (int i = 0; i < visibleCount; ++i)
        {
            const auto& si = visibleSlices[i];
            if (si.selected != drawSelected) continue;
            int sw = si.x2 - si.x1;
            g.setColour(si.selected ? si.col.withAlpha(0.55f) : si.col.withAlpha(0.25f));
            g.fillRect(si.x1, 1, sw, h - 2);
            if (si.selected)
            {
                g.setColour(si.col.withAlpha(0.9f));
                g.drawRect(si.x1, 1, sw, h - 2, 1);
            }
        }
    }

    // Draw slice labels (numbered)
    g.setFont(DysektLookAndFeel::makeFont(12.0f, true));
    for (int i = 0; i < visibleCount; ++i)
    {
        const auto& si = visibleSlices[i];
        juce::String label = juce::String(si.idx + 1);
        int labelW = g.getCurrentFont().getStringWidth(label) + 6;
        int labelX = si.x1 + 3;
        g.setColour(si.selected ? getTheme().accent : si.col.withAlpha(0.7f));
        g.drawText(label, labelX, 0, labelW, h, juce::Justification::centredLeft);
    }

    // Draw bottom separator line
    g.setColour(getTheme().gridLine);
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);
}

void SliceLane::mouseDown(const juce::MouseEvent& e)
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    int numFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 0;
    if (numFrames <= 0)
        return;
    const auto& ui = processor.getUiSliceSnapshot();
    int w = getWidth();
    int num = ui.numSlices;
    int sel = ui.selectedSlice;
    const float z = std::max(1.0f, processor.zoom.load());
    const float sc = processor.scroll.load();
    const int visLen = juce::jlimit(1, numFrames, (int)(numFrames / z));
    const int maxStart = juce::jmax(0, numFrames - visLen);
    const int visStart = juce::jlimit(0, maxStart, (int)(sc * (float)maxStart));
    // Find overlapping slice
    for (int i = 0; i < num; ++i)
    {
        const auto& s = ui.slices[i];
        if (!s.active) continue;
        int x1 = (int)((float)(s.startSample - visStart) / visLen * w);
        int x2 = (int)((float)(s.endSample - visStart) / visLen * w);
        if (e.x >= x1 && e.x < x2)
        {
            // Select this slice
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdSelectSlice;
            cmd.intParam1 = i;
            processor.pushCommand(cmd);
            break;
        }
    }
}