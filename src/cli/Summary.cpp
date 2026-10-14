#include "Summary.h"

#include "Options.h"

#include <algorithm>

namespace fez::cli
{

QString formatSummary(const std::vector<PhaseReport>& phases)
{
    auto text = QString();
    auto failed_files = std::size_t{0};

    for(const auto& phase : phases)
    {
        const auto total = phase.m_Report.m_Files.size();
        const auto failed = phase.m_Report.failureCount();
        failed_files += failed;

        text += QString("%1: %2 of %3 converted, %4 failed\n").arg(phase.m_Name).arg(total - failed).arg(total).arg(failed);
    }

    if(failed_files == 0)
        return text;

    text += QString("Failed files: %1\n").arg(failed_files);

    for(const auto& phase : phases)
    {
        for(const auto& file : phase.m_Report.m_Files)
        {
            if(file.ok())
                continue;

            text += "  " + file.m_File + "\n";

            for(const auto& error : file.m_Errors)
            {
                text += QString("    %1: %2 (%3)\n")
                            .arg(QString::fromUtf8(toString(error.kind)), QString::fromStdString(error.message), QString::fromStdString(error.source));
            }
        }
    }

    return text;
}

int exitCode(const std::vector<PhaseReport>& phases)
{
    const auto all_ok = std::ranges::all_of(phases, [](const PhaseReport& phase) { return phase.m_Report.ok(); });
    return all_ok ? success_exit_code : failure_exit_code;
}

} // namespace fez::cli
