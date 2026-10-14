#include "pipeline/ConversionReport.h"
#include "pipeline/Pipeline.h"

#include "repository/AssetRepository.h"

#include "support/TestData.h"
#include "support/TreeCompare.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>

#include <string>

// The pipeline converts the three asset kinds of an export (art objects, trile sets, levels), each phase in parallel
// on its own QThreadPool, and reports every input file in input order (sorted file names) as ok or with its errors.
// These tests run it in-process on temporary copies of tests/data/mini_export.

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

void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(content) == content.size());
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

    QString input(const QString& relative) const
    {
        return m_Input.path() + "/" + relative;
    }
};

// Runs the three phases in order with one repository, as the converter does.
struct PhaseReports
{
    fez::ConversionReport m_ArtObjects;
    fez::ConversionReport m_TrileSets;
    fez::ConversionReport m_Levels;
};

PhaseReports convertAll(const QString& input, const QString& output, const fez::PipelineOptions& options = {})
{
    auto repository = fez::AssetRepository(input);

    PhaseReports reports;
    reports.m_ArtObjects = fez::convertArtObjects(input, output, repository, options);
    reports.m_TrileSets = fez::convertTrileSets(input, output, repository, options);
    reports.m_Levels = fez::convertLevels(input, output, repository, options);
    return reports;
}

QStringList fileNames(const fez::ConversionReport& report)
{
    QStringList names;

    for(const auto& entry : report.m_Files)
        names << QFileInfo(entry.m_File).fileName();

    return names;
}

std::string describe(const fez::ConversionReport& report)
{
    std::string text;

    for(const auto& entry : report.m_Files)
    {
        text += toStd(entry.m_File) + (entry.ok() ? ": ok\n" : ":\n");

        for(const auto& error : entry.m_Errors)
            text += "  " + std::string(fez::toString(error.kind)) + " " + error.message + " - " + error.source + "\n";
    }

    return text;
}

void requireAllOk(const fez::ConversionReport& report)
{
    INFO(describe(report));
    CHECK(report.ok());
    CHECK(report.failureCount() == 0);

    for(const auto& entry : report.m_Files)
        CHECK(entry.m_Errors.empty());
}

void requireTree(const QString& expected, const QString& actual)
{
    const auto comparison = fez::testing::compareTrees(expected, actual);
    INFO(toStd(comparison.describe()));
    CHECK(comparison.identical());
}

} // namespace

TEST_CASE("Pipeline: every asset of mini_export is reported ok and the output equals mini_expected", "[pipeline]")
{
    for(const auto jobs : {1, 4})
    {
        INFO("jobs " << jobs);
        MiniCopy mini;

        const auto reports = convertAll(mini.m_Input.path(), mini.m_Output.path(), {.m_Jobs = jobs});

        CHECK(fileNames(reports.m_ArtObjects) == QStringList{"mini_ao.xml"});
        CHECK(fileNames(reports.m_TrileSets) == QStringList{"mini_set.xml"});
        CHECK(fileNames(reports.m_Levels) == QStringList{"mini.xml"});

        // the report names the input file itself
        CHECK(QFileInfo(reports.m_ArtObjects.m_Files.at(0).m_File) == QFileInfo(mini.input("art objects/mini_ao.xml")));
        CHECK(QFileInfo(reports.m_TrileSets.m_Files.at(0).m_File) == QFileInfo(mini.input("trile sets/mini_set.xml")));
        CHECK(QFileInfo(reports.m_Levels.m_Files.at(0).m_File) == QFileInfo(mini.input("levels/mini.xml")));

        requireAllOk(reports.m_ArtObjects);
        requireAllOk(reports.m_TrileSets);
        requireAllOk(reports.m_Levels);

        requireTree(fez::testing::testDataPath("mini_expected"), mini.m_Output.path());
    }
}

TEST_CASE("Pipeline: a corrupted art object is reported and every other asset still converts", "[pipeline]")
{
    MiniCopy mini;

    // two truncated copies of the art object, sorted before and after mini_ao.xml
    QFile original(mini.input("art objects/mini_ao.xml"));
    REQUIRE(original.open(QIODevice::ReadOnly));
    const auto truncated = original.readAll().left(120);
    REQUIRE(truncated.size() == 120);
    writeFile(mini.input("art objects/a_broken.xml"), truncated);
    writeFile(mini.input("art objects/z_broken.xml"), truncated);

    const auto reports = convertAll(mini.m_Input.path(), mini.m_Output.path());

    INFO(describe(reports.m_ArtObjects));
    REQUIRE(fileNames(reports.m_ArtObjects) == QStringList{"a_broken.xml", "mini_ao.xml", "z_broken.xml"});
    CHECK_FALSE(reports.m_ArtObjects.ok());
    CHECK(reports.m_ArtObjects.failureCount() == 2);

    for(const auto index : {0, 2})
    {
        const auto& entry = reports.m_ArtObjects.m_Files.at(index);
        REQUIRE(entry.m_Errors.size() == 1);
        CHECK(entry.m_Errors.front().kind == fez::ErrorKind::XmlSyntax);
        CHECK(entry.m_Errors.front().source.find(QFileInfo(entry.m_File).fileName().toStdString()) != std::string::npos);
    }

    CHECK(reports.m_ArtObjects.m_Files.at(1).ok());
    requireAllOk(reports.m_TrileSets);
    requireAllOk(reports.m_Levels);

    // the broken art objects produce no file at all
    requireTree(fez::testing::testDataPath("mini_expected"), mini.m_Output.path());
}

TEST_CASE("Pipeline: a corrupted level is reported, art objects and trile sets still convert", "[pipeline]")
{
    MiniCopy mini;

    // a level without its four sections
    writeFile(mini.input("levels/mini.xml"), "<?xml version='1.0' encoding='UTF-8'?>\n<Level name=\"MINI\" trileSetName=\"mini_set\"/>\n");

    const auto reports = convertAll(mini.m_Input.path(), mini.m_Output.path());

    requireAllOk(reports.m_ArtObjects);
    requireAllOk(reports.m_TrileSets);

    INFO(describe(reports.m_Levels));
    REQUIRE(fileNames(reports.m_Levels) == QStringList{"mini.xml"});
    CHECK(reports.m_Levels.failureCount() == 1);
    REQUIRE(reports.m_Levels.m_Files.at(0).m_Errors.size() == 1);
    CHECK(reports.m_Levels.m_Files.at(0).m_Errors.front().kind == fez::ErrorKind::MissingElement);

    requireTree(fez::testing::testDataPath("mini_expected/ao_export"), mini.m_Output.path() + "/ao_export");
    requireTree(fez::testing::testDataPath("mini_expected/ts_export"), mini.m_Output.path() + "/ts_export");
    CHECK_FALSE(QFileInfo::exists(mini.m_Output.path() + "/lv_export"));
}

TEST_CASE("Pipeline: a level whose art object fails is reported with the repository error", "[pipeline]")
{
    MiniCopy mini;
    REQUIRE(QFile::remove(mini.input("art objects/mini_ao.xml")));

    const auto reports = convertAll(mini.m_Input.path(), mini.m_Output.path());

    CHECK(reports.m_ArtObjects.m_Files.empty());
    requireAllOk(reports.m_TrileSets);

    INFO(describe(reports.m_Levels));
    REQUIRE(reports.m_Levels.m_Files.size() == 1);
    REQUIRE(reports.m_Levels.m_Files.at(0).m_Errors.size() == 1);
    CHECK(reports.m_Levels.m_Files.at(0).m_Errors.front().kind == fez::ErrorKind::FileNotFound);
    CHECK_FALSE(QFileInfo::exists(mini.m_Output.path() + "/lv_export"));
}

TEST_CASE("Pipeline: export errors are reported per file", "[pipeline]")
{
    MiniCopy mini;

    // the output root is a regular file, so no output directory can be created
    const auto output = mini.m_Output.path() + "/not_a_directory";
    writeFile(output, "x");

    const auto reports = convertAll(mini.m_Input.path(), output);

    for(const auto* report : {&reports.m_ArtObjects, &reports.m_TrileSets, &reports.m_Levels})
    {
        INFO(describe(*report));
        REQUIRE(report->m_Files.size() == 1);
        CHECK(report->failureCount() == 1);

        const auto& errors = report->m_Files.at(0).m_Errors;
        REQUIRE_FALSE(errors.empty());

        for(const auto& error : errors)
            CHECK(error.kind == fez::ErrorKind::Export);
    }

    // a trile set reports one error per trile that failed to export (mini_set has two triles)
    CHECK(reports.m_TrileSets.m_Files.at(0).m_Errors.size() == 2);
}

TEST_CASE("Pipeline: a missing asset folder gives an empty report", "[pipeline]")
{
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());

    const auto reports = convertAll(input.path(), output.path());

    CHECK(reports.m_ArtObjects.m_Files.empty());
    CHECK(reports.m_TrileSets.m_Files.empty());
    CHECK(reports.m_Levels.m_Files.empty());
    CHECK(reports.m_ArtObjects.ok());
    CHECK(QDir(output.path()).isEmpty());
}

TEST_CASE("ConversionReport: ok, failure count and append", "[pipeline]")
{
    fez::ConversionReport report;
    CHECK(report.ok());
    CHECK(report.failureCount() == 0);

    report.m_Files.push_back({.m_File = "a.xml", .m_Errors = {}});
    report.m_Files.push_back({.m_File = "b.xml", .m_Errors = {{fez::ErrorKind::BadValue, "bad", "b.xml"}, {fez::ErrorKind::Export, "export", "out"}}});
    CHECK_FALSE(report.ok());
    CHECK(report.failureCount() == 1);
    CHECK(report.m_Files.at(0).ok());
    CHECK_FALSE(report.m_Files.at(1).ok());

    fez::ConversionReport other;
    other.m_Files.push_back({.m_File = "c.xml", .m_Errors = {{fez::ErrorKind::Io, "io", "c.xml"}}});
    report.append(other);

    REQUIRE(report.m_Files.size() == 3);
    CHECK(report.m_Files.at(2).m_File == "c.xml");
    CHECK(report.failureCount() == 2);
}

TEST_CASE("PipelineOptions: jobs default to QThread::idealThreadCount", "[pipeline]")
{
    CHECK(fez::PipelineOptions{}.m_Jobs == fez::defaultJobCount());
    CHECK(fez::defaultJobCount() >= 1);
}
