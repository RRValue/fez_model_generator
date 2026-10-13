#pragma once

#include <QtCore/QCoreApplication>

class Application : public QCoreApplication
{
    Q_OBJECT;

public:
    using QCoreApplication::QCoreApplication;

public slots:
    void onRun();

private:
    void processArtObjects(const QString& input_path, const QString& output_path);
    void processTrileSets(const QString& input_path, const QString& output_path);
    void processLevels(const QString& input_path, const QString& output_path);
};