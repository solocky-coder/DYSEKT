#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>

// Simplified: Each Peak has min/max value in [-1,1]
struct WaveformPeak
{
    float minVal = 0.0f, maxVal = 0.0f;
};

constexpr int kMaxWaveformWidth = 4000;

using PeakArray = std::vector<WaveformPeak>;

// For SampleData::Snapshot, which has audio & peaks
struct SampleDataSnapshot
{
    juce::AudioBuffer<float> buffer;              // The raw sample data
    PeakArray peakPeaks;                          // One sample per pixel, already generated
};

using SnapshotPtr = std::shared_ptr<const SampleDataSnapshot>;

struct SampleData
{
    SnapshotPtr getSnapshot() const
    {
        // For testing, you might want a stub here that returns a
        // std::make_shared<SampleDataSnapshot>(...);
        return nullptr; // Replace with actual code
    }
};