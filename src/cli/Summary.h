#pragma once

#include "pipeline/ConversionReport.h"

#include <QtCore/QString>

#include <vector>

namespace fez::cli
{

// The report of one phase that ran, with its display name ("Art objects", "Trile sets", "Levels").
struct PhaseReport
{
    QString m_Name;
    ConversionReport m_Report;
};

// One line per phase ("<name>: <ok> of <total> converted, <failed> failed"), then, if any file failed,
// "Failed files: <count>" and every failed file with one indented line per error ("<kind>: <message> (<source>)").
// Every line ends with '\n'.
QString formatSummary(const std::vector<PhaseReport>& phases);

// success_exit_code when every file of every phase converted, failure_exit_code otherwise.
int exitCode(const std::vector<PhaseReport>& phases);

} // namespace fez::cli
