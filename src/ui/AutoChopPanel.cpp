// src/ui/AutoChopPanel.cpp

#include "AutoChopPanel.h"
#include "DysektLookAndFeel.h"
#include "WaveformView.h"
#include "../PluginProcessor.h"
#include "../audio/AudioAnalysis.h"
#include <algorithm>

AutoChopPanel::AutoChopPanel(DysektProcessor& p, WaveformView& wv)
    : processor(p), waveformView(wv)
{
    addAndMakeVisible(sensitivitySlider);
    addAndMakeVisible(modeCombo);
    addAndMakeVisible(divisionsEditor);
    addAndMakeVisible(splitEqualBtn);
    addAndMakeVisible(detectBtn);
    addAndMakeVisible(cancelBtn);

    // Configure controls
    sensitivitySlider.setRange(0.0, 100.0, 1.0);
    sensitivitySlider.setValue(50.0, juce::dontSendNotification);
    sensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 32, 20);
    sensitivitySlider.setColour(juce::Slider::trackColourId, getTheme().accent);
    sensitivitySlider.setColour(juce::Slider::thumbColourId, getTheme().foreground);
    sensitivitySlider.setColour(juce::Slider::backgroundColourId, getTheme().background);
    sensitivitySlider.setColour(juce::Slider::textBoxTextColourId, getTheme().foreground);
    sensitivitySlider.setColour(juce::Slider::textBoxBackgroundColourId, getTheme().header);
    sensitivitySlider.setColour(juce::Slider::textBoxOutlineColourId, getTheme().gridLine);

    sensitivitySlider.onValueChange = [this] { updatePreview(); };

    modeCombo.addItem("Conservative", 1);
    modeCombo.addItem("Normal", 2);
    modeCombo.addItem("Aggressive", 3);
    modeCombo.setSelectedId(2, juce::dontSendNotification);
    modeCombo.setColour(juce::ComboBox::backgroundColourId, getTheme().header);
    modeCombo.setColour(juce::ComboBox::textColourId, getTheme().foreground);
    modeCombo.setColour(juce::ComboBox::outlineColourId, getTheme().gridLine);
    modeCombo.setColour(juce::ComboBox::arrowColourId, getTheme().foreground.withAlpha(0.6f));
    modeCombo.onChange = [this] { updatePreview(); };

    divisionsEditor.setText("16");
    divisionsEditor.setColour(juce::TextEditor::backgroundColourId, getTheme().header);
    divisionsEditor.setColour(juce::TextEditor::textColourId, getTheme().foreground);
    divisionsEditor.setColour(juce::TextEditor::outlineColourId, getTheme().gridLine);
    divisionsEditor.setFont(DysektLookAndFeel::makeFont(13.0f));
    divisionsEditor.setJustification(juce::Justification::centred);

    for (auto* btn : { &splitEqualBtn, &detectBtn, &cancelBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, getTheme().button);
        btn->setColour(juce::TextButton::textColourOnId, getTheme().foreground);
        btn->setColour(juce::TextButton::textColourOffId, getTheme().foreground);
    }

    splitEqualBtn.setButtonText("Split Equal");
    detectBtn.setButtonText("Transient Detect");
    cancelBtn.setButtonText("Cancel");

    splitEqualBtn.onClick = [this] {
        int count = divisionsEditor.getText().getIntValue();
        if (count >= 2 && count <= 128)
        {
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdSplitSlice;
            cmd.intParam1 = count;
            processor.pushCommand(cmd);
        }
        waveformView.transientPreviewPositions.clear();
        waveformView.repaint();
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
    };

    detectBtn.onClick = [this] {
        if (!waveformView.transientPreviewPositions.empty())
        {
            DysektProcessor::Command cmd;
            cmd.type = DysektProcessor::CmdTransientChop;
            cmd.numPositions = (int)waveformView.transientPreviewPositions.size();
            for (int i = 0; i < cmd.numPositions; ++i)
                cmd.positions[i] = waveformView.transientPreviewPositions[i];
            processor.pushCommand(cmd);
        }
        waveformView.transientPreviewPositions.clear();
        waveformView.repaint();
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
    };

    cancelBtn.onClick = [this] {
        waveformView.transientPreviewPositions.clear();
        waveformView.repaint();
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
    };

    updatePreview();
}

AutoChopPanel::~AutoChopPanel()
{
    waveformView.transientPreviewPositions.clear();
    waveformView.repaint();
}

void AutoChopPanel::paint(juce::Graphics& g)
{
    g.fillAll(getTheme().background);
    g.setColour(getTheme().gridLine);
    g.drawRect(getLocalBounds(), 1);

    g.setFont(DysektLookAndFeel::makeFont(11.0f));
    g.setColour(getTheme().foreground.withAlpha(0.8f));
    g.drawText("SENS", 6, 0, 36, getHeight(), juce::Justification::centredLeft);
    g.drawText("MODE", modeCombo.getX() - 38, 0, 36, getHeight(), juce::Justification::centredLeft);
    g.drawText("DIV", divisionsEditor.getX() - 26, 0, 24, getHeight(), juce::Justification::centredLeft);
}

void AutoChopPanel::resized()
{
    int h = getHeight();
    int pad = 4;
    int btnH = h - pad * 2;
    int gap = 8;

    int x = pad;
    sensitivitySlider.setBounds(x + 40, pad, 160, btnH);
    modeCombo.setBounds(x + 220, pad, 100, btnH);
    detectBtn.setBounds(x + 330, pad, 140, btnH); // adjust as needed

    divisionsEditor.setBounds(x + 480, pad, 36, btnH);
    splitEqualBtn.setBounds(x + 530, pad, 100, btnH);
    cancelBtn.setBounds(getWidth() - 60 - pad, pad, 60, btnH);
}

void AutoChopPanel::updatePreview()
{
    auto sampleSnap = processor.sampleData.getSnapshot();
    const auto& ui = processor.getUiSliceSnapshot();
    int sel = ui.selectedSlice;
    if (sel < 0 || sel >= ui.numSlices || sampleSnap == nullptr)
    {
        waveformView.transientPreviewPositions.clear();
        waveformView.repaint();
        return;
    }
    const auto& s = ui.slices[(size_t)sel];
    float sens = (float)sensitivitySlider.getValue() / 100.0f;
    int modeId = juce::jlimit(1, 3, modeCombo.getSelectedId());
    auto mode = static_cast<AudioAnalysis::SensitivityMode>(modeId - 1);
    auto positions = AudioAnalysis::detectTransientsHybrid(
        sampleSnap->buffer, s.startSample, s.endSample, mode, sens, processor.getSampleRate());
    waveformView.transientPreviewPositions = std::move(positions);
    waveformView.repaint();
}