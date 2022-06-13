#pragma once

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class aiMesh;
class aiMaterial;

class GeometryParser
{
public:
    GeometryParser();
    ~GeometryParser();

    bool parseGeometry(const QDomElement& elem, aiMesh* mesh, aiMaterial* material, const QString& textureName);

private:
    bool parseVertices(const QDomElement& elem, aiMesh* mesh);
    bool parseIndices(const QDomElement& elem, aiMesh* mesh);
};