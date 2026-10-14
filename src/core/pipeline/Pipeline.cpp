#include "pipeline/Pipeline.h"

#include "export/ObjExport.h"
#include "export/SceneComposition.h"
#include "export/TextureCopier.h"
#include "level/ResolveLevel.h"
#include "log/Log.h"
#include "parser/level/LevelReader.h"
#include "repository/AssetRepository.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>

#include <QtConcurrent/QtConcurrentMap>

#include <algorithm>
#include <exception>
#include <expected>
#include <string>
#include <utility>

namespace fez
{

namespace
{

using ExportResult = std::expected<void, Error>;

// The *.xml files directly in dir (no symlinks), sorted by path so the report order does not depend on the file system.
QStringList listXmlFiles(const QString& dir)
{
    auto iterator = QDirIterator(dir, {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList files;

    while(iterator.hasNext())
        files.push_back(iterator.next());

    files.sort();

    return files;
}

// Exports <dir>/<name>.obj/.mtl and, only when that succeeded, copies the scene's textures into dir (as the old
// Writer::save). On an export error the files of this scene are missing from the output. A texture that cannot be
// copied does not stop the other copies; the first copy error (kind Io) is returned.
ExportResult exportScene(const ComposedScene& composed, const QString& dir, const QString& name, TextureCopier& texture_copier)
{
    if(auto exported = exportObj(*composed.m_Scene, dir, name); !exported)
        return exported;

    ExportResult result;

    for(const auto& texture : composed.m_Textures)
    {
        // the copier skips a destination it already copied, so a scene's duplicate entries cost nothing
        if(auto copied = texture_copier.copy(texture.m_Source, dir + "/" + texture.m_Name); !copied && result)
            result = std::unexpected(std::move(copied.error()));
    }

    return result;
}

// The output directory of one geometry export is created before its scene is built, as the old GeometryWriter's
// constructor did (also when the geometry is empty and nothing is exported). A scene error (a plane texture that
// cannot be loaded) is returned.
ExportResult exportGeometry(const Geometry& geometry, const QString& dir, TextureCopier& texture_copier)
{
    if(auto created = makeDirectory(dir); !created)
        return created;

    const auto composed = buildGeometryScene(geometry);

    if(!composed)
        return std::unexpected(composed.error());

    if(!composed->has_value())
        return {};

    return exportScene(**composed, dir, geometry.m_Name, texture_copier);
}

// Converts every file of dir with convert_one(file, report) on a pool of options.m_Jobs threads; one FileReport per
// file in list order. convert_one adds the file's errors to the report.
template<typename ConvertOne>
ConversionReport convertFiles(const QString& dir, const PipelineOptions& options, ConvertOne convert_one)
{
    const auto files = listXmlFiles(dir);

    QThreadPool pool;
    pool.setMaxThreadCount(std::max(1, options.m_Jobs));

    const auto convert = [&convert_one](const QString& file) -> FileReport {
        auto report = FileReport{.m_File = file, .m_Errors = {}};

        try
        {
            convert_one(file, report);
        }
        catch(const std::exception& exception)
        {
            report.m_Errors.push_back({ErrorKind::Io, std::string("unexpected exception: ") + exception.what(), file.toStdString()});
        }

        return report;
    };

    const auto results = QtConcurrent::blockingMapped<QList<FileReport>>(&pool, files, convert);

    auto report = ConversionReport{};
    report.m_Files.assign(results.begin(), results.end());

    return report;
}

} // namespace

int defaultJobCount()
{
    return QThread::idealThreadCount();
}

ConversionReport convertArtObjects(const QString& input, const QString& output, AssetRepository& repository, const PipelineOptions& options)
{
    auto texture_copier = TextureCopier();

    // <input>/art objects/<complete base name>.xml is the file itself
    return convertFiles(input + "/art objects", options, [&](const QString& file, FileReport& report) {
        const auto loaded = repository.artObject(QFileInfo(file).completeBaseName());

        if(!loaded)
        {
            report.m_Errors.push_back(loaded.error());
            return;
        }

        const auto& result = *loaded;

        log::debug("Write: {}", result->m_Name);

        if(const auto exported = exportGeometry(*result, output + "/ao_export", texture_copier); !exported)
            report.m_Errors.push_back(exported.error());
    });
}

ConversionReport convertTrileSets(const QString& input, const QString& output, AssetRepository& repository, const PipelineOptions& options)
{
    auto texture_copier = TextureCopier();

    // <input>/trile sets/<complete base name>.xml is the file itself
    return convertFiles(input + "/trile sets", options, [&](const QString& file, FileReport& report) {
        const auto loaded = repository.trileSet(QFileInfo(file).completeBaseName());

        if(!loaded)
        {
            report.m_Errors.push_back(loaded.error());
            return;
        }

        const auto& trile_set = *loaded;

        for(const auto& r : trile_set->m_Triles)
        {
            log::debug("Write: {}", r.second.m_Name);

            if(const auto exported = exportGeometry(r.second, output + "/ts_export/" + trile_set->m_Name, texture_copier); !exported)
                report.m_Errors.push_back(exported.error());
        }
    });
}

ConversionReport convertLevels(const QString& input, const QString& output, AssetRepository& repository, const PipelineOptions& options)
{
    auto texture_copier = TextureCopier();

    return convertFiles(input + "/levels", options, [&](const QString& file, FileReport& report) {
        // parseLevel logs "parsing:" and XML syntax errors; any failure skips the level (no output, as before)
        const auto description = parseLevel(file);

        if(!description)
        {
            report.m_Errors.push_back(description.error());
            return;
        }

        const auto level = resolveLevel(*description, repository);

        if(!level)
        {
            report.m_Errors.push_back(level.error());
            return;
        }

        // a scene error (a plane or NPC texture that cannot be loaded) fails the level before anything is written
        const auto composed = buildLevelScene(*level);

        if(!composed)
        {
            report.m_Errors.push_back(composed.error());
            return;
        }

        const auto dir = output + "/lv_export/" + level->m_LevelName;

        if(const auto exported = exportScene(*composed, dir, level->m_LevelName, texture_copier); !exported)
            report.m_Errors.push_back(exported.error());
    });
}

} // namespace fez
