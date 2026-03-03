// src/ui/KeysPanel.cpp

#include "KeysPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

const int KeysPanel::whiteOffsets[14] = { 0,2,4,5,7,9,11,12,14,16,17,19,21,23 };
const KeysPanel::BlackDef KeysPanel::blackDefs[10] = {
    {0,1},{1,3},{3,6},{4,8},{5,10},
    {7,13},{8,15},{10,18},{11,20},{12,22}
};

KeysPanel::KeysPanel(DysektProcessor& p)
    : processor(p)
{
    addAndMakeVisible(transposeDownBtn);
    addAndMakeVisible(transposeUpBtn);

    transposeDownBtn.setButtonText("<<");
    transposeUpBtn.setButtonText(">>");
    transposeDownBtn.setColour(juce::TextButton::buttonColourId, getTheme().button);
    transposeDownBtn.setColour(juce::TextButton::textColourOffId, getTheme().accent);
    transposeUpBtn.setColour(juce::TextButton::buttonColourId, getTheme().button);
    transposeUpBtn.setColour(juce::TextButton::textColourOffId, getTheme().accent);

    transposeDownBtn.onClick = [this] { if (baseOctave > 0) { --baseOctave; resized(); repaint(); } };
    transposeUpBtn.onClick = [this] { if (baseOctave < 7) { ++baseOctave; resized(); repaint(); } };

    startTimerHz(30);
}

KeysPanel::~KeysPanel()
{
    stopTimer();
}

void KeysPanel::resized()
{
    int cx = getWidth() / 2;
    transposeDownBtn.setBounds(cx - 90, 3, 26, 20);
    transposeUpBtn.setBounds(cx + 64, 3, 26, 20);

    keyRects.clear();
    int kbW = kWhiteKeyW * kNumWhite;
    int keyboardX = (getWidth() - kbW) / 2;
    int keyboardY = kTransposeRowH;

    for (int i = 0; i < kNumWhite; ++i)
    {
        KeyRect kr;
        kr.bounds = { keyboardX + i * kWhiteKeyW, keyboardY, kWhiteKeyW - 1, kKeyH };
        kr.note = baseOctave * 12 + whiteOffsets[i];
        kr.isBlack = false;
        keyRects.push_back(kr);
    }
    for (int i = 0; i < kNumBlack; ++i)
    {
        KeyRect kr;
        int bx = keyboardX + (blackDefs[i].afterWhite + 1) * kWhiteKeyW - kBlackKeyW / 2;
        kr.bounds = { bx, keyboardY, kBlackKeyW, kBlackKeyH };
        kr.note = baseOctave * 12 + blackDefs[i].offset;
        kr.isBlack = true;
        keyRects.push_back(kr);
    }
}

void KeysPanel::paint(juce::Graphics& g)
{
    g.fillAll(getTheme().background);

    // Draw white keys
    for (const auto& kr : keyRects)
    {
        if (!kr.isBlack)
        {
            g.setColour(getTheme().foreground);
            g.fillRect(kr.bounds);
            g.setColour(getTheme().gridLine);
            g.drawRect(kr.bounds);
        }
    }
    // Draw black keys on top
    for (const auto& kr : keyRects)
    {
        if (kr.isBlack)
        {
            g.setColour(getTheme().accent);
            g.fillRect(kr.bounds);
        }
    }
    // Draw octave and transpose button labels, etc as needed.
}

void KeysPanel::timerCallback()
{
    // update keys as needed
    repaint();
}