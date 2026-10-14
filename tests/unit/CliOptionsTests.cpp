#include "Options.h"
#include "Summary.h"

#include "pipeline/ConversionReport.h"
#include "pipeline/Pipeline.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>

#include <string>

// Command-line parsing (fez::cli::parseOptions) and the failure summary (fez::cli::formatSummary) of the converter,
// tested in-process. The converter executable itself is driven via QProcess in tests/e2e/CliTests.cpp.

namespace
{

std::string toStd(const QString& text)
{
    return text.toStdString();
}

// parseOptions takes the full argument list including the program name.
auto parse(const QStringList& arguments)
{
    return fez::cli::parseOptions(QStringList{"fez_model_generator"} + arguments);
}

// Expects a usage error (exit code 2) whose text contains fragment.
void requireUsageError(const QStringList& arguments, const QString& fragment)
{
    INFO("arguments: " << toStd(arguments.join(' ')));
    const auto result = parse(arguments);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().m_ExitCode == fez::cli::usage_error_exit_code);
    INFO("text: " << toStd(result.error().m_Text));
    CHECK(result.error().m_Text.contains(fragment));
}

fez::FileReport okFile(const QString& file)
{
    return {.m_File = file, .m_Errors = {}};
}

} // namespace

TEST_CASE("CLI options: --input alone selects every phase with the defaults", "[cli]")
{
    QTemporaryDir input;
    REQUIRE(input.isValid());

    const auto result = parse({"--input", input.path()});
    REQUIRE(result.has_value());

    CHECK(result->m_Input == input.path());
    CHECK(result->m_Output == input.path()); // --output defaults to --input
    CHECK(result->m_ArtObjects);
    CHECK(result->m_TrileSets);
    CHECK(result->m_Levels);
    CHECK(result->m_Jobs == fez::defaultJobCount());
    CHECK_FALSE(result->m_Verbose);
}

TEST_CASE("CLI options: --output, --only, --jobs and --verbose", "[cli]")
{
    QTemporaryDir input;
    REQUIRE(input.isValid());

    SECTION("--output")
    {
        const auto result = parse({"--input", input.path(), "--output", "C:/somewhere/else"});
        REQUIRE(result.has_value());
        CHECK(result->m_Output == "C:/somewhere/else");
    }

    SECTION("--only lv")
    {
        const auto result = parse({"--input", input.path(), "--only", "lv"});
        REQUIRE(result.has_value());
        CHECK_FALSE(result->m_ArtObjects);
        CHECK_FALSE(result->m_TrileSets);
        CHECK(result->m_Levels);
    }

    SECTION("--only ao,ts (order and repetition do not matter)")
    {
        const auto result = parse({"--input", input.path(), "--only", "ts,ao,ts"});
        REQUIRE(result.has_value());
        CHECK(result->m_ArtObjects);
        CHECK(result->m_TrileSets);
        CHECK_FALSE(result->m_Levels);
    }

    SECTION("--only with spaces around the kinds and given twice")
    {
        const auto result = parse({"--input", input.path(), "--only", " ao , lv", "--only", "ts"});
        REQUIRE(result.has_value());
        CHECK(result->m_ArtObjects);
        CHECK(result->m_TrileSets);
        CHECK(result->m_Levels);
    }

    SECTION("--jobs")
    {
        const auto result = parse({"--input", input.path(), "--jobs", "3"});
        REQUIRE(result.has_value());
        CHECK(result->m_Jobs == 3);
    }

    SECTION("--verbose")
    {
        const auto result = parse({"--input", input.path(), "--verbose"});
        REQUIRE(result.has_value());
        CHECK(result->m_Verbose);
    }
}

TEST_CASE("CLI options: usage errors exit with code 2 and say what is wrong", "[cli]")
{
    QTemporaryDir input;
    REQUIRE(input.isValid());
    const QString dir = input.path();

    requireUsageError({}, "--input is required");
    requireUsageError({"--output", dir}, "--input is required");
    requireUsageError({"--input", dir + "/does_not_exist"}, "does not exist");
    requireUsageError({"--input", dir, "--only", "xy"}, "--only");
    requireUsageError({"--input", dir, "--only", "xy"}, "xy");
    requireUsageError({"--input", dir, "--only", "ao,,lv"}, "--only");
    requireUsageError({"--input", dir, "--only", ""}, "--only");
    requireUsageError({"--input", dir, "--only", "LV"}, "--only");
    requireUsageError({"--input", dir, "--jobs", "0"}, "--jobs");
    requireUsageError({"--input", dir, "--jobs", "-2"}, "--jobs");
    requireUsageError({"--input", dir, "--jobs", "many"}, "--jobs");
    requireUsageError({"--input", dir, "--jobs"}, "jobs");
    requireUsageError({"--input", dir, "--frobnicate"}, "frobnicate");
    requireUsageError({"--input", dir, "stray"}, "stray");
}

TEST_CASE("CLI options: --help exits with code 0 and describes every option", "[cli]")
{
    const auto result = parse({"--help"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().m_ExitCode == 0);

    for(const auto* option : {"--input", "--output", "--only", "--jobs", "--verbose"})
    {
        INFO(option);
        CHECK(result.error().m_Text.contains(option));
    }
}

TEST_CASE("CLI summary: counts per phase, failed files with their errors, exit code", "[cli]")
{
    auto art_objects = fez::ConversionReport{};
    art_objects.m_Files = {okFile("in/art objects/a.xml"),
                           {.m_File = "in/art objects/broken.xml",
                            .m_Errors = {{fez::ErrorKind::XmlSyntax, "unexpected end of file", "in/art objects/broken.xml:3:7"}}},
                           okFile("in/art objects/c.xml")};

    auto trile_sets = fez::ConversionReport{};
    trile_sets.m_Files = {{.m_File = "in/trile sets/set.xml",
                           .m_Errors = {{fez::ErrorKind::Export, "cannot write", "out/ts_export/set/one.obj"},
                                        {fez::ErrorKind::Export, "cannot write", "out/ts_export/set/two.obj"}}}};

    auto levels = fez::ConversionReport{};
    levels.m_Files = {okFile("in/levels/level.xml")};

    SECTION("with failures")
    {
        const auto phases = std::vector<fez::cli::PhaseReport>{
            {"Art objects", art_objects}, {"Trile sets", trile_sets}, {"Levels", levels}};

        const auto summary = fez::cli::formatSummary(phases);
        INFO(toStd(summary));

        CHECK(summary.contains("Art objects: 2 of 3 converted, 1 failed"));
        CHECK(summary.contains("Trile sets: 0 of 1 converted, 1 failed"));
        CHECK(summary.contains("Levels: 1 of 1 converted, 0 failed"));
        CHECK(summary.contains("Failed files: 2"));
        CHECK(summary.contains("in/art objects/broken.xml\n"));
        CHECK(summary.contains("XmlSyntax: unexpected end of file (in/art objects/broken.xml:3:7)"));
        CHECK(summary.contains("in/trile sets/set.xml\n"));
        CHECK(summary.contains("Export: cannot write (out/ts_export/set/one.obj)"));
        CHECK(summary.contains("Export: cannot write (out/ts_export/set/two.obj)"));
        CHECK_FALSE(summary.contains("in/art objects/a.xml"));
        CHECK_FALSE(summary.contains("in/levels/level.xml"));

        CHECK(fez::cli::exitCode(phases) == fez::cli::failure_exit_code);
    }

    SECTION("all ok (and phases not run are not listed)")
    {
        const auto phases = std::vector<fez::cli::PhaseReport>{{"Levels", levels}};

        const auto summary = fez::cli::formatSummary(phases);
        INFO(toStd(summary));

        CHECK(summary == "Levels: 1 of 1 converted, 0 failed\n");
        CHECK(fez::cli::exitCode(phases) == fez::cli::success_exit_code);
    }
}
