#pragma once

#include "geom/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class ArtObjectParser
{
    using GeometryResult = std::optional<Geometry>;
    using VerticesResult = std::optional<Geometry::Vertices>;
    using IndicesResult = std::optional<Geometry::Indices>;

public:
    ArtObjectParser();
    ~ArtObjectParser();

    void parse(const QString& path) noexcept;

private:
    GeometryResult parseGeometry(const QDomElement& elem);
    VerticesResult parseVertices(const QDomElement& elem);
    IndicesResult parseIndices(const QDomElement& elem);

    void writeObj(const Geometry& geometry);

private:
    QDomDocument m_Document;
    QString m_Name;
    QString m_Path;
};