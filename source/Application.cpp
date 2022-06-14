#include "Application.h"

#include "parser/ArtObjectParser.h"
#include "parser/TrileSetParser.h"
#include "writer/GeometryWriter.h"

#include <QtCore/QDirIterator>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QFileDialog>

using Geometries = std::list<Geometry>;

void Application::onRun()
{
    const auto path =
        QFileDialog::getExistingDirectory(nullptr, "Export", QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DesktopLocation));

    auto ao_xml_iter = QDirIterator(path + "/art objects", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);
    auto ts_xml_iter = QDirIterator(path + "/trile sets", {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    Geometries geometries;

    while(ao_xml_iter.hasNext())
    {
        const auto file = ao_xml_iter.next();

        ArtObjectParser parser;
        auto result = parser.parse(file);

        if(result)
            geometries.push_back(std::move(*result));
    }

    while(ts_xml_iter.hasNext())
    {
        const auto file = ts_xml_iter.next();

        TrileSetParser parser;
        auto result = parser.parse(file);

        for(auto& r : result)
            geometries.push_back(std::move(r));
    }

    for(const auto& r : geometries)
    {
        qDebug() << "Write: " << r.m_Name << " to " << r.m_OutPath;

        GeometryWriter(r.m_OutPath).writeObj(r);
    }


    exit();
}