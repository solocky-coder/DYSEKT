#include "ParamLayout.h"
#include "ParamIds.h"

juce::AudioProcessorValueTreeState::ParameterLayout ParamLayout::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Most-reached-for (first 8) ────────────────────────────────────────────

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultBpm, 1 },
        "Sample BPM",
        juce::NormalisableRange<float> (20.0f, 999.0f, 0.01f),
        120.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultPitch, 1 },
        "Sample Pitch",
        juce::NormalisableRange<float> (-48.0f, 48.0f, 0.01f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultCentsDetune, 1 },
        "Sample Cents Detune",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIds::defaultAlgorithm, 1 },
        "Sample Algorithm",
        juce::StringArray { "Repitch", "Stretch", "Bungee" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultAttack, 1 },
        "Sample Attack",
        juce::NormalisableRange<float> (0.0f, 1000.0f, 0.1f),
        5.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultRelease, 1 },
        "Sample Release",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 0.1f),
        20.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::masterVolume, 1 },
        "Master Gain",
        juce::NormalisableRange<float> (-100.0f, 24.0f, 0.1f),
        0.0f));

    // ── Secondary sample params ────────────────────────────────────────────────

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIds::defaultReverse, 1 },
        "Sample Reverse",
        false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIds::defaultLoop, 1 },
        "Sample Loop Mode",
        juce::StringArray { "Off", "Loop", "Ping-Pong" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIds::defaultStretchEnabled, 1 },
        "Sample Stretch",
        false));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIds::defaultMuteGroup, 1 },
        "Sample Mute Group",
        0, 32, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultDecay, 1 },
        "Sample Decay",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 0.1f),
        100.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultSustain, 1 },
        "Sample Sustain",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIds::defaultReleaseTail, 1 },
        "Sample Release Tail",
        false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIds::defaultOneShot, 1 },
        "Sample One Shot",
        false));

    // ── Advanced / algorithm-specific ─────────────────────────────────────────

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultTonality, 1 },
        "Sample Tonality",
        juce::NormalisableRange<float> (0.0f, 8000.0f, 1.0f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultFormant, 1 },
        "Sample Formant",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIds::defaultFormantComp, 1 },
        "Sample Formant Comp",
        false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIds::defaultGrainMode, 1 },
        "Sample Grain Mode",
        juce::StringArray { "Fast", "Normal", "Smooth" },
        1));

    // ── Global utility ─────────────────────────────────────────────────────────

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIds::maxVoices, 1 },
        "Max Voices",
        1, 31, 16));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::uiScale, 1 },
        "UI Scale",
        juce::NormalisableRange<float> (0.5f, 3.0f, 0.25f),
        1.0f));

    // ── Selected slice boundary ────────────────────────────────────────────────
    // Normalised 0..1 position within the loaded sample.
    // Reflects the selected slice's start/end; automatable and MIDI-learnable.

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::sliceStart, 1 },
        "Slice Start",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::sliceEnd, 1 },
        "Slice End",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
        1.0f));

    // ── v17: Pan, Filter ──────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultPan, 1 },
        "Pan",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultFilterCutoff, 1 },
        "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f),  // skewed toward low end
        20000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIds::defaultFilterRes, 1 },
        "Filter Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.0f));

    return { params.begin(), params.end() };
}
