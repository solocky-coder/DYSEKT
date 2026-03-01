#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
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

class DysektProcessor : public juce::AudioProcessor
{
public:
    // =========================================================================
    // Inner types
    // =========================================================================

    enum CommandType
    {
        CmdNone,
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
        CmdFileLoadFailed,
        CmdUndo,
        CmdRedo,
        CmdBeginGesture,
        CmdPanic,
        CmdSelectSlice,
        CmdSetRootNote,
        CmdApplyTrim,
    };

    // Enum of every knob/field that can be dragged in the control bar.
    // Values are also used as MidiLearnManager slot indices — do not reorder.
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
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    {
        return uiSnapshotVersion.load (std::memory_order_acquire);
    }

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

    // Coalescing queue for high-frequency param drag commands
    std::atomic<bool>  pendingSetSliceParam      { false };
    std::atomic<int>   pendingSetSliceParamField { 0 };
    std::atomic<float> pendingSetSliceParamValue { 0.0f };

    std::atomic<bool> pendingSetSliceBounds     { false };
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
    std::atomic<int>      uiSliceSnapshotIndex { 0 };
    std::atomic<uint32_t> uiSnapshotVersion    { 0 };
    std::atomic<bool>     uiSnapshotDirty      { false };

    // =========================================================================
    // Playback state
    // =========================================================================
    double currentSampleRate { 44100.0 };
    bool   heldNotes[128]    {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektProcessor)
};
