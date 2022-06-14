#pragma once

#include "geom/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

struct aiMesh;
struct aiMaterial;

class ArtObjectParser
{
    using GeometryResult = std::optional<Geometry>;

public:
    ArtObjectParser();
    ~ArtObjectParser();

    GeometryResult parse(const QString& path) noexcept;

private:
    QDomDocument m_Document;
};
