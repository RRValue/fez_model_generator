#include "Application.h"

#include "parser/ArtObjectParser.h"
#include "parser/LevelParser.h"
#include "parser/TrileSetParser.h"

#include "writer/GeometryWriter.h"
#include "writer/LevelWriter.h"

#include <QtCore/QDirIterator>
#include <QtCore/QStandardPaths>
#include <QtCore/QFutureSynchronizer>

#include <QtWidgets/QFileDialog>

#include <QtConcurrent/QtConcurrentRun>

void Application::onRun()
{
    const auto path = QFileDialog::getExistingDirectory(nullptr, "Export", QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DesktopLocation));
    
    processArtObjects(path);
    processTrileSets(path);
    processLevels(path);

    exit();
}

void Application::processArtObjects(const QString& path)
{
    auto ao_xml_iter = QDirIterator(path + "/art objects", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList art_objects_files;

    while(ao_xml_iter.hasNext())
        art_objects_files.push_back(ao_xml_iter.next());

    const auto export_function = [](const auto& file, const auto& path) -> void{
        ArtObjectParser parser;
        const auto result = parser.parse(file);

        if(!result)
            return;

        qDebug() << "Write: " << result->m_Name;

        GeometryWriter(path + "/ao_export").writeObj(*result);
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : art_objects_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, path));

    waiter.waitForFinished();
}

void Application::processTrileSets(const QString& path)
{
    auto ts_xml_iter = QDirIterator(path + "/trile sets", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList trile_set_files;

    while(ts_xml_iter.hasNext())
        trile_set_files.push_back(ts_xml_iter.next());

    const auto export_function = [](const auto& file, const auto& path) -> void {
        TrileSetParser parser;
        const auto result = parser.parse(file);
        const auto& set_name = parser.getSetName();

        for(const auto& r : result)
        {
            qDebug() << "Write: " << r.second.m_Name;

            GeometryWriter(path + "/ts_export/" + set_name).writeObj(r.second);
        }
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : trile_set_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, path));

    waiter.waitForFinished();
}

void Application::processLevels(const QString& path)
{
    auto lvl_xml_iter = QDirIterator(path + "/levels", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    QStringList level_files;

    while(lvl_xml_iter.hasNext())
        level_files.push_back(lvl_xml_iter.next());

    const auto export_function = [](const auto& file, const auto& path) -> void {
        LevelParser parser;
        const auto level = parser.parse(file);

        if(!level)
            return;

        LevelWriter(path + "/lv_export/" + level->m_LevelName).writeLevel(*level);
    };

    auto waiter = QFutureSynchronizer<void>();

    for(const auto& file : level_files)
        waiter.addFuture(QtConcurrent::run(export_function, file, path));

    waiter.waitForFinished();
}