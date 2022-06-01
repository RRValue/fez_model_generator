#include "Application.h"

#include <QtCore/QTimer>

#include <memory>

int main(int argc, char* argv[])
{
    const auto application = std::make_shared<Application>(argc, argv);

    QTimer::singleShot(0, application.get(), &Application::onRun);

    return application->exec();
}