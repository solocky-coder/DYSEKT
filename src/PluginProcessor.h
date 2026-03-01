#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <string>

#include "audio/SampleData.h"
#include "audio/SliceManager.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"
#include "audio/SoundFontLoader.h"
#include "MidiLearnManager.h"
#include "UndoManager.h"
#include "params/ParamLayout.h"
#include "params/ParamIds.h"

class DysektEditor;

// =============================================================================
class DysektProcessor : public juce::AudioProcessor
{
public:
    // ── Nested types ──────────────────────────────────────────────────────────

    enum CommandType
    {
        CmdNone = 0,
        CmdLoadFile,
        CmdCreateSlice,
        CmdDeleteSlice,
        CmdDuplicateSlice,
        CmdSplitSlice,
        CmdTransientChop,
        CmdStretch,
        CmdToggleLock,
        CmdSetSliceParam,
        CmdSetSliceBounds,
        CmdLazyChopStart,
        CmdLazyChopStop,
        CmdUndo,
        CmdRedo,
        CmdBeginGesture,
        CmdPanic,
        CmdSelectSlice,
        CmdSetRootNote,
        CmdRelinkFile,
        CmdFileLoadFailed,
        CmdApplyTrim,
    };

    // Slice parameter field identifiers (used by CmdSetSliceParam and MIDI Learn)
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

    struct Command
    {
        CommandType type     = CmdNone;
        int         intParam1  = 0;
        int         intParam2  = 0;
        float       floatParam1 = 0.0f;
        juce::File  fileParam;
        std::array<int, SliceManager::kMaxSlices> positions {};
        int         numPositions = 0;
    };

    enum LoadKind { LoadKindReplace = 0, LoadKindRelink };

    enum SampleState { SampleStateEmpty = 0, SampleStateLoaded, SampleStateMissingAwaitingRelink };

    struct UiSliceSnapshot
    {
        int  numSlices     = 0;
        int  selectedSlice = -1;
        int  rootNote      = 36;
        bool sampleLoaded  = false;
        bool sampleMissing = false;
        int  sampleNumFrames = 0;
        juce::String sampleFileName;
        std::array<Slice, SliceManager::kMaxSlices> slices {};
    };

    struct FailedLoadResult
    {
        int      token = 0;
        LoadKind kind  = LoadKindReplace;
        juce::File file;
    };

    // ── Constructor / Destructor ───────────────────────────────────────────────
    DysektProcessor();
    ~DysektProcessor() override;

    // ── AudioProcessor overrides ───────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Public API ─────────────────────────────────────────────────────────────
    void loadFileAsync      (const juce::File& file);
    void loadSoundFontAsync (const juce::File& file);
    void relinkFileAsync    (const juce::File& file);
    void requestSampleLoad  (const juce::File& file, LoadKind kind);

    void pushCommand (Command cmd);

    const UiSliceSnapshot& getUiSliceSnapshot() const noexcept
    {
        return uiSliceSnapshots[(size_t) uiSliceSnapshotIndex.load (std::memory_order_acquire)];
    }
    uint32_t getUiSliceSnapshotVersion() const noexcept
    {
        return (uint32_t) uiSnapshotVersion.load (std::memory_order_acquire);
    }

    // ── Public data members ────────────────────────────────────────────────────
    SampleData    sampleData;
    SliceManager  sliceManager;
    VoicePool     voicePool;
    LazyChopEngine lazyChop;
    juce::AudioProcessorValueTreeState apvts;
    MidiLearnManager midiLearn;

    std::atomic<float> zoom   { 1.0f };
    std::atomic<float> scroll { 0.0f };
    std::atomic<float> dawBpm { 120.0f };

    std::atomic<bool> chromaticMode      { false };
    std::atomic<bool> midiSelectsSlice   { false };
    std::atomic<bool> snapToZeroCrossing { false };
    std::atomic<bool> slicesLinked       { false };
    std::atomic<bool> sampleMissing      { false };

    // Trim state atomics
    std::atomic<int> trimInSample    { 0 };
    std::atomic<int> trimOutSample   { 0 };
    std::atomic<int> trimPreference  { 0 };  // 0=ask, 1=always, 2=never
    std::atomic<int> trimRegionStart { 0 };
    std::atomic<int> trimRegionEnd   { 0 };

    // Live drag preview (used by WaveformView and processBlock)
    std::atomic<int> liveDragSliceIdx    { -1 };
    std::atomic<int> liveDragBoundsStart { 0  };
    std::atomic<int> liveDragBoundsEnd   { 0  };

    // Shift preview (used by WaveformView)
    std::atomic<int> shiftPreviewRequest { -2 };

    // Sample availability
    std::atomic<int> sampleAvailability { (int) SampleStateEmpty };

    // File loading (used by SoundFontLoader and FileBrowserPanel)
    juce::ThreadPool          fileLoadPool { 1 };
    std::atomic<int>          nextLoadToken  { 0 };
    std::atomic<int>          latestLoadToken { 0 };
    std::atomic<int>          latestLoadKind  { 0 };
    std::atomic<SampleData::DecodedSample*> completedLoadData  { nullptr };
    std::atomic<FailedLoadResult*>          completedLoadFailure { nullptr };
    std::atomic<SfzSlicePayload*>           pendingSfzSlices    { nullptr };
    double currentSampleRate = 44100.0;

    // Oscilloscope ring buffer (read by OscilloscopeView)
    static constexpr int kOscRingBufferSize = 2048;
    std::array<float, kOscRingBufferSize> oscRingBuffer {};
    std::atomic<int> oscRingWriteHead { 0 };

    // Missing file path (read by publishUiSliceSnapshot)
    juce::String missingFilePath;

private:
    // ── FIFO command queue ─────────────────────────────────────────────────────
    static constexpr int kCommandFifoSize   = 256;
    static constexpr int kOverflowFifoSize  = 16;

    juce::AbstractFifo            commandFifo { kCommandFifoSize };
    std::array<Command, kCommandFifoSize> commandBuffer;

    std::array<Command, kOverflowFifoSize> overflowCommandBuffer;
    std::atomic<int> overflowWriteIndex { 0 };
    std::atomic<int> overflowReadIndex  { 0 };

    // ── Coalesced command slots ────────────────────────────────────────────────
    std::atomic<bool>  pendingSetSliceParam  { false };
    std::atomic<int>   pendingSetSliceParamField { 0 };
    std::atomic<float> pendingSetSliceParamValue { 0.0f };

    std::atomic<bool>  pendingSetSliceBounds     { false };
    std::atomic<int>   pendingSetSliceBoundsIdx   { -1 };
    std::atomic<int>   pendingSetSliceBoundsStart { 0   };
    std::atomic<int>   pendingSetSliceBoundsEnd   { 0   };

    // ── Dropped-command telemetry ──────────────────────────────────────────────
    std::atomic<int> droppedCommandCount          { 0 };
    std::atomic<int> droppedCommandTotal          { 0 };
    std::atomic<int> droppedCriticalCommandTotal  { 0 };

    // ── Double-buffered UI snapshot ────────────────────────────────────────────
    std::array<UiSliceSnapshot, 2> uiSliceSnapshots;
    std::atomic<int>  uiSliceSnapshotIndex { 0 };
    std::atomic<int>  uiSnapshotVersion    { 0 };
    std::atomic<bool> uiSnapshotDirty      { false };

    // ── Undo manager ──────────────────────────────────────────────────────────
    UndoManager undoMgr;

    // ── Gesture coalescing (for param drag undo) ───────────────────────────────
    bool gestureSnapshotCaptured  = false;
    int  blocksSinceGestureActivity = 0;

    // ── APVTS parameter pointers ───────────────────────────────────────────────
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

    // ── MIDI state ─────────────────────────────────────────────────────────────
    bool heldNotes[128] = {};

    // ── Internal helpers ───────────────────────────────────────────────────────
    void drainCommands();
    void drainOverflowCommands   (bool& handledAny);
    void drainCoalescedCommands  (bool& handledAny);
    bool enqueueOverflowCommand  (Command cmd);
    bool enqueueCoalescedCommand (const Command& cmd);
    void handleCommand           (const Command& cmd);
    void processMidi             (const juce::MidiBuffer& midi);
    void clampSlicesToSampleBounds();
    void clearVoicesBeforeSampleSwap();
    void publishUiSliceSnapshot();
    void captureSnapshot();
    void restoreSnapshot (const UndoManager::Snapshot& snap);
    UndoManager::Snapshot makeSnapshot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektProcessor)
};
