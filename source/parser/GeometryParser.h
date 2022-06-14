#pragma once

#include "geom/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class GeometryParser
{
    using GeometryResult = std::optional<Geometry>;
    using VerticesResult = std::optional<Geometry::Vertices>;
    using IndicesResult = std::optional<Geometry::Indices>;

public:
    GeometryParser();
    ~GeometryParser();

    GeometryResult parseGeometry(const QDomElement& elem);

private:
    VerticesResult parseVertices(const QDomElement& elem);
    IndicesResult parseIndices(const QDomElement& elem);

private:
    QDomDocument m_Document;
    QString m_Name;
    QString m_Path;
};
