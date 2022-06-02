#pragma once

#include "geom/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class ArtObjectParser
{
    using GeometryResult = std::optional<Geometry>;
    using VerticesResult = std::optional<Geometry::Vertices>;
    using TrianglesResult = std::optional<Geometry::Triangles>;

public:
    ArtObjectParser();
    ~ArtObjectParser();

    void parse(const QString& path) noexcept;

private:
    GeometryResult parseGeometry(const QDomElement& elem);
    VerticesResult parseVertices(const QDomElement& elem);
    TrianglesResult parseTriangles(const QDomElement& elem);

private:
    QDomDocument m_Document;
};