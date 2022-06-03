#include "Application.h"

#include "parser/ArtObjectParser.h"

#include <QtCore/QDirIterator>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QFileDialog>

void Application::onRun()
{
    const auto path =
        QFileDialog::getExistingDirectory(nullptr, "ArtObject", QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DesktopLocation));

    auto xml_iter = QDirIterator(path, {"*.xml"}, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot | QDir::Filter::NoSymLinks);

    while(xml_iter.hasNext())
    {
        const auto file = xml_iter.next();

        ArtObjectParser parser;
        parser.parse(file);
    }

    /*ArtObjectParser parser;
    parser.parse("C:/Users/pkruttke/Desktop/fez_data/export/art objects/fractal_globeao.xml");*/

    exit();
}