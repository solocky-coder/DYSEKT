#ifndef TRIMSESSION_H
#define TRIMSESSION_H

#include <string>

struct TrimSession {
    std::string file;
    int trimStart;
    int trimEnd;
    int trimPreference;

    TrimSession(std::string f, int start, int end, int preference) 
        : file(std::move(f)), trimStart(start), trimEnd(end), trimPreference(preference) {}

    TrimSession(TrimSession&& other) noexcept
        : file(std::move(other.file)), trimStart(other.trimStart), trimEnd(other.trimEnd), trimPreference(other.trimPreference) {}

    TrimSession(const TrimSession&) = delete; // Disable copy constructor
    TrimSession& operator=(const TrimSession&) = delete; // Disable copy assignment
    TrimSession& operator=(TrimSession&& other) noexcept {
        // Implement move assignment operator
        if (this != &other) {
            file = std::move(other.file);
            trimStart = other.trimStart;
            trimEnd = other.trimEnd;
            trimPreference = other.trimPreference;
        }
        return *this;
    }
};

#endif // TRIMSESSION_H
