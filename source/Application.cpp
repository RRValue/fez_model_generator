#include "Application.h"

#include "parser/ArtObjectParser.h"
#include "parser/LevelParser.h"
#include "parser/TrileSetParser.h"

#include "writer/GeometryWriter.h"
#include "writer/LevelWriter.h"

#include <QtCore/QDirIterator>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QFileDialog>

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

    while(ao_xml_iter.hasNext())
    {
        const auto file = ao_xml_iter.next();

        ArtObjectParser parser;
        const auto result = parser.parse(file);

        if(!result)
            continue;

        qDebug() << "Write: " << result->m_Name;

        GeometryWriter(path + "/ao_export").writeObj(*result);
    }
}

void Application::processTrileSets(const QString& path)
{
    auto ts_xml_iter = QDirIterator(path + "/trile sets", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    while(ts_xml_iter.hasNext())
    {
        const auto file = ts_xml_iter.next();

        TrileSetParser parser;
        const auto result = parser.parse(file);
        const auto& set_name = parser.getSetName();

        for(const auto& r : result)
        {
            qDebug() << "Write: " << r.second.m_Name;

            GeometryWriter(path + "/ts_export/" + set_name).writeObj(r.second);
        }
    }
}

void Application::processLevels(const QString& path)
{
    auto lvl_xml_iter = QDirIterator(path + "/levels", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    while(lvl_xml_iter.hasNext())
    {
        const auto file = lvl_xml_iter.next();

        LevelParser parser;
        const auto level = parser.parse(file);
        
        if(!level)
            break;

        LevelWriter(path + "/lv_export/" + level->m_LevelName).writeLevel(*level);
    }
}