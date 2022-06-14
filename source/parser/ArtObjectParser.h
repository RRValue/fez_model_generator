#pragma once

#include "geom/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

struct aiMesh;
struct aiMaterial;

class ArtObjectParser
{
public:
    ArtObjectParser();
    ~ArtObjectParser();

    void parse(const QString& path) noexcept;

private:
    QDomDocument m_Document;
};
