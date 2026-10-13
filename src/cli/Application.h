#pragma once

#include <QtCore/QCoreApplication>

namespace fez
{
class AssetRepository;
}

class Application : public QCoreApplication
{
    Q_OBJECT;

public:
    using QCoreApplication::QCoreApplication;

public slots:
    void onRun();

private:
    // All three phases share one repository (trile sets, art objects, textures are loaded once per run).
    void processArtObjects(fez::AssetRepository& repository, const QString& input_path, const QString& output_path);
    void processTrileSets(fez::AssetRepository& repository, const QString& input_path, const QString& output_path);
    void processLevels(fez::AssetRepository& repository, const QString& input_path, const QString& output_path);
};