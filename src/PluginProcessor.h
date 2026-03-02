#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include "audio/SampleData.h"
#include "audio/SliceManager.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"
#include "audio/SoundFontLoader.h"
#include "UndoManager.h"
#include "MidiLearnManager.h"
#include "params/ParamLayout.h"
#include "params/ParamIds.h"

//==============================================================================
class DysektProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    // Command types sent from UI thread to audio thread
    enum CommandType
    {
        CmdNone,
        CmdBeginGesture,
        CmdSetSliceParam,
        CmdSetSliceBounds,
        CmdCreateSlice,
        CmdDeleteSlice,
        CmdDuplicateSlice,
        CmdSplitSlice,
        CmdTransientChop,
        CmdStretch,
        CmdToggleLock,
        CmdLazyChopStart,
        CmdLazyChopStop,
        CmdLoadFile,
        CmdRelinkFile,
        CmdFileLoadFailed,
        CmdUndo,
        CmdRedo,
#include <array>
#include <atomic>
#include <deque>

#include "audio/Slice.h"
#include "audio/SliceManager.h"
#include "audio/SampleData.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"
#include "MidiLearnManager.h"
#include "UndoManager.h"
#include "params/ParamIds.h"
#include "params/ParamLayout.h"

struct SfzSlicePayload;
#include <vector>
#include "audio/SampleData.h"
#include "audio/SliceManager.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"
#include "UndoManager.h"
#include "params/ParamIds.h"
#include "params/ParamLayout.h"
#include "MidiLearnManager.h"
#include "audio/SoundFontLoader.h"

class DysektProcessor : public juce::AudioProcessor
{
public:
    // =========================================================================
    // Inner types
    // =========================================================================

    enum CommandType
    {
        CmdNone,
    DysektProcessor();
    ~DysektProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    const juce::String getName() const override { return "DYSEKT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 5.0; }  // max ADSR release is 5000 ms

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Command FIFO for thread-safe communication from UI
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
        CmdLazyChopStart,
        CmdLazyChopStop,
        CmdStretch,
        CmdToggleLock,
        CmdSetSliceParam,
        CmdSetSliceBounds,
        CmdDuplicateSlice,
        CmdSplitSlice,
        CmdTransientChop,
        CmdRelinkFile,
        CmdFileLoadCompleted,
        CmdFileLoadFailed,
        CmdUndo,
        CmdRedo,
        CmdBeginGesture,
        CmdPanic,
        CmdSelectSlice,
        CmdSetRootNote,
        CmdApplyTrim,
    };

    //==========================================================================
    // How a sample load should be treated
    enum LoadKind
    {
        LoadKindReplace = 0,
        LoadKindRelink  = 1,
    };

    //==========================================================================
    // Sample buffer availability state (audio-thread writable, UI-thread readable)
    enum SampleAvailabilityState
    {
        SampleStateEmpty                = 0,
        SampleStateLoaded               = 1,
        SampleStateMissingAwaitingRelink = 2,
    };

    //==========================================================================
    /** UI-thread-safe snapshot of the current slice state.
        Double-buffered: audio thread writes to inactive slot, then
        atomically flips the index; UI thread reads from active slot. */
    struct UiSliceSnapshot
    {
        std::array<Slice, SliceManager::kMaxSlices> slices;
        int          numSlices     = 0;
        int          selectedSlice = -1;
        int          rootNote      = 36;
        bool         sampleLoaded  = false;
        bool         sampleMissing = false;
        int          sampleNumFrames = 0;
        juce::String sampleFileName;
        bool         snapToZeroCrossing = false;
    };

    //==========================================================================
    /** Command passed through the lock-free FIFO from UI → audio thread. */
    struct Command
    {
        CommandType type       = CmdBeginGesture;
        int         intParam1  = 0;
        int         intParam2  = 0;
        float       floatParam1 = 0.0f;
        juce::File  fileParam;
        int         numPositions = 0;
        std::array<int, SliceManager::kMaxSlices> positions {};
    };

    //==========================================================================
    // Ring-buffer size for the oscilloscope
    static constexpr int kOscRingBufferSize = 2048;  // must be a power of two

    //==========================================================================
    DysektProcessor();
    ~DysektProcessor() override;

    //==========================================================================
    // AudioProcessor interface
    // Enum of every knob/field that can be dragged in the control bar.
    // Values are also used as MidiLearnManager slot indices — do not reorder.
    };

    enum LoadKind
    {
        LoadKindReplace = 0,
        LoadKindRelink = 1,
    };

    enum SampleAvailabilityState
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
        FieldPan,          // v17
        FieldFilterCutoff, // v17
        FieldFilterRes,    // v17
        FieldMidiNote,
        FieldSliceStart,   // MIDI Learn boundary
        FieldSliceEnd,     // MIDI Learn boundary
    };

    enum LoadKind { LoadKindReplace, LoadKindRelink };

    enum SampleState
    {
        SampleStateEmpty,
        SampleStateLoaded,
        SampleStateMissingAwaitingRelink,
    };

    struct Command
    {
        CommandType   type         { CmdNone };
        int           intParam1    { 0 };
        int           intParam2    { 0 };
        float         floatParam1  { 0.0f };
        juce::File    fileParam;
        int           numPositions { 0 };
        std::array<int, SliceManager::kMaxSlices> positions {};
    };

    struct UiSliceSnapshot
    {
        std::array<Slice, SliceManager::kMaxSlices> slices;
        int           numSlices      { 0 };
        int           selectedSlice  { -1 };
        int           rootNote       { 36 };
        bool          sampleLoaded   { false };
        bool          sampleMissing  { false };
        int           sampleNumFrames{ 0 };
        juce::String  sampleFileName;
        bool          midiSelectsSlice   { false };
        bool          snapToZeroCrossing { false };
    };

    struct FailedLoadResult
    {
        int      token { 0 };
        LoadKind kind  { LoadKindReplace };
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

    struct UiSliceSnapshot
    {
        int numSlices = 0;
        int selectedSlice = -1;
        int rootNote = 36;
        bool sampleLoaded = false;
        bool sampleMissing = false;
        int sampleNumFrames = 0;
        juce::String sampleFileName;
        std::array<Slice, SliceManager::kMaxSlices> slices {};
    };

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
    // Oscilloscope ring buffer
    // =========================================================================
    static constexpr int kOscRingBufferSize = 4096;  // must be power of 2

    // =========================================================================
    // Constructor / Destructor
    // =========================================================================
    DysektProcessor();
    ~DysektProcessor() override;

    // =========================================================================
    // AudioProcessor overrides
    // =========================================================================
    DysektProcessor();
    ~DysektProcessor() override;

    // ── AudioProcessor API ───────────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "DYSEKT"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Thread-safe command queue (UI thread → audio thread)
    void pushCommand (Command cmd);

    //==========================================================================
    // Sample loading helpers (called from UI thread)
    void loadFileAsync      (const juce::File& file);
    void loadSoundFontAsync (const juce::File& file);
    void relinkFileAsync    (const juce::File& file);

    //==========================================================================
    // UI snapshot accessors (call from UI thread only)

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYSEKT"; }

    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
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

    // =========================================================================
    // Public API
    // =========================================================================
    void pushCommand (Command cmd);

    void loadFileAsync     (const juce::File& file);
    void loadSoundFontAsync(const juce::File& file);
    void relinkFileAsync   (const juce::File& file);

    /** Read-only access to the latest published UI snapshot (UI thread). */
    const UiSliceSnapshot& getUiSliceSnapshot() const
    {
        const int idx = uiSliceSnapshotIndex.load (std::memory_order_acquire);
        return uiSliceSnapshots[(size_t) idx];
    }

    uint32_t getUiSliceSnapshotVersion() const
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
    int getUiSliceSnapshotVersion() const noexcept

    uint32_t getUiSliceSnapshotVersion() const noexcept
    {
        return uiSnapshotVersion.load (std::memory_order_acquire);
    }

    //==========================================================================
    // Public state visible to UI components
    juce::AudioProcessorValueTreeState apvts;

    SampleData    sampleData;
    SliceManager  sliceManager;
    VoicePool     voicePool;
    LazyChopEngine lazyChop;
    MidiLearnManager midiLearn;

    // =========================================================================
    // Public subsystem members (accessed directly by UI)
    // =========================================================================
    juce::AudioProcessorValueTreeState apvts;
    SliceManager sliceManager;
    VoicePool    voicePool;
    LazyChopEngine lazyChop;
    SampleData   sampleData;
    MidiLearnManager midiLearn;
    UndoManager  undoMgr;

    // =========================================================================
    // UI-readable state atomics
    // =========================================================================
    std::atomic<float> zoom   { 1.0f };
    std::atomic<float> scroll { 0.0f };
    std::atomic<float> dawBpm { 120.0f };

    std::atomic<bool> snapToZeroCrossing { true };
    std::atomic<bool> midiSelectsSlice   { false };
    std::atomic<bool> chromaticMode      { false };
    std::atomic<bool> slicesLinked       { false };

    // Live drag bounds (UI → audio thread, bypasses FIFO for low latency)
    std::atomic<int> liveDragBoundsStart { 0 };
    std::atomic<int> liveDragBoundsEnd   { 0 };
    std::atomic<int> liveDragSliceIdx    { -1 };

    // Shift-preview request
    std::atomic<int> shiftPreviewRequest { -2 };

    // Trim region markers (stored in samples)
    std::atomic<bool> snapToZeroCrossing { false };
    std::atomic<bool> midiSelectsSlice   { false };
    std::atomic<bool> chromaticMode      { false };
    std::atomic<bool> slicesLinked       { false };
    std::atomic<bool> sampleMissing      { false };
    std::atomic<int>  sampleAvailability { (int)SampleStateEmpty };

    // Live-drag slice boundary preview (set by UI, read by processBlock)
    std::atomic<int> liveDragSliceIdx    { -1 };
    std::atomic<int> liveDragBoundsStart { 0 };
    std::atomic<int> liveDragBoundsEnd   { 0 };

    // Shift-preview (UI thread sets this; processBlock acts on it)
    std::atomic<int> shiftPreviewRequest { -2 };

    // Oscilloscope ring buffer (audio thread writes, UI reads)
    std::array<float, kOscRingBufferSize> oscRingBuffer {};
    std::atomic<int> oscRingWriteHead { 0 };

    // Trim in/out points (set by WaveformView, consumed by CmdApplyTrim)
    std::atomic<int> trimInSample  { 0 };
    std::atomic<int> trimOutSample { 0 };
    std::atomic<int> trimPreference  { 0 };
    std::atomic<int> trimRegionStart { 0 };
    std::atomic<int> trimRegionEnd   { 0 };

    // Oscilloscope ring buffer (written by audio thread, read by UI thread)
    std::array<float, kOscRingBufferSize> oscRingBuffer {};
    std::atomic<int> oscRingWriteHead { 0 };

    // Sample availability (see SampleAvailabilityState enum)
    std::atomic<int> sampleAvailability { (int) SampleStateEmpty };

private:
    //==========================================================================
    // Struct for propagating a load failure back to processBlock
    struct FailedLoadResult
    {
        int        token = 0;
        LoadKind   kind  = LoadKindReplace;
        juce::File file;
    };

    //==========================================================================
    // FIFO plumbing
    static constexpr int kCommandFifoSize  = 128;
    static constexpr int kOverflowFifoSize = 32;

    juce::AbstractFifo commandFifo { kCommandFifoSize };
    std::array<Command, kCommandFifoSize> commandBuffer;
    // Peak metering (written in processBlock, read by mixer UI)
    std::atomic<float> masterPeakL { 0.0f };
    std::atomic<float> masterPeakR { 0.0f };

    // SFZ/SF2 slice payload (posted by SoundFontLoader, consumed in processBlock)
    std::atomic<SfzSlicePayload*> pendingSfzSlices { nullptr };

    // =========================================================================
    // Missing file path (UI thread uses this for relink display)
    // =========================================================================
    juce::String missingFilePath;

private:
    // =========================================================================
    // Private helpers
    // =========================================================================
    // Public data (accessed from UI thread)
    SampleData   sampleData;
    SliceManager sliceManager;
    VoicePool    voicePool;
    LazyChopEngine lazyChop;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> zoom   { 1.0f };
    std::atomic<float> scroll { 0.0f };

    // DAW BPM (read from playhead)
    std::atomic<float> dawBpm { 120.0f };

    // MIDI-selects-slice toggle
    std::atomic<bool> midiSelectsSlice { false };

    // Chromatic mode — any MIDI note plays the selected slice pitched relative to rootNote
    std::atomic<bool> chromaticMode { false };

    // Snap-to-zero-crossing toggle
    std::atomic<bool> snapToZeroCrossing { false };

    // Link mode — moving a slice end automatically moves the next slice's start
    std::atomic<bool> slicesLinked { false };

    // Trim markers stored in samples (updated by WaveformView during trim mode)
    std::atomic<int> trimInSample  { 0 };
    std::atomic<int> trimOutSample { 0 };

    // Undo/redo
    UndoManager undoMgr;

    // MIDI Learn — maps CC numbers to SliceParamField ids
    MidiLearnManager midiLearn;

    // Shift preview request: -2=no-op, -1=stop, >=0=start at this sample position
    std::atomic<int> shiftPreviewRequest { -2 };

    // Live slice bounds during edge/move drag — updated every mouseDrag, no undo.
    // Audio thread applies these each block so note-ons during drag use current bounds.
    // Set liveDragSliceIdx to -1 to deactivate.
    std::atomic<int> liveDragSliceIdx   { -1 };
    std::atomic<int> liveDragBoundsStart {  0 };
    std::atomic<int> liveDragBoundsEnd   {  0 };

    // Ring buffer for oscilloscope display (written on audio thread, read on UI thread)
    static constexpr int kOscRingBufferSize = 1024; // must be power of 2
    std::array<float, kOscRingBufferSize> oscRingBuffer {};
    std::atomic<int> oscRingWriteHead { 0 };
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

    SampleAvailabilityState getSampleAvailabilityState() const
    {
        return (SampleAvailabilityState) sampleAvailability.load (std::memory_order_relaxed);
    }

private:
    friend class SoundFontLoader;
#if DYSEKT_HAS_SFIZZ
    friend class SoundFontLoader::LoadJob;
#endif

    struct FailedLoadResult
    {
        int token = 0;
        LoadKind kind = LoadKindReplace;
        juce::File file;
    };

    void drainCommands();
    void handleCommand (const Command& cmd);
    void processMidi (const juce::MidiBuffer& midi);
    void requestSampleLoad (const juce::File& file, LoadKind kind);
    void clearVoicesBeforeSampleSwap();
    void clampSlicesToSampleBounds();
    void publishUiSliceSnapshot();
    void handleCommand (const Command& cmd);
    void processMidi (const juce::MidiBuffer& midi);
    void drainCommands();
    void drainOverflowCommands (bool& handledAny);
    void drainCoalescedCommands (bool& handledAny);
    bool enqueueOverflowCommand (Command cmd);
    bool enqueueCoalescedCommand (const Command& cmd);

    UndoManager::Snapshot makeSnapshot();
    void captureSnapshot();
    void restoreSnapshot (const UndoManager::Snapshot& snap);

    // =========================================================================
    // APVTS parameter pointers (assigned in constructor)
    // =========================================================================
    std::atomic<float>* masterVolParam    { nullptr };
    std::atomic<float>* bpmParam          { nullptr };
    std::atomic<float>* pitchParam        { nullptr };
    std::atomic<float>* algoParam         { nullptr };
    std::atomic<float>* attackParam       { nullptr };
    std::atomic<float>* decayParam        { nullptr };
    std::atomic<float>* sustainParam      { nullptr };
    std::atomic<float>* releaseParam      { nullptr };
    std::atomic<float>* muteGroupParam    { nullptr };
    std::atomic<float>* stretchParam      { nullptr };
    std::atomic<float>* tonalityParam     { nullptr };
    std::atomic<float>* formantParam      { nullptr };
    std::atomic<float>* formantCompParam  { nullptr };
    std::atomic<float>* grainModeParam    { nullptr };
    std::atomic<float>* releaseTailParam  { nullptr };
    std::atomic<float>* reverseParam      { nullptr };
    std::atomic<float>* loopParam         { nullptr };
    std::atomic<float>* oneShotParam      { nullptr };
    std::atomic<float>* maxVoicesParam    { nullptr };
    std::atomic<float>* centsDetuneParam  { nullptr };
    std::atomic<float>* panParam          { nullptr };
    std::atomic<float>* filterCutoffParam { nullptr };
    std::atomic<float>* filterResParam    { nullptr };
    std::atomic<float>* sliceStartParam   { nullptr };
    std::atomic<float>* sliceEndParam     { nullptr };

    // =========================================================================
    // Command queue
    // =========================================================================
    static constexpr int kCommandFifoSize  = 64;
    static constexpr int kOverflowFifoSize = 32;

    juce::AbstractFifo commandFifo { kCommandFifoSize };
    std::array<Command, kCommandFifoSize>  commandBuffer;

    std::array<Command, kOverflowFifoSize> overflowCommandBuffer;
    std::atomic<int> overflowWriteIndex { 0 };
    std::atomic<int> overflowReadIndex  { 0 };

    // Coalesced command atomics (SetSliceParam + SetSliceBounds)
    std::atomic<bool>  pendingSetSliceParam     { false };
    // Coalescing queue for high-frequency param drag commands
    std::atomic<bool>  pendingSetSliceParam      { false };
    std::atomic<int>   pendingSetSliceParamField { 0 };
    std::atomic<float> pendingSetSliceParamValue { 0.0f };

    std::atomic<bool> pendingSetSliceBounds     { false };
    std::atomic<int>  pendingSetSliceBoundsIdx  { 0 };
    std::atomic<int>  pendingSetSliceBoundsStart { 0 };
    std::atomic<int>  pendingSetSliceBoundsEnd   { 0 };

    // Dropped-command diagnostics
    std::atomic<int> droppedCommandCount         { 0 };
    std::atomic<int> droppedCommandTotal         { 0 };
    std::atomic<int> droppedCriticalCommandTotal { 0 };

    //==========================================================================
    // UI snapshot double-buffer
    std::array<UiSliceSnapshot, 2> uiSliceSnapshots;
    std::atomic<int>  uiSliceSnapshotIndex { 0 };
    std::atomic<int>  uiSnapshotVersion    { 0 };
    std::atomic<bool> uiSnapshotDirty      { false };

    //==========================================================================
    // Undo / redo
    UndoManager undoMgr;
    bool gestureSnapshotCaptured  = false;
    int  blocksSinceGestureActivity = 0;

    //==========================================================================
    // Async sample loading
    juce::ThreadPool fileLoadPool { 1 };
    std::atomic<SampleData::DecodedSample*> completedLoadData    { nullptr };
    std::atomic<FailedLoadResult*>          completedLoadFailure  { nullptr };
    std::atomic<int>                        latestLoadToken       { 0 };
    std::atomic<int>                        latestLoadKind        { (int) LoadKindReplace };

    // SF2/SFZ slices posted alongside the decoded buffer
    std::atomic<SfzSlicePayload*> pendingSfzSlices { nullptr };

    // Missing / relink state
    std::atomic<bool> sampleMissing { false };
    juce::String      missingFilePath;

    //==========================================================================
    // APVTS parameter pointers (set in constructor, constant thereafter)
    std::atomic<int>  pendingSetSliceBoundsIdx  { -1 };
    std::atomic<int>  pendingSetSliceBoundsStart{ 0 };
    std::atomic<int>  pendingSetSliceBoundsEnd  { 0 };

    // Diagnostics
    std::atomic<int> droppedCommandCount          { 0 };
    std::atomic<int> droppedCommandTotal          { 0 };
    std::atomic<int> droppedCriticalCommandTotal  { 0 };

    // =========================================================================
    // Gesture / undo state
    // =========================================================================
    bool gestureSnapshotCaptured     { false };
    int  blocksSinceGestureActivity  { 0 };

    // =========================================================================
    // Sample loading
    // =========================================================================
    juce::ThreadPool fileLoadPool { 1 };
    std::atomic<int>  nextLoadToken  { 0 };
    std::atomic<int>  latestLoadToken{ 0 };
    std::atomic<int>  latestLoadKind { (int)LoadKindReplace };
    std::atomic<SampleData::DecodedSample*> completedLoadData    { nullptr };
    std::atomic<FailedLoadResult*>          completedLoadFailure { nullptr };

    // =========================================================================
    // UI snapshot double-buffer
    // =========================================================================
    std::array<UiSliceSnapshot, 2>  uiSliceSnapshots;
    UndoManager::Snapshot makeSnapshot();
    void captureSnapshot();
    void restoreSnapshot (const UndoManager::Snapshot& snap);
    bool enqueueOverflowCommand (Command cmd);
    void drainOverflowCommands (bool& handledAny);
    bool enqueueCoalescedCommand (const Command& cmd);
    void drainCoalescedCommands (bool& handledAny);

    // Command FIFO
    static constexpr int kFifoSize = 256;
    std::array<Command, kFifoSize> commandBuffer;
    juce::AbstractFifo commandFifo { kFifoSize };
    std::atomic<uint32_t> droppedCommandCount { 0 };
    std::atomic<uint32_t> droppedCommandTotal { 0 };
    std::atomic<uint32_t> droppedCriticalCommandTotal { 0 };
    static constexpr int kOverflowFifoSize = 32;
    std::array<Command, kOverflowFifoSize> overflowCommandBuffer {};
    std::atomic<int> overflowReadIndex { 0 };
    std::atomic<int> overflowWriteIndex { 0 };
    std::atomic<bool> pendingSetSliceParam { false };
    std::atomic<int> pendingSetSliceParamField { 0 };
    std::atomic<float> pendingSetSliceParamValue { 0.0f };
    std::atomic<bool> pendingSetSliceBounds { false };
    std::atomic<int> pendingSetSliceBoundsIdx { -1 };
    std::atomic<int> pendingSetSliceBoundsStart { 0 };
    std::atomic<int> pendingSetSliceBoundsEnd { 0 };

    double currentSampleRate = 44100.0;
    bool gestureSnapshotCaptured = false;
    int blocksSinceGestureActivity = 0;

    juce::ThreadPool fileLoadPool { 1 };
    std::atomic<int> nextLoadToken { 0 };
    std::atomic<int> latestLoadToken { 0 };
    std::atomic<int> latestLoadKind { (int) LoadKindReplace };
    std::atomic<SampleData::DecodedSample*> completedLoadData { nullptr };
    std::atomic<FailedLoadResult*> completedLoadFailure { nullptr };
    std::atomic<SfzSlicePayload*>  pendingSfzSlices     { nullptr };
    std::array<UiSliceSnapshot, 2> uiSliceSnapshots {};
    std::atomic<int> uiSliceSnapshotIndex { 0 };
    std::atomic<bool> uiSnapshotDirty { true };
    std::atomic<uint32_t> uiSnapshotVersion { 0 };

    bool heldNotes[128] = {};

    // Cached parameter pointers
    std::atomic<float>* masterVolParam   = nullptr;
    std::atomic<float>* bpmParam         = nullptr;
    std::atomic<float>* pitchParam       = nullptr;
    std::atomic<float>* algoParam        = nullptr;
    std::atomic<float>* attackParam      = nullptr;
    std::atomic<float>* decayParam       = nullptr;
    std::atomic<float>* sustainParam     = nullptr;
    std::atomic<float>* releaseParam     = nullptr;
    std::atomic<float>* muteGroupParam   = nullptr;
    std::atomic<float>* stretchParam     = nullptr;
    std::atomic<float>* tonalityParam    = nullptr;
    std::atomic<float>* formantParam     = nullptr;
    std::atomic<float>* formantCompParam = nullptr;
    std::atomic<float>* grainModeParam   = nullptr;
    std::atomic<float>* releaseTailParam = nullptr;
    std::atomic<float>* reverseParam     = nullptr;
    std::atomic<float>* loopParam        = nullptr;
    std::atomic<float>* oneShotParam     = nullptr;
    std::atomic<float>* maxVoicesParam   = nullptr;
    std::atomic<float>* centsDetuneParam  = nullptr;
    std::atomic<float>* panParam          = nullptr;
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* filterResParam    = nullptr;
    std::atomic<float>* sliceStartParam  = nullptr;
    std::atomic<float>* sliceEndParam    = nullptr;

    double currentSampleRate = 44100.0;

    bool heldNotes[128] {};

    //==========================================================================
    // Internal helpers
    void requestSampleLoad   (const juce::File& file, LoadKind kind);
    void clearVoicesBeforeSampleSwap();
    void clampSlicesToSampleBounds();
    void publishUiSliceSnapshot();
    void handleCommand       (const Command& cmd);
    void drainCommands();
    void drainOverflowCommands (bool& handledAny);
    bool enqueueOverflowCommand (Command cmd);
    bool enqueueCoalescedCommand (const Command& cmd);
    void drainCoalescedCommands (bool& handledAny);
    void processMidi (const juce::MidiBuffer& midi);

    std::atomic<float>* centsDetuneParam = nullptr;
    std::atomic<float>* panParam         = nullptr;
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* filterResParam   = nullptr;
    std::atomic<float>* sliceStartParam  = nullptr;   // normalised 0-1, selected slice start
    std::atomic<float>* sliceEndParam    = nullptr;   // normalised 0-1, selected slice end
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

    // =========================================================================
    // Playback state
    // =========================================================================
    double currentSampleRate { 44100.0 };
    bool   heldNotes[128]    {};
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
