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
    void mouseMove  (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;

    // ── Keyzone overlay ───────────────────────────────────────────────────────
    struct Keyzone { int loKey{0}; int hiKey{127}; juce::Colour colour; };
    void setKeyzones (std::vector<Keyzone> zones);
    void clearKeyzones();
    void autoScrollToZones();

private:
    void timerCallback() override;
    void buildKeys();
    int  midiNoteForPos (juce::Point<int> pos) const;

    // Returns MIDI note number of white key i in current octave
    int whiteNote (int i) const;
    int blackNote (int i) const;   // i = index into blackDefs

    struct KeyRect { juce::Rectangle<int> bounds; int note; bool isBlack; };
    std::vector<KeyRect> keyRects;
    std::vector<Keyzone> keyzones;

    DysektProcessor& processor;

    int  baseOctave   = 3;
    int  lastActiveNote = -1;
    int  hoveredNote  = -1;

    juce::TextButton transposeDownBtn { "<" };
    juce::TextButton transposeUpBtn   { ">" };

    // Layout constants — computed dynamically from component size
    int kTransposeRowH = 24;
    int kZoneBarH      = 8;   // keyzone bar strip height
    int kKeyH          = 60;
    int kWhiteKeyW     = 0;   // set in resized()
    int kBlackKeyW     = 0;
    int kBlackKeyH     = 0;
    int kNumWhite      = 14;  // 2 octaves
    int kNumBlack      = 10;

    static const int whiteOffsets[14];
    struct BlackDef { int afterWhite; int offset; };
    static const BlackDef blackDefs[10];

    // helpers
    void drawKey        (juce::Graphics&, const KeyRect&, bool hasSlice, bool hovered, bool active) const;
    void drawZoneBar    (juce::Graphics&, int keyboardX, int zoneBarY, int kbW) const;
    juce::Colour zoneColourForNote (int note) const;
};
