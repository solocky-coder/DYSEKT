#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

class DysektProcessor;

class KeysPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit KeysPanel (DysektProcessor& p);
    ~KeysPanel() override;

    void paint    (juce::Graphics& g) override;
    void resized  () override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    // ── Keyzone overlay ───────────────────────────────────────────────────────
    struct Keyzone
    {
        int loKey    { 0 };
        int hiKey    { 127 };
        int loVel    { 0 };          // velocity range low  (0–127)
        int hiVel    { 127 };        // velocity range high (0–127)
        int rootPitch{ -1 };         // root note MIDI number (-1 = none)
        bool isLooped{ false };      // loop flag
        juce::Colour colour;
        juce::String name;
    };

    void setKeyzones      (std::vector<Keyzone> zones);
    void clearKeyzones    ();
    void autoScrollToZones();

private:
    // =========================================================================
    // ZoneMatrixContent — scrollable inner component.
    // Rows are styled to match MixerPanel's slice-row look:
    //   [3px colour bar] [name col, colour-tinted bg] [key-range bar] [vel badge]
    // =========================================================================
    class ZoneMatrixContent : public juce::Component
    {
    public:
        // Row and column layout constants
        static constexpr int kRowH    = 20;   // row height (px)
        static constexpr int kHeaderH = 14;   // header row height

        ZoneMatrixContent (KeysPanel& owner) : owner (owner) {}

        /** Call whenever zone list or keyboard geometry changes. */
        void rebuild (const std::vector<Keyzone>& zones,
                      int kbX, int kbW,
                      int baseOctave,
                      int whiteKeyW, int blackKeyW,
                      int componentWidth);

        void paint     (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

        int  selectedRow = -1;

    private:
        KeysPanel& owner;

        struct Row
        {
            Keyzone zone;
        };
        std::vector<Row> rows;

        // Stored geometry for painting
        int kbX_      = 0;
        int kbW_      = 0;
        int baseOctave_ = 3;
        int contentW_   = 0;

        // No helper needed — table uses text columns only
    };

    // =========================================================================
    void timerCallback() override;
    void buildKeys();
    int  midiNoteForPos (juce::Point<int> pos) const;

    int  whiteNote (int i) const;
    int  blackNote (int i) const;

    struct KeyRect { juce::Rectangle<int> bounds; int note; bool isBlack; };
    std::vector<KeyRect>  keyRects;
    std::vector<Keyzone>  keyzones;

    DysektProcessor& processor;

    int baseOctave    = 3;
    int lastActiveNote = -1;
    int hoveredNote    = -1;

    uint64_t sfzActiveSnap[2] = { 0, 0 };

    juce::TextButton transposeDownBtn { "<" };
    juce::TextButton transposeUpBtn   { ">" };

    // ── Zone matrix Viewport ──────────────────────────────────────────────────
    juce::Viewport    zoneViewport;
    ZoneMatrixContent zoneMatrix { *this };

    // Layout constants — computed dynamically from component size
    int kTransposeRowH = 24;
    int kZoneViewH     = 0;
    int kKeyH          = 60;
    int kWhiteKeyW     = 0;
    int kBlackKeyW     = 0;
    int kBlackKeyH     = 0;
    int kNumWhite      = 14;
    int kNumBlack      = 10;

    static const int    whiteOffsets[14];
    struct BlackDef { int afterWhite; int offset; };
    static const BlackDef blackDefs[10];

    // helpers
    void drawKey         (juce::Graphics&, const KeyRect&,
                          bool hasSlice, bool hovered, bool active) const;
    float noteToX        (int note, int kbX) const;
    float noteKeyWidth   (int note) const;
    juce::Colour zoneColourForNote (int note) const;

    void rebuildZoneMatrix();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeysPanel)
};
