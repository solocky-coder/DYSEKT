/**
 * @file TransientDetector.cpp
 * @brief Implementation of the TransientDetector onset detection engine.
 *
 * The main detection logic delegates to AudioAnalysis::detectTransientsHybrid()
 * which combines multi-scale RMS-energy and spectral-flux onset detection
 * functions.  This file is responsible for the post-processing steps:
 *
 *  - Optional zero-crossing snap: each detected onset is shifted to the
 *    nearest zero-crossing within ±512 samples to minimise slice-boundary
 *    clicks (see AudioAnalysis::findNearestZeroCrossing).
 *
 * The algorithm used in detectTransientsHybrid():
 *  1. For each of three window sizes (512, 1024, 2048 samples) and hop=256:
 *     a. Compute per-frame RMS energy.
 *     b. Compute per-frame spectral flux as the positive increment in the
 *        RMS of the first-difference signal (a high-frequency energy proxy).
 *     c. Form a combined onset detection function (ODF) as a weighted sum of
 *        the log-ratio energy ODF and the normalised flux ODF.
 *     d. Threshold at a percentile of the positive ODF values (controlled by
 *        the sensitivity parameter: 1.0 → 50th percentile, 0.0 → 99.5th).
 *     e. Peak-pick: accept local maxima within ±3 frames and refine the
 *        sub-frame position via parabolic interpolation.
 *  2. Pool all candidates, sort by time, then merge those within 50 ms by
 *     keeping the highest-scoring candidate.
 *  3. Apply a minimum onset distance constraint (50 ms / 100 ms / 200 ms
 *     depending on Mode) to the final merged list.
 */

#include "TransientDetector.h"
#include "AudioAnalysis.h"
#include <algorithm>

//==============================================================================
std::vector<int> TransientDetector::detect (const juce::AudioBuffer<float>& buffer,
                                            int startSample,
                                            int endSample,
                                            double sampleRate) const
{
    // Delegate to the hybrid multi-scale detector.
    // sensitivity is passed directly — detectTransientsHybrid already clamps it.
    auto positions = AudioAnalysis::detectTransientsHybrid (
        buffer, startSample, endSample, mode, sensitivity, sampleRate);

    // Optionally snap each detected position to the nearest zero crossing.
    // This reduces audible clicks on slice boundaries at the cost of a small
    // (typically < 1 ms) shift in the onset time.
    if (snapToZero)
    {
        std::transform (positions.begin(), positions.end(), positions.begin(),
                        [&buffer] (int p)
                        {
                            return AudioAnalysis::findNearestZeroCrossing (buffer, p);
                        });

        // Re-sort: zero-crossing snapping can alter relative order in rare
        // edge cases where two onsets share a nearby crossing.
        std::sort (positions.begin(), positions.end());

        // Remove any duplicates introduced by snapping
        positions.erase (std::unique (positions.begin(), positions.end()),
                         positions.end());
    }

    return positions;
}
