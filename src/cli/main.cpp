#include "Options.h"
#include "Summary.h"

#include "log/Log.h"
#include "pipeline/Pipeline.h"
#include "repository/AssetRepository.h"

#include <QtCore/QCoreApplication>
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

    // Debug messages only with --verbose; Qt's own messages go through the same facade.
    fez::log::routeQtMessages();

    if(options->m_Verbose)
        fez::log::setLevel(fez::log::Level::Debug);

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

    // The summary goes to stderr through the facade (one message, so it stays together); it ends with a line break.
    if(const auto summary = fez::cli::formatSummary(phases); !summary.isEmpty())
        fez::log::info("{}", summary.chopped(1));

    return fez::cli::exitCode(phases);
}
