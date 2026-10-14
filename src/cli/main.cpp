#include "pipeline/Pipeline.h"
#include "repository/AssetRepository.h"

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>

namespace
{

constexpr auto usage_error_code = 2;

// Lists every failed file with its errors (the summary and exit codes follow in Task 25).
void logFailures(const fez::ConversionReport& report)
{
    for(const auto& file : report.m_Files)
    {
        for(const auto& error : file.m_Errors)
        {
            qWarning().noquote() << "Failed:" << file.m_File << "-" << QString::fromUtf8(fez::toString(error.kind)) << QString::fromStdString(error.message) << "-"
                                 << QString::fromStdString(error.source);
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Converts a FEZ export into OBJ models.");
    parser.addHelpOption();

    const auto input_option = QCommandLineOption("input", "FEZ export root (contains \"art objects\", \"trile sets\", \"levels\", ...).", "dir");
    const auto output_option = QCommandLineOption("output", "Output root for ao_export/, ts_export/ and lv_export/. Defaults to --input.", "dir");

    parser.addOption(input_option);
    parser.addOption(output_option);

    parser.process(QCoreApplication::arguments());

    auto err = QTextStream(stderr);

    if(!parser.isSet(input_option))
    {
        err << "Error: --input is required.\n\n" << parser.helpText();
        err.flush();
        return usage_error_code;
    }

    const auto input_path = parser.value(input_option);
    const auto output_path = parser.isSet(output_option) ? parser.value(output_option) : input_path;

    if(!QFileInfo(input_path).isDir())
    {
        err << "Error: input directory \"" << input_path << "\" does not exist.\n";
        err.flush();
        return usage_error_code;
    }

    // All three phases share one repository: trile sets, art objects and textures are loaded once per run.
    auto repository = fez::AssetRepository(input_path);

    logFailures(fez::convertArtObjects(input_path, output_path, repository));
    logFailures(fez::convertTrileSets(input_path, output_path, repository));
    logFailures(fez::convertLevels(input_path, output_path, repository));

    return 0;
}
