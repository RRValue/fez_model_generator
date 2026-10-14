#pragma once

#include "pipeline/Pipeline.h"

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <expected>

namespace fez::cli
{

// Exit codes of fez_model_generator.
inline constexpr int success_exit_code = 0;     // every asset converted (also --help)
inline constexpr int failure_exit_code = 1;     // at least one asset failed (see the summary)
inline constexpr int usage_error_exit_code = 2; // bad command line

// The validated command line.
struct Options
{
    QString m_Input;  // existing FEZ export root
    QString m_Output; // output root; defaults to m_Input
    bool m_ArtObjects = true; // --only: which phases run (all three by default)
    bool m_TrileSets = true;
    bool m_Levels = true;
    int m_Jobs = defaultJobCount(); // --jobs, at least 1
    bool m_Verbose = false;         // --verbose: show debug messages
};

// Parsing ended without options: --help (exit code 0, m_Text for stdout) or a usage error (exit code 2, m_Text is the
// error message followed by the help text, for stderr).
struct OptionsExit
{
    int m_ExitCode;
    QString m_Text;
};

// Parses arguments (arguments[0] is the program name):
//   --input <dir>      required, must be an existing directory
//   --output <dir>     defaults to --input
//   --only <kinds>     comma-separated subset of ao, ts, lv (may be given more than once); default all
//   --jobs <N>         positive integer; default defaultJobCount()
//   --verbose
//   --help
// Unknown options and positional arguments are usage errors.
std::expected<Options, OptionsExit> parseOptions(const QStringList& arguments);

} // namespace fez::cli
