#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DysektAudioProcessor::DysektAudioProcessor()
{
    // Fix for "Full soundfont support":
    // We add 32 voices to the sfzero engine so it can play polyphonic chords.
    for (int i = 0; i < 32; ++i)
        sampler.addVoice (new sfzero::Voice());
}

DysektAudioProcessor::~DysektAudioProcessor()
{
}

//==============================================================================
const juce::String DysektAudioProcessor::getName() const { return JucePlugin_Name; }

bool DysektAudioProcessor::acceptsMidi() const { return true; }
bool DysektAudioProcessor::producesMidi() const { return true; }
bool DysektAudioProcessor::isMidiEffect() const { return false; }
double DysektAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int DysektAudioProcessor::getNumPrograms() { return 1; }
int DysektAudioProcessor::getCurrentProgram() { return 0; }
void DysektAudioProcessor::setCurrentProgram (int index) {}
const juce::String DysektAudioProcessor::getProgramName (int index) { return {}; }
void DysektAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

//==============================================================================
void DysektAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Crucial: Tell the SFZero engine the sample rate so pitch is correct
    sampler.setCurrentPlaybackSampleRate (sampleRate);
}

void DysektAudioProcessor::releaseResources() {}

bool DysektAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void DysektAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output buffers
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // ACTUAL PLAYBACK: This renders the SoundFont audio into the buffer
    sampler.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool DysektAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* DysektAudioProcessor::createEditor()
{
    return new DysektAudioProcessorEditor (*this);
}

//==============================================================================
void DysektAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void DysektAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

//==============================================================================
/** 
 * This fulfills the request: "The current stub accepts the files and attempts decode — 
 * you'll want to link SFZero for actual playback."
 */
void DysektAudioProcessor::loadSoundFont (const juce::File& file)
{
    sampler.clearSounds();
    juce::ReferenceCountedObjectPtr<sfzero::Sound> newSound;

    // 1. Decode the file based on extension
    if (file.getFileExtension().equalsIgnoreCase(".sf2"))
        newSound = new sfzero::SF2Sound (file);
    else if (file.getFileExtension().equalsIgnoreCase(".sfz"))
        newSound = new sfzero::SFZSound (file);

    if (newSound != nullptr)
    {
        // 2. MULTI-SAMPLE MAPPING: 
        // This is the critical step that connects samples to keys
        newSound->loadRegions(); 
        
        // 3. Add to the playback engine
        sampler.addSound (newSound);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DysektAudioProcessor();
}
