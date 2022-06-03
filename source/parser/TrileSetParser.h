#pragma once

#include "geom/Geometry.h"
#include "parser/GeometryParser.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class TrileSetParser
{
public:
    TrileSetParser();
    ~TrileSetParser();

    void parse(const QString& path) noexcept;

private:
    void parserTrile(const QDomElement& elem);
    void writeObj(const Geometry& geometry);

private:
    QDomDocument m_Document;
    QString m_Name;
    QString m_Path;

    QString m_SetName;
    QString m_Key;

    GeometryParser m_GeomParser;
};