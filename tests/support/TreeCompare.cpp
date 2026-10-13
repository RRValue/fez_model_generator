#include "support/TreeCompare.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace fez::testing
{
    namespace
    {
        // Sorted '/'-separated paths of all files below root (empty if root does not exist).
        QStringList listFiles(const QString& root)
        {
            QStringList files;
            if (!QFileInfo(root).isDir())
            {
                return files;
            }

            const QDir rootDir(root);
            QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                files.append(rootDir.relativeFilePath(it.next()));
            }
            files.sort();
            return files;
        }

        std::optional<QByteArray> readAll(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                return std::nullopt;
            }
            return file.readAll();
        }

        bool hasLineReport(const QString& relativePath)
        {
            return relativePath.endsWith(QLatin1String(".obj"), Qt::CaseInsensitive) ||
                   relativePath.endsWith(QLatin1String(".mtl"), Qt::CaseInsensitive);
        }

        // Content of the line that contains byte offset `pos` (pos may be content.size()), without "\r\n" / "\n".
        QByteArray lineAt(const QByteArray& content, qsizetype pos)
        {
            if (pos >= content.size())
            {
                return {};
            }
            const qsizetype begin = pos == 0 ? 0 : content.lastIndexOf('\n', pos - 1) + 1;
            qsizetype end = content.indexOf('\n', pos);
            if (end < 0)
            {
                end = content.size();
            }
            if (end > begin && content[end - 1] == '\r')
            {
                --end;
            }
            return content.mid(begin, end - begin);
        }

        // Fills the line report for two different contents.
        void reportFirstDifferingLine(const QByteArray& expected, const QByteArray& actual, FileDifference& diff)
        {
            const qsizetype pos = std::ranges::mismatch(expected, actual).in1 - expected.begin();

            // Report the whole line containing the first differing byte; start at the beginning of that line so that
            // a difference in a "\r\n" terminator still reports the line it ends.
            const qsizetype lineBegin = pos == 0 ? 0 : expected.left(pos).lastIndexOf('\n') + 1;

            diff.firstDifferingLine = expected.left(lineBegin).count('\n') + 1;
            diff.expectedLine = lineAt(expected, lineBegin);
            diff.actualLine = lineAt(actual, lineBegin);
        }
    } // namespace

    bool TreeComparison::identical() const
    {
        return missing.isEmpty() && extra.isEmpty() && differing.empty();
    }

    QString TreeComparison::describe() const
    {
        if (identical())
        {
            return {};
        }

        QString text = QStringLiteral("%1 missing, %2 extra, %3 differing file(s)\n").arg(missing.size()).arg(extra.size()).arg(differing.size());
        for (const QString& path : missing)
        {
            text += QStringLiteral("  missing:   %1\n").arg(path);
        }
        for (const QString& path : extra)
        {
            text += QStringLiteral("  extra:     %1\n").arg(path);
        }
        for (const FileDifference& diff : differing)
        {
            if (diff.firstDifferingLine)
            {
                text += QStringLiteral("  differing: %1 (first difference at line %2)\n"
                                       "    expected: %3\n"
                                       "    actual:   %4\n")
                            .arg(diff.relativePath)
                            .arg(*diff.firstDifferingLine)
                            .arg(QString::fromUtf8(diff.expectedLine), QString::fromUtf8(diff.actualLine));
            }
            else
            {
                text += QStringLiteral("  differing: %1\n").arg(diff.relativePath);
            }
        }
        return text;
    }

    TreeComparison compareTrees(const QString& expectedRoot, const QString& actualRoot)
    {
        const QStringList expectedFiles = listFiles(expectedRoot);
        const QStringList actualFiles = listFiles(actualRoot);

        TreeComparison result;
        std::ranges::set_difference(expectedFiles, actualFiles, std::back_inserter(result.missing));
        std::ranges::set_difference(actualFiles, expectedFiles, std::back_inserter(result.extra));

        QStringList common;
        std::ranges::set_intersection(expectedFiles, actualFiles, std::back_inserter(common));

        for (const QString& relativePath : std::as_const(common))
        {
            const std::optional<QByteArray> expected = readAll(expectedRoot + QLatin1Char('/') + relativePath);
            const std::optional<QByteArray> actual = readAll(actualRoot + QLatin1Char('/') + relativePath);
            if (expected && actual && *expected == *actual)
            {
                continue;
            }

            // An unreadable file always counts as differing (without a line report).
            FileDifference diff;
            diff.relativePath = relativePath;
            if (expected && actual && hasLineReport(relativePath))
            {
                reportFirstDifferingLine(*expected, *actual, diff);
            }
            result.differing.push_back(std::move(diff));
        }
        return result;
    }
} // namespace fez::testing
