#pragma once

#include "geom/Geometry.h"
#include "parser/GeometryParser.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class ArtObjectParser
{
public:
    ArtObjectParser();
    ~ArtObjectParser();

    void parse(const QString& path) noexcept;

private:
    void writeObj(const Geometry& geometry);

private:
    QDomDocument m_Document;
    QString m_Name;
    QString m_Path;

    GeometryParser m_GeomParser;
};