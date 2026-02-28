#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DysektAudioProcessor::DysektAudioProcessor()
{
    // Add 32 voices to allow for polyphonic SoundFont playback
    for (int i = 0; i < 32; ++i)
        sampler.addVoice (new sfzero::Voice());
}

DysektAudioProcessor::~DysektAudioProcessor()
{
}

//==============================================================================
const juce::String DysektAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DysektAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DysektAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DysektAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DysektAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DysektAudioProcessor::getNumPrograms()
{
    return 1;   // Only one program
}

int DysektAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DysektAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DysektAudioProcessor::getProgramName (int index)
{
    return {};
}

void DysektAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DysektAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The sampler needs to know your sound card's speed to play at the correct pitch
    sampler.setCurrentPlaybackSampleRate (sampleRate);
}

void DysektAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // resources that were allocated in prepareToPlay().
}

#ifndef JucePlugin_PreferredBusArrangement
bool DysektAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DysektAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // SFZero renders the sound based on the MIDI input here, replacing the old "stub"
    sampler.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool DysektAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DysektAudioProcessor::createEditor()
{
    return new DysektAudioProcessorEditor (*this);
}

//==============================================================================
void DysektAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do this using XML or an RLP stream.
}

void DysektAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block.
    // You could do this using XML or an RLP stream.
}

//==============================================================================
// The fix for the original request:
void DysektAudioProcessor::loadSoundFont (const juce::File& file)
{
    sampler.clearSounds();
    juce::ReferenceCountedObjectPtr<sfzero::Sound> newSound;

    // Identify and decode the file
    if (file.getFileExtension() == ".sf2")
        newSound = new sfzero::SF2Sound (file);
    else if (file.getFileExtension() == ".sfz")
        newSound = new sfzero::SFZSound (file);

    if (newSound != nullptr)
    {
        // CRITICAL: This performs the "multi-sample mapping" 
        // requested in your original note so the notes actually play back.
        newSound->loadRegions(); 
        sampler.addSound (newSound);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DysektAudioProcessor();
}
