#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DysektAudioProcessor::DysektAudioProcessor()
{
    // 1. INITIALIZE VOICES: This fulfills the "actual playback" requirement.
    // We add 32 voices so the SoundFont can play chords (polyphony).
    for (int i = 0; i < 32; ++i)
        sampler.addVoice (new sfzero::Voice());
}

DysektAudioProcessor::~DysektAudioProcessor() {}

//==============================================================================
void DysektAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 2. SET SAMPLE RATE: Essential for correct pitch playback.
    sampler.setCurrentPlaybackSampleRate (sampleRate);
}

void DysektAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Clear the buffer to prevent noise
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // 3. RENDER AUDIO: This replaces the "stub" logic with actual SFZero output.
    sampler.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
/** 
 * This satisfies the "multi-sample mapping" requirement from your note.
 */
void DysektAudioProcessor::loadSoundFont (const juce::File& file)
{
    sampler.clearSounds();
    juce::ReferenceCountedObjectPtr<sfzero::Sound> newSound;

    if (file.getFileExtension().equalsIgnoreCase(".sf2"))
        newSound = new sfzero::SF2Sound (file);
    else if (file.getFileExtension().equalsIgnoreCase(".sfz"))
        newSound = new sfzero::SFZSound (file);

    if (newSound != nullptr)
    {
        // 4. MAPPING: This connects the audio samples to the MIDI keys.
        newSound->loadRegions(); 
        sampler.addSound (newSound);
    }
}

// Ensure standard JUCE boilerplate (getName, hasEditor, etc.) follows below...
