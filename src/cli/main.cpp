#include "Options.h"
#include "Summary.h"

#include "pipeline/Pipeline.h"
#include "repository/AssetRepository.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTextStream>

#include <vector>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    const auto options = fez::cli::parseOptions(QCoreApplication::arguments());

    if(!options)
    {
        auto stream = QTextStream(options.error().m_ExitCode == fez::cli::success_exit_code ? stdout : stderr);
        stream << options.error().m_Text;
        stream.flush();
        return options.error().m_ExitCode;
    }

    // Debug messages (qDebug) only with --verbose.
    if(!options->m_Verbose)
        QLoggingCategory::setFilterRules("default.debug=false");

    const auto pipeline_options = fez::PipelineOptions{.m_Jobs = options->m_Jobs};

    // All phases share one repository: trile sets, art objects and textures are loaded once per run.
    auto repository = fez::AssetRepository(options->m_Input);
    auto phases = std::vector<fez::cli::PhaseReport>();

    if(options->m_ArtObjects)
        phases.push_back({"Art objects", fez::convertArtObjects(options->m_Input, options->m_Output, repository, pipeline_options)});

    if(options->m_TrileSets)
        phases.push_back({"Trile sets", fez::convertTrileSets(options->m_Input, options->m_Output, repository, pipeline_options)});

    if(options->m_Levels)
        phases.push_back({"Levels", fez::convertLevels(options->m_Input, options->m_Output, repository, pipeline_options)});

    // The summary goes to stderr directly: qWarning ends up in OutputDebugString when the process has no console.
    auto err = QTextStream(stderr);
    err << fez::cli::formatSummary(phases);
    err.flush();

    return fez::cli::exitCode(phases);
}
