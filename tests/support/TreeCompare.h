#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>
#include <vector>

namespace fez::testing
{
    // A file present in both trees whose bytes differ.
    struct FileDifference
    {
        QString relativePath; // '/'-separated, relative to the tree root

        // Set for .obj and .mtl files only (case-insensitive suffix): the 1-based number of the first line that
        // differs, and that line's content in both files (without the line terminator; empty past the end of a file).
        std::optional<qsizetype> firstDifferingLine;
        QByteArray expectedLine;
        QByteArray actualLine;
    };

    struct TreeComparison
    {
        QStringList missing; // in expected, not in actual (sorted, relative paths)
        QStringList extra;   // in actual, not in expected (sorted, relative paths)
        std::vector<FileDifference> differing; // sorted by relativePath

        bool identical() const;

        // Human-readable report of every difference, for test failure messages. Empty when identical.
        QString describe() const;
    };

    // Recursively compares all files below expectedRoot and actualRoot by relative path and content.
    // Directories themselves are not compared (an empty directory is invisible). A root that does not
    // exist is treated as an empty tree.
    TreeComparison compareTrees(const QString& expectedRoot, const QString& actualRoot);
} // namespace fez::testing
