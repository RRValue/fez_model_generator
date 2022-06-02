#include "Application.h"

#include "parser/ArtObjectParser.h"

#include <QtCore/QStandardPaths>

#include <QtWidgets/QFileDialog>

void Application::onRun()
{
    const auto file = QFileDialog::getOpenFileName(nullptr, "ArtObject", QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DesktopLocation));

    ArtObjectParser parser;
    parser.parse(file);

    exit();
}