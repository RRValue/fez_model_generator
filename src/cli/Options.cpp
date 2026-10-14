#include "Options.h"

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QFileInfo>

namespace fez::cli
{

namespace
{

std::unexpected<OptionsExit> usageError(const QCommandLineParser& parser, const QString& message)
{
    return std::unexpected(OptionsExit{.m_ExitCode = usage_error_exit_code, .m_Text = "Error: " + message + "\n\n" + parser.helpText()});
}

} // namespace

std::expected<Options, OptionsExit> parseOptions(const QStringList& arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Converts a FEZ export into OBJ models.\n"
                                     "Exit codes: 0 all assets converted, 1 at least one asset failed, 2 usage error.");
    const auto help_option = parser.addHelpOption();

    const auto input_option = QCommandLineOption("input", "FEZ export root (contains \"art objects\", \"trile sets\", \"levels\", ...).", "dir");
    const auto output_option = QCommandLineOption("output", "Output root for ao_export/, ts_export/ and lv_export/. Defaults to --input.", "dir");
    const auto only_option = QCommandLineOption("only", "Convert only these kinds: comma-separated subset of ao (art objects), ts (trile sets), lv (levels). Default: all.", "kinds");
    const auto jobs_option = QCommandLineOption("jobs", QString("Number of parallel conversions (positive). Default: %1.").arg(defaultJobCount()), "N");
    const auto verbose_option = QCommandLineOption("verbose", "Print debug messages.");

    parser.addOption(input_option);
    parser.addOption(output_option);
    parser.addOption(only_option);
    parser.addOption(jobs_option);
    parser.addOption(verbose_option);

    if(!parser.parse(arguments))
        return usageError(parser, parser.errorText());

    if(parser.isSet(help_option))
        return std::unexpected(OptionsExit{.m_ExitCode = success_exit_code, .m_Text = parser.helpText()});

    if(!parser.positionalArguments().isEmpty())
        return usageError(parser, QString("unexpected argument \"%1\".").arg(parser.positionalArguments().first()));

    if(!parser.isSet(input_option))
        return usageError(parser, "--input is required.");

    auto options = Options{};
    options.m_Input = parser.value(input_option);
    options.m_Output = parser.isSet(output_option) ? parser.value(output_option) : options.m_Input;
    options.m_Verbose = parser.isSet(verbose_option);

    if(parser.isSet(only_option))
    {
        options.m_ArtObjects = false;
        options.m_TrileSets = false;
        options.m_Levels = false;

        for(const auto& value : parser.values(only_option))
        {
            for(const auto& part : value.split(','))
            {
                const auto kind = part.trimmed();

                if(kind == "ao")
                    options.m_ArtObjects = true;
                else if(kind == "ts")
                    options.m_TrileSets = true;
                else if(kind == "lv")
                    options.m_Levels = true;
                else
                    return usageError(parser, QString("invalid --only value \"%1\" (expected a comma-separated list of ao, ts, lv).").arg(kind));
            }
        }
    }

    if(parser.isSet(jobs_option))
    {
        const auto value = parser.value(jobs_option);
        auto ok = false;
        const auto jobs = value.toInt(&ok);

        if(!ok || jobs < 1)
            return usageError(parser, QString("invalid --jobs value \"%1\" (expected a positive integer).").arg(value));

        options.m_Jobs = jobs;
    }

    if(!QFileInfo(options.m_Input).isDir())
        return usageError(parser, QString("input directory \"%1\" does not exist.").arg(options.m_Input));

    return options;
}

} // namespace fez::cli
