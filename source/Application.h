#pragma once

#include <QtWidgets/QApplication>

class Application : public QApplication
{
    Q_OBJECT;

public:
    using QApplication::QApplication;

public slots:
    void onRun();

private:
    void processArtObjects(const QString& path);
    void processTrileSets(const QString& path);
    void processLevels(const QString& path);
};