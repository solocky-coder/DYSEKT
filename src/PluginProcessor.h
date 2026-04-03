#pragma once
#include
#include
#include
#include

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
 FieldSliceStart, // 21 - normalised 0-1 -> startSample via MIDI CC
 FieldSliceEnd, // 22 - normalised 0-1 -> endSample via MIDI CC
 FieldPan, // 23 - per-slice pan -1..+1
 FieldFilterCutoff, // 24 - per-slice LP filter cutoff Hz
 FieldFilterRes, // 25 - per-slice LP filter resonance 0..1
 FieldChromaticChannel, // 26 - per-slice chromatic MIDI channel (0=off, 1-16)
 FieldChromaticLegato, // 27 - per-slice chromatic legato (bool)
 // NOTE: slot 28 is reserved for FieldTrimOut (static constexpr in PluginProcessor.cpp)
 FieldHold = 29, // 29 - per-slice AHDSR hold time (seconds)
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
 CmdEqualChop,
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
 CmdSetSliceLockAll, // intParam1 = slice index, floatParam1 = 1.0 lock all / 0.0 unlock all
 CmdSetSliceColour, // intParam1 = slice index, intParam2 = ARGB colour
 };

 // ── Load kind ────────────────────────────────────────────────────────────
 enum LoadKind { LoadKindReplace = 0, LoadKindRelink = 1 };

 // ── Trim preference ───────────────────────────────────────────────────────
 enum TrimPreference { TrimPrefAsk = 0, TrimPrefAlways = 1, TrimPrefNever = 2 };

 // ── Sample availability state ────────────────────────────────────────────
 enum SampleAvailabilityState
 {
 SampleStateEmpty = 0,
 SampleStateLoaded = 1,
 SampleStateMissingAwaitingRelink = 2,
 };

 // ── Command ──────────────────────────────────────────────────────────────
 struct Command
 {
 CommandType type { CmdNone };
 int intParam1 { 0 };
 int intParam2 { 0 };
 float floatParam1 { 0.0f };
 juce::File fileParam;
 static constexpr int kMaxPositions = SliceManager::kMaxSlices + 2;
 std::array positions {};
 int numPositions { 0 };
 };

 // ── UI snapshot (double-buffered, written on audio thread) ───────────────
 struct UiSliceSnapshot
 {
 std::array slices;
 int numSlices { 0 };
 int selectedSlice { -1 };
 int rootNote { 36 };
 bool sampleLoaded { false };
 bool sampleMissing { false };
 int sampleNumFrames { 0 };
 juce::String sampleFileName;
 bool isDefaultSample { false };
 bool midiSelectsSlice { false };
 bool snapToZeroCrossing { false };
 };

 // ── Oscilloscope ring buffer size ─────────────────────────────────────────
 static constexpr int kOscRingBufferSize = 4096; // must be power of 2

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
 void processBlock (juce::AudioBuffer&, juce::MidiBuffer&) override;

 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override { return true; }

 const juce::String getName() const override { return "DYSEKT"; }
 bool acceptsMidi() const override { return true; }
 bool producesMidi() const override { return false; }
 bool isMidiEffect() const override { return false; }
 double getTailLengthSeconds() const override { return 0.0; }

 int getNumPrograms() override { return 1; }
 int getCurrentProgram() override { return 0; }
 void setCurrentProgram (int) override {}
 const juce::String getProgramName (int) override { return "Default"; }
 void changeProgramName (int, const juce::String&) override {}

 void getStateInformation (juce::MemoryBlock& destData) override;
 void setStateInformation (const void* data, int sizeInBytes) override;

 // =========================================================================
 // Public API
 // =========================================================================
 void pushCommand (Command cmd);

 void loadFileAsync (const juce::File& file);
 void loadDefaultSampleIfNeeded(); // loads Empty.wav on first launch

 // ADSR param pointers — read by SliceWaveformLcd for envelope display
 std::atomic\* attackParam { nullptr };
 std::atomic\* decayParam { nullptr };
 std::atomic\* sustainParam { nullptr };
 std::atomic\* releaseParam { nullptr };
 std::atomic\* holdParam { nullptr };
 void loadSoundFontAsync (const juce::File& file);
 void relinkFileAsync (const juce::File& file);
 void applyTrimToCurrentSample (int trimStart, int trimEnd);

 /\*\* Read-only access to the latest published UI snapshot (UI thread only). */
 const UiSliceSnapshot& getUiSliceSnapshot() const noexcept
 {
 return uiSliceSnapshots[(size_t) uiSliceSnapshotIndex.load (std::memory_order_acquire)];
 }

 int getUiSliceSnapshotVersion() const noexcept
 {
 return (int) uiSnapshotVersion.load (std::memory_order_acquire);
 }

 void publishUiSliceSnapshot();

 /\*\* Returns the peak amplitude (0..1) at a given sample position in the
 \* loaded audio buffer. Used by SliceWaveformLcd to render the mini waveform.
 \* Safe to call from the UI (message) thread. */
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
 SliceManager sliceManager;
 VoicePool voicePool;
 LazyChopEngine lazyChop;
 SampleData sampleData;
 MidiLearnManager midiLearn;

 // =========================================================================
 // UI-readable state atomics
 // =========================================================================
 std::atomic zoom { 1.0f };
 std::atomic scroll { 0.0f };
 std::atomic dawBpm { 120.0f };

 std::atomic snapToZeroCrossing { false };
 std::atomic midiSelectsSlice { false };

 // Per-slice peak meters (0..1, decaying, written from audio thread)
 static constexpr int kMaxMeterSlices = 128;
 std::array, kMaxMeterSlices> slicePeakL {};
 std::array, kMaxMeterSlices> slicePeakR {};
 // Master output peak meters (0..1, decaying, written in audio thread, read by UI)
 std::atomic masterPeakL {0.0f}, masterPeakR {0.0f};
 // Live drag bounds (UI -> audio thread, bypasses FIFO for low latency)
 std::atomic liveDragBoundsStart { 0 };
 std::atomic liveDragBoundsEnd { 0 };
 std::atomic liveDragSliceIdx { -1 };
 // --- Optimistic marker commit notification for UI (set on knob/CC commit, cleared by UI) ---
 std::atomic pendingUiOptimisticIdx { -1 };
 std::atomic pendingUiOptimisticSample { -1 };
 std::atomic paramsSyncedForSlice { -1 }; // slice index that sliceStartParam/sliceEndParam currently describe
 std::atomic sliceStartPublished { -1.0f }; // value written when syncing, used to detect real CC moves

 // Pickup mode state — one flag per MIDI learn slot.
 // Reset when the selected slice changes or a new CC is learned.
 // Audio-thread write, audio-thread read only.
 std::array ccPickedUp {};

 // Commit-on-idle for FieldSliceStart CC — write live drag atomics during
 // movement, commit to SliceManager only after kIdleBlocks of silence.
 static constexpr int kMarkerIdleBlocks = 4; // ~80ms at 512/44100
 int markerIdleCounter = 0; // counts blocks since last CC message
 bool markerPending = false; // true while a commit is outstanding
 int markerPendingSlice = -1; // which slice the pending commit is for
 int markerSmootherSlice = -1; // slice active when absolute-CC smoother was seeded
 int lastProcessedSlice = -1; // detects direct selectedSlice.store() changes between blocks

 // Per-slot smoothed values for CC — prevents audible steps on absolute knobs.
 // Target is set in processMidi(); smoother is stepped each processBlock().
 std::array,
 kMidiLearnNumSlots> ccSmoothers;
 std::array ccSmootherActive {};
 std::atomic sliceEndPublished { -1.0f };

 // Shift-preview request (-2 = idle, -1 = stop, >= 0 = start at position)
 std::atomic shiftPreviewRequest { -2 };

 // Trim region markers (stored in samples)
 std::atomic trimRegionStart { 0 };
 std::atomic trimRegionEnd { 0 };
 std::atomic trimModeActive { false }; // set by editor; CC routes to trim when true
 std::atomic trimInSample { 0 };
 std::atomic trimOutSample { 0 };

 // Trim dialog preference (persisted)
 std::atomic trimPreference { (int) TrimPrefAsk };

 // Oscilloscope ring buffer (audio thread writes, UI reads)
 std::array oscRingBuffer {};
 std::atomic oscRingWriteHead { 0 };

 // Sample availability (see SampleAvailabilityState enum)
 std::atomic sampleAvailability { (int) SampleStateEmpty };
 std::atomic sampleMissing { false };
 juce::String missingFilePath;

 // Peak metering (written in processBlock, read by UI)

 /\*\* Signal that the UI slice snapshot needs to be rebuilt on the next audio block.
 Safe to call from the UI thread. */
 void markUiDirty() noexcept { uiSnapshotDirty.store (true, std::memory_order_release); }

private:
 // =========================================================================
 // Private types
 // =========================================================================
 struct FailedLoadResult
 {
 int token { 0 };
 LoadKind kind { LoadKindReplace };
 juce::File file;
 };

 // =========================================================================
 // Private helpers
 // =========================================================================
 void requestSampleLoad (const juce::File& file, LoadKind kind);
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
 static constexpr int kCommandFifoSize = 256;
 static constexpr int kOverflowFifoSize = 32;

 juce::AbstractFifo commandFifo { kCommandFifoSize };
 std::array commandBuffer;

 std::array overflowCommandBuffer {};
 std::atomic overflowWriteIndex { 0 };
 std::atomic overflowReadIndex { 0 };

 // Coalescing slots for high-frequency drag commands
 std::atomic pendingSetSliceParam { false };
 std::atomic pendingSetSliceParamField { 0 };
 std::atomic pendingSetSliceParamValue { 0.0f };

 std::atomic pendingSetSliceBounds { false };
 std::atomic pendingSetSliceBoundsIdx { -1 };
 std::atomic pendingSetSliceBoundsStart { 0 };
 std::atomic pendingSetSliceBoundsEnd { 0 };

 // Diagnostics
 std::atomic droppedCommandCount { 0 };
 std::atomic droppedCommandTotal { 0 };
 std::atomic droppedCriticalCommandTotal { 0 };

 // =========================================================================
 // UI snapshot double-buffer
 // =========================================================================
 std::array uiSliceSnapshots;
 std::atomic uiSliceSnapshotIndex { 0 };
 std::atomic uiSnapshotVersion { 0 };
 std::atomic uiSnapshotDirty { false };

 // =========================================================================
 // Undo / redo
 // =========================================================================
 UndoManager undoMgr;
    juce::UndoManager juceUndoMgr;  // for UndoableAction-based undo (e.g. marker drag)
 bool gestureSnapshotCaptured { false };
 int blocksSinceGestureActivity { 0 };

 // =========================================================================
 // Sample loading
 // =========================================================================
 juce::ThreadPool fileLoadPool { 1 };
 bool defaultSampleScheduled { false }; // true once default or saved sample is queued
 std::atomic nextLoadToken { 0 };
 std::atomic latestLoadToken{ 0 };
 std::atomic latestLoadKind { (int) LoadKindReplace };
 std::atomic completedLoadData { nullptr };
 std::atomic completedLoadFailure { nullptr };
 std::atomic pendingSfzSlices { nullptr };

 // =========================================================================
 // APVTS parameter pointers (assigned in constructor, constant thereafter)
 // =========================================================================
 std::atomic\* masterVolParam { nullptr };
 std::atomic\* bpmParam { nullptr };
 std::atomic\* pitchParam { nullptr };
 std::atomic\* algoParam { nullptr };

 std::atomic\* muteGroupParam { nullptr };
 std::atomic\* stretchParam { nullptr };
 std::atomic\* tonalityParam { nullptr };
 std::atomic\* formantParam { nullptr };
 std::atomic\* formantCompParam { nullptr };
 std::atomic\* grainModeParam { nullptr };
 std::atomic\* releaseTailParam { nullptr };
 std::atomic\* reverseParam { nullptr };
 std::atomic\* loopParam { nullptr };
 std::atomic\* oneShotParam { nullptr };
 std::atomic\* maxVoicesParam { nullptr };
 std::atomic\* centsDetuneParam { nullptr };
 std::atomic\* panParam { nullptr };
 std::atomic\* filterCutoffParam { nullptr };
 std::atomic\* filterResParam { nullptr };
 std::atomic\* sliceStartParam { nullptr };
 std::atomic\* sliceEndParam { nullptr };

 // =========================================================================
 // Playback state
 // =========================================================================
 double currentSampleRate { 44100.0 };
 bool heldNotes[128] {};

 friend class SoundFontLoader;

 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektProcessor)
};