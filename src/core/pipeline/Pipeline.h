#pragma once

#include "pipeline/ConversionReport.h"

#include <QtCore/QString>

namespace fez
{

class AssetRepository;

// QThread::idealThreadCount(): what a QThreadPool would use on its own.
int defaultJobCount();

struct PipelineOptions
{
    int m_Jobs = defaultJobCount(); // threads of each phase's pool (values below 1 are treated as 1)
};

// The three conversion phases. Each lists "<input>/<asset folder>/*.xml" (files only, sorted by path; a missing folder
// gives an empty report), converts the files in parallel with QtConcurrent::blockingMapped on a QThreadPool owned by
// the call (options.m_Jobs threads) and returns one FileReport per file in that order. All phases of a run share the
// repository, so an asset is loaded once per run; each call copies textures with its own TextureCopier (the phases
// write into separate folders). A failed file is reported and skipped; the other files still convert. An exception
// from converting one file (only std::bad_alloc in practice) is reported as an Io error of that file.
//
// Phase 1 rule: the output is exactly what the old Application wrote.

// "art objects/*.xml": repository.artObject(<base name>), exported as <output>/ao_export/<name>.obj/.mtl plus its
// texture. The directory is created even when the geometry gets no mesh (nothing else is written then).
ConversionReport convertArtObjects(const QString& input, const QString& output, AssetRepository& repository, const PipelineOptions& options = {});

// "trile sets/*.xml": repository.trileSet(<base name>), every trile exported as
// <output>/ts_export/<set name>/<trile name>.obj/.mtl plus the set's texture. A trile that fails to export is
// reported (one error per trile); the remaining triles are still exported.
ConversionReport convertTrileSets(const QString& input, const QString& output, AssetRepository& repository, const PipelineOptions& options = {});

// "levels/*.xml": parseLevel, resolveLevel(repository), buildLevelScene, exported as
// <output>/lv_export/<level name>/<level name>.obj/.mtl plus the textures. Any failure fails the whole level and it
// gets no output.
ConversionReport convertLevels(const QString& input, const QString& output, AssetRepository& repository, const PipelineOptions& options = {});

} // namespace fez
