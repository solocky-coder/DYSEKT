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
    const UiSliceSnapshot& getUiSliceSnapshot() const noexcept
    {
        return uiSliceSnapshots[(size_t) uiSliceSnapshotIndex.load (std::memory_order_acquire)];
    }
    int getUiSliceSnapshotVersion() const noexcept
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

    std::array<Command, kOverflowFifoSize> overflowCommandBuffer;
    std::atomic<int> overflowWriteIndex { 0 };
    std::atomic<int> overflowReadIndex  { 0 };

    // Coalesced command atomics (SetSliceParam + SetSliceBounds)
    std::atomic<bool>  pendingSetSliceParam     { false };
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

    UndoManager::Snapshot makeSnapshot();
    void captureSnapshot();
    void restoreSnapshot (const UndoManager::Snapshot& snap);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektProcessor)
};
