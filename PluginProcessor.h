#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <deque>

#include "audio/Slice.h"
#include "audio/SampleData.h"
#include "audio/SliceManager.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"
#include "audio/SoundFontLoader.h"
#include "UndoManager.h"
#include "MidiLearnManager.h"
#include "params/ParamIds.h"
#include "params/ParamLayout.h"

struct SfzSlicePayload;

//==============================================================================
class DysektProcessor : public juce::AudioProcessor
{
public:
    // =========================================================================
    // Inner types
    // =========================================================================

    // Param field identifiers for CmdSetSliceParam.
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
        FieldMidiNote,
        FieldSliceStart,   // 21 - normalised 0-1 -> startSample via MIDI CC
        FieldSliceEnd,     // 22 - normalised 0-1 -> endSample via MIDI CC
        FieldPan,          // 23 - per-slice pan -1..+1
        FieldFilterCutoff, // 24 - per-slice LP filter cutoff Hz
        FieldFilterRes,    // 25 - per-slice LP filter resonance 0..1
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
        CmdSetSliceLockAll,  // intParam1 = slice index, floatParam1 = 1.0 lock all / 0.0 unlock all
    };

    // ── Load kind ────────────────────────────────────────────────────────────
    enum LoadKind { LoadKindReplace = 0, LoadKindRelink = 1 };

    // ── Trim preference ───────────────────────────────────────────────────────
    enum TrimPreference { TrimPrefAsk = 0, TrimPrefAlways = 1, TrimPrefNever = 2 };

    // ── Sample availability state ────────────────────────────────────────────
    enum SampleAvailabilityState
    {
        SampleStateEmpty                 = 0,
        SampleStateLoaded                = 1,
        SampleStateMissingAwaitingRelink = 2,
    };

    // ── Command ──────────────────────────────────────────────────────────────
    struct Command
    {
        CommandType type        { CmdNone };
        int         intParam1   { 0 };
        int         intParam2   { 0 };
        float       floatParam1 { 0.0f };
        juce::File  fileParam;
        static constexpr int kMaxPositions = SliceManager::kMaxSlices + 2;
        std::array<int, kMaxPositions> positions {};
        int numPositions { 0 };
    };

    // ── UI snapshot (double-buffered, written on audio thread) ───────────────
    struct UiSliceSnapshot
    {
        std::array<Slice, SliceManager::kMaxSlices> slices;
        int          numSlices          { 0 };
        int          selectedSlice      { -1 };
        int          rootNote           { 36 };
        bool         sampleLoaded       { false };
        bool         sampleMissing      { false };
        int          sampleNumFrames    { 0 };
        juce::String sampleFileName;
        bool         midiSelectsSlice   { false };
        bool         snapToZeroCrossing { false };
    };

    // ── Oscilloscope ring buffer size ─────────────────────────────────────────
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
    bool hasEditor() const override              { return true; }

    const juce::String getName() const override  { return "DYSEKT"; }
    bool acceptsMidi()  const override           { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
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

    void loadFileAsync      (const juce::File& file);
    void loadSoundFontAsync (const juce::File& file);
    void relinkFileAsync    (const juce::File& file);
    void applyTrimToCurrentSample (int trimStart, int trimEnd);

    /** Read-only access to the latest published UI snapshot (UI thread only). */
    const UiSliceSnapshot& getUiSliceSnapshot() const noexcept
    {
        return uiSliceSnapshots[(size_t) uiSliceSnapshotIndex.load (std::memory_order_acquire)];
    }

    int getUiSliceSnapshotVersion() const noexcept
    {
        return (int) uiSnapshotVersion.load (std::memory_order_acquire);
    }

    void publishUiSliceSnapshot();

    /** Returns the peak amplitude (0..1) at a given sample position in the
     *  loaded audio buffer.  Used by SliceWaveformLcd to render the mini waveform.
     *  Safe to call from the UI (message) thread. */
    float getWaveformPeakAt (int samplePosition) const noexcept
    {
        if (! sampleData.isLoaded()) return 0.0f;
        const auto& buf = sampleData.getBuffer();
        const int n = buf.getNumSamples();
        if (samplePosition < 0 || samplePosition >= n) return 0.0f;
        float peak = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            peak = std::max (peak, std::abs (buf.getSample (ch, samplePosition)));
        return peak;
    }

    // =========================================================================
    // Public subsystem members (accessed directly by UI)
    // =========================================================================
    juce::AudioProcessorValueTreeState apvts;
    SliceManager     sliceManager;
    VoicePool        voicePool;
    LazyChopEngine   lazyChop;
    SampleData       sampleData;
    MidiLearnManager midiLearn;

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

    // Live drag bounds (UI -> audio thread, bypasses FIFO for low latency)
    std::atomic<int> liveDragBoundsStart { 0 };
    std::atomic<int> liveDragBoundsEnd   { 0 };
    std::atomic<int> liveDragSliceIdx    { -1 };

    // Shift-preview request (-2 = idle, -1 = stop, >= 0 = start at position)
    std::atomic<int> shiftPreviewRequest { -2 };

    // Trim region markers (stored in samples)
    std::atomic<int> trimRegionStart { 0 };
    std::atomic<int> trimRegionEnd   { 0 };
    std::atomic<int> trimInSample    { 0 };
    std::atomic<int> trimOutSample   { 0 };

    // Trim dialog preference (persisted)
    std::atomic<int> trimPreference  { (int) TrimPrefAsk };

    // Oscilloscope ring buffer (audio thread writes, UI reads)
    std::array<float, kOscRingBufferSize> oscRingBuffer {};
    std::atomic<int> oscRingWriteHead { 0 };

    // Sample availability (see SampleAvailabilityState enum)
    std::atomic<int>  sampleAvailability { (int) SampleStateEmpty };
    std::atomic<bool> sampleMissing      { false };
    juce::String      missingFilePath;

    // Peak metering (written in processBlock, read by UI)
    std::atomic<float> masterPeakL { 0.0f };
    std::atomic<float> masterPeakR { 0.0f };

private:
    // =========================================================================
    // Private types
    // =========================================================================
    struct FailedLoadResult
    {
        int        token { 0 };
        LoadKind   kind  { LoadKindReplace };
        juce::File file;
    };

    // =========================================================================
    // Private helpers
    // =========================================================================
    void requestSampleLoad (const juce::File& file, LoadKind kind);
    void loadDefaultSampleIfNeeded();   // loads Empty.wav on first launch
    void clearVoicesBeforeSampleSwap();
    void clampSlicesToSampleBounds();
    void handleCommand (const Command& cmd);
    void drainCommands();
    void drainOverflowCommands (bool& handledAny);
    void drainCoalescedCommands (bool& handledAny);
    bool enqueueOverflowCommand (Command cmd);
    bool enqueueCoalescedCommand (const Command& cmd);
    void processMidi (const juce::MidiBuffer& midi);

    UndoManager::Snapshot makeSnapshot();
    void captureSnapshot();
    void restoreSnapshot (const UndoManager::Snapshot& snap);

    static float sanitiseSample (float s) noexcept
    {
        return (std::isfinite (s) && s >= -8.0f && s <= 8.0f) ? s : 0.0f;
    }

    // =========================================================================
    // Command FIFO
    // =========================================================================
    static constexpr int kCommandFifoSize  = 256;
    static constexpr int kOverflowFifoSize = 32;

    juce::AbstractFifo commandFifo { kCommandFifoSize };
    std::array<Command, kCommandFifoSize> commandBuffer;

    std::array<Command, kOverflowFifoSize> overflowCommandBuffer {};
    std::atomic<int> overflowWriteIndex { 0 };
    std::atomic<int> overflowReadIndex  { 0 };

    // Coalescing slots for high-frequency drag commands
    std::atomic<bool>  pendingSetSliceParam      { false };
    std::atomic<int>   pendingSetSliceParamField { 0 };
    std::atomic<float> pendingSetSliceParamValue { 0.0f };

    std::atomic<bool> pendingSetSliceBounds      { false };
    std::atomic<int>  pendingSetSliceBoundsIdx   { -1 };
    std::atomic<int>  pendingSetSliceBoundsStart { 0 };
    std::atomic<int>  pendingSetSliceBoundsEnd   { 0 };

    // Diagnostics
    std::atomic<int> droppedCommandCount         { 0 };
    std::atomic<int> droppedCommandTotal         { 0 };
    std::atomic<int> droppedCriticalCommandTotal { 0 };

    // =========================================================================
    // UI snapshot double-buffer
    // =========================================================================
    std::array<UiSliceSnapshot, 2> uiSliceSnapshots;
    std::atomic<int>      uiSliceSnapshotIndex { 0 };
    std::atomic<uint32_t> uiSnapshotVersion    { 0 };
    std::atomic<bool>     uiSnapshotDirty      { false };

    // =========================================================================
    // Undo / redo
    // =========================================================================
    UndoManager undoMgr;
    bool gestureSnapshotCaptured    { false };
    int  blocksSinceGestureActivity { 0 };

    // =========================================================================
    // Sample loading
    // =========================================================================
    juce::ThreadPool fileLoadPool { 1 };
    bool             defaultSampleScheduled { false }; // true once default or saved sample is queued
    std::atomic<int>  nextLoadToken  { 0 };
    std::atomic<int>  latestLoadToken{ 0 };
    std::atomic<int>  latestLoadKind { (int) LoadKindReplace };
    std::atomic<SampleData::DecodedSample*> completedLoadData   { nullptr };
    std::atomic<FailedLoadResult*>          completedLoadFailure { nullptr };
    std::atomic<SfzSlicePayload*>           pendingSfzSlices     { nullptr };

    // =========================================================================
    // APVTS parameter pointers (assigned in constructor, constant thereafter)
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
    // Playback state
    // =========================================================================
    double currentSampleRate { 44100.0 };
    bool   heldNotes[128]    {};

    friend class SoundFontLoader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektProcessor)
};
