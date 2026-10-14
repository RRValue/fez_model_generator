// End-to-end tests of the converter's command line: --only, --jobs, --verbose, usage errors and exit codes
// (0 all assets converted, 1 at least one asset failed, 2 usage error). Each test runs the real executable with
// QProcess on a temporary copy of tests/data/mini_export into a fresh output directory.

#include "support/TestData.h"
#include "support/TreeCompare.h"

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>

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

// Recursively copies every file below source into target (which must exist).
bool copyTree(const QString& source, const QString& target)
{
    const QDir source_dir(source);
    QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while(it.hasNext())
    {
        const QString file_path = it.next();
        const QString destination = target + "/" + source_dir.relativeFilePath(file_path);

        if(!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(file_path, destination))
            return false;
    }

    return true;
}

// The top-level entries of dir, sorted.
QStringList topLevelEntries(const QString& dir)
{
    return QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
}

struct RunResult
{
    int m_ExitCode = -1;
    QString m_Output; // stdout and stderr merged, with '\n' line ends
};

// Runs the converter with arguments and waits for it to finish.
RunResult runConverter(const QStringList& arguments)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral(FEZ_MODEL_GENERATOR_EXE), arguments);

    const bool finished = process.waitForFinished(60'000);
    // stdout and stderr are text-mode streams on Windows: normalize their CRLF line ends
    const auto output = QString::fromLocal8Bit(process.readAll()).replace("\r\n", "\n");

    INFO("converter output:\n" << toStd(output));
    REQUIRE(finished);
    REQUIRE(process.exitStatus() == QProcess::NormalExit);

    return {.m_ExitCode = process.exitCode(), .m_Output = output};
}

// The mini export copied into a fresh temporary input directory, plus a fresh output directory.
struct MiniCopy
{
    QTemporaryDir m_Input;
    QTemporaryDir m_Output;

    MiniCopy()
    {
        REQUIRE(m_Input.isValid());
        REQUIRE(m_Output.isValid());
        REQUIRE(copyTree(fez::testing::testDataPath("mini_export"), m_Input.path()));
    }

    QStringList arguments(const QStringList& extra = {}) const
    {
        return QStringList{"--input", m_Input.path(), "--output", m_Output.path()} + extra;
    }
};

void requireTree(const QString& expected, const QString& actual)
{
    const auto comparison = fez::testing::compareTrees(expected, actual);
    INFO(toStd(comparison.describe()));
    CHECK(comparison.identical());
}

} // namespace

TEST_CASE("CLI: --only lv converts only the levels", "[cli][e2e]")
{
    MiniCopy mini;

    const auto result = runConverter(mini.arguments({"--only", "lv"}));
    INFO(toStd(result.m_Output));
    CHECK(result.m_ExitCode == 0);

    CHECK(topLevelEntries(mini.m_Output.path()) == QStringList{"lv_export"});
    requireTree(fez::testing::testDataPath("mini_expected/lv_export"), mini.m_Output.path() + "/lv_export");

    CHECK(result.m_Output.contains("Levels: 1 of 1 converted, 0 failed"));
    CHECK_FALSE(result.m_Output.contains("Art objects:"));
    CHECK_FALSE(result.m_Output.contains("Trile sets:"));
}

TEST_CASE("CLI: --only ao,ts converts art objects and trile sets only", "[cli][e2e]")
{
    MiniCopy mini;

    const auto result = runConverter(mini.arguments({"--only", "ao,ts"}));
    INFO(toStd(result.m_Output));
    CHECK(result.m_ExitCode == 0);

    CHECK(topLevelEntries(mini.m_Output.path()) == QStringList{"ao_export", "ts_export"});
    requireTree(fez::testing::testDataPath("mini_expected/ao_export"), mini.m_Output.path() + "/ao_export");
    requireTree(fez::testing::testDataPath("mini_expected/ts_export"), mini.m_Output.path() + "/ts_export");
}

TEST_CASE("CLI: --jobs 1 --verbose converts everything and prints the summary", "[cli][e2e]")
{
    MiniCopy mini;

    const auto result = runConverter(mini.arguments({"--jobs", "1", "--verbose"}));
    INFO(toStd(result.m_Output));
    CHECK(result.m_ExitCode == 0);

    requireTree(fez::testing::testDataPath("mini_expected"), mini.m_Output.path());

    CHECK(result.m_Output.contains("Art objects: 1 of 1 converted, 0 failed"));
    CHECK(result.m_Output.contains("Trile sets: 1 of 1 converted, 0 failed"));
    CHECK(result.m_Output.contains("Levels: 1 of 1 converted, 0 failed"));
    CHECK_FALSE(result.m_Output.contains("Failed files"));
}

TEST_CASE("CLI: a second run into the same output folder gives the same output as a fresh run", "[cli][e2e]")
{
    MiniCopy mini;

    const auto first = runConverter(mini.arguments());
    INFO(toStd(first.m_Output));
    CHECK(first.m_ExitCode == 0);
    requireTree(fez::testing::testDataPath("mini_expected"), mini.m_Output.path());

    // a stale texture left by an earlier run is replaced
    QFile stale(mini.m_Output.path() + "/lv_export/MINI/mini_npc/idle.ani.png");
    REQUIRE(stale.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(stale.write("stale") == 5);
    stale.close();

    const auto second = runConverter(mini.arguments());
    INFO(toStd(second.m_Output));
    CHECK(second.m_ExitCode == 0);
    CHECK_FALSE(second.m_Output.contains("Failed files"));
    requireTree(fez::testing::testDataPath("mini_expected"), mini.m_Output.path());
}

TEST_CASE("CLI: usage errors exit with code 2 and write nothing", "[cli][e2e]")
{
    MiniCopy mini;

    struct Case
    {
        QStringList m_Arguments;
        const char* m_Fragment;
    };

    for(const auto& usage : {Case{mini.arguments({"--only", "xy"}), "--only"},
                             Case{mini.arguments({"--jobs", "0"}), "--jobs"},
                             Case{{"--output", mini.m_Output.path()}, "--input is required"},
                             Case{{"--input", mini.m_Input.path() + "/missing"}, "does not exist"}})
    {
        INFO("arguments: " << toStd(usage.m_Arguments.join(' ')));
        const auto result = runConverter(usage.m_Arguments);
        INFO(toStd(result.m_Output));
        CHECK(result.m_ExitCode == 2);
        CHECK(result.m_Output.contains(usage.m_Fragment));
        CHECK(topLevelEntries(mini.m_Output.path()).isEmpty());
    }
}

TEST_CASE("CLI: --help exits with code 0", "[cli][e2e]")
{
    const auto result = runConverter({"--help"});
    INFO(toStd(result.m_Output));
    CHECK(result.m_ExitCode == 0);
    CHECK(result.m_Output.contains("--only"));
}

TEST_CASE("CLI: a corrupted asset gives exit code 1 and is listed in the summary", "[cli][e2e]")
{
    MiniCopy mini;

    // a truncated copy of the art object next to the intact one
    QFile original(mini.m_Input.path() + "/art objects/mini_ao.xml");
    REQUIRE(original.open(QIODevice::ReadOnly));
    const auto truncated = original.readAll().left(120);
    REQUIRE(truncated.size() == 120);

    QFile broken(mini.m_Input.path() + "/art objects/broken_ao.xml");
    REQUIRE(broken.open(QIODevice::WriteOnly));
    REQUIRE(broken.write(truncated) == truncated.size());
    broken.close();

    const auto result = runConverter(mini.arguments());
    INFO(toStd(result.m_Output));
    CHECK(result.m_ExitCode == 1);

    CHECK(result.m_Output.contains("Art objects: 1 of 2 converted, 1 failed"));
    CHECK(result.m_Output.contains("Failed files: 1"));
    CHECK(result.m_Output.contains("broken_ao.xml\n"));
    CHECK(result.m_Output.contains("XmlSyntax: "));
    CHECK_FALSE(result.m_Output.contains("mini_ao.xml"));

    // every other asset still converts
    requireTree(fez::testing::testDataPath("mini_expected"), mini.m_Output.path());
}

TEST_CASE("CLI: without --verbose a successful run prints only the summary", "[cli][e2e][log]")
{
    MiniCopy mini;

    const auto result = runConverter(mini.arguments());
    CHECK(result.m_ExitCode == 0);

    CHECK(result.m_Output == "Art objects: 1 of 1 converted, 0 failed\n"
                             "Trile sets: 1 of 1 converted, 0 failed\n"
                             "Levels: 1 of 1 converted, 0 failed\n");
}

TEST_CASE("CLI: --verbose adds whole debug lines from parallel workers", "[cli][e2e][log]")
{
    MiniCopy mini;

    const auto result = runConverter(mini.arguments({"--jobs", "4", "--verbose"}));
    CHECK(result.m_ExitCode == 0);

    CHECK(result.m_Output.contains("parsing: "));
    CHECK(result.m_Output.contains("Write: "));
    CHECK(result.m_Output.endsWith("Art objects: 1 of 1 converted, 0 failed\n"
                                   "Trile sets: 1 of 1 converted, 0 failed\n"
                                   "Levels: 1 of 1 converted, 0 failed\n"));

    // every line is one whole message
    const auto lines = result.m_Output.split('\n', Qt::SkipEmptyParts);
    for(const auto& line : lines)
    {
        INFO("line: " << toStd(line));
        CHECK((line.startsWith("parsing: ") || line.startsWith("trile: ") || line.startsWith("Write: ") ||
               line.endsWith(" converted, 0 failed")));
    }
}
