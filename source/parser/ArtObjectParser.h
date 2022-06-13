#pragma once

#include "geom/Geometry.h"
#include "parser/GeometryParser.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class aiScene;
class aiMesh;
class aiMaterial;

class ArtObjectParser
{
public:
    ArtObjectParser();
    ~ArtObjectParser();

    void parse(const QString& path) noexcept;

private:
    void writeObj();

    aiMesh* allocateMesh();
    aiMaterial* allocateMaterial();

    void deallocateScene();

private:
    QDomDocument m_Document;
    QString m_Name;
    QString m_Path;

    aiScene* m_Scene;

    GeometryParser m_GeomParser;
};