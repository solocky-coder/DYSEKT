#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DysektProcessor;

class KeysPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit KeysPanel (DysektProcessor& p);
    ~KeysPanel() override;

    void paint      (juce::Graphics& g) override;
    void resized    () override;
    void mouseDown  (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void buildKeys();
    int  midiNoteForPos (juce::Point<int> pos) const;

    // Returns MIDI note number of white key i in current octave
    int whiteNote (int i) const;
    int blackNote (int i) const;   // i = index into blackDefs

    struct KeyRect { juce::Rectangle<int> bounds; int note; bool isBlack; };
    std::vector<KeyRect> keyRects;

    DysektProcessor& processor;

    int  baseOctave = 3;   // shows C(baseOctave) to B(baseOctave+1)
    int  lastActiveNote = -1;

    juce::TextButton transposeDownBtn { "<" };
    juce::TextButton transposeUpBtn   { ">" };

    static constexpr int kTransposeRowH = 28;
    static constexpr int kKeyH          = 70;
    static constexpr int kWhiteKeyW     = 63;  // 900 / 14 ≈ 63
    static constexpr int kBlackKeyW     = 37;
    static constexpr int kBlackKeyH     = 44;
    static constexpr int kNumWhite      = 14;
    static constexpr int kNumBlack      = 10;

    // white-key semitone offsets for two octaves
    static const int whiteOffsets[14];
    // black key: {afterWhite index, semitone offset}
    struct BlackDef { int afterWhite; int offset; };
    static const BlackDef blackDefs[10];
};
