// Mini end-to-end test: converts the hand-written fixture tests/data/mini_export (one art object, one trile set with
// two triles, one level with two trile emplacements (the second with one overlapped trile), one art object instance, a
// static and an animated background plane and one NPC) and compares the whole output tree byte for byte with tests/data/mini_expected.
//
// The converter runs on a temporary COPY of mini_export, so the committed fixture is never touched; afterwards the
// copy must still equal the fixture (the converter writes nothing into its input tree).

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

    // Recursively copies every file below source into target (which must exist). Returns false on the first failure.
    bool copyTree(const QString& source, const QString& target)
    {
        const QDir sourceDir(source);
        QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

        while (it.hasNext())
        {
            const QString filePath = it.next();
            const QString destination = target + QLatin1Char('/') + sourceDir.relativeFilePath(filePath);

            if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(filePath, destination))
                return false;
        }

        return true;
    }

    // Every directory and file below root, as sorted relative paths (directories end with '/').
    QStringList listEntries(const QString& root)
    {
        const QDir rootDir(root);
        QStringList entries;
        QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);

        while (it.hasNext())
        {
            const QString path = it.next();
            entries << rootDir.relativeFilePath(path) + (it.fileInfo().isDir() ? QStringLiteral("/") : QString());
        }

        entries.sort();
        return entries;
    }
} // namespace

TEST_CASE("Mini export converts to the expected output", "[e2e]")
{
    const QString fixtureDir = fez::testing::testDataPath(QStringLiteral("mini_export"));
    const QString expectedDir = fez::testing::testDataPath(QStringLiteral("mini_expected"));

    REQUIRE(QFileInfo(fixtureDir).isDir());
    REQUIRE(QFileInfo(expectedDir).isDir());

    QTemporaryDir inputDir;
    QTemporaryDir outputDir;
    REQUIRE(inputDir.isValid());
    REQUIRE(outputDir.isValid());

    REQUIRE(copyTree(fixtureDir, inputDir.path()));

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral(FEZ_MODEL_GENERATOR_EXE),
                  {QStringLiteral("--input"), inputDir.path(), QStringLiteral("--output"), outputDir.path()});

    const bool finished = process.waitForFinished(60'000);
    const QByteArray output = process.readAll();

    {
        INFO("converter output:\n" << toStd(QString::fromLocal8Bit(output)));
        REQUIRE(finished);
        REQUIRE(process.exitStatus() == QProcess::NormalExit);
        REQUIRE(process.exitCode() == 0);
    }

    for (const char* folder : {"ao_export", "ts_export", "lv_export"})
    {
        INFO(folder);
        CHECK(QFileInfo(outputDir.path() + QLatin1Char('/') + QLatin1String(folder)).isDir());
    }

    const fez::testing::TreeComparison comparison = fez::testing::compareTrees(expectedDir, outputDir.path());
    INFO(toStd(comparison.describe()));
    CHECK(comparison.identical());

    // The input tree is untouched: same files with the same bytes, and no new (possibly empty) directories such as the
    // "trile sets/exported/" folder the converter used to create.
    const fez::testing::TreeComparison inputComparison = fez::testing::compareTrees(fixtureDir, inputDir.path());
    INFO(toStd(inputComparison.describe()));
    CHECK(inputComparison.identical());
    CHECK(toStd(listEntries(inputDir.path()).join(QLatin1Char('\n'))) ==
          toStd(listEntries(fixtureDir).join(QLatin1Char('\n'))));
}
