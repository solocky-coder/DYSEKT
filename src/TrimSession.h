#pragma once
#include <juce_core/juce_core.h>

struct TrimSession
{
    juce::File file;
    int trimStart = 0;
    int trimEnd = 0;
    int trimPreference = 0;  // 0=ask, 1=always trim, 2=never trim

    TrimSession() = default;
    ~TrimSession() = default;

    TrimSession(const TrimSession&) = delete;
    TrimSession& operator=(const TrimSession&) = delete;

    TrimSession(TrimSession&&) = default;
    TrimSession& operator=(TrimSession&&) = default;
};
