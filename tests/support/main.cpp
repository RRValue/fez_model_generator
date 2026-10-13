// fez_tests entry point. Tests use QProcess, QImage and QtConcurrent, which are only reliable with an
// application object, so a QCoreApplication lives for the whole Catch2 session.

#include <QtCore/QCoreApplication>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
