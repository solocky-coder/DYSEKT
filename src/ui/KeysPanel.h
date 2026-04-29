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

    void paint     (juce::Graphics& g) override;
    void resized   () override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    // ── Keyzone overlay ───────────────────────────────────────────────────────
    struct Keyzone
    {
        int loKey      { 0 };
        int hiKey      { 127 };
        int loVel      { 0 };
        int hiVel      { 127 };
        int rootPitch  { -1 };
        bool isLooped  { false };
        juce::Colour colour;
        juce::String name;
        float volDb      { -7.0f };   // per-zone volume in dB (SFZ: volume=, SF2: gen48 centibels)
        float pan        { 0.0f };    // per-zone pan -1..+1  (SFZ: pan=,    SF2: gen17 0.1% units)
        float tuneCents  { 0.0f };    // per-zone pitch in cents (SFZ: tune=,  SF2: gen52 cents)
        float releaseSec { 0.664f };  // ampeg_release in seconds (SFZ: ampeg_release=, SF2: gen38 timecents)
        bool  isSfz      { false };   // true = SFZ (editable), false = SF2 (read-only)
    };

    void setKeyzones      (std::vector<Keyzone> zones);
    void clearKeyzones    ();
    void autoScrollToZones();

    /** Fired on the UI thread when the user drags a vol/pan/pitch cell in the zone matrix.
     *  Only fires for SFZ zones (isSfz == true). zoneIndex is 0-based row index. */
    std::function<void (int zoneIndex, float volDb, float pan, float tuneCents)> onZoneChanged;

    /** Scroll the zone matrix to highlight the row covering 'note'. */
    void highlightNoteInMatrix (int note);

    // Called by ZoneMatrixContent to schedule a deferred note-off
    void scheduleNoteOff (int note);

private:
    // =========================================================================
    // ZoneMatrixContent
    // =========================================================================
    class ZoneMatrixContent : public juce::Component
    {
    public:
        static constexpr int kRowH    = 20;
        static constexpr int kHeaderH = 14;

        ZoneMatrixContent (KeysPanel& owner) : owner (owner) {}

        void rebuild (const std::vector<Keyzone>& zones,
                      int kbX, int kbW,
                      int baseOctave,
                      int whiteKeyW, int blackKeyW,
                      int componentWidth);

        /** Select and scroll to the first row that covers 'note'. -1 = clear. */
        void highlightNote (int note);

        void paint     (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp   (const juce::MouseEvent& e) override;

        int  selectedRow = -1;

    private:
        KeysPanel& owner;
        struct Row { Keyzone zone; };
        std::vector<Row> rows;
        int kbX_ = 0, kbW_ = 0, baseOctave_ = 0, contentW_ = 0;

        // Drag state for vol/pan cells
        enum class DragCol { None, Vol, Pan, Tune };
        DragCol dragCol     { DragCol::None };
        int     dragRow     { -1 };
        float   dragStartY  { 0.f };
        float   dragStartVal{ 0.f };

        int  colHitTest (int x) const;  // returns kVolX or kPanX start, or -1
    };

    // =========================================================================
    void timerCallback() override;
    void releaseLastNote();

    struct KeyRect { juce::Rectangle<int> bounds; int note; bool isBlack; };
    std::vector<KeyRect>  keyRects;
    std::vector<Keyzone>  keyzones;

    DysektProcessor& processor;

    int baseOctave     = 3;
    int lastActiveNote = -1;
    int hoveredNote    = -1;
    int pendingNoteOff = -1;

    uint64_t sfzActiveSnap[2] = { 0, 0 };

    juce::TextButton transposeDownBtn { "<" };
    juce::TextButton transposeUpBtn   { ">" };

    juce::Viewport    zoneViewport;
    ZoneMatrixContent zoneMatrix { *this };

    // Full-keyboard: MIDI 0-127 = 75 white + 53 black keys
    static constexpr int kTotalWhite = 75;
    static constexpr int kTotalBlack = 53;

    int   kTransposeRowH = 0;
    int   kZoneViewH     = 0;
    int   kKeyH          = 44;
    int   kWhiteKeyW     = 0;
    float kWhiteKeyWf    = 1.f;
    int   kBlackKeyW     = 0;
    int   kBlackKeyH     = 0;
    int   kNumWhite      = kTotalWhite;
    int   kNumBlack      = kTotalBlack;

    void  drawKey           (juce::Graphics&, const KeyRect&,
                             bool hasSlice, bool hovered, bool active) const;
    float noteToX           (int note, int kbX) const;
    float noteKeyWidth      (int note) const;
    juce::Colour zoneColourForNote (int note) const;
    void  rebuildZoneMatrix ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeysPanel)
};
