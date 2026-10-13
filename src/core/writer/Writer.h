#pragma once

#include "model/Geometry.h"

#include <QtCore/QString>

struct aiScene;
struct aiMesh;
struct aiMaterial;

class Writer
{
    using Textures = std::vector<std::pair<QString, QString>>;
    
    using MeshAllocation = std::pair<unsigned int, aiMesh*>;
    using MaterialAllocation = std::pair<unsigned int, aiMaterial*>;
    using MeshId = std::optional<unsigned int>;

public:
    Writer(const QString& path);
    ~Writer();

public:
    void save();

protected:
    MeshAllocation allocateMesh();
    MaterialAllocation allocateMaterial();

    void deallocateScene();

    MeshId addGeometry(const Geometry& geometry);

protected:
    QString m_SaveName;

protected:
    QString m_Path;
    
    aiScene* m_Scene;
    Textures m_Textures;
};
