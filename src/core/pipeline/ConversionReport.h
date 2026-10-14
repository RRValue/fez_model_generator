#pragma once

#include "error/Error.h"

#include <QtCore/QString>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace fez
{

// The outcome of converting one input file: ok when it has no errors. A trile set reports one error per trile that
// failed to export; every other file has at most one error (the first failure ends its conversion).
struct FileReport
{
    QString m_File; // the input file as listed by the pipeline
    std::vector<Error> m_Errors;

    bool ok() const noexcept
    {
        return m_Errors.empty();
    }
};

// One entry per input file, in input order (see Pipeline.h).
struct ConversionReport
{
    std::vector<FileReport> m_Files;

    // True when every file converted (also for an empty report).
    bool ok() const noexcept
    {
        return failureCount() == 0;
    }

    // Number of files with at least one error.
    std::size_t failureCount() const noexcept
    {
        return static_cast<std::size_t>(std::ranges::count_if(m_Files, [](const FileReport& file) { return !file.ok(); }));
    }

    // Appends the entries of other (e.g. the next phase) after this report's entries.
    void append(const ConversionReport& other)
    {
        m_Files.insert(m_Files.end(), other.m_Files.begin(), other.m_Files.end());
    }
};

} // namespace fez
