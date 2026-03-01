#pragma once

/// Move-only struct for async trim state management.
/// Holds the pending trim preference and region while the post-load
/// trim dialog is visible or being decided.
struct TrimSession
{
    int trimPreference = 0;   ///< 0 = ask, 1 = always trim, 2 = never trim
    int pendingTrimIn  = 0;
    int pendingTrimOut = 0;

    TrimSession()                               = default;
    TrimSession (TrimSession&&)                 = default;
    TrimSession& operator= (TrimSession&&)      = default;
    TrimSession (const TrimSession&)            = delete;
    TrimSession& operator= (const TrimSession&) = delete;
};
