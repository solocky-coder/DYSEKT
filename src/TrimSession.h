#pragma once
#include <juce_core/juce_core.h>

/** Move-only struct that tracks all state for one trim operation.
    The file path, trim bounds, and user preference are bundled so they
    can be passed through a lambda or stored as a member without copying. */
struct TrimSession
{
    juce::File file;          ///< The audio file being trimmed
    int        trimIn  = 0;   ///< Trim-in point in samples
    int        trimOut = 0;   ///< Trim-out point in samples
    int        preference = 0;///< 0 = ask, 1 = always trim, 2 = never trim

    TrimSession() = default;

    TrimSession (juce::File f, int in, int out, int pref)
        : file (std::move (f)), trimIn (in), trimOut (out), preference (pref) {}

    // Move-only semantics
    TrimSession (TrimSession&&)            = default;
    TrimSession& operator= (TrimSession&&) = default;

    TrimSession (const TrimSession&)            = delete;
    TrimSession& operator= (const TrimSession&) = delete;
};
