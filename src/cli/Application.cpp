#include "Application.h"

#include "export/ObjExport.h"
#include "export/SceneComposition.h"
#include "export/TextureCopier.h"
#include "level/ResolveLevel.h"
#include "parser/level/LevelReader.h"
#include "repository/AssetRepository.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureSynchronizer>
#include <QtCore/QTextStream>

#include <QtConcurrent/QtConcurrentRun>

namespace
{

void logExportError(const fez::Error& error)
{
    qWarning().noquote() << "Export failed:" << QString::fromStdString(error.message) << "-" << QString::fromStdString(error.source);
}

// Exports <dir>/<name>.obj/.mtl and, only when that succeeded, copies the scene's textures into dir (as the old
// Writer::save). An export error is logged; the files of this scene are then missing from the output.
void exportScene(const fez::ComposedScene& composed, const QString& dir, const QString& name, fez::TextureCopier& texture_copier)
{
    const auto exported = fez::exportObj(*composed.m_Scene, dir, name);

    if(!exported)
    {
        logExportError(exported.error());
        return;
    }

    for(const auto& texture : composed.m_Textures)
        texture_copier.copy(texture.m_Source, dir + "/" + texture.m_Name);
}

// The output directory of one geometry export is created before its scene is built, as the old GeometryWriter's
// constructor did (also when the geometry gets no mesh and nothing is exported).
void exportGeometry(const fez::Geometry& geometry, const QString& dir, fez::TextureCopier& texture_copier)
{
    if(const auto created = fez::makeDirectory(dir); !created)
    {
        logExportError(created.error());
        return;
    }

    const auto composed = fez::buildGeometryScene(geometry);

    if(!composed)
        return;

    exportScene(*composed, dir, geometry.m_Name, texture_copier);
}

} // namespace

void Application::onRun()
{
    static constexpr auto usage_error_code = 2;

    QCommandLineParser parser;
    parser.setApplicationDescription("Converts a FEZ export into OBJ models.");
    parser.addHelpOption();

    const auto input_option = QCommandLineOption("input", "FEZ export root (contains \"art objects\", \"trile sets\", \"levels\", ...).", "dir");
    const auto output_option = QCommandLineOption("output", "Output root for ao_export/, ts_export/ and lv_export/. Defaults to --input.", "dir");

    parser.addOption(input_option);
    parser.addOption(output_option);

    parser.process(arguments());

    auto err = QTextStream(stderr);

    if(!parser.isSet(input_option))
    {
        err << "Error: --input is required.\n\n" << parser.helpText();
        err.flush();
        exit(usage_error_code);
        return;
    }

    const auto input_path = parser.value(input_option);
    const auto output_path = parser.isSet(output_option) ? parser.value(output_option) : input_path;

    if(!QFileInfo(input_path).isDir())
    {
        err << "Error: input directory \"" << input_path << "\" does not exist.\n";
        err.flush();
        exit(usage_error_code);
        return;
    }

    auto repository = fez::AssetRepository(input_path);
    auto texture_copier = fez::TextureCopier();

    processArtObjects(repository, texture_copier, input_path, output_path);
    processTrileSets(repository, texture_copier, input_path, output_path);
    processLevels(repository, texture_copier, input_path, output_path);

    exit();
}

void Application::processArtObjects(fez::AssetRepository& repository, fez::TextureCopier& texture_copier, const QString& input_path, const QString& output_path)
{
    auto ao_xml_iter = QDirIterator(input_path + "/art objects", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList art_objects_files;

    while(ao_xml_iter.hasNext())
        art_objects_files.push_back(ao_xml_iter.next());

    // <input>/art objects/<complete base name>.xml is the file itself
    const auto export_function = [&repository, &texture_copier](const auto& file, const auto& path) -> void{
        const auto loaded = repository.artObject(QFileInfo(file).completeBaseName());

        if(!loaded)
            return;

        const auto& result = *loaded;

        qDebug() << "Write: " << result->m_Name;

        exportGeometry(*result, path + "/ao_export", texture_copier);
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : art_objects_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, output_path));

    waiter.waitForFinished();
}

void Application::processTrileSets(fez::AssetRepository& repository, fez::TextureCopier& texture_copier, const QString& input_path, const QString& output_path)
{
    auto ts_xml_iter = QDirIterator(input_path + "/trile sets", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList trile_set_files;

    while(ts_xml_iter.hasNext())
        trile_set_files.push_back(ts_xml_iter.next());

    // <input>/trile sets/<complete base name>.xml is the file itself
    const auto export_function = [&repository, &texture_copier](const auto& file, const auto& path) -> void {
        const auto loaded = repository.trileSet(QFileInfo(file).completeBaseName());

        if(!loaded)
            return;

        const auto& trile_set = *loaded;

        for(const auto& r : trile_set->m_Triles)
        {
            qDebug() << "Write: " << r.second.m_Name;

            exportGeometry(r.second, path + "/ts_export/" + trile_set->m_Name, texture_copier);
        }
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : trile_set_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, output_path));

    waiter.waitForFinished();
}

void Application::processLevels(fez::AssetRepository& repository, fez::TextureCopier& texture_copier, const QString& input_path, const QString& output_path)
{
    auto lvl_xml_iter = QDirIterator(input_path + "/levels", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList level_files;

    while(lvl_xml_iter.hasNext())
        level_files.push_back(lvl_xml_iter.next());

    const auto export_function = [&repository, &texture_copier](const auto& file, const auto& path) -> void {
        // parseLevel logs "parsing:" and XML syntax errors; any failure skips the level (no output, as before)
        const auto description = fez::parseLevel(file);

        if(!description)
            return;

        const auto level = fez::resolveLevel(*description, repository);

        if(!level)
            return;

        exportScene(fez::buildLevelScene(*level), path + "/lv_export/" + level->m_LevelName, level->m_LevelName, texture_copier);
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : level_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, output_path));

    waiter.waitForFinished();

    /*const auto level = resolveLevel(*parseLevel("Z:/fez_data/export/levels/villageville_2d.xml"), repository);
    exportScene(buildLevelScene(*level), "Z:/fez_data/export/lv_export/" + level->m_LevelName, level->m_LevelName, texture_copier);*/
}