// Golden regression test: runs the converter on the full FEZ export copy (FEZ_DATA_DIR) into a fresh temporary
// directory and compares the whole output tree byte for byte against the reference (FEZ_GOLDEN_DIR).
//
// Hidden ("[.golden]"), so a plain fez_tests run and catch_discover_tests skip it. It is registered as its own CTest
// entry with label "golden" (tests/CMakeLists.txt): ctest --preset windows-debug-golden

#include "support/TreeCompare.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimeZone>
#include <QtCore/QtEnvironmentVariables>

#include <catch2/catch_test_macros.hpp>

#include <string>

#ifndef FEZ_MODEL_GENERATOR_EXE
#error "FEZ_MODEL_GENERATOR_EXE must be defined by the build (tests/CMakeLists.txt)"
#endif

namespace
{
    std::string toStd(const QString& text)
    {
        return text.toStdString();
    }

    // Every directory and file below root as "<relative path>[/] <size> <last modified>", sorted. Cheap enough for the
    // full input copy; used to prove the converter writes nothing into its input tree.
    QStringList snapshotTree(const QString& root)
    {
        const QDir rootDir(root);
        QStringList entries;
        QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);

        while (it.hasNext())
        {
            const QString path = it.next();
            const QFileInfo info = it.fileInfo();
            entries << rootDir.relativeFilePath(path) + (info.isDir() ? QStringLiteral("/") : QString()) +
                           QLatin1Char(' ') + QString::number(info.isDir() ? 0 : info.size()) + QLatin1Char(' ') +
                           info.lastModified(QTimeZone::UTC).toString(Qt::ISODateWithMs);
        }

        entries.sort();
        return entries;
    }

    // Entries in one snapshot but not the other, for the failure message.
    QString describeSnapshotDifference(const QStringList& before, const QStringList& after)
    {
        QStringList lines;
        for (const QString& entry : before)
            if (!after.contains(entry))
                lines << QStringLiteral("- ") + entry;
        for (const QString& entry : after)
            if (!before.contains(entry))
                lines << QStringLiteral("+ ") + entry;
        return lines.mid(0, 40).join(QLatin1Char('\n'));
    }

    // Last lines of the converter's output, enough to see why it failed without flooding the log.
    QString tail(const QByteArray& output, qsizetype lineCount)
    {
        const QStringList lines = QString::fromLocal8Bit(output).split(QLatin1Char('\n'));
        return lines.mid(qMax<qsizetype>(0, lines.size() - lineCount)).join(QLatin1Char('\n'));
    }
} // namespace

TEST_CASE("Converter output matches the golden reference", "[.golden]")
{
    const QString dataDir = qEnvironmentVariable("FEZ_DATA_DIR");
    const QString goldenDir = qEnvironmentVariable("FEZ_GOLDEN_DIR");
    if (dataDir.isEmpty() || goldenDir.isEmpty())
    {
        SKIP("FEZ_DATA_DIR and/or FEZ_GOLDEN_DIR are not set");
    }

    INFO("FEZ_DATA_DIR = " << toStd(dataDir));
    INFO("FEZ_GOLDEN_DIR = " << toStd(goldenDir));
    REQUIRE(QFileInfo(dataDir).isDir());
    REQUIRE(QFileInfo(goldenDir).isDir());

    const QString converter = QStringLiteral(FEZ_MODEL_GENERATOR_EXE);
    INFO("converter = " << toStd(converter));
    REQUIRE(QFileInfo(converter).isExecutable());

    // The converter must write into a fresh, empty directory: QFile::copy does not overwrite, so stale files would
    // hide differences.
    QTemporaryDir outputDir;
    REQUIRE(outputDir.isValid());
    INFO("output = " << toStd(outputDir.path()));

    const QStringList inputBefore = snapshotTree(dataDir);
    REQUIRE_FALSE(inputBefore.isEmpty());

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram(converter);
    process.setArguments({QStringLiteral("--input"), dataDir, QStringLiteral("--output"), outputDir.path()});

    QElapsedTimer timer;
    timer.start();
    process.start();
    REQUIRE(process.waitForStarted());
    const bool finished = process.waitForFinished(-1); // a full Debug conversion takes ~1 h; CTest enforces TIMEOUT
    const QByteArray output = process.readAll();
    const double seconds = static_cast<double>(timer.elapsed()) / 1000.0;

    {
        INFO("converter ran " << seconds << " s; last output lines:\n" << toStd(tail(output, 40)));
        REQUIRE(finished);
        REQUIRE(process.exitStatus() == QProcess::NormalExit);
        REQUIRE(process.exitCode() == 0);
    }

    const fez::testing::TreeComparison comparison = fez::testing::compareTrees(goldenDir, outputDir.path());
    INFO(toStd(comparison.describe()));
    CHECK(comparison.identical());

    // The input tree is untouched: no new directory (such as the old "trile sets/exported/"), no changed file.
    const QStringList inputAfter = snapshotTree(dataDir);
    INFO("input changes (first 40):\n" << toStd(describeSnapshotDifference(inputBefore, inputAfter)));
    const bool inputUnchanged = inputAfter == inputBefore;
    CHECK(inputUnchanged);
}
