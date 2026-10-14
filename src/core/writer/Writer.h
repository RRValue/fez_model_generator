#pragma once

#include "export/SceneBuilder.h"
#include "model/Geometry.h"

#include <QtCore/QString>

namespace fez
{

class Writer
{
    using Textures = std::vector<std::pair<QString, QString>>;
    using MeshId = std::optional<SceneBuilder::MeshId>;

public:
    Writer(const QString& path);

public:
    void save();

protected:
    MeshId addGeometry(const Geometry& geometry);

protected:
    QString m_SaveName;

protected:
    QString m_Path;
    
    SceneBuilder m_Builder;
    Textures m_Textures;
};

} // namespace fez
