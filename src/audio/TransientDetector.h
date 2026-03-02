#pragma once
/**
 * @file TransientDetector.h
 * @brief Dedicated transient detection engine for DYSEKT AutoChop.
 *
 * This module provides a standalone, configurable transient detection engine
 * that wraps the hybrid analysis from AudioAnalysis.h and exposes a clean
 * interface for use in the AutoChop workflow.
 *
 * ## Algorithm Overview
 *
 * Detection uses three complementary analysis passes over fixed-size windows:
 *  1. **RMS energy log-ratio** — captures large amplitude onsets (kick, snare).
 *  2. **Spectral flux** — detects high-frequency transient content (hi-hats,
 *     consonants, string attacks) that may have modest energy change.
 *  3. **Multi-scale fusion** — windows of 512, 1024, and 2048 samples are
 *     analysed simultaneously; each scale votes with a weighted score, and
 *     candidates within 50 ms are merged by keeping the highest-scoring one.
 *
 * ## Usage Example
 *
 * @code
 * TransientDetector detector;
 * detector.setMode (TransientDetector::Mode::Normal);
 * detector.setSensitivity (0.65f);                     // 0 = few, 1 = many
 * detector.setSnapToZeroCrossing (true);
 *
 * auto positions = detector.detect (audioBuffer, startSample, endSample,
 *                                   sampleRate);
 * // positions contains absolute sample indices ready for slice boundaries
 * @endcode
 *
 * ## Thread Safety
 *
 * TransientDetector is **not** thread-safe. Construct one instance per call
 * site, or guard shared instances with an external mutex.
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "AudioAnalysis.h"

//==============================================================================
/**
 * @class TransientDetector
 * @brief Configurable onset detector combining energy and spectral-flux ODFs.
 *
 * Wraps AudioAnalysis::detectTransientsHybrid() with a stateful, reusable API.
 * All parameters can be changed between calls without re-creating the object.
 */
class TransientDetector
{
public:
    //==========================================================================
    /**
     * @brief Sensitivity presets controlling minimum onset spacing and analysis
     *        weights.
     *
     * - Conservative: 200 ms minimum spacing, energy-biased; ideal for sparse
     *   content (solo instruments, speech) where false detections are costly.
     * - Normal: 100 ms minimum spacing, balanced weights; good default for
     *   mixed material and drum loops.
     * - Aggressive: 50 ms minimum spacing, flux-biased; catches subtle
     *   transients in dense material such as hi-hat patterns or shakers.
     */
    using Mode = AudioAnalysis::SensitivityMode;

    //==========================================================================
    TransientDetector() = default;

    //==========================================================================
    /**
     * @brief Set the detection sensitivity preset.
     * @param m  One of Mode::Conservative, Mode::Normal, Mode::Aggressive.
     *
     * The preset adjusts minimum onset spacing and the weighting between
     * energy-based and flux-based onset functions.  See Mode for details.
     */
    void setMode (Mode m) noexcept { mode = m; }

    /** @returns the currently active sensitivity mode. */
    Mode getMode() const noexcept { return mode; }

    //==========================================================================
    /**
     * @brief Set the fine-grain detection threshold on a 0–1 scale.
     *
     * @param s  0.0 = only the very strongest onsets; 1.0 = all candidates
     *           above the 50th-percentile threshold.  Default 0.5.
     *
     * Internally this maps to a percentile of the combined onset detection
     * function: sensitivity 1.0 → 50th-percentile threshold (many hits),
     * sensitivity 0.0 → 99.5th-percentile threshold (fewest hits).
     */
    void setSensitivity (float s) noexcept
    {
        sensitivity = juce::jlimit (0.0f, 1.0f, s);
    }

    /** @returns the current sensitivity in [0, 1]. */
    float getSensitivity() const noexcept { return sensitivity; }

    //==========================================================================
    /**
     * @brief Enable or disable zero-crossing snap for detected positions.
     *
     * @param enable  When true, every detected onset is shifted to the nearest
     *                zero-crossing within a 512-sample search window, reducing
     *                audible clicks on slice boundaries.
     */
    void setSnapToZeroCrossing (bool enable) noexcept { snapToZero = enable; }

    /** @returns true if zero-crossing snap is active. */
    bool isSnapToZeroCrossing() const noexcept { return snapToZero; }

    //==========================================================================
    /**
     * @brief Run transient detection over a region of an audio buffer.
     *
     * @param buffer      Stereo (or mono) audio buffer to analyse.  The
     *                    buffer must remain valid for the duration of this call.
     * @param startSample First sample of the region to search (inclusive).
     * @param endSample   First sample past the end of the region (exclusive).
     * @param sampleRate  Audio sample rate in Hz; used for all time constants.
     *
     * @returns A sorted list of absolute sample positions (within
     *          [startSample, endSample]) where transients were detected.
     *          The list is empty if no transients were found or the region
     *          is too short for analysis (< 5 analysis windows).
     *
     * Detection combines RMS-energy and spectral-flux onset detection
     * functions across three analysis scales (512, 1024, 2048 samples)
     * using the current mode and sensitivity settings.  Duplicate detections
     * within 50 ms are merged before applying the minimum onset distance
     * constraint from the selected Mode.
     */
    std::vector<int> detect (const juce::AudioBuffer<float>& buffer,
                             int startSample, int endSample,
                             double sampleRate = 44100.0) const;

    //==========================================================================
    /**
     * @brief Convenience overload that analyses the full buffer.
     *
     * Equivalent to calling detect(buffer, 0, buffer.getNumSamples(), sampleRate).
     */
    std::vector<int> detectFull (const juce::AudioBuffer<float>& buffer,
                                 double sampleRate = 44100.0) const
    {
        return detect (buffer, 0, buffer.getNumSamples(), sampleRate);
    }

private:
    Mode  mode        = Mode::Normal;  ///< Active sensitivity preset
    float sensitivity = 0.5f;          ///< Fine-tune threshold density [0, 1]
    bool  snapToZero  = false;         ///< Snap detections to zero crossings
};
