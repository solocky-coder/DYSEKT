#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include "audio/SliceManager.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"
#include "audio/SoundFontLoader.h"
#include "MidiLearnManager.h"
#include "UndoManager.h"
#include "params/ParamIds.h"
#include "params/ParamLayout.h"

class DysektProcessor : public juce::AudioProcessor
{
public:
    // ── Load kind ────────────────────────────────────────────────────────────
    enum LoadKind { LoadKindReplace, LoadKindRelink };

    // ── Slice param fields (used by SliceControlBar and MIDI Learn) ──────────
    enum SliceParamField
    {
        FieldBpm = 0,
        FieldPitch,
        FieldAlgorithm,
        FieldAttack,
        FieldDecay,
        FieldSustain,
        FieldRelease,
        FieldMuteGroup,
        FieldStretchEnabled,
        FieldTonality,
        FieldFormant,
        FieldFormantComp,
        FieldGrainMode,
        FieldVolume,
        FieldReleaseTail,
        FieldReverse,
        FieldOutputBus,
        FieldLoop,
        FieldOneShot,
        FieldCentsDetune,
        FieldMidiNote,
        FieldSliceStart,
        FieldSliceEnd,
        FieldPan,
        FieldFilterCutoff,
        FieldFilterRes,
    };

    // ── Command types ────────────────────────────────────────────────────────
    enum CommandType
    {
        CmdNone = 0,
        CmdLoadFile,
        CmdCreateSlice,
        CmdDeleteSlice,
        CmdStretch,
        CmdToggleLock,
        CmdSetSliceParam,
        CmdSetSliceBounds,
        CmdDuplicateSlice,
        CmdSplitSlice,
        CmdTransientChop,
        CmdRelinkFile,
        CmdFileLoadFailed,
        CmdUndo,
        CmdRedo,
        CmdBeginGesture,
        CmdPanic,
        CmdSelectSlice,
        CmdSetRootNote,
        CmdLazyChopStart,
        CmdLazyChopStop,
        CmdApplyTrim,
    };

    // ── Command ──────────────────────────────────────────────────────────────
    struct Command
    {
        CommandType type      = CmdNone;
        int         intParam1 = 0;
        int         intParam2 = 0;
        float       floatParam1 = 0.0f;
        juce::File  fileParam;
        // Variable-length position list (used by CmdTransientChop, etc.)
        static constexpr int kMaxPositions = SliceManager::kMaxSlices + 2;
        std::array<int, kMaxPositions> positions {};
        int numPositions = 0;
    };

    // ── Sample availability state ────────────────────────────────────────────
    enum SampleState
    {
        SampleStateEmpty = 0,
        SampleStateLoaded,
        SampleStateMissingAwaitingRelink,
    };

    // Param field identifiers for CmdSetSliceParam
    enum SliceParamField
    {
        FieldBpm = 0,
        FieldPitch,
        FieldAlgorithm,
        FieldAttack,
        FieldDecay,
        FieldSustain,
        FieldRelease,
        FieldMuteGroup,
        FieldMidiNote,
        FieldStretchEnabled,
        FieldTonality,
        FieldFormant,
        FieldFormantComp,
        FieldGrainMode,
        FieldVolume,
        FieldReleaseTail,
        FieldReverse,
        FieldOutputBus,
        FieldLoop,
        FieldOneShot,
        FieldCentsDetune,
        FieldSliceStart,   // 21 — normalised 0-1 → startSample via MIDI CC
        FieldSliceEnd,     // 22 — normalised 0-1 → endSample via MIDI CC
        FieldPan,          // 23 — per-slice pan -1..+1
        FieldFilterCutoff, // 24 — per-slice LP filter cutoff Hz
        FieldFilterRes,    // 25 — per-slice LP filter resonance 0..1
    };

    struct Command
    {
        CommandType type = CmdNone;
        int intParam1 = 0;
        int intParam2 = 0;
        float floatParam1 = 0.0f;
        juce::File fileParam;
        // Fixed-size array avoids heap allocation/deallocation on the audio thread.
        std::array<int, 128> positions {};
        int numPositions = 0;
    };

    void pushCommand (Command cmd);
    void loadFileAsync (const juce::File& file);
    void relinkFileAsync (const juce::File& file);
    void loadSoundFontAsync (const juce::File& file);  // SF2 / SFZ loader
    void applyTrimToCurrentSample (int trimStart, int trimEnd);

    // ── UI snapshot (double-buffered, written on audio thread) ───────────────
    struct UiSliceSnapshot
    {
        std::array<Slice, SliceManager::kMaxSlices> slices;
        int numSlices = 0;
        int selectedSlice = -1;
        int rootNote = 36;
        bool sampleLoaded  = false;
        bool sampleMissing = false;
        int  sampleNumFrames = 0;
        juce::String sampleFileName;
    };

    // ── Heap-allocated load failure result ───────────────────────────────────
    struct FailedLoadResult
    {
        int      token = 0;
        LoadKind kind  = LoadKindReplace;
        juce::File file;
    };

    // =========================================================================
    DysektProcessor();
    ~DysektProcessor() override;

    // ── AudioProcessor API ───────────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override             { return true; }
    const juce::String getName() const override { return "DYSEKT"; }
    bool acceptsMidi()  const override          { return true; }
    bool producesMidi() const override          { return false; }
    bool isMidiEffect() const override          { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override                              { return 1; }
    int  getCurrentProgram() override                           { return 0; }
    void setCurrentProgram (int) override                       {}
    const juce::String getProgramName (int) override            { return "Default"; }
    void changeProgramName (int, const juce::String&) override  {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Sample loading ───────────────────────────────────────────────────────
    void requestSampleLoad (const juce::File& file, LoadKind kind);
    void loadFileAsync     (const juce::File& file);
    void loadSoundFontAsync (const juce::File& file);
    void relinkFileAsync   (const juce::File& file);

    // ── Command queue (UI → audio thread) ────────────────────────────────────
    void pushCommand (Command cmd);

    // ── UI Snapshot access (UI thread only) ──────────────────────────────────
    const UiSliceSnapshot& getUiSliceSnapshot() const noexcept
    {
        return uiSliceSnapshots[(size_t) uiSliceSnapshotIndex.load (std::memory_order_acquire)];
    }

    uint32_t getUiSliceSnapshotVersion() const noexcept
    {
        return (uint32_t) uiSnapshotVersion.load (std::memory_order_acquire);
    }

    void publishUiSliceSnapshot();

    // ── Public data (accessed by UI components) ───────────────────────────────
    SliceManager  sliceManager;
    SampleData    sampleData;
    VoicePool     voicePool;
    LazyChopEngine lazyChop;
    MidiLearnManager midiLearn;
    juce::AudioProcessorValueTreeState apvts;

    // UI view state
    std::atomic<float> zoom   { 1.0f };
    std::atomic<float> scroll { 0.0f };

    // Feature flags
    std::atomic<bool> snapToZeroCrossing { false };
    std::atomic<bool> midiSelectsSlice   { false };
    std::atomic<bool> chromaticMode      { false };
    /// When true, editing slice N's END also moves slice N+1's START
    std::atomic<bool> slicesLinked       { false };

    // Live drag (updated every audio block while user drags a slice edge)
    std::atomic<int>   liveDragSliceIdx    { -1 };
    std::atomic<int>   liveDragBoundsStart { 0 };
    std::atomic<int>   liveDragBoundsEnd   { 0 };

    // Shift preview request (-2 = idle, -1 = stop, >= 0 = start at this pos)
    std::atomic<int>   shiftPreviewRequest { -2 };

    // DAW tempo
    std::atomic<float> dawBpm { 120.0f };

    // Sample load state
    std::atomic<int>   sampleAvailability { (int) SampleStateEmpty };
    std::atomic<bool>  sampleMissing      { false };
    juce::String       missingFilePath;

    // Async load plumbing (also accessed by SoundFontLoader)
    std::atomic<SampleData::DecodedSample*> completedLoadData   { nullptr };
    std::atomic<FailedLoadResult*>          completedLoadFailure { nullptr };
    std::atomic<SfzSlicePayload*>           pendingSfzSlices     { nullptr };
    std::atomic<int> latestLoadToken  { 0 };
    std::atomic<int> latestLoadKind   { (int) LoadKindReplace };
    std::atomic<int> nextLoadToken    { 0 };
    double currentSampleRate = 44100.0;
    juce::ThreadPool fileLoadPool { 1 };

    // Oscilloscope ring buffer (written on audio thread, read on UI thread)
    static constexpr int kOscRingBufferSize = 4096;
    std::array<float, kOscRingBufferSize> oscRingBuffer {};
    std::atomic<int> oscRingWriteHead { 0 };

    // Trim marker positions in samples (0 = off / use full range)
    std::atomic<int> trimInSample  { 0 };
    std::atomic<int> trimOutSample { 0 };

private:
    // ── APVTS raw parameter pointers (cached for audio-thread access) ─────────
    std::atomic<float>* masterVolParam    = nullptr;
    std::atomic<float>* bpmParam          = nullptr;
    std::atomic<float>* pitchParam        = nullptr;
    std::atomic<float>* algoParam         = nullptr;
    std::atomic<float>* attackParam       = nullptr;
    std::atomic<float>* decayParam        = nullptr;
    std::atomic<float>* sustainParam      = nullptr;
    std::atomic<float>* releaseParam      = nullptr;
    std::atomic<float>* muteGroupParam    = nullptr;
    std::atomic<float>* stretchParam      = nullptr;
    std::atomic<float>* tonalityParam     = nullptr;
    std::atomic<float>* formantParam      = nullptr;
    std::atomic<float>* formantCompParam  = nullptr;
    std::atomic<float>* grainModeParam    = nullptr;
    std::atomic<float>* releaseTailParam  = nullptr;
    std::atomic<float>* reverseParam      = nullptr;
    std::atomic<float>* loopParam         = nullptr;
    std::atomic<float>* oneShotParam      = nullptr;
    std::atomic<float>* maxVoicesParam    = nullptr;
    std::atomic<float>* centsDetuneParam  = nullptr;
    std::atomic<float>* panParam          = nullptr;
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* filterResParam    = nullptr;
    std::atomic<float>* sliceStartParam   = nullptr;
    std::atomic<float>* sliceEndParam     = nullptr;

    // ── Command FIFO ─────────────────────────────────────────────────────────
    static constexpr int kCommandFifoSize = 256;
    juce::AbstractFifo commandFifo { kCommandFifoSize };
    std::array<Command, kCommandFifoSize> commandBuffer;

    // Overflow ring buffer for critical commands that can't fit in the FIFO
    static constexpr int kOverflowFifoSize = 32;
    std::array<Command, kOverflowFifoSize> overflowCommandBuffer;
    std::atomic<int> overflowWriteIndex { 0 };
    std::atomic<int> overflowReadIndex  { 0 };

    // Trim preference (0 = ask every time, 1 = always trim, 2 = never trim)
    enum TrimPreference { TrimPrefAsk = 0, TrimPrefAlways = 1, TrimPrefNever = 2 };
    std::atomic<int> trimPreference { (int) TrimPrefAsk };

    // Trim region stored in samples (set when user applies trim)
    std::atomic<int> trimRegionStart { 0 };
    std::atomic<int> trimRegionEnd   { 0 };

    // Missing sample state (for relink UI)
    std::atomic<bool> sampleMissing { false };
    juce::String missingFilePath;
    std::atomic<int> sampleAvailability { (int) SampleStateEmpty };
    // Coalesced single-write slots for high-frequency commands
    std::atomic<bool>  pendingSetSliceBounds   { false };
    std::atomic<int>   pendingSetSliceBoundsIdx   { -1 };
    std::atomic<int>   pendingSetSliceBoundsStart { 0 };
    std::atomic<int>   pendingSetSliceBoundsEnd   { 0 };

    std::atomic<bool>  pendingSetSliceParam  { false };
    std::atomic<int>   pendingSetSliceParamField { 0 };
    std::atomic<float> pendingSetSliceParamValue { 0.0f };

    // ── UI snapshot double-buffer ─────────────────────────────────────────────
    std::array<UiSliceSnapshot, 2> uiSliceSnapshots;
    std::atomic<int>      uiSliceSnapshotIndex { 0 };
    std::atomic<uint32_t> uiSnapshotVersion    { 0 };
    std::atomic<bool>     uiSnapshotDirty      { false };

    // ── Undo / redo ───────────────────────────────────────────────────────────
    UndoManager undoMgr;
    bool gestureSnapshotCaptured  = false;
    int  blocksSinceGestureActivity = 0;

    // ── Audio state ───────────────────────────────────────────────────────────
    bool heldNotes[128] {};

    // ── Diagnostics ───────────────────────────────────────────────────────────
    std::atomic<int> droppedCommandCount         { 0 };
    std::atomic<int> droppedCommandTotal         { 0 };
    std::atomic<int> droppedCriticalCommandTotal { 0 };

    // ── Private methods ───────────────────────────────────────────────────────
    void processMidi (const juce::MidiBuffer& midi);
    void handleCommand (const Command& cmd);
    void drainCommands();
    void drainOverflowCommands (bool& handledAny);
    void drainCoalescedCommands (bool& handledAny);
    bool enqueueOverflowCommand (Command cmd);
    bool enqueueCoalescedCommand (const Command& cmd);
    void clampSlicesToSampleBounds();
    void clearVoicesBeforeSampleSwap();
    UndoManager::Snapshot makeSnapshot();
    void captureSnapshot();
    void restoreSnapshot (const UndoManager::Snapshot& snap);

    static float sanitiseSample (float s) noexcept
    {
        return (std::isfinite (s) && s >= -8.0f && s <= 8.0f) ? s : 0.0f;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektProcessor)
};
