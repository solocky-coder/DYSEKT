#pragma once

#include <JuceHeader.h>
#include "audio/SampleData.h"
#include "audio/SliceManager.h"
#include "audio/VoicePool.h"
#include "audio/LazyChopEngine.h"

// ==============================================================================
/** Dysekt Audio Processor - Handles SF2/SFZ playback via SFZero */
class DysektAudioProcessor  : public juce::AudioProcessor
{
public:
    // ==============================================================================
    DysektAudioProcessor();
    ~DysektAudioProcessor() override;

    // ==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // ==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // ==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // ==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    // ==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Loads a SoundFont or SFZ file and performs multi-sample mapping */
    void loadSoundFont (const juce::File& file);

    // ==============================================================================
    // Logic Components
    SampleData sampleData;
    SliceManager sliceManager;
    VoicePool voicePool;
    LazyChopEngine lazyChop;

    // State atomics for UI sync
    std::atomic<float> zoom { 1.0f };
    std::atomic<float> scroll { 0.0f };

private:
    // ==============================================================================
    /** 
     * Replacing the standard JUCE stub with SFZero.
     * This fulfills the "actual playback" requirement by mapping samples to keys.
     */
    sfzero::Synth sampler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DysektAudioProcessor)
};
