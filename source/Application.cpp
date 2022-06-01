#include "Application.h"

#include "parser/ArtObjectParser.h"

#include <QtWidgets/QFileDialog>

void Application::onRun()
{
    const auto file = QFileDialog::getOpenFileName();

    ArtObjectParser parser;
    parser.parse(file);

    exit();
}