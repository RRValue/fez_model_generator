#pragma once

#include "model/Geometry.h"

#include <QtCore/QString>

struct aiScene;
struct aiMesh;
struct aiMaterial;

class GeometryWriter
{
public:
    GeometryWriter(const QString& path);
    ~GeometryWriter();

    void writeObj(const Geometry& geometry);

private:
    aiMesh* allocateMesh();
    aiMaterial* allocateMaterial();

    void deallocateScene();

private:
    QString m_Path;
    
    aiScene* m_Scene;
};
