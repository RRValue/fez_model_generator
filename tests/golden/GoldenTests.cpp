// Golden regression test: runs the converter on the full FEZ export copy (FEZ_DATA_DIR) into a fresh temporary
// directory and compares the whole output tree byte for byte against the reference (FEZ_GOLDEN_DIR).
//
// Hidden ("[.golden]"), so a plain fez_tests run and catch_discover_tests skip it. It is registered as its own CTest
// entry with label "golden" (tests/CMakeLists.txt): ctest --preset windows-debug-golden

#include "support/TreeCompare.h"

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
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

    // The converter writes "trile sets/exported/" into its INPUT tree (TrileSetParser). Remove it so every run starts
    // from the same pristine input and the result does not depend on earlier runs. Nothing else in the input is
    // touched. Obsolete once Task 16 (parseTrileSet) stops writing into the input.
    void removeTrileSetExportFromInput(const QString& dataDir)
    {
        QDir exported(dataDir + QStringLiteral("/trile sets/exported"));
        if (exported.exists())
        {
            INFO("removing " << toStd(exported.absolutePath()));
            REQUIRE(exported.removeRecursively());
        }
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

    removeTrileSetExportFromInput(dataDir);

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

    removeTrileSetExportFromInput(dataDir);

    {
        INFO("converter ran " << seconds << " s; last output lines:\n" << toStd(tail(output, 40)));
        REQUIRE(finished);
        REQUIRE(process.exitStatus() == QProcess::NormalExit);
        REQUIRE(process.exitCode() == 0);
    }

    const fez::testing::TreeComparison comparison = fez::testing::compareTrees(goldenDir, outputDir.path());
    INFO(toStd(comparison.describe()));
    CHECK(comparison.identical());
}
