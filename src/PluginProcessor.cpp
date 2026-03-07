#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "audio/GrainEngine.h"
#include "audio/AudioAnalysis.h"
#include "audio/SoundFontLoader.h"
#include <functional>
#include <memory>

namespace
{
class SampleDecodeJob final : public juce::ThreadPoolJob
{
public:
    using SuccessFn = std::function<void (int, DysektProcessor::LoadKind,
                                          std::unique_ptr<SampleData::DecodedSample>)>;
    using FailureFn = std::function<void (int, DysektProcessor::LoadKind, const juce::File&)>;

    SampleDecodeJob (juce::File sourceFile, double targetRate, int loadToken,
                     DysektProcessor::LoadKind kind,
                     SuccessFn onSuccessIn, FailureFn onFailureIn)
        : juce::ThreadPoolJob ("SampleDecodeJob"),
          file (std::move (sourceFile)),
          sampleRate (targetRate),
          token (loadToken),
          loadKind (kind),
          onSuccess (std::move (onSuccessIn)),
          onFailure (std::move (onFailureIn))
    {
    }

    JobStatus runJob() override
    {
        auto decoded = SampleData::decodeFromFile (file, sampleRate);
        if (shouldExit())
            return jobHasFinished;

        if (decoded != nullptr)
            onSuccess (token, loadKind, std::move (decoded));
        else
            onFailure (token, loadKind, file);
        return jobHasFinished;
    }

private:
    juce::File file;
    double sampleRate = 44100.0;
    int token = 0;
    DysektProcessor::LoadKind loadKind = DysektProcessor::LoadKindReplace;
    SuccessFn onSuccess;
    FailureFn onFailure;
};

static constexpr uint32_t kValidLockMask =
    kLockBpm | kLockPitch | kLockAlgorithm | kLockAttack | kLockDecay | kLockSustain
    | kLockRelease | kLockMuteGroup | kLockStretch | kLockTonality | kLockFormant
    | kLockFormantComp | kLockGrainMode | kLockVolume | kLockReleaseTail | kLockReverse
    | kLockOutputBus | kLockLoop | kLockOneShot | kLockCentsDetune
    | kLockPan | kLockFilter;

static Slice sanitiseRestoredSlice (Slice s)
{
    s.startSample = juce::jmax (0, s.startSample);
    // Marker model: endSample derived from next marker — no field to sanitise.

    s.midiNote = juce::jlimit (0, 127, s.midiNote);
    s.bpm = juce::jlimit (20.0f, 999.0f, s.bpm);
    s.pitchSemitones = juce::jlimit (-48.0f, 48.0f, s.pitchSemitones);
    s.algorithm = juce::jlimit (0, 2, s.algorithm);
    s.attackSec = juce::jlimit (0.0f, 1.0f, s.attackSec);
    s.decaySec = juce::jlimit (0.0f, 5.0f, s.decaySec);
    s.sustainLevel = juce::jlimit (0.0f, 1.0f, s.sustainLevel);
    s.releaseSec = juce::jlimit (0.0f, 5.0f, s.releaseSec);
    s.muteGroup = juce::jlimit (0, 32, s.muteGroup);
    s.loopMode = juce::jlimit (0, 2, s.loopMode);
    s.tonalityHz = juce::jlimit (0.0f, 8000.0f, s.tonalityHz);
    s.formantSemitones = juce::jlimit (-24.0f, 24.0f, s.formantSemitones);
    s.grainMode = juce::jlimit (0, 2, s.grainMode);
    s.volume = juce::jlimit (-100.0f, 24.0f, s.volume);
    s.outputBus = juce::jlimit (0, 15, s.outputBus);
    s.centsDetune = juce::jlimit (-100.0f, 100.0f, s.centsDetune);
    s.pan         = juce::jlimit (-1.0f, 1.0f, s.pan);
    s.filterCutoff = juce::jlimit (20.0f, 20000.0f, s.filterCutoff);
    s.filterRes    = juce::jlimit (0.0f, 1.0f, s.filterRes);
    s.lockMask &= kValidLockMask;
    return s;
}

static bool isCoalescableCommand (DysektProcessor::CommandType type)
{
    return type == DysektProcessor::CmdSetSliceParam
        || type == DysektProcessor::CmdSetSliceBounds;
}

static bool isCriticalCommand (DysektProcessor::CommandType type)
{
    switch (type)
    {
        case DysektProcessor::CmdLoadFile:
        case DysektProcessor::CmdCreateSlice:
        case DysektProcessor::CmdDeleteSlice:
        case DysektProcessor::CmdDuplicateSlice:
        case DysektProcessor::CmdSplitSlice:
        case DysektProcessor::CmdTransientChop:
        case DysektProcessor::CmdRelinkFile:
        case DysektProcessor::CmdUndo:
        case DysektProcessor::CmdRedo:
        case DysektProcessor::CmdPanic:
        case DysektProcessor::CmdSelectSlice:
        case DysektProcessor::CmdSetRootNote:
            return true;
        default:
            return false;
    }
}
} // namespace

DysektProcessor::DysektProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Out 2", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 3", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 4", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 5", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 6", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 7", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 8", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 9", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 10", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 11", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 12", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 13", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 14", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 15", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out 16", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMETERS", ParamLayout::createLayout())
{
    masterVolParam = apvts.getRawParameterValue (ParamIds::masterVolume);
    bpmParam       = apvts.getRawParameterValue (ParamIds::defaultBpm);
    pitchParam     = apvts.getRawParameterValue (ParamIds::defaultPitch);
    algoParam      = apvts.getRawParameterValue (ParamIds::defaultAlgorithm);
    attackParam    = apvts.getRawParameterValue (ParamIds::defaultAttack);
    decayParam     = apvts.getRawParameterValue (ParamIds::defaultDecay);
    sustainParam   = apvts.getRawParameterValue (ParamIds::defaultSustain);
    releaseParam   = apvts.getRawParameterValue (ParamIds::defaultRelease);
    muteGroupParam = apvts.getRawParameterValue (ParamIds::defaultMuteGroup);
    stretchParam   = apvts.getRawParameterValue (ParamIds::defaultStretchEnabled);
    tonalityParam  = apvts.getRawParameterValue (ParamIds::defaultTonality);
    formantParam   = apvts.getRawParameterValue (ParamIds::defaultFormant);
    formantCompParam = apvts.getRawParameterValue (ParamIds::defaultFormantComp);
    grainModeParam   = apvts.getRawParameterValue (ParamIds::defaultGrainMode);
    releaseTailParam = apvts.getRawParameterValue (ParamIds::defaultReleaseTail);
    reverseParam     = apvts.getRawParameterValue (ParamIds::defaultReverse);
    loopParam        = apvts.getRawParameterValue (ParamIds::defaultLoop);
    oneShotParam     = apvts.getRawParameterValue (ParamIds::defaultOneShot);
    maxVoicesParam   = apvts.getRawParameterValue (ParamIds::maxVoices);
    centsDetuneParam  = apvts.getRawParameterValue (ParamIds::defaultCentsDetune);
    panParam          = apvts.getRawParameterValue (ParamIds::defaultPan);
    filterCutoffParam = apvts.getRawParameterValue (ParamIds::defaultFilterCutoff);
    filterResParam    = apvts.getRawParameterValue (ParamIds::defaultFilterRes);
    sliceStartParam  = apvts.getRawParameterValue (ParamIds::sliceStart);
    sliceEndParam    = apvts.getRawParameterValue (ParamIds::sliceEnd);
    publishUiSliceSnapshot();
}

DysektProcessor::~DysektProcessor()
{
    fileLoadPool.removeAllJobs (true, 5000);
    auto* pending = completedLoadData.exchange (nullptr, std::memory_order_acq_rel);
    delete pending;
    auto* failed = completedLoadFailure.exchange (nullptr, std::memory_order_acq_rel);
    delete failed;
}

bool DysektProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Main output must be stereo
    if (layouts.outputBuses.isEmpty())
        return false;
    if (layouts.outputBuses[0] != juce::AudioChannelSet::stereo())
        return false;

    // Additional outputs: stereo or disabled
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        if (! layouts.outputBuses[i].isDisabled()
            && layouts.outputBuses[i] != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void DysektProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    voicePool.setSampleRate (sampleRate);
    std::fill (std::begin (heldNotes), std::end (heldNotes), false);
}

void DysektProcessor::releaseResources() {}

void DysektProcessor::requestSampleLoad (const juce::File& file, LoadKind kind)
{
    const int token = nextLoadToken.fetch_add (1, std::memory_order_relaxed) + 1;
    latestLoadToken.store (token, std::memory_order_release);
    latestLoadKind.store ((int) kind, std::memory_order_release);

    // Keep only the latest completed decode payload.
    auto* oldDecoded = completedLoadData.exchange (nullptr, std::memory_order_acq_rel);
    delete oldDecoded;
    auto* oldFailure = completedLoadFailure.exchange (nullptr, std::memory_order_acq_rel);
    delete oldFailure;

    if (! file.existsAsFile())
    {
        if (kind == LoadKindRelink)
        {
            auto* payload = new FailedLoadResult();
            payload->token = token;
            payload->kind = kind;
            payload->file = file;
            auto* old = completedLoadFailure.exchange (payload, std::memory_order_acq_rel);
            delete old;
        }
        return;
    }

    const double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;

    auto onSuccess = [this] (int finishedToken, LoadKind finishedKind,
                             std::unique_ptr<SampleData::DecodedSample> decoded)
    {
        if (finishedToken != latestLoadToken.load (std::memory_order_acquire))
            return;

        auto* old = completedLoadData.exchange (decoded.release(), std::memory_order_acq_rel);
        delete old;
        latestLoadKind.store ((int) finishedKind, std::memory_order_release);
    };

    auto onFailure = [this] (int finishedToken, LoadKind finishedKind, const juce::File& failedFile)
    {
        if (finishedToken != latestLoadToken.load (std::memory_order_acquire))
            return;

        auto* payload = new FailedLoadResult();
        payload->token = finishedToken;
        payload->kind = finishedKind;
        payload->file = failedFile;
        auto* old = completedLoadFailure.exchange (payload, std::memory_order_acq_rel);
        delete old;
    };

    fileLoadPool.addJob (new SampleDecodeJob (file, sr, token, kind, onSuccess, onFailure), true);
}

void DysektProcessor::loadFileAsync (const juce::File& file)
{
    requestSampleLoad (file, LoadKindReplace);
}

// ─────────────────────────────────────────────────────────────────────────────
//  applyTrimToCurrentSample
//  Stores the trim region and dispatches CmdApplyTrim so the audio thread
//  can react (e.g. clamp slice boundaries, adjust waveform display start).
//  intParam1 = trimStart (samples), intParam2 = trimEnd (samples).
// ─────────────────────────────────────────────────────────────────────────────
void DysektProcessor::applyTrimToCurrentSample (int trimStart, int trimEnd)
{
    const int total = sampleData.getBuffer().getNumSamples();
    trimStart = juce::jlimit (0, juce::jmax (0, total - 1), trimStart);
    trimEnd   = juce::jlimit (trimStart + 1, total, trimEnd);

    trimRegionStart.store (trimStart, std::memory_order_relaxed);
    trimRegionEnd  .store (trimEnd,   std::memory_order_relaxed);

    Command c;
    c.type      = CmdApplyTrim;
    c.intParam1 = trimStart;
    c.intParam2 = trimEnd;
    pushCommand (c);
}

void DysektProcessor::loadSoundFontAsync (const juce::File& file)
{
#if DYSEKT_HAS_SFIZZ
    // Delegate to SoundFontLoader which uses sfizz to render all active notes
    // into a single stereo buffer and posts the result back via completedLoadData.
    SoundFontLoader loader (*this);
    loader.load (file);
#else
    // sfizz is not linked — SF2/SFZ files cannot be decoded.
    // Post a failure result so the UI shows the normal "failed to load" state
    // rather than silently doing nothing.
    const int token = nextLoadToken.fetch_add (1, std::memory_order_relaxed) + 1;
    latestLoadToken.store (token, std::memory_order_release);
    latestLoadKind.store  ((int) LoadKindReplace, std::memory_order_release);

    delete completedLoadData.exchange    (nullptr, std::memory_order_acq_rel);
    delete completedLoadFailure.exchange (nullptr, std::memory_order_acq_rel);

    auto* payload  = new FailedLoadResult();
    payload->token = token;
    payload->kind  = LoadKindReplace;
    payload->file  = file;
    delete completedLoadFailure.exchange (payload, std::memory_order_acq_rel);
#endif
}

void DysektProcessor::relinkFileAsync (const juce::File& file)
{
    requestSampleLoad (file, LoadKindRelink);
}

void DysektProcessor::clearVoicesBeforeSampleSwap()
{
    // Stop lazy chop before killing voices; its preview voice and buffer pointer
    // must be torn down before the sample data is replaced.
    lazyChop.stop (voicePool, sliceManager);

    // Kill all active voices before replacing the sample buffer
    // to prevent dangling reads from stretcher pipelines.
    for (int vi = 0; vi < VoicePool::kMaxVoices; ++vi)
    {
        auto& v = voicePool.getVoice (vi);
        v.active = false;
        voicePool.voicePositions[vi].store (0.0f,
            vi == VoicePool::kPreviewVoiceIndex
                ? std::memory_order_release
                : std::memory_order_relaxed);
    }
}

void DysektProcessor::clampSlicesToSampleBounds()
{
    const int maxLen = sampleData.getNumFrames();
    if (maxLen <= 1)
        return;

    const int numSlices = sliceManager.getNumSlices();
    for (int i = 0; i < numSlices; ++i)
    {
        auto& s = sliceManager.getSlice (i);
        s.startSample = juce::jlimit (0, maxLen - 1, s.startSample);
        // Marker model: endSample derived from next marker — no field to clamp.
    }
}

void DysektProcessor::publishUiSliceSnapshot()
{
    const int writeIndex = 1 - uiSliceSnapshotIndex.load (std::memory_order_relaxed);
    auto& snap = uiSliceSnapshots[(size_t) writeIndex];
    auto sampleSnap = sampleData.getSnapshot();
    snap.numSlices = sliceManager.getNumSlices();
    snap.selectedSlice = sliceManager.selectedSlice.load (std::memory_order_relaxed);
    snap.rootNote = sliceManager.rootNote.load (std::memory_order_relaxed);
    snap.sampleLoaded = (sampleSnap != nullptr);
    snap.sampleMissing = sampleMissing.load (std::memory_order_relaxed);
    snap.sampleNumFrames = sampleSnap ? sampleSnap->buffer.getNumSamples() : 0;
    if (sampleSnap != nullptr)
        snap.sampleFileName = sampleSnap->fileName;
    else if (snap.sampleMissing && missingFilePath.isNotEmpty())
        snap.sampleFileName = juce::File (missingFilePath).getFileName();
    else if (sampleData.getFileName().isNotEmpty())
        snap.sampleFileName = sampleData.getFileName();
    else
        snap.sampleFileName.clear();

    for (int i = 0; i < SliceManager::kMaxSlices; ++i)
    {
        if (i < snap.numSlices)
            snap.slices[(size_t) i] = sliceManager.getSlice (i);
        else
            snap.slices[(size_t) i].active = false;
    }

    uiSliceSnapshotIndex.store (writeIndex, std::memory_order_release);
    uiSnapshotVersion.fetch_add (1, std::memory_order_release);
    uiSnapshotDirty.store (false, std::memory_order_release);

    // Keep sliceStart / sliceEnd APVTS params in sync with the selected slice
    // so hosts can map them to Quick Controls and MIDI CC.
    if (sliceStartParam != nullptr && sliceEndParam != nullptr)
    {
        const int sel     = snap.selectedSlice;
        const int total   = snap.sampleNumFrames;
        if (sel >= 0 && sel < snap.numSlices && total > 0)
        {
            const auto& sl = snap.slices[(size_t) sel];
            sliceStartParam->store ((float) sl.startSample / (float) total,
                                    std::memory_order_relaxed);
            sliceEndParam->store   ((float) sliceManager.getEndForSlice (sel, total) / (float) total,
                                    std::memory_order_relaxed);
            paramsSyncedForSlice.store (sel, std::memory_order_relaxed);
        }
    }
}

void DysektProcessor::pushCommand (Command cmd)
{
    const bool critical = isCriticalCommand (cmd.type);
    const auto scope = commandFifo.write (1);
    if (scope.blockSize1 > 0)
    {
        commandBuffer[(size_t) scope.startIndex1] = std::move (cmd);
        uiSnapshotDirty.store (true, std::memory_order_release);
        return;
    }
    if (scope.blockSize2 > 0)
    {
        commandBuffer[(size_t) scope.startIndex2] = std::move (cmd);
        uiSnapshotDirty.store (true, std::memory_order_release);
        return;
    }

    if (enqueueCoalescedCommand (cmd))
    {
        uiSnapshotDirty.store (true, std::memory_order_release);
        return;
    }

    if (critical && enqueueOverflowCommand (std::move (cmd)))
    {
        uiSnapshotDirty.store (true, std::memory_order_release);
        return;
    }

    droppedCommandCount.fetch_add (1, std::memory_order_relaxed);
    droppedCommandTotal.fetch_add (1, std::memory_order_relaxed);
    if (critical)
        droppedCriticalCommandTotal.fetch_add (1, std::memory_order_relaxed);
}

bool DysektProcessor::enqueueOverflowCommand (Command cmd)
{
    const int write = overflowWriteIndex.load (std::memory_order_relaxed);
    const int read = overflowReadIndex.load (std::memory_order_acquire);
    const int next = (write + 1) % kOverflowFifoSize;
    if (next == read)
        return false;

    overflowCommandBuffer[(size_t) write] = std::move (cmd);
    overflowWriteIndex.store (next, std::memory_order_release);
    return true;
}

void DysektProcessor::drainOverflowCommands (bool& handledAny)
{
    for (;;)
    {
        const int read = overflowReadIndex.load (std::memory_order_relaxed);
        const int write = overflowWriteIndex.load (std::memory_order_acquire);
        if (read == write)
            break;

        handleCommand (overflowCommandBuffer[(size_t) read]);
        overflowReadIndex.store ((read + 1) % kOverflowFifoSize, std::memory_order_release);
        handledAny = true;
    }
}

bool DysektProcessor::enqueueCoalescedCommand (const Command& cmd)
{
    if (! isCoalescableCommand (cmd.type))
        return false;

    if (cmd.type == CmdSetSliceParam)
    {
        pendingSetSliceParamField.store (cmd.intParam1, std::memory_order_relaxed);
        pendingSetSliceParamValue.store (cmd.floatParam1, std::memory_order_relaxed);
        pendingSetSliceParam.store (true, std::memory_order_release);
        return true;
    }

    if (cmd.type == CmdSetSliceBounds)
    {
        const int end = cmd.numPositions > 0 ? cmd.positions[0] : (int) cmd.floatParam1;
        pendingSetSliceBoundsIdx.store (cmd.intParam1, std::memory_order_relaxed);
        pendingSetSliceBoundsStart.store (cmd.intParam2, std::memory_order_relaxed);
        pendingSetSliceBoundsEnd.store (end, std::memory_order_relaxed);
        pendingSetSliceBounds.store (true, std::memory_order_release);
        return true;
    }

    return false;
}

void DysektProcessor::drainCoalescedCommands (bool& handledAny)
{
    if (pendingSetSliceBounds.exchange (false, std::memory_order_acq_rel))
    {
        Command cmd;
        cmd.type = CmdSetSliceBounds;
        cmd.intParam1 = pendingSetSliceBoundsIdx.load (std::memory_order_relaxed);
        cmd.intParam2 = pendingSetSliceBoundsStart.load (std::memory_order_relaxed);
        cmd.positions[0] = pendingSetSliceBoundsEnd.load (std::memory_order_relaxed);
        cmd.numPositions = 1;
        handleCommand (cmd);
        handledAny = true;
    }

    if (pendingSetSliceParam.exchange (false, std::memory_order_acq_rel))
    {
        Command cmd;
        cmd.type = CmdSetSliceParam;
        cmd.intParam1 = pendingSetSliceParamField.load (std::memory_order_relaxed);
        cmd.floatParam1 = pendingSetSliceParamValue.load (std::memory_order_relaxed);
        handleCommand (cmd);
        handledAny = true;
    }
}

void DysektProcessor::drainCommands()
{
    bool handledAny = false;

    drainOverflowCommands (handledAny);

    const auto scope = commandFifo.read (commandFifo.getNumReady());

    for (int i = 0; i < scope.blockSize1; ++i)
        handleCommand (commandBuffer[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i)
        handleCommand (commandBuffer[(size_t) (scope.startIndex2 + i)]);

    if (scope.blockSize1 + scope.blockSize2 > 0)
        handledAny = true;

    drainCoalescedCommands (handledAny);

    if (handledAny)
        uiSnapshotDirty.store (true, std::memory_order_release);

    const auto dropped = droppedCommandCount.exchange (0, std::memory_order_relaxed);
    if (handledAny || dropped > 0)
        updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));

    // Apply live drag bounds every block so note-ons during edge/move drag use
    // the current preview position. No snapshot — undo is handled by the
    // CmdBeginGesture + CmdSetSliceBounds pair sent on mouseDown/mouseUp.
    const int liveIdx = liveDragSliceIdx.load (std::memory_order_acquire);
    if (liveIdx >= 0 && liveIdx < sliceManager.getNumSlices())
    {
        const int maxLen = sampleData.getNumFrames();
        if (maxLen > 1)
        {
            auto& s = sliceManager.getSlice (liveIdx);
            int start = liveDragBoundsStart.load (std::memory_order_relaxed);
            int end   = liveDragBoundsEnd.load   (std::memory_order_relaxed);
            start = juce::jlimit (0, juce::jmax (0, maxLen - 1), start);
            end   = juce::jlimit (start + 1, juce::jmax (start + 1, maxLen), end);
            if (end - start < 64)
                end = juce::jmin (maxLen, start + 64);
            s.startSample = start;
            // Marker model: move next slice's start to represent this slice's new end.
            if (liveIdx + 1 < sliceManager.getNumSlices())
                sliceManager.getSlice (liveIdx + 1).startSample = end;
        }
    }
}

UndoManager::Snapshot DysektProcessor::makeSnapshot()
{
    UndoManager::Snapshot snap;
    for (int i = 0; i < SliceManager::kMaxSlices; ++i)
        snap.slices[(size_t) i] = sliceManager.getSlice (i);
    snap.numSlices = sliceManager.getNumSlices();
    snap.selectedSlice = sliceManager.selectedSlice;
    snap.rootNote = sliceManager.rootNote.load();
    snap.apvtsState = apvts.copyState();
    snap.midiSelectsSlice = midiSelectsSlice.load();
    snap.snapToZeroCrossing = snapToZeroCrossing.load();
    return snap;
}

void DysektProcessor::captureSnapshot()
{
    undoMgr.push (makeSnapshot());
}

void DysektProcessor::restoreSnapshot (const UndoManager::Snapshot& snap)
{
    for (int i = 0; i < SliceManager::kMaxSlices; ++i)
        sliceManager.getSlice (i) = snap.slices[(size_t) i];
    sliceManager.setNumSlices (snap.numSlices);
    sliceManager.selectedSlice = snap.selectedSlice;
    sliceManager.rootNote.store (snap.rootNote);
    apvts.replaceState (snap.apvtsState);
    midiSelectsSlice.store (snap.midiSelectsSlice);
    snapToZeroCrossing.store (snap.snapToZeroCrossing);
    sliceManager.rebuildMidiMap();
    uiSnapshotDirty.store (true, std::memory_order_release);
}

void DysektProcessor::handleCommand (const Command& cmd)
{
    switch (cmd.type)
    {
        case CmdBeginGesture:
            if (! gestureSnapshotCaptured)
                captureSnapshot();
            gestureSnapshotCaptured = true;
            blocksSinceGestureActivity = 0;
            break;

        case CmdSetSliceParam:
            if (! gestureSnapshotCaptured)
                captureSnapshot();
            gestureSnapshotCaptured = true;
            blocksSinceGestureActivity = 0;
            break;

        case CmdSetSliceBounds:
        case CmdCreateSlice:
        case CmdDeleteSlice:
        case CmdStretch:
        case CmdToggleLock:
        case CmdDuplicateSlice:
        case CmdSplitSlice:
        case CmdTransientChop:
            if (! gestureSnapshotCaptured)
                captureSnapshot();
            gestureSnapshotCaptured = false;
            blocksSinceGestureActivity = 0;
            break;

        default:
            // Leave param gesture mode after idle/non-param commands.
            if (cmd.type != CmdSetSliceParam)
                gestureSnapshotCaptured = false;
            break;
    }

    switch (cmd.type)
    {
        case CmdLoadFile:
            loadFileAsync (cmd.fileParam);
            break;

        case CmdCreateSlice:
            sliceManager.createSlice (cmd.intParam1, cmd.intParam2);
            break;

        case CmdDeleteSlice:
            sliceManager.deleteSlice (cmd.intParam1);
            break;

        case CmdLazyChopStart:
            if (sampleData.isLoaded())
            {
                PreviewStretchParams psp;
                psp.stretchEnabled = stretchParam->load() > 0.5f;
                psp.algorithm      = (int) algoParam->load();
                psp.bpm            = bpmParam->load();
                psp.pitch          = pitchParam->load();
                psp.dawBpm         = dawBpm.load();
                psp.tonality       = tonalityParam->load();
                psp.formant        = formantParam->load();
                psp.formantComp    = formantCompParam->load() > 0.5f;
                psp.grainMode      = (int) grainModeParam->load();
                psp.sampleRate     = currentSampleRate;
                psp.sample         = &sampleData;
                lazyChop.start (sampleData.getNumFrames(), sliceManager, psp,
                                true /*snap always on*/, &sampleData.getBuffer());
            }
            break;

        case CmdLazyChopStop:
            lazyChop.stop (voicePool, sliceManager);
            break;

        case CmdStretch:
        {
            int sel = sliceManager.selectedSlice;
            if (sel >= 0 && sel < sliceManager.getNumSlices())
            {
                auto& s = sliceManager.getSlice (sel);
                const int stretchSliceEnd = sliceManager.getEndForSlice (sel, sampleData.getBuffer().getNumSamples());
                float newBpm = GrainEngine::calcStretchBpm (
                    s.startSample, stretchSliceEnd, cmd.floatParam1, currentSampleRate);
                s.bpm = newBpm;
                s.lockMask |= kLockBpm;
                s.algorithm = 1;
                s.lockMask |= kLockAlgorithm;
            }
            break;
        }

        case CmdToggleLock:
        {
            int sel = sliceManager.selectedSlice;
            if (sel >= 0 && sel < sliceManager.getNumSlices())
            {
                auto& s = sliceManager.getSlice (sel);
                uint32_t bit = (uint32_t) cmd.intParam1;
                bool turningOn = !(s.lockMask & bit);

                if (turningOn)
                {
                    // Snapshot the current effective (global) value into the slice field
                    // so the locked value matches what was displayed before locking.
                    if      (bit == kLockBpm)         s.bpm               = bpmParam->load();
                    else if (bit == kLockPitch)        s.pitchSemitones    = pitchParam->load();
                    else if (bit == kLockAlgorithm)    s.algorithm         = (int) algoParam->load();
                    else if (bit == kLockAttack)       s.attackSec         = attackParam->load() / 1000.0f;
                    else if (bit == kLockDecay)        s.decaySec          = decayParam->load() / 1000.0f;
                    else if (bit == kLockSustain)      s.sustainLevel      = sustainParam->load() / 100.0f;
                    else if (bit == kLockRelease)      s.releaseSec        = releaseParam->load() / 1000.0f;
                    else if (bit == kLockMuteGroup)    s.muteGroup         = (int) muteGroupParam->load();
                    else if (bit == kLockLoop)         s.loopMode          = (int) loopParam->load();
                    else if (bit == kLockStretch)      s.stretchEnabled    = stretchParam->load()     > 0.5f;
                    else if (bit == kLockReleaseTail)  s.releaseTail       = releaseTailParam->load() > 0.5f;
                    else if (bit == kLockReverse)      s.reverse           = reverseParam->load()     > 0.5f;
                    else if (bit == kLockOneShot)      s.oneShot           = oneShotParam->load()     > 0.5f;
                    else if (bit == kLockCentsDetune)  s.centsDetune       = centsDetuneParam->load();
                    else if (bit == kLockTonality)     s.tonalityHz        = tonalityParam->load();
                    else if (bit == kLockFormant)      s.formantSemitones  = formantParam->load();
                    else if (bit == kLockFormantComp)  s.formantComp       = formantCompParam->load() > 0.5f;
                    else if (bit == kLockGrainMode)    s.grainMode         = (int) grainModeParam->load();
                    else if (bit == kLockVolume)       s.volume            = masterVolParam->load();
                    else if (bit == kLockPan)          s.pan               = panParam->load();
                    else if (bit == kLockFilter)       { s.filterCutoff    = filterCutoffParam->load();
                                                         s.filterRes       = filterResParam->load(); }
                    // kLockOutputBus: no global default param — slice default (0) is correct
                }

                s.lockMask ^= bit;
            }
            break;
        }

        case CmdSetSliceParam:
        {
            int sel = sliceManager.selectedSlice;
            if (sel >= 0 && sel < sliceManager.getNumSlices())
            {
                auto& s = sliceManager.getSlice (sel);
                int field = cmd.intParam1;
                float val = cmd.floatParam1;

                switch (field)
                {
                    case FieldBpm:       s.bpm = val;            s.lockMask |= kLockBpm;       break;
                    case FieldPitch:     s.pitchSemitones = val; s.lockMask |= kLockPitch;     break;
                    case FieldAlgorithm: s.algorithm = (int) val; s.lockMask |= kLockAlgorithm; break;
                    case FieldAttack:    s.attackSec = val;      s.lockMask |= kLockAttack;    break;
                    case FieldDecay:     s.decaySec = val;       s.lockMask |= kLockDecay;     break;
                    case FieldSustain:   s.sustainLevel = val;   s.lockMask |= kLockSustain;   break;
                    case FieldRelease:   s.releaseSec = val;     s.lockMask |= kLockRelease;   break;
                    case FieldMuteGroup: s.muteGroup = (int) val; s.lockMask |= kLockMuteGroup; break;
                    case FieldStretchEnabled: s.stretchEnabled = val > 0.5f; s.lockMask |= kLockStretch; break;
                    case FieldTonality:  s.tonalityHz = val;        s.lockMask |= kLockTonality;    break;
                    case FieldFormant:   s.formantSemitones = val;   s.lockMask |= kLockFormant;     break;
                    case FieldFormantComp: s.formantComp = val > 0.5f; s.lockMask |= kLockFormantComp; break;
                    case FieldGrainMode:  s.grainMode = (int) val;   s.lockMask |= kLockGrainMode;  break;
                    case FieldVolume:     s.volume = val;            s.lockMask |= kLockVolume;    break;
                    case FieldReleaseTail: s.releaseTail = val > 0.5f; s.lockMask |= kLockReleaseTail; break;
                    case FieldReverse:    s.reverse = val > 0.5f;    s.lockMask |= kLockReverse;    break;
                    case FieldOutputBus:  s.outputBus = juce::jlimit (0, 15, (int) val); s.lockMask |= kLockOutputBus; break;
                    case FieldLoop:       s.loopMode = (int) val;    s.lockMask |= kLockLoop;      break;
                    case FieldOneShot:    s.oneShot = val > 0.5f;    s.lockMask |= kLockOneShot;   break;
                    case FieldCentsDetune:   s.centsDetune    = val;       s.lockMask |= kLockCentsDetune; break;
                    case FieldPan:           s.pan            = val;       s.lockMask |= kLockPan;         break;
                    case FieldFilterCutoff:  s.filterCutoff   = val;       s.lockMask |= kLockFilter;      break;
                    case FieldFilterRes:     s.filterRes      = val;       s.lockMask |= kLockFilter;      break;
                    case FieldMidiNote:
                        s.midiNote = juce::jlimit (0, 127, (int) val);
                        sliceManager.rebuildMidiMap();
                        break;

                }
            }
            break;
        }

        case CmdSetSliceBounds:
        {
            int idx = cmd.intParam1;
            if (idx >= 0 && idx < sliceManager.getNumSlices())
            {
                const int maxLen = sampleData.getNumFrames();
                if (maxLen <= 1)
                    break;

                auto& s = sliceManager.getSlice (idx);
                int requestedEnd = (cmd.numPositions > 0) ? cmd.positions[0] : (int) cmd.floatParam1;
                int start = juce::jmin (cmd.intParam2, requestedEnd);
                int end = juce::jmax (cmd.intParam2, requestedEnd);
                start = juce::jlimit (0, juce::jmax (0, maxLen - 1), start);
                end = juce::jlimit (start + 1, juce::jmax (start + 1, maxLen), end);
                if (end - start < 64)
                    end = juce::jmin (maxLen, start + 64);
                const int totalF = sampleData.getBuffer().getNumSamples();
                int oldEnd = sliceManager.getEndForSlice (idx, totalF);
                s.startSample = start;
                // Marker model: end boundary = next slice's start.
                if (idx + 1 < sliceManager.getNumSlices())
                    sliceManager.getSlice (idx + 1).startSample = end;

                // Link mode: if end moved, push the adjacent slice's start
                if (slicesLinked.load (std::memory_order_relaxed) && end != oldEnd)
                {
                    // Find the slice whose startSample was closest to oldEnd
                    int bestNi = -1, bestDist = 256;
                    const int numSl = sliceManager.getNumSlices();
                    for (int ni = 0; ni < numSl; ++ni)
                    {
                        if (ni == idx) continue;
                        auto& ns = sliceManager.getSlice (ni);
                        if (! ns.active) continue;
                        int dist = std::abs (ns.startSample - oldEnd);
                        if (dist < bestDist) { bestDist = dist; bestNi = ni; }
                    }
                    if (bestNi >= 0)
                    {
                        auto& ns = sliceManager.getSlice (bestNi);
                        // Only push if the neighbour would remain >= 64 samples wide
                        const int nsEnd = sliceManager.getEndForSlice (bestNi, totalF);
                        if (nsEnd - end >= 64)
                            ns.startSample = end;
                    }
                }

                sliceManager.rebuildMidiMap();
            }
            break;
        }

        case CmdDuplicateSlice:
        {
            int sel = sliceManager.selectedSlice;
            if (sel >= 0 && sel < sliceManager.getNumSlices())
            {
                const auto& src = sliceManager.getSlice (sel);
                const int srcEnd = sliceManager.getEndForSlice (sel, sampleData.getBuffer().getNumSamples());
                int newIdx = sliceManager.createSlice (src.startSample, srcEnd);
                if (newIdx >= 0)
                {
                    auto& dst = sliceManager.getSlice (newIdx);
                    int savedNote = dst.midiNote;  // assigned by createSlice
                    dst = src;                     // copy all params, lockMask, colour
                    dst.midiNote = savedNote;      // restore unique MIDI note
                    if (cmd.intParam1 >= 0)        // ctrl-drag: use explicit position
                    {
                        dst.startSample = cmd.intParam1;
                        // Marker model: end is derived; intParam2 (requested end) ignored.
                    }
                    // else (intParam1 == -1): inherit src.startSample as-is
                    sliceManager.selectedSlice = newIdx;
                }
            }
            break;
        }

        case CmdSplitSlice:
        {
            int sel = sliceManager.selectedSlice;
            if (sel >= 0 && sel < sliceManager.getNumSlices())
            {
                Slice srcCopy = sliceManager.getSlice (sel);
                int startS = srcCopy.startSample;
                int endS   = sliceManager.getEndForSlice (sel, sampleData.getBuffer().getNumSamples());
                int count = juce::jlimit (2, 128, cmd.intParam1);
                int len = endS - startS;

                sliceManager.deleteSlice (sel);

                bool doSnap = sampleData.isLoaded(); // snap always on
                int firstNew = -1;
                for (int i = 0; i < count; ++i)
                {
                    int s = startS + i * len / count;
                    int e = startS + (i + 1) * len / count;
                    if (doSnap)
                    {
                        if (i > 0)
                            s = AudioAnalysis::findNearestZeroCrossing (sampleData.getBuffer(), s);
                        if (i < count - 1)
                            e = AudioAnalysis::findNearestZeroCrossing (sampleData.getBuffer(), e);
                    }
                    if (e - s < 64) e = s + 64;
                    int idx = sliceManager.createSlice (s, e);
                    if (idx >= 0)
                    {
                        auto& dst = sliceManager.getSlice (idx);
                        int savedNote      = dst.midiNote;  // assigned by createSlice
                        juce::Colour savedColour = dst.colour;  // assigned from palette
                        dst = srcCopy;         // copy all params + lockMask
                        dst.startSample = s;
                        // Marker model: end derived from next marker — no write needed.
                        dst.midiNote    = savedNote;
                        dst.colour      = savedColour;
                        dst.active      = true;
                    }
                    if (i == 0) firstNew = idx;
                }

                sliceManager.rebuildMidiMap();
                if (firstNew >= 0)
                    sliceManager.selectedSlice = firstNew;
            }
            break;
        }

        case CmdTransientChop:
        {
            int sel = sliceManager.selectedSlice;
            if (sel >= 0 && sel < sliceManager.getNumSlices() && cmd.numPositions > 0)
            {
                Slice srcCopy = sliceManager.getSlice (sel);
                int startS = srcCopy.startSample;
                int endS   = sliceManager.getEndForSlice (sel, sampleData.getBuffer().getNumSamples());

                sliceManager.deleteSlice (sel);

                // Build fixed-size boundary list: [startS, ...positions..., endS]
                int bounds[SliceManager::kMaxSlices + 2];
                int numBounds = 0;
                bounds[numBounds++] = startS;
                for (int bi = 0; bi < cmd.numPositions; ++bi)
                    bounds[numBounds++] = cmd.positions[(size_t) bi];
                bounds[numBounds++] = endS;

                int firstNew = -1;
                for (int i = 0; i + 1 < numBounds; ++i)
                {
                    int s = bounds[i];
                    int e = bounds[i + 1];
                    if (e - s < 64) continue;
                    int idx = sliceManager.createSlice (s, e);
                    if (idx >= 0)
                    {
                        auto& dst = sliceManager.getSlice (idx);
                        int savedNote        = dst.midiNote;
                        juce::Colour savedColour = dst.colour;
                        dst = srcCopy;
                        dst.startSample = s;
                        // Marker model: end derived from next marker — no write needed.
                        dst.midiNote    = savedNote;
                        dst.colour      = savedColour;
                        dst.active      = true;
                    }
                    if (firstNew < 0) firstNew = idx;
                }

                sliceManager.rebuildMidiMap();
                if (firstNew >= 0)
                    sliceManager.selectedSlice = firstNew;
            }
            break;
        }

        case CmdRelinkFile:
            relinkFileAsync (cmd.fileParam);
            break;

        case CmdFileLoadFailed:
            if (cmd.intParam1 == latestLoadToken.load (std::memory_order_acquire)
                && cmd.intParam2 == (int) LoadKindRelink)
            {
                sampleMissing.store (true);
                missingFilePath = cmd.fileParam.getFullPathName();
                sampleData.setFileName (cmd.fileParam.getFileName());
                sampleData.setFilePath (cmd.fileParam.getFullPathName());
                sampleAvailability.store ((int) SampleStateMissingAwaitingRelink,
                                         std::memory_order_relaxed);
            }
            break;

        case CmdUndo:
            if (undoMgr.canUndo())
                restoreSnapshot (undoMgr.undo (makeSnapshot()));
            break;

        case CmdRedo:
            if (undoMgr.canRedo())
                restoreSnapshot (undoMgr.redo());
            break;

        case CmdBeginGesture:
            break;

        case CmdPanic:
            voicePool.killAll();
            lazyChop.stop (voicePool, sliceManager);
            std::fill (std::begin (heldNotes), std::end (heldNotes), false);
            break;

        case CmdSelectSlice:
            sliceManager.selectedSlice.store (
                juce::jlimit (-1, juce::jmax (-1, sliceManager.getNumSlices() - 1), cmd.intParam1),
                std::memory_order_relaxed);
            break;

        case CmdSetRootNote:
            sliceManager.rootNote.store (juce::jlimit (0, 127, cmd.intParam1),
                                         std::memory_order_relaxed);
            break;

        case CmdApplyTrim:
            // Clamp all slice start/end markers to the new trim region so no
            // slice can reference audio outside the trimmed bounds.
            {
                const int tStart = cmd.intParam1;
                const int tEnd   = cmd.intParam2;
                const int n      = sliceManager.getNumSlices();
                for (int i = 0; i < n; ++i)
                {
                    auto& sl = sliceManager.getSlice (i);
                    sl.startSample = juce::jlimit (tStart, tEnd - 1, sl.startSample);
                }
                sliceManager.sortByStart();
            }
            publishUiSliceSnapshot();
            break;

        case CmdNone:
            break;
    }
}

void DysektProcessor::processMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        // ── MIDI Learn CC dispatch ────────────────────────────────────
        if (msg.isController())
        {
            int   outFieldId = -1;
            float outNorm    = 0.0f;
            if (midiLearn.processCc (msg.getControllerNumber(),
                                     msg.getControllerValue(),
                                     outFieldId, outNorm))
            {
                const int sel = sliceManager.selectedSlice.load (std::memory_order_relaxed);
                if (sel >= 0 && sel < sliceManager.getNumSlices())
                {
                    // Convert normalised 0-1 CC value to the native range for each field
                    float nativeVal = outNorm;
                    switch (outFieldId)
                    {
                        case FieldBpm:         nativeVal = 20.0f + outNorm * (999.0f - 20.0f); break;
                        case FieldPitch:       nativeVal = -24.0f + outNorm * 48.0f;           break;
                        case FieldCentsDetune: nativeVal = -100.0f + outNorm * 200.0f;           break;
                        case FieldPan:         nativeVal = -1.0f + outNorm * 2.0f;               break;
                        case FieldFilterCutoff: nativeVal = 20.0f + outNorm * (20000.0f - 20.0f); break;
                        case FieldFilterRes:   nativeVal = outNorm;                               break;
                        case FieldTonality:    nativeVal = outNorm * 8000.0f;                  break;
                        case FieldFormant:     nativeVal = -24.0f + outNorm * 48.0f;           break;
                        case FieldAttack:      nativeVal = outNorm * 1.0f;                     break;
                        case FieldDecay:       nativeVal = outNorm * 5.0f;                     break;
                        case FieldSustain:     nativeVal = outNorm;                            break;
                        case FieldRelease:     nativeVal = outNorm * 5.0f;                     break;
                        case FieldMuteGroup:   nativeVal = std::round (outNorm * 32.0f);       break;
                        case FieldMidiNote:    nativeVal = std::round (outNorm * 127.0f);      break;
                        case FieldVolume:      nativeVal = -100.0f + outNorm * 124.0f;         break;
                        case FieldOutputBus:   nativeVal = std::round (outNorm * 15.0f);       break;
                        case FieldAlgorithm:   nativeVal = std::round (outNorm * 2.0f);        break;
                        case FieldLoop:        nativeVal = std::round (outNorm * 2.0f);        break;
                        case FieldGrainMode:   nativeVal = std::round (outNorm * 2.0f);        break;
                        default: break;
                    }

                    // Slice bounds go through CmdSetSliceBounds (correct undo + Link support)
                    if (outFieldId == FieldSliceStart || outFieldId == FieldSliceEnd)
                    {
                        const int total = sampleData.getNumFrames();
                        if (total > 1)
                        {
                            auto& sl = sliceManager.getSlice (sel);
                            Command ccCmd;
                            ccCmd.type = CmdSetSliceBounds;
                            ccCmd.intParam1 = sel;
                            if (outFieldId == FieldSliceStart)
                            {
                                const int slEnd = sliceManager.getEndForSlice (sel, total);
                                int newStart = juce::jlimit (0, slEnd - 64,
                                    (int) (outNorm * (float) total));
                                ccCmd.intParam2    = newStart;
                                ccCmd.positions[0] = slEnd;
                            }
                            else
                            {
                                int newEnd = juce::jlimit (sl.startSample + 64, total,
                                    (int) (outNorm * (float) total));
                                ccCmd.intParam2    = sl.startSample;
                                ccCmd.positions[0] = newEnd;
                            }
                            ccCmd.numPositions = 1;
                            // Route through the coalescing queue — NOT handleCommand directly.
                            // handleCommand calls rebuildMidiMap() which allocates on the audio
                            // thread and causes freezes / erratic jumps.
                            enqueueCoalescedCommand (ccCmd);
                        }
                    }
                    else
                    {
                        Command ccCmd;
                        ccCmd.type        = CmdSetSliceParam;
                        ccCmd.intParam1   = outFieldId;
                        ccCmd.floatParam1 = nativeVal;
                        handleCommand (ccCmd);
                    }
                    uiSnapshotDirty.store (true, std::memory_order_release);
                }
            }
        }

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            float velocity = (float) msg.getVelocity();

            if (lazyChop.isActive())
            {
                // Any MIDI note places a chop boundary at the playhead
                int newSliceIdx = lazyChop.onNote (note, voicePool, sliceManager);
                if (newSliceIdx >= 0)
                {
                    uiSnapshotDirty.store (true, std::memory_order_release);
                    if (midiSelectsSlice.load (std::memory_order_relaxed))
                        sliceManager.selectedSlice.store (newSliceIdx, std::memory_order_relaxed);
                }
            }
            else
            {
                heldNotes[note] = true;

                // Build params once; all param loads happen here, not inside the slice loop.
                VoiceStartParams p;
                p.note             = note;
                p.velocity         = velocity;
                p.globalBpm        = bpmParam->load();
                p.globalPitch      = pitchParam->load();
                p.globalAlgorithm  = (int) algoParam->load();
                p.globalAttackSec  = attackParam->load()  / 1000.0f;
                p.globalDecaySec   = decayParam->load()   / 1000.0f;
                p.globalSustain    = sustainParam->load() / 100.0f;
                p.globalReleaseSec = releaseParam->load() / 1000.0f;
                p.globalMuteGroup  = (int) muteGroupParam->load();
                p.globalStretch    = stretchParam->load()      > 0.5f;
                p.dawBpm           = dawBpm.load();
                p.globalTonality   = tonalityParam->load();
                p.globalFormant    = formantParam->load();
                p.globalFormantComp = formantCompParam->load() > 0.5f;
                p.globalGrainMode  = (int) grainModeParam->load();
                p.globalVolume     = masterVolParam->load();
                p.globalReleaseTail = releaseTailParam->load() > 0.5f;
                p.globalReverse    = reverseParam->load()      > 0.5f;
                p.globalLoopMode   = (int) loopParam->load();
                p.globalOneShot    = oneShotParam->load()      > 0.5f;
                p.globalCentsDetune  = centsDetuneParam->load();
                p.globalPan          = panParam->load();
                p.globalFilterCutoff = filterCutoffParam->load();
                p.globalFilterRes    = filterResParam->load();

                // ── Chromatic mode ─────────────────────────────────────────────
                // Any note plays the selected slice, pitched relative to root note.
                if (chromaticMode.load (std::memory_order_relaxed))
                {
                    const int sel = sliceManager.selectedSlice.load (std::memory_order_relaxed);
                    if (sel >= 0 && sel < sliceManager.getNumSlices())
                    {
                        const int root = sliceManager.rootNote.load (std::memory_order_relaxed);
                        const float semitoneOffset = (float) (note - root);

                        int voiceIdx = voicePool.allocate();

                        const auto& s = sliceManager.getSlice (sel);
                        int mg = (int) sliceManager.resolveParam (sel, kLockMuteGroup,
                                                                   (float) s.muteGroup,
                                                                   (float) p.globalMuteGroup);
                        voicePool.muteGroup (mg, voiceIdx);

                        p.sliceIdx    = sel;
                        const float savedGlobalPitch = p.globalPitch;
                        p.globalPitch = savedGlobalPitch + semitoneOffset;
                        voicePool.startVoice (voiceIdx, p, sliceManager, sampleData);
                        p.globalPitch = savedGlobalPitch;

                        if (midiSelectsSlice.load (std::memory_order_relaxed))
                        {
                            sliceManager.selectedSlice.store (sel, std::memory_order_relaxed);
                            uiSnapshotDirty.store (true, std::memory_order_release);
                        }
                    }
                }
                else
                {
                    // ── Standard: one slice per note ──────────────────────────
                    const int sliceIdx = sliceManager.midiNoteToSlice (note);
                    if (sliceIdx >= 0)
                    {
                        if (midiSelectsSlice.load (std::memory_order_relaxed))
                        {
                            const int previous = sliceManager.selectedSlice.load (std::memory_order_relaxed);
                            sliceManager.selectedSlice.store (sliceIdx, std::memory_order_relaxed);
                            if (previous != sliceIdx)
                                uiSnapshotDirty.store (true, std::memory_order_release);
                        }

                        int voiceIdx = voicePool.allocate();
                        const auto& s = sliceManager.getSlice (sliceIdx);
                        int mg = (int) sliceManager.resolveParam (sliceIdx, kLockMuteGroup,
                                                                  (float) s.muteGroup, (float) p.globalMuteGroup);
                        voicePool.muteGroup (mg, voiceIdx);
                        p.sliceIdx = sliceIdx;
                        voicePool.startVoice (voiceIdx, p, sliceManager, sampleData);
                    }
                }
            }
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            if (heldNotes[note])
            {
                heldNotes[note] = false;
                voicePool.releaseNote (note);           // normal: respects oneShot
            }
            else
            {
                voicePool.releaseNoteForced (note);     // host sweep: kills even oneShot voices
            }
        }
        else if (msg.isAllNotesOff())
        {
            voicePool.releaseAll();  // 50ms fade on all active voices
            lazyChop.stop (voicePool, sliceManager);
            std::fill (std::begin (heldNotes), std::end (heldNotes), false);
        }
        else if (msg.isAllSoundOff())
        {
            voicePool.killAll();     // 5ms hard kill on all active voices
            lazyChop.stop (voicePool, sliceManager);
            std::fill (std::begin (heldNotes), std::end (heldNotes), false);
        }
    }
}

static inline float sanitiseSample (float x)
{
    if (! std::isfinite (x)) return 0.0f;
    return juce::jlimit (-1.0f, 1.0f, x);
}

void DysektProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Read DAW BPM from playhead
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpmOpt = pos->getBpm())
                dawBpm.store ((float) *bpmOpt, std::memory_order_relaxed);
        }
    }

    // Poll shift preview request (atomic, avoids FIFO latency)
    {
        int req = shiftPreviewRequest.exchange (-2, std::memory_order_relaxed);
        if (req == -1)
            voicePool.stopShiftPreview();
        else if (req >= 0 && ! lazyChop.isActive() && sampleData.isLoaded())
            voicePool.startShiftPreview (req, sampleData.getNumFrames(),
                                         currentSampleRate, sampleData);
    }

    bool loadStateChanged = false;
    {
        auto* rawDecoded = completedLoadData.exchange (nullptr, std::memory_order_acq_rel);
        if (rawDecoded != nullptr)
        {
            std::unique_ptr<SampleData::DecodedSample> decoded (rawDecoded);
            clearVoicesBeforeSampleSwap();
            sampleData.applyDecodedSample (std::move (decoded));
            sampleMissing.store (false);
            missingFilePath.clear();
            sampleAvailability.store ((int) SampleStateLoaded, std::memory_order_relaxed);

            if (latestLoadKind.load (std::memory_order_acquire) == (int) LoadKindReplace)
                sliceManager.clearAll();
            else
            {
                clampSlicesToSampleBounds();
                sliceManager.rebuildMidiMap();
            }

            // ── SF2/SFZ: auto-create slices (one per rendered note) ──────────
            auto* sfzPayload = pendingSfzSlices.exchange (nullptr, std::memory_order_acq_rel);
            if (sfzPayload != nullptr)
            {
                std::unique_ptr<SfzSlicePayload> sfzOwner (sfzPayload);
                // sliceManager was just cleared above — safe to create fresh slices
                for (auto& desc : sfzOwner->slices)
                {
                    int idx = sliceManager.createSlice (desc.startSample, desc.endSample);
                    if (idx >= 0)
                    {
                        auto& s = sliceManager.getSlice (idx);
                        s.midiNote = juce::jlimit (0, 127, desc.midiNote);
                    }
                }
                sliceManager.rebuildMidiMap();
            }
            // ────────────────────────────────────────────────────────────────

            loadStateChanged = true;
            uiSnapshotDirty.store (true, std::memory_order_release);
        }
    }

    {
        auto* rawFailure = completedLoadFailure.exchange (nullptr, std::memory_order_acq_rel);
        if (rawFailure != nullptr)
        {
            std::unique_ptr<FailedLoadResult> failed (rawFailure);
            if (failed->token == latestLoadToken.load (std::memory_order_acquire)
                && failed->kind == LoadKindRelink)
            {
                sampleMissing.store (true);
                missingFilePath = failed->file.getFullPathName();
                sampleData.setFileName (failed->file.getFileName());
                sampleData.setFilePath (failed->file.getFullPathName());
                sampleAvailability.store ((int) SampleStateMissingAwaitingRelink,
                                         std::memory_order_relaxed);
                loadStateChanged = true;
                uiSnapshotDirty.store (true, std::memory_order_release);
            }
        }
    }

    if (loadStateChanged)
        updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));

    drainCommands();

    if (gestureSnapshotCaptured)
    {
        ++blocksSinceGestureActivity;
        if (blocksSinceGestureActivity > 2)
            gestureSnapshotCaptured = false;
    }

    // Update max active voices from param
    voicePool.setMaxActiveVoices ((int) maxVoicesParam->load());

    // Apply DAW automation of sliceStart / sliceEnd to the selected slice.
    // Guard: only act when the params have already been published for the
    // currently-selected slice.  If the selection just changed, sliceStartParam
    // and sliceEndParam still hold the previous slice's values; acting on them
    // would corrupt the newly-selected slice's boundaries and cause the rapid
    // knob-jumping seen when clicking a slice name.
    if (sliceStartParam != nullptr && sliceEndParam != nullptr)
    {
        const int sel        = sliceManager.selectedSlice.load (std::memory_order_relaxed);
        const int syncedFor  = paramsSyncedForSlice.load (std::memory_order_relaxed);
        const int total      = sampleData.getNumFrames();

        if (sel >= 0 && sel < sliceManager.getNumSlices() && total > 0
            && syncedFor == sel)   // params are up-to-date for this slice
        {
            const auto sl = sliceManager.getSlice (sel);

            const int newStart = juce::roundToInt (sliceStartParam->load (std::memory_order_relaxed) * (float) total);
            const int newEnd   = juce::roundToInt (sliceEndParam->load   (std::memory_order_relaxed) * (float) total);

            const int  slCurEnd     = sliceManager.getEndForSlice (sel, total);
            const bool startChanged = (newStart != sl.startSample);
            const bool endChanged   = (newEnd   != slCurEnd);

            if ((startChanged || endChanged) && newStart < newEnd)
            {
                Command cmd;
                cmd.type         = CmdSetSliceBounds;
                cmd.intParam1    = sel;
                cmd.intParam2    = newStart;
                cmd.positions[0] = newEnd;
                cmd.numPositions = 1;
                pushCommand (cmd);
            }
        }
    }

    processMidi (midi);

    if (uiSnapshotDirty.exchange (false, std::memory_order_acq_rel))
        publishUiSliceSnapshot();

    if (! sampleData.isLoaded())
    {
        if (! sampleMissing.load (std::memory_order_relaxed))
            sampleAvailability.store ((int) SampleStateEmpty, std::memory_order_relaxed);
        return;
    }

    // Collect write pointers for all enabled output buses
    static constexpr int kMaxBuses = 16;
    float* busL[kMaxBuses] = {};
    float* busR[kMaxBuses] = {};
    int numActiveBuses = 0;

    for (int b = 0; b < std::min (getBusCount (false), kMaxBuses); ++b)
    {
        auto* bus = getBus (false, b);
        if (bus != nullptr && bus->isEnabled())
        {
            int chOff = getChannelIndexInProcessBlockBuffer (false, b, 0);
            if (chOff < buffer.getNumChannels())
            {
                busL[b] = buffer.getWritePointer (chOff);
                busR[b] = (chOff + 1 < buffer.getNumChannels())
                              ? buffer.getWritePointer (chOff + 1) : nullptr;
                if (b + 1 > numActiveBuses) numActiveBuses = b + 1;
            }
        }
    }

    buffer.clear();

    if (numActiveBuses <= 1)
    {
        // Fast path: single stereo output
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sL = 0.0f, sR = 0.0f;
            voicePool.processSample (sampleData, currentSampleRate, sL, sR);
            if (busL[0]) busL[0][i] = sanitiseSample (sL);
            if (busR[0]) busR[0][i] = sanitiseSample (sR);
        }
    }
    else
    {
        // Multi-out: route each voice to its assigned bus
        constexpr int previewIdx = VoicePool::kPreviewVoiceIndex;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int vi = 0; vi < voicePool.getMaxActiveVoices(); ++vi)
            {
                float vL = 0.0f, vR = 0.0f;
                voicePool.processVoiceSample (vi, sampleData, currentSampleRate, vL, vR);

                int bus = voicePool.getVoice (vi).outputBus;
                if (bus < 0 || bus >= numActiveBuses || busL[bus] == nullptr) bus = 0;
                if (busL[bus]) busL[bus][i] += vL;
                if (busR[bus]) busR[bus][i] += vR;
            }

            // Always process preview voice (LazyChopEngine) on main bus
            if (previewIdx >= voicePool.getMaxActiveVoices()
                && voicePool.getVoice (previewIdx).active)
            {
                float vL = 0.0f, vR = 0.0f;
                voicePool.processVoiceSample (previewIdx, sampleData, currentSampleRate, vL, vR);
                if (busL[0]) busL[0][i] += vL;
                if (busR[0]) busR[0][i] += vR;
            }
        }
        // Clamp / NaN-guard every active bus after accumulation
        for (int b = 0; b < numActiveBuses; ++b)
        {
            if (busL[b])
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    busL[b][i] = sanitiseSample (busL[b][i]);
            if (busR[b])
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    busR[b][i] = sanitiseSample (busR[b][i]);
        }
    }

    // Pass through MIDI
    // (already in the buffer, no action needed)
}

juce::AudioProcessorEditor* DysektProcessor::createEditor()
{
    return new DysektEditor (*this);
}

void DysektProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream (destData, false);

    // Version
    stream.writeInt (17);

    // APVTS state
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto xmlString = xml->toString();
    stream.writeString (xmlString);

    // UI state
    stream.writeFloat (zoom.load());
    stream.writeFloat (scroll.load());
    stream.writeInt (sliceManager.selectedSlice);
    stream.writeBool (midiSelectsSlice.load());
    stream.writeBool (chromaticMode.load());
    stream.writeInt (sliceManager.rootNote.load());

    // Slice data
    int numSlices = sliceManager.getNumSlices();
    stream.writeInt (numSlices);
    for (int i = 0; i < numSlices; ++i)
    {
        const auto& s = sliceManager.getSlice (i);
        stream.writeBool (s.active);
        stream.writeInt (s.startSample);
        stream.writeInt (sliceManager.getEndForSlice (i, sampleData.getBuffer().getNumSamples()));
        stream.writeInt (s.midiNote);
        stream.writeFloat (s.bpm);
        stream.writeFloat (s.pitchSemitones);
        stream.writeInt (s.algorithm);
        stream.writeFloat (s.attackSec);
        stream.writeFloat (s.decaySec);
        stream.writeFloat (s.sustainLevel);
        stream.writeFloat (s.releaseSec);
        stream.writeInt (s.muteGroup);
        stream.writeInt (s.loopMode);
        stream.writeBool (s.stretchEnabled);
        stream.writeInt ((int) s.lockMask);
        stream.writeInt ((int) s.colour.getARGB());
        // v5 fields
        stream.writeFloat (s.tonalityHz);
        stream.writeFloat (s.formantSemitones);
        stream.writeBool (s.formantComp);
        // v6 fields
        stream.writeInt (s.grainMode);
        // v7 fields
        stream.writeFloat (s.volume);
        // v10 fields
        stream.writeBool (s.releaseTail);
        // v11 fields
        stream.writeBool (s.reverse);
        stream.writeInt (s.outputBus);
        // v15 fields
        stream.writeBool (s.oneShot);
        // v16 fields
        stream.writeFloat (s.centsDetune);
        // v17 fields
        stream.writeFloat (s.pan);
        stream.writeFloat (s.filterCutoff);
        stream.writeFloat (s.filterRes);
    }

    // v9: store file path only (no PCM)
    stream.writeString (sampleData.getFilePath());
    stream.writeString (sampleData.getFileName());

    // v12: snap-to-zero-crossing toggle
    stream.writeBool (snapToZeroCrossing.load());

    // v17: MIDI Learn CC mappings
    midiLearn.writeState (stream);
}

void DysektProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream (data, (size_t) sizeInBytes, false);

    int version = stream.readInt();
    if (version != 16 && version != 17)
        return;

    // APVTS state
    auto xmlString = stream.readString();
    if (auto xml = juce::parseXML (xmlString))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // UI state
    zoom.store (juce::jlimit (1.0f, 16384.0f, stream.readFloat()));
    scroll.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()));
    int savedSelectedSlice = stream.readInt();

    midiSelectsSlice.store (stream.readBool());
    chromaticMode.store (stream.readBool());
    sliceManager.rootNote.store (juce::jlimit (0, 127, stream.readInt()));

    // Slice data
    const int storedNumSlices = stream.readInt();
    if (storedNumSlices < 0 || storedNumSlices > 4096)
        return;

    const int validatedNumSlices = juce::jlimit (0, SliceManager::kMaxSlices, storedNumSlices);
    sliceManager.setNumSlices (validatedNumSlices);
    sliceManager.selectedSlice = juce::jlimit (-1, validatedNumSlices - 1, savedSelectedSlice);

    for (int i = 0; i < storedNumSlices; ++i)
    {
        Slice parsed;
        parsed.active         = stream.readBool();
        parsed.startSample    = stream.readInt();
        stream.readInt();  // legacy endSample — marker model derives this at runtime
        parsed.midiNote       = stream.readInt();
        parsed.bpm            = stream.readFloat();
        parsed.pitchSemitones = stream.readFloat();
        parsed.algorithm      = stream.readInt();
        parsed.attackSec      = stream.readFloat();
        parsed.decaySec       = stream.readFloat();
        parsed.sustainLevel   = stream.readFloat();
        parsed.releaseSec     = stream.readFloat();
        parsed.muteGroup      = stream.readInt();
        parsed.loopMode       = stream.readInt();
        parsed.stretchEnabled = stream.readBool();
        parsed.lockMask       = (uint32_t) stream.readInt();
        parsed.colour         = juce::Colour ((juce::uint32) stream.readInt());
        parsed.tonalityHz     = stream.readFloat();
        parsed.formantSemitones = stream.readFloat();
        parsed.formantComp    = stream.readBool();
        parsed.grainMode      = stream.readInt();
        parsed.volume         = stream.readFloat();
        parsed.releaseTail    = stream.readBool();
        parsed.reverse        = stream.readBool();
        parsed.outputBus      = stream.readInt();
        parsed.oneShot        = stream.readBool();
        parsed.centsDetune    = stream.readFloat();
        // v17 fields (with defaults for v16 presets)
        parsed.pan          = (version >= 17) ? stream.readFloat() : 0.0f;
        parsed.filterCutoff = (version >= 17) ? stream.readFloat() : 20000.0f;
        parsed.filterRes    = (version >= 17) ? stream.readFloat() : 0.0f;

        if (i < validatedNumSlices)
            sliceManager.getSlice (i) = sanitiseRestoredSlice (parsed);
    }

    for (int i = validatedNumSlices; i < SliceManager::kMaxSlices; ++i)
        sliceManager.getSlice (i).active = false;

    // Path-based sample restore
    auto filePath = stream.readString();
    auto fileName = stream.readString();

    clearVoicesBeforeSampleSwap();
    sampleData.clear();

    if (filePath.isNotEmpty())
    {
        const juce::File restoredFile (filePath);
        sampleMissing.store (false);
        missingFilePath.clear();
        sampleData.setFileName (fileName.isNotEmpty() ? fileName : restoredFile.getFileName());
        sampleData.setFilePath (filePath);
        sampleAvailability.store ((int) SampleStateEmpty, std::memory_order_relaxed);
        // Preserve restored slices while loading, and report missing path via relink state.
        requestSampleLoad (restoredFile, LoadKindRelink);
    }
    else
    {
        sampleMissing.store (false);
        missingFilePath.clear();
        sampleData.setFileName ({});
        sampleData.setFilePath ({});
        sampleAvailability.store ((int) SampleStateEmpty, std::memory_order_relaxed);
    }

    sliceManager.rebuildMidiMap();
    publishUiSliceSnapshot();

    snapToZeroCrossing.store (stream.readBool());

    // v17: MIDI Learn CC mappings (optional — older presets simply leave all unassigned)
    if (! stream.isExhausted())
        midiLearn.readState (stream);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return static_cast<juce::AudioProcessor*> (new DysektProcessor());
}
