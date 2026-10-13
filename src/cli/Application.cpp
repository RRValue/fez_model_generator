#include "Application.h"

#include "parser/ArtObjectParser.h"
#include "parser/LevelParser.h"
#include "parser/TrileSetParser.h"

#include "writer/GeometryWriter.h"
#include "writer/LevelWriter.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureSynchronizer>
#include <QtCore/QTextStream>

#include <QtConcurrent/QtConcurrentRun>

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

    processArtObjects(input_path, output_path);
    processTrileSets(input_path, output_path);
    processLevels(input_path, output_path);

    exit();
}

void Application::processArtObjects(const QString& input_path, const QString& output_path)
{
    auto ao_xml_iter = QDirIterator(input_path + "/art objects", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList art_objects_files;

    while(ao_xml_iter.hasNext())
        art_objects_files.push_back(ao_xml_iter.next());

    const auto export_function = [](const auto& file, const auto& path) -> void{
        fez::ArtObjectParser parser;
        const auto result = parser.parse(file);

        if(!result)
            return;

        qDebug() << "Write: " << result->m_Name;

        fez::GeometryWriter(path + "/ao_export").writeObj(*result);
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : art_objects_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, output_path));

    waiter.waitForFinished();
}

void Application::processTrileSets(const QString& input_path, const QString& output_path)
{
    auto ts_xml_iter = QDirIterator(input_path + "/trile sets", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList trile_set_files;

    while(ts_xml_iter.hasNext())
        trile_set_files.push_back(ts_xml_iter.next());

    const auto export_function = [](const auto& file, const auto& path) -> void {
        fez::TrileSetParser parser;
        const auto result = parser.parse(file);
        const auto& set_name = parser.getSetName();

        for(const auto& r : result)
        {
            qDebug() << "Write: " << r.second.m_Name;

            fez::GeometryWriter(path + "/ts_export/" + set_name).writeObj(r.second);
        }
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : trile_set_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, output_path));

    waiter.waitForFinished();
}

void Application::processLevels(const QString& input_path, const QString& output_path)
{
    auto lvl_xml_iter = QDirIterator(input_path + "/levels", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList level_files;

    while(lvl_xml_iter.hasNext())
        level_files.push_back(lvl_xml_iter.next());

    const auto export_function = [](const auto& file, const auto& path) -> void {
        fez::LevelParser parser;
        const auto level = parser.parse(file);

        if(!level)
            return;

        fez::LevelWriter(path + "/lv_export/" + level->m_LevelName).writeLevel(*level);
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : level_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, output_path));

    waiter.waitForFinished();

    /*const auto level = LevelParser().parse("Z:/fez_data/export/levels/villageville_2d.xml");
    LevelWriter("Z:/fez_data/export/lv_export/" + level->m_LevelName).writeLevel(*level);*/
}